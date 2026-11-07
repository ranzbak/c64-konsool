#include "DiskController.hpp"
#include <cstring>

DiskController::DiskController()
{
    memset(gcrTrack, GCR_GAP_BYTE, sizeof(gcrTrack));
}

void DiskController::reset()
{
    flushTrack();
    halfTrack = 2 * 18;
    headPos   = 0;  // a reset does park the head
    // Reload rather than just clearing: a reset parks the head on the
    // directory track, it does not take the disk out. Clearing without
    // reloading leaves the head reading gap forever, which looks exactly like
    // an unformatted disk.
    loadTrack();
}

void DiskController::setDisk(DiskImage* image)
{
    flushTrack();
    disk        = image;
    trackLoaded = false;
    trackDirty  = false;

    id1 = 0;
    id2 = 0;
    if (disk != nullptr) {
        // The disk id lives in the BAM and is part of every sector header, so
        // the drive's own checks only pass if it matches the real disk.
        uint8_t bam[CBM_SECTOR_SIZE];
        if (disk->readSector(disk->dirTrack(), 0, bam)) {
            id1 = bam[0xA2];
            id2 = bam[0xA3];
        }
    }
    loadTrack();
}

void DiskController::loadTrack()
{
    trackLoaded = false;
    trackDirty  = false;
    memset(gcrTrack, GCR_GAP_BYTE, sizeof(gcrTrack));

    unsigned int track = currentTrack();

    // The head is not put back to the start of the track. The disk keeps
    // turning while the head steps, so it comes down wherever the rotation
    // has got to, not at the start of a sector. Zeroing here restarts the
    // revolution on every half step, and a 1541 half steps constantly while
    // it settles on a track, so the DOS never got to see the sectors near the
    // end of a track at all.
    //
    // Tracks are not all the same length, so the position is carried over as
    // a fraction of a revolution rather than as a byte count. VICE does the
    // same in drive_set_half_track().
    unsigned int newLen = gcrTrackBytes(track);
    headPos             = trackLen != 0 ? static_cast<unsigned int>(
                              (static_cast<uint64_t>(headPos) * newLen) / trackLen)
                                        : 0;
    trackLen            = newLen;
    if (headPos >= trackLen) headPos = 0;

    if (disk == nullptr) return;
    if (track < 1 || track > disk->tracks()) return;

    trackLoaded = gcrEncodeTrack(*disk, track, id1, id2, gcrTrack);
}

void DiskController::moveHeadOut()
{
    if (halfTrack > 2) {
        // Anything written to this track has to reach the image before the
        // head leaves it.
        flushTrack();
        halfTrack--;
        loadTrack();
    }
}

void DiskController::moveHeadIn()
{
    // A 1541 can step past the last formatted track; the head just finds
    // nothing there.
    if (halfTrack < 2 * 42) {
        flushTrack();
        halfTrack++;
        loadTrack();
    }
}

void DiskController::flush()
{
    flushTrack();
}

// Walks the track looking for sector headers, and writes back the data block
// that follows each one. The drive only rewrites data blocks, leaving headers
// alone, so the headers are what say where each block belongs.
void DiskController::flushTrack()
{
    if (!trackDirty || !trackLoaded || disk == nullptr || !disk->writable()) {
        trackDirty = false;
        return;
    }

    unsigned int sectors = disk->sectorsPerTrack(currentTrack());
    unsigned int stride  = gcrSectorSizeForTrack(currentTrack());
    for (unsigned int sector = 0; sector < sectors; sector++) {
        const uint8_t* sectorGcr = gcrTrack + sector * stride;

        // Confirm the header still says what we think it does before writing
        // anywhere, so a garbled track cannot scribble over the wrong sector.
        uint8_t header[4];
        if (!gcrDecode5(sectorGcr + GCR_SYNC_BYTES, header)) continue;
        if (header[0] != 0x08) continue;
        if (header[2] != sector || header[3] != currentTrack()) continue;

        uint8_t block[CBM_SECTOR_SIZE];
        if (!gcrDecodeSector(sectorGcr, block)) continue;

        disk->writeSector(currentTrack(), sector, block);
    }

    trackDirty = false;
}

void DiskController::writeGcrByte(uint8_t value)
{
    if (!trackLoaded) return;

    gcrTrack[headPos] = value;
    trackDirty        = true;

    headPos++;
    if (headPos >= trackLen) headPos = 0;
}

uint8_t DiskController::readGcrByte()
{
    if (!trackLoaded) return GCR_GAP_BYTE;

    uint8_t value = gcrTrack[headPos];
    headPos++;
    if (headPos >= trackLen) headPos = 0;
    return value;
}

void DiskController::rotate()
{
    if (!trackLoaded) return;
    headPos++;
    if (headPos >= trackLen) headPos = 0;
}

bool DiskController::syncFound() const
{
    if (!trackLoaded) return false;
    if (gcrTrack[headPos] != 0xff) return false;

    // A sync mark is a run of one bits, and the drive writes five whole bytes
    // of them. A single $ff on its own is not one: encoded data really does
    // contain byte aligned $ff bytes, dozens of them on a real disk, and
    // treating those as a sync mark holds off BYTE READY in the middle of a
    // data block. The byte is lost, the checksum fails, and the drive reports
    // error 23. Requiring a second $ff next to it is enough to tell them
    // apart, since the encoding cannot produce sixteen one bits in a row.
    unsigned int prev = (headPos == 0) ? (trackLen - 1) : (headPos - 1);
    unsigned int next = (headPos + 1 >= trackLen) ? 0 : (headPos + 1);
    return gcrTrack[prev] == 0xff || gcrTrack[next] == 0xff;
}

// How long each part of a swap is held, in drive cycles. Following VICE's
// drive_writeprotect_sense(): the disk coming out, then the gap before another
// can go in, then the new one going in. The drive cannot see a disk change any
// other way, so these have to last long enough for the DOS to notice.
static const unsigned int CHANGE_REMOVE = 200000;   // disk pulled out
static const unsigned int CHANGE_GAP    = 400000;   // nothing in the drive
static const unsigned int CHANGE_INSERT = 600000;   // new disk going in
static const unsigned int CHANGE_TOTAL  = CHANGE_REMOVE + CHANGE_GAP + CHANGE_INSERT;

void DiskController::swapDisk(DiskImage* image)
{
    // Anything written to the old disk has to reach it before it comes out.
    flushTrack();

    disk        = image;
    trackLoaded = false;
    trackDirty  = false;

    id1 = 0;
    id2 = 0;
    if (disk != nullptr) {
        uint8_t bam[CBM_SECTOR_SIZE];
        if (disk->readSector(disk->dirTrack(), 0, bam)) {
            id1 = bam[0xA2];
            id2 = bam[0xA3];
        }
    }

    // Deliberately not reset(): the head stays on its track and the drive CPU
    // is left alone. loadTrack() keeps the rotational position, so the disk
    // carries on turning from where it was.
    loadTrack();

    changeCycles = CHANGE_TOTAL;
}

void DiskController::countChange(unsigned int cycles)
{
    if (changeCycles == 0) return;
    changeCycles = (changeCycles > cycles) ? changeCycles - cycles : 0;
}

uint8_t DiskController::writeProtectBit() const
{
    // While a disk is being swapped the line moves through the states a real
    // drive produces, whatever the disks themselves say. Without this the
    // drive has no way of knowing anything happened.
    if (changeCycles != 0) {
        if (changeCycles > CHANGE_GAP + CHANGE_INSERT) return 0x00;  // coming out
        if (changeCycles > CHANGE_INSERT) return 0x10;               // drive empty
        return 0x00;                                                 // going in
    }

    // Bit 4 low means write protected.
    if (disk != nullptr && disk->writable()) return 0x10;
    return 0x00;
}

uint8_t DiskController::speedZone() const
{
    return static_cast<uint8_t>(gcrSpeedZone(currentTrack()));
}
