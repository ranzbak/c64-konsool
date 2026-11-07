#include "T64Image.hpp"
#include <fcntl.h>
#include <unistd.h>
#include <algorithm>
#include <cstring>

// Header layout
static const int T64_HDR_SIZE     = 0x40;
static const int T64_HDR_MAGIC    = 0x00;
static const int T64_HDR_REC_MAX  = 0x22;
static const int T64_HDR_REC_USED = 0x24;

// Record layout
static const int T64_REC_SIZE       = 0x20;
static const int T64_REC_C64S_TYPE  = 0x00;
static const int T64_REC_START_ADDR = 0x02;
static const int T64_REC_END_ADDR   = 0x04;
static const int T64_REC_CONTENTS   = 0x08;
static const int T64_REC_FILENAME   = 0x10;
static const int T64_REC_NAME_LEN   = 0x10;

// The signature is not written consistently, so accept every variant that is
// known to occur in the wild.
static const char* kMagicStrings[] = {
    "C64S tape image file",
    "C64S tape file",
    "C64 tape image file",
};

static uint16_t le16(const uint8_t* p)
{
    return static_cast<uint16_t>(p[0] | (p[1] << 8));
}

static uint32_t le32(const uint8_t* p)
{
    return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) | (static_cast<uint32_t>(p[2]) << 16) |
           (static_cast<uint32_t>(p[3]) << 24);
}

// read() can come back short on a slow card, so always loop.
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

T64Image::~T64Image()
{
    close();
}

void T64Image::close()
{
    if (fd >= 0) {
        ::close(fd);
        fd = -1;
    }
    records.clear();
    imageEntries.clear();
}

bool T64Image::open(const char* path)
{
    close();

    fd = ::open(path, O_RDONLY);
    if (fd < 0) return false;

    off_t fileSize = lseek(fd, 0, SEEK_END);
    if (fileSize < T64_HDR_SIZE || lseek(fd, 0, SEEK_SET) != 0) {
        close();
        return false;
    }

    uint8_t hdr[T64_HDR_SIZE];
    if (!readFully(fd, hdr, sizeof(hdr))) {
        close();
        return false;
    }

    bool magicOk = false;
    for (size_t i = 0; i < sizeof(kMagicStrings) / sizeof(kMagicStrings[0]); i++) {
        if (memcmp(hdr + T64_HDR_MAGIC, kMagicStrings[i], strlen(kMagicStrings[i])) == 0) {
            magicOk = true;
            break;
        }
    }
    if (!magicOk) {
        close();
        return false;
    }

    // Both counters are regularly written as zero; a container with a bad
    // count still holds one usable file, so fall back to one record.
    uint32_t recMax  = le16(hdr + T64_HDR_REC_MAX);
    uint32_t recUsed = le16(hdr + T64_HDR_REC_USED);
    if (recMax == 0) recMax = 1;
    if (recUsed == 0) recUsed = 1;
    if (recUsed > recMax) recUsed = recMax;

    // Never trust the count past the end of the actual file.
    uint32_t maxRecords = static_cast<uint32_t>((fileSize - T64_HDR_SIZE) / T64_REC_SIZE);
    if (recUsed > maxRecords) recUsed = maxRecords;

    for (uint32_t i = 0; i < recUsed; i++) {
        uint8_t rec[T64_REC_SIZE];
        if (lseek(fd, T64_HDR_SIZE + static_cast<off_t>(i) * T64_REC_SIZE, SEEK_SET) < 0) break;
        if (!readFully(fd, rec, sizeof(rec))) break;

        // Type 0 is a free slot, anything above 1 is a memory snapshot rather
        // than a file. Neither can be loaded.
        uint8_t c64sType = rec[T64_REC_C64S_TYPE];
        if (c64sType == 0 || c64sType > 1) continue;

        Record r;
        r.offset         = le32(rec + T64_REC_CONTENTS);
        r.startAddr      = le16(rec + T64_REC_START_ADDR);
        uint16_t endAddr = le16(rec + T64_REC_END_ADDR);
        r.length         = static_cast<uint16_t>(endAddr > r.startAddr ? endAddr - r.startAddr : 0);

        if (r.offset >= static_cast<uint32_t>(fileSize)) continue;

        ImageEntry entry;
        entry.loadable = true;  // every record in a tape image can be extracted
        entry.name = petsciiToDisplay(rec + T64_REC_FILENAME, T64_REC_NAME_LEN);
        // Keep the original bytes so the name can be drawn in the C64 charset.
        entry.petscii = petsciiRaw(rec + T64_REC_FILENAME, T64_REC_NAME_LEN);
        entry.index   = static_cast<uint16_t>(records.size());
        entry.blocks  = 0;
        if (entry.name.empty()) entry.name = "(unnamed)";

        records.push_back(r);
        imageEntries.push_back(entry);
    }

    if (records.empty()) {
        close();
        return false;
    }

    // Correct the lengths. The end address in a record cannot be trusted, but
    // the data blobs are laid out back to back, so the distance to the next
    // blob (or to the end of the file, for the last one) is the real length.
    std::vector<size_t> byOffset(records.size());
    for (size_t i = 0; i < records.size(); i++) {
        byOffset[i] = i;
    }
    const std::vector<Record>& recs = records;
    std::sort(byOffset.begin(), byOffset.end(),
              [&recs](size_t a, size_t b) { return recs[a].offset < recs[b].offset; });

    for (size_t i = 0; i < byOffset.size(); i++) {
        Record&  r      = records[byOffset[i]];
        uint32_t next   = (i + 1 < byOffset.size()) ? records[byOffset[i + 1]].offset : static_cast<uint32_t>(fileSize);
        uint32_t actual = (next > r.offset) ? next - r.offset : 0;
        bool     isLast = (i + 1 == byOffset.size());

        // The last blob is often padded, so only grow it, never shrink a
        // record that already claims less than the space available.
        if (r.length != actual && !(isLast && r.length < actual && r.length != 0)) {
            r.length = static_cast<uint16_t>(actual > 0xFFFF ? 0xFFFF : actual);
        }

        // Never let a file run off the top of the address space. The highest
        // end address we allow is $FFFF so the value handed to VARTAB cannot
        // wrap back to zero.
        uint32_t room = (C64_RAM_SIZE - 1) - r.startAddr;
        if (r.length > room) {
            r.length = static_cast<uint16_t>(room);
        }

        imageEntries[byOffset[i]].blocks = static_cast<uint16_t>((r.length + 253) / 254);
    }

    return true;
}

bool T64Image::extract(uint16_t index, uint8_t* ram, uint16_t* endAddr)
{
    if (fd < 0 || index >= records.size()) return false;

    const Record& r = records[index];
    if (r.length == 0) return false;
    if (lseek(fd, static_cast<off_t>(r.offset), SEEK_SET) < 0) return false;

    // A truncated container is still worth loading as far as it goes, so
    // count what actually arrived rather than insisting on the full length.
    size_t   remaining = r.length;
    uint8_t* dst       = ram + r.startAddr;
    size_t   total     = 0;
    while (remaining > 0) {
        ssize_t got = read(fd, dst + total, remaining);
        if (got <= 0) break;
        total     += static_cast<size_t>(got);
        remaining -= static_cast<size_t>(got);
    }
    if (total == 0) return false;

    *endAddr = static_cast<uint16_t>(r.startAddr + total);
    return true;
}
