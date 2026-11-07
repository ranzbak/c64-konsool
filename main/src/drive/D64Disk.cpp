#include "D64Disk.hpp"
#include <fcntl.h>
#include <unistd.h>

static const unsigned int MIN_TRACKS = 35;
static const unsigned int MAX_TRACKS = 42;

static bool writeFully(int fd, const void* buf, size_t len)
{
    const uint8_t* p = static_cast<const uint8_t*>(buf);
    while (len > 0) {
        ssize_t put = write(fd, p, len);
        if (put <= 0) return false;
        p   += put;
        len -= static_cast<size_t>(put);
    }
    return true;
}

static bool readFully(int fd, void* buf, size_t len)
{
    uint8_t* p = static_cast<uint8_t*>(buf);
    while (len > 0) {
        ssize_t got = read(fd, p, len);
        if (got <= 0) return false;
        p   += got;
        len -= static_cast<size_t>(got);
    }
    return true;
}

unsigned int D64Disk::sectorsOnTrack(unsigned int track)
{
    if (track < 1 || track > MAX_TRACKS) return 0;
    if (track <= 17) return 21;
    if (track <= 24) return 19;
    if (track <= 30) return 18;
    return 17;  // tracks 31 and up, including the non standard extra tracks
}

unsigned int D64Disk::sectorsPerTrack(unsigned int track) const
{
    return sectorsOnTrack(track);
}

D64Disk::~D64Disk()
{
    close();
}

void D64Disk::close()
{
    if (fd >= 0) {
        ::close(fd);
        fd = -1;
    }
    trackCount = MIN_TRACKS;
    readOnly   = true;
}

bool D64Disk::open(const char* path)
{
    close();

    // Prefer read and write so the C64 can save; a card or file that will not
    // allow it still opens read only.
    fd       = ::open(path, O_RDWR);
    readOnly = false;
    if (fd < 0) {
        fd       = ::open(path, O_RDONLY);
        readOnly = true;
    }
    if (fd < 0) return false;

    off_t fileSize = lseek(fd, 0, SEEK_END);
    if (fileSize < 0) {
        close();
        return false;
    }

    // Walk 35 up to 42 tracks and see which track count the file size matches,
    // with or without the trailing error info byte per block.
    unsigned int candidate = MIN_TRACKS;
    off_t        blocks    = 683;  // blocks on a 35 track disk
    bool         matched   = false;
    while (true) {
        off_t bytes = blocks * static_cast<off_t>(CBM_SECTOR_SIZE);
        if (fileSize == bytes || fileSize == bytes + blocks) {
            matched = true;
            break;
        }
        candidate++;
        blocks += 17;
        if (candidate > MAX_TRACKS) break;
    }
    if (!matched) {
        close();
        return false;
    }

    trackCount = candidate;
    return true;
}

bool D64Disk::readSector(unsigned int track, unsigned int sector, uint8_t* buf)
{
    if (fd < 0) return false;
    unsigned int perTrack = sectorsOnTrack(track);
    if (perTrack == 0 || track > trackCount || sector >= perTrack) return false;

    // Sum the sectors on every track before this one to find the offset.
    off_t offset = 0;
    for (unsigned int t = 1; t < track; t++) {
        offset += static_cast<off_t>(sectorsOnTrack(t)) * CBM_SECTOR_SIZE;
    }
    offset += static_cast<off_t>(sector) * CBM_SECTOR_SIZE;

    if (lseek(fd, offset, SEEK_SET) < 0) return false;
    return readFully(fd, buf, CBM_SECTOR_SIZE);
}

bool D64Disk::writeSector(unsigned int track, unsigned int sector, const uint8_t* buf)
{
    if (!writable()) return false;
    unsigned int perTrack = sectorsOnTrack(track);
    if (perTrack == 0 || track > trackCount || sector >= perTrack) return false;

    off_t offset = 0;
    for (unsigned int t = 1; t < track; t++) {
        offset += static_cast<off_t>(sectorsOnTrack(t)) * CBM_SECTOR_SIZE;
    }
    offset += static_cast<off_t>(sector) * CBM_SECTOR_SIZE;

    if (lseek(fd, offset, SEEK_SET) < 0) return false;
    return writeFully(fd, buf, CBM_SECTOR_SIZE);
}
