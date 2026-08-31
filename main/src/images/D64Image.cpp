#include "D64Image.hpp"
#include <cstring>

static const unsigned int SECTOR_SIZE   = 256;
static const unsigned int DIR_TRACK     = 18;
static const unsigned int DIR_SECTOR    = 1;
static const unsigned int SLOTS_PER_DIR = 8;
static const unsigned int SLOT_SIZE     = 32;

// Directory slot layout
static const int SLOT_TYPE         = 2;
static const int SLOT_FIRST_TRACK  = 3;
static const int SLOT_FIRST_SECTOR = 4;
static const int SLOT_NAME         = 5;
static const int SLOT_NAME_LEN     = 16;
static const int SLOT_NR_BLOCKS    = 30;

// CBM DOS file types
static const uint8_t FT_MASK   = 0x07;
static const uint8_t FT_PRG    = 0x02;
static const uint8_t FT_CLOSED = 0x80;

bool D64Image::open(const char* path)
{
    close();

    if (!disk.open(path)) return false;

    // The directory always starts at 18/1, even when the link bytes in the
    // BAM sector at 18/0 claim otherwise.
    unsigned int track   = DIR_TRACK;
    unsigned int sector  = DIR_SECTOR;
    unsigned int visited = 0;

    // A directory cannot be longer than the disk, and a corrupt image can
    // point its chain back at itself, so cap the walk.
    const unsigned int maxDirSectors = 40;

    uint8_t buf[SECTOR_SIZE];
    while (visited < maxDirSectors) {
        if (!disk.readSector(track, sector, buf)) break;
        visited++;

        for (unsigned int slot = 0; slot < SLOTS_PER_DIR; slot++) {
            const uint8_t* e    = buf + slot * SLOT_SIZE;
            uint8_t        type = e[SLOT_TYPE];

            // The DOS lists every slot whose type byte is not zero, and a
            // disk draws its title box with scratched slots, so dropping
            // everything that is not a PRG threw the artwork away before it
            // could be shown. Keep those rows, but only a PRG can be loaded.
            if (type == 0) continue;
            bool loadable = (type & FT_MASK) == FT_PRG;

            unsigned int ft = e[SLOT_FIRST_TRACK];
            unsigned int fs = e[SLOT_FIRST_SECTOR];
            if (loadable && (ft < 1 || ft > disk.tracks())) continue;
            if (loadable && fs >= disk.sectorsPerTrack(ft)) continue;

            Entry entry;
            entry.track  = static_cast<uint8_t>(ft);
            entry.sector = static_cast<uint8_t>(fs);

            ImageEntry item;
            item.loadable = loadable;
            item.name = petsciiToDisplay(e + SLOT_NAME, SLOT_NAME_LEN);
            // Keep the bytes as they were as well. A directory is full of
            // graphics characters, and only the original bytes can be drawn
            // with the C64's own charset.
            item.petscii = petsciiRaw(e + SLOT_NAME, SLOT_NAME_LEN);
            item.index   = static_cast<uint16_t>(dirEntries.size());
            item.blocks  = static_cast<uint16_t>(e[SLOT_NR_BLOCKS] | (e[SLOT_NR_BLOCKS + 1] << 8));
            if (item.name.empty()) item.name = "(unnamed)";
            // An unclosed "splat" file was never written completely. Show it,
            // marked the way a C64 directory listing marks it, but expect the
            // sector chain to be broken.
            if (!(type & FT_CLOSED)) {
                item.name += "*";
                if (!item.petscii.empty()) item.petscii += '*';
            }

            if (loadable) {
                dirEntries.push_back(entry);
            } else {
                // Nothing to extract, so it must not claim a sector chain.
                item.index = 0xFFFF;
            }
            imageEntries.push_back(item);
        }

        if (buf[0] == 0) break;  // end of the directory chain
        track  = buf[0];
        sector = buf[1];
    }

    if (dirEntries.empty()) {
        close();
        return false;
    }
    return true;
}

D64Image::~D64Image()
{
    close();
}

void D64Image::close()
{
    disk.close();
    dirEntries.clear();
    imageEntries.clear();
}

bool D64Image::extract(uint16_t index, uint8_t* ram, uint16_t* endAddr)
{
    if (!disk.isOpen() || index >= dirEntries.size()) return false;

    unsigned int track  = dirEntries[index].track;
    unsigned int sector = dirEntries[index].sector;

    uint8_t buf[SECTOR_SIZE];
    if (!disk.readSector(track, sector, buf)) return false;

    // The first two bytes of the first sector are the link, and the file's own
    // byte stream starts right after. For a PRG that stream opens with the
    // load address.
    unsigned int used = (buf[0] == 0) ? (buf[1] >= 2 ? buf[1] - 1u : 0u) : 254u;
    if (used < 2) return false;

    uint16_t     startAddr = static_cast<uint16_t>(buf[2] | (buf[3] << 8));
    unsigned int pos       = startAddr;
    unsigned int skip      = 2;  // the load address, only on the first sector

    // Follow the chain, bounded by the number of blocks the disk can hold so a
    // circular chain cannot spin forever.
    unsigned int maxSectors = 0;
    for (unsigned int t = 1; t <= disk.tracks(); t++) {
        maxSectors += disk.sectorsPerTrack(t);
    }

    for (unsigned int count = 0; count < maxSectors; count++) {
        unsigned int avail = used - skip;
        if (avail > 0) {
            // Stop at the top of the address space rather than running past
            // the end of the 64K RAM buffer.
            if (pos >= C64_RAM_SIZE - 1) break;
            unsigned int room = (C64_RAM_SIZE - 1) - pos;
            if (avail > room) avail = room;
            memcpy(ram + pos, buf + 2 + skip, avail);
            pos += avail;
        }

        if (buf[0] == 0) break;  // last sector of the file

        track  = buf[0];
        sector = buf[1];
        if (!disk.readSector(track, sector, buf)) break;

        used = (buf[0] == 0) ? (buf[1] >= 2 ? buf[1] - 1u : 0u) : 254u;
        skip = 0;
    }

    if (pos == startAddr) return false;
    *endAddr = static_cast<uint16_t>(pos);
    return true;
}
