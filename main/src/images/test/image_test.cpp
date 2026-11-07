/*
 Host side tests for the T64 and D64 readers.

 The readers deliberately depend on nothing but POSIX file calls, so they can
 be built and exercised on a normal machine instead of only on the badge.

     make -C main/src/images/test run
*/

#include <fcntl.h>
#include <unistd.h>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include "../CbmImage.hpp"
#include "../D64Image.hpp"
#include "../T64Image.hpp"

static int failures = 0;
static int checks   = 0;

#define CHECK(cond, ...)                                  \
    do {                                                  \
        checks++;                                         \
        if (!(cond)) {                                    \
            failures++;                                   \
            printf("  FAIL %s:%d: ", __FILE__, __LINE__); \
            printf(__VA_ARGS__);                          \
            printf("\n");                                 \
        }                                                 \
    } while (0)

static std::string tmpPath(const char* name)
{
    return std::string("/tmp/konsool-imgtest-") + name;
}

static void writeFile(const std::string& path, const std::vector<uint8_t>& data)
{
    int fd = open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        printf("cannot write %s\n", path.c_str());
        return;
    }
    ssize_t written = write(fd, data.data(), data.size());
    (void)written;
    close(fd);
}

static void putLe16(std::vector<uint8_t>& v, size_t off, uint16_t val)
{
    v[off]     = static_cast<uint8_t>(val & 0xff);
    v[off + 1] = static_cast<uint8_t>(val >> 8);
}

static void putLe32(std::vector<uint8_t>& v, size_t off, uint32_t val)
{
    v[off]     = static_cast<uint8_t>(val & 0xff);
    v[off + 1] = static_cast<uint8_t>((val >> 8) & 0xff);
    v[off + 2] = static_cast<uint8_t>((val >> 16) & 0xff);
    v[off + 3] = static_cast<uint8_t>((val >> 24) & 0xff);
}

static void putPetName(std::vector<uint8_t>& v, size_t off, const char* name, uint8_t pad, size_t len)
{
    for (size_t i = 0; i < len; i++) {
        v[off + i] = (i < strlen(name)) ? static_cast<uint8_t>(name[i]) : pad;
    }
}

/* ------------------------------------------------------------------ T64 -- */

struct T64File {
    const char* name;
    uint16_t    startAddr;
    uint16_t    declaredEnd;  // what to write in the record, may be wrong
    size_t      dataLen;
};

// Builds a T64 whose records are laid out back to back after the record table.
static std::vector<uint8_t> buildT64(const std::vector<T64File>& files, uint16_t recMax, uint16_t recUsed,
                                     const char* magic, size_t trailingPad = 0)
{
    size_t dirSize    = files.size() * 32;
    size_t dataOffset = 0x40 + dirSize;

    size_t total = dataOffset + trailingPad;
    for (size_t i = 0; i < files.size(); i++) {
        total += files[i].dataLen;
    }

    std::vector<uint8_t> img(total, 0);
    memcpy(&img[0], magic, strlen(magic));
    putLe16(img, 0x20, 0x0101);
    putLe16(img, 0x22, recMax);
    putLe16(img, 0x24, recUsed);

    size_t off = dataOffset;
    for (size_t i = 0; i < files.size(); i++) {
        size_t rec      = 0x40 + i * 32;
        img[rec + 0x00] = 1;     // normal tape file
        img[rec + 0x01] = 0x82;  // PRG
        putLe16(img, rec + 0x02, files[i].startAddr);
        putLe16(img, rec + 0x04, files[i].declaredEnd);
        putLe32(img, rec + 0x08, static_cast<uint32_t>(off));
        putPetName(img, rec + 0x10, files[i].name, 0x20, 16);

        // fill the payload with a recognisable ramp
        for (size_t b = 0; b < files[i].dataLen; b++) {
            img[off + b] = static_cast<uint8_t>((i * 7 + b) & 0xff);
        }
        off += files[i].dataLen;
    }
    return img;
}

static void testT64Basic()
{
    printf("T64: two files, honest end addresses\n");
    std::vector<T64File> files;
    T64File              a = {"HELLO", 0x0801, static_cast<uint16_t>(0x0801 + 100), 100};
    T64File              b = {"SECOND", 0xC000, static_cast<uint16_t>(0xC000 + 50), 50};
    files.push_back(a);
    files.push_back(b);

    std::string path = tmpPath("basic.t64");
    writeFile(path, buildT64(files, 2, 2, "C64S tape image file"));

    T64Image img;
    CHECK(img.open(path.c_str()), "open failed");
    CHECK(img.entries().size() == 2, "expected 2 entries, got %zu", img.entries().size());
    CHECK(img.entries()[0].name == "HELLO", "name was '%s'", img.entries()[0].name.c_str());
    CHECK(img.entries()[1].name == "SECOND", "name was '%s'", img.entries()[1].name.c_str());
    CHECK(img.entries()[0].petscii == "HELLO", "raw name was '%s'", img.entries()[0].petscii.c_str());

    std::vector<uint8_t> ram(C64_RAM_SIZE, 0);
    uint16_t             end = 0;
    CHECK(img.extract(0, ram.data(), &end), "extract 0 failed");
    CHECK(end == 0x0801 + 100, "end was $%04x", end);
    CHECK(ram[0x0801] == 0 && ram[0x0801 + 99] == 99, "payload mismatch");

    CHECK(img.extract(1, ram.data(), &end), "extract 1 failed");
    CHECK(end == 0xC000 + 50, "end was $%04x", end);
    CHECK(ram[0xC000] == 7, "payload mismatch, got %u", ram[0xC000]);
}

static void testT64BadEndAddress()
{
    printf("T64: the $C3C6 end address bug\n");
    std::vector<T64File> files;
    // CONV64 wrote $C3C6 as the end address of every file it packed.
    T64File              a = {"BROKEN", 0x0801, 0xC3C6, 100};
    T64File              b = {"AFTER", 0x2000, 0xC3C6, 40};
    files.push_back(a);
    files.push_back(b);

    std::string path = tmpPath("badend.t64");
    writeFile(path, buildT64(files, 2, 2, "C64S tape image file"));

    T64Image img;
    CHECK(img.open(path.c_str()), "open failed");
    std::vector<uint8_t> ram(C64_RAM_SIZE, 0);
    uint16_t             end = 0;

    // The real length has to come from the gap to the next data blob, not
    // from the bogus end address.
    CHECK(img.extract(0, ram.data(), &end), "extract 0 failed");
    CHECK(end == 0x0801 + 100, "end was $%04x, expected $%04x", end, 0x0801 + 100);
    CHECK(img.extract(1, ram.data(), &end), "extract 1 failed");
    CHECK(end == 0x2000 + 40, "end was $%04x, expected $%04x", end, 0x2000 + 40);
}

static void testT64ZeroCounters()
{
    printf("T64: zeroed record counters\n");
    std::vector<T64File> files;
    T64File              a = {"ONLYONE", 0x0801, static_cast<uint16_t>(0x0801 + 64), 64};
    files.push_back(a);

    std::string path = tmpPath("zerocount.t64");
    writeFile(path, buildT64(files, 0, 0, "C64S tape image file"));

    T64Image img;
    CHECK(img.open(path.c_str()), "open failed with zeroed counters");
    CHECK(img.entries().size() == 1, "expected 1 entry, got %zu", img.entries().size());
}

static void testT64AltMagicAndPadding()
{
    printf("T64: alternate signature and a padded last record\n");
    std::vector<T64File> files;
    T64File              a = {"PADDED", 0x0801, static_cast<uint16_t>(0x0801 + 30), 30};
    files.push_back(a);

    std::string path = tmpPath("padded.t64");
    // 200 bytes of slack after the payload; the declared length is smaller
    // than the space available, which is legitimate padding and must be kept.
    writeFile(path, buildT64(files, 1, 1, "C64 tape image file", 200));

    T64Image img;
    CHECK(img.open(path.c_str()), "open failed for alternate magic");
    std::vector<uint8_t> ram(C64_RAM_SIZE, 0);
    uint16_t             end = 0;
    CHECK(img.extract(0, ram.data(), &end), "extract failed");
    CHECK(end == 0x0801 + 30, "padding was loaded as data, end was $%04x", end);
}

static void testT64Overflow()
{
    printf("T64: entry that would run past $FFFF is clamped\n");
    std::vector<T64File> files;
    // Loads near the top of memory and claims far more data than fits.
    T64File              a = {"TOOBIG", 0xFF00, 0xFFFF, 0x4000};
    files.push_back(a);

    std::string path = tmpPath("overflow.t64");
    writeFile(path, buildT64(files, 1, 1, "C64S tape image file"));

    T64Image img;
    CHECK(img.open(path.c_str()), "open failed");

    std::vector<uint8_t> ram(C64_RAM_SIZE, 0);
    uint16_t             end = 0;
    CHECK(img.extract(0, ram.data(), &end), "extract failed");
    // Address sanitiser catches a write past the buffer; this checks the end
    // address stayed inside the address space as well.
    CHECK(end <= 0xFFFF && end > 0xFF00, "end was $%04x", end);
}

static void testT64Rejects()
{
    printf("T64: rejects non-T64 input\n");
    std::vector<uint8_t> junk(1024, 0x41);
    std::string          path = tmpPath("junk.t64");
    writeFile(path, junk);

    T64Image img;
    CHECK(!img.open(path.c_str()), "accepted a file with no signature");
    CHECK(!img.open("/tmp/konsool-imgtest-does-not-exist"), "accepted a missing file");
}

/* ------------------------------------------------------------------ D64 -- */

static size_t d64Offset(unsigned int track, unsigned int sector)
{
    size_t off = 0;
    for (unsigned int t = 1; t < track; t++) {
        off += D64Image::sectorsPerTrack(t) * 256;
    }
    return off + sector * 256;
}

struct D64File {
    const char*          name;
    std::vector<uint8_t> data;  // including the two byte load address
    bool                 closed;
};

// Lays the files out from track 1 onward and writes a directory on track 18.
static std::vector<uint8_t> buildD64(const std::vector<D64File>& files, unsigned int tracks, bool errorInfo)
{
    size_t blocks = 0;
    for (unsigned int t = 1; t <= tracks; t++) {
        blocks += D64Image::sectorsPerTrack(t);
    }
    std::vector<uint8_t> img(blocks * 256 + (errorInfo ? blocks : 0), 0);

    // Hand out data sectors starting at track 1, skipping the directory track.
    unsigned int nextTrack  = 1;
    unsigned int nextSector = 0;

    struct Placed {
        unsigned int track;
        unsigned int sector;
    };
    std::vector<Placed> placed;

    for (size_t f = 0; f < files.size(); f++) {
        const std::vector<uint8_t>& data      = files[f].data;
        size_t                      pos       = 0;
        Placed                      first     = {0, 0};
        bool                        haveFirst = false;

        while (pos < data.size() || !haveFirst) {
            if (nextTrack == 18) nextTrack = 19;
            unsigned int t = nextTrack;
            unsigned int s = nextSector;
            if (!haveFirst) {
                first.track  = t;
                first.sector = s;
                haveFirst    = true;
            }

            size_t chunk = data.size() - pos;
            if (chunk > 254) chunk = 254;
            size_t off = d64Offset(t, s);
            memcpy(&img[off + 2], &data[pos], chunk);
            pos += chunk;

            // advance the allocation cursor
            nextSector++;
            if (nextSector >= D64Image::sectorsPerTrack(nextTrack)) {
                nextSector = 0;
                nextTrack++;
                if (nextTrack == 18) nextTrack = 19;
            }

            if (pos < data.size()) {
                img[off]     = static_cast<uint8_t>(nextTrack);
                img[off + 1] = static_cast<uint8_t>(nextSector);
            } else {
                // last sector: track 0, and the sector byte points at the
                // last byte in use
                img[off]     = 0;
                img[off + 1] = static_cast<uint8_t>(chunk + 1);
            }
        }
        placed.push_back(first);
    }

    // directory: slots of 32 bytes, 8 per sector, chained from 18/1
    size_t dirOff   = d64Offset(18, 1);
    img[dirOff]     = 0;  // single directory sector is enough for the tests
    img[dirOff + 1] = 0xff;
    for (size_t f = 0; f < files.size() && f < 8; f++) {
        size_t slot   = dirOff + f * 32;
        img[slot + 2] = static_cast<uint8_t>(files[f].closed ? 0x82 : 0x02);
        img[slot + 3] = static_cast<uint8_t>(placed[f].track);
        img[slot + 4] = static_cast<uint8_t>(placed[f].sector);
        putPetName(img, slot + 5, files[f].name, 0xA0, 16);
        uint16_t nblocks = static_cast<uint16_t>((files[f].data.size() + 253) / 254);
        img[slot + 30]   = static_cast<uint8_t>(nblocks & 0xff);
        img[slot + 31]   = static_cast<uint8_t>(nblocks >> 8);
    }
    return img;
}

static std::vector<uint8_t> makePrg(uint16_t loadAddr, size_t payloadLen, uint8_t seed)
{
    std::vector<uint8_t> prg;
    prg.push_back(static_cast<uint8_t>(loadAddr & 0xff));
    prg.push_back(static_cast<uint8_t>(loadAddr >> 8));
    for (size_t i = 0; i < payloadLen; i++) {
        prg.push_back(static_cast<uint8_t>((seed + i) & 0xff));
    }
    return prg;
}

static void testD64SingleSector()
{
    printf("D64: file that fits in one sector\n");
    std::vector<D64File> files;
    D64File              a = {"SMALL", makePrg(0x0801, 100, 1), true};
    files.push_back(a);

    std::string path = tmpPath("small.d64");
    writeFile(path, buildD64(files, 35, false));

    D64Image img;
    CHECK(img.open(path.c_str()), "open failed");
    CHECK(img.entries().size() == 1, "expected 1 entry, got %zu", img.entries().size());
    CHECK(img.entries()[0].name == "SMALL", "name was '%s'", img.entries()[0].name.c_str());

    std::vector<uint8_t> ram(C64_RAM_SIZE, 0);
    uint16_t             end = 0;
    CHECK(img.extract(0, ram.data(), &end), "extract failed");
    CHECK(end == 0x0801 + 100, "end was $%04x, expected $%04x", end, 0x0801 + 100);
    for (size_t i = 0; i < 100; i++) {
        if (ram[0x0801 + i] != static_cast<uint8_t>(1 + i)) {
            CHECK(false, "payload mismatch at %zu", i);
            break;
        }
    }
}

static void testD64MultiSector()
{
    printf("D64: file spanning many sectors\n");
    // 2000 bytes of payload needs nine data sectors and crosses a track
    std::vector<D64File> files;
    D64File              a = {"BIG", makePrg(0x0801, 2000, 5), true};
    files.push_back(a);

    std::string path = tmpPath("big.d64");
    writeFile(path, buildD64(files, 35, false));

    D64Image img;
    CHECK(img.open(path.c_str()), "open failed");
    std::vector<uint8_t> ram(C64_RAM_SIZE, 0);
    uint16_t             end = 0;
    CHECK(img.extract(0, ram.data(), &end), "extract failed");
    CHECK(end == 0x0801 + 2000, "end was $%04x, expected $%04x", end, 0x0801 + 2000);
    for (size_t i = 0; i < 2000; i++) {
        if (ram[0x0801 + i] != static_cast<uint8_t>(5 + i)) {
            CHECK(false, "payload mismatch at %zu", i);
            break;
        }
    }
}

static void testD64Variants()
{
    printf("D64: 40 track and error info variants\n");
    std::vector<D64File> files;
    D64File              a = {"ONFORTY", makePrg(0x0801, 300, 3), true};
    files.push_back(a);

    std::string p40 = tmpPath("forty.d64");
    writeFile(p40, buildD64(files, 40, false));
    D64Image img40;
    CHECK(img40.open(p40.c_str()), "40 track image rejected");
    CHECK(img40.entries().size() == 1, "40 track: expected 1 entry");

    std::string pErr = tmpPath("errinfo.d64");
    writeFile(pErr, buildD64(files, 35, true));
    D64Image imgErr;
    CHECK(imgErr.open(pErr.c_str()), "image with error info rejected");

    std::vector<uint8_t> ram(C64_RAM_SIZE, 0);
    uint16_t             end = 0;
    CHECK(imgErr.extract(0, ram.data(), &end), "extract from error info image failed");
    CHECK(end == 0x0801 + 300, "end was $%04x", end);
}

static void testD64SplatAndRejects()
{
    printf("D64: splat files and invalid input\n");
    std::vector<D64File> files;
    D64File              a = {"OPENFILE", makePrg(0x0801, 60, 9), false};
    files.push_back(a);

    std::string path = tmpPath("splat.d64");
    writeFile(path, buildD64(files, 35, false));

    D64Image img;
    CHECK(img.open(path.c_str()), "open failed");
    CHECK(img.entries().size() == 1, "expected the splat file to be listed");
    CHECK(img.entries()[0].name == "OPENFILE*", "splat marker missing, got '%s'", img.entries()[0].name.c_str());
    CHECK(img.entries()[0].petscii == "OPENFILE*", "raw name not kept, got '%s'",
          img.entries()[0].petscii.c_str());

    // wrong size is not a d64
    std::vector<uint8_t> junk(1000, 0);
    std::string          jpath = tmpPath("junk.d64");
    writeFile(jpath, junk);
    D64Image bad;
    CHECK(!bad.open(jpath.c_str()), "accepted a file of the wrong size");
}

static void testD64Overflow()
{
    printf("D64: file that would run past $FFFF is clamped\n");
    std::vector<D64File> files;
    // 8000 bytes loading at $FF00 cannot possibly fit.
    D64File              a = {"TOOBIG", makePrg(0xFF00, 8000, 4), true};
    files.push_back(a);

    std::string path = tmpPath("overflow.d64");
    writeFile(path, buildD64(files, 35, false));

    D64Image img;
    CHECK(img.open(path.c_str()), "open failed");

    std::vector<uint8_t> ram(C64_RAM_SIZE, 0);
    uint16_t             end = 0;
    CHECK(img.extract(0, ram.data(), &end), "extract failed");
    CHECK(end <= 0xFFFF && end > 0xFF00, "end was $%04x", end);
}

static void testD64CircularChain()
{
    printf("D64: circular sector chain terminates\n");
    std::vector<D64File> files;
    D64File              a = {"LOOP", makePrg(0x0801, 600, 2), true};
    files.push_back(a);

    std::vector<uint8_t> img   = buildD64(files, 35, false);
    // Point the first data sector's link back at itself.
    size_t               first = d64Offset(1, 0);
    img[first]                 = 1;
    img[first + 1]             = 0;

    std::string path = tmpPath("loop.d64");
    writeFile(path, img);

    D64Image d;
    CHECK(d.open(path.c_str()), "open failed");
    std::vector<uint8_t> ram(C64_RAM_SIZE, 0);
    uint16_t             end = 0;
    // Must return rather than spin, and must not run past the address space.
    d.extract(0, ram.data(), &end);
    CHECK(true, "returned from a circular chain");
}

/* ---------------------------------------------------------------- misc -- */

static void testFormatDetection()
{
    printf("format detection by extension\n");
    CHECK(imageFormatFromName("game.prg") == ImageFormat::PRG, "prg");
    CHECK(imageFormatFromName("GAME.PRG") == ImageFormat::PRG, "PRG uppercase");
    CHECK(imageFormatFromName("tape.t64") == ImageFormat::T64, "t64");
    CHECK(imageFormatFromName("disk.D64") == ImageFormat::D64, "D64 uppercase");
    CHECK(imageFormatFromName("readme.txt") == ImageFormat::UNKNOWN, "txt");
    CHECK(imageFormatFromName("noextension") == ImageFormat::UNKNOWN, "no extension");
}

static void testPetsciiNames()
{
    printf("PETSCII name conversion\n");
    uint8_t disk[16];
    memset(disk, 0xA0, sizeof(disk));
    memcpy(disk, "GAME", 4);
    CHECK(petsciiToDisplay(disk, 16) == "GAME", "disk padding not stripped");

    uint8_t tape[16];
    memset(tape, 0x20, sizeof(tape));
    memcpy(tape, "TAPE FILE", 9);
    CHECK(petsciiToDisplay(tape, 16) == "TAPE FILE", "tape padding not stripped");

    // $c1-$c3 are SHIFT+A/B/C: a spade and the two line segments a disk draws
    // its title box from. They are graphics in the charset a directory is
    // listed in, so they must not come back as the letters A, B and C.
    uint8_t art[4] = {0xC1, 0xC2, 0xC3, 0xA0};
    CHECK(petsciiToDisplay(art, 4) == "#|-", "directory art mapped to letters");

    // A real title box, as drawn on kloten.d64.
    uint8_t box[16] = {0xD5, 0xC3, 0xC3, 0xC3, 0xC3, 0xC3, 0xC3, 0xC3,
                       0xC3, 0xC3, 0xC3, 0xC3, 0xC3, 0xC3, 0xC3, 0xC9};
    CHECK(petsciiToDisplay(box, 16) == "+--------------+", "title box not drawn as a box");

    uint8_t control[3] = {0x01, 0x93, 0x41};
    CHECK(petsciiToDisplay(control, 3) == "__A", "control codes not sanitised");
}

static void testPetsciiScreenCodes()
{
    printf("PETSCII to screen code\n");

    // The title box from kloten.d64: SHIFT+U, fourteen SHIFT+C, SHIFT+I, which
    // is a rounded corner, a rule and a rounded corner. In the unshifted
    // charset those are screen codes $55, $43 and $49. Folding the $c0 block
    // onto the letters (the old bug) would give $15, $03, $09 instead.
    uint8_t box[16]   = {0xD5, 0xC3, 0xC3, 0xC3, 0xC3, 0xC3, 0xC3, 0xC3,
                         0xC3, 0xC3, 0xC3, 0xC3, 0xC3, 0xC3, 0xC3, 0xC9};
    bool    boxCodes  = true;
    for (int i = 0; i < 16; i++) {
        uint8_t want = (i == 0) ? 0x55 : (i == 15 ? 0x49 : 0x43);
        if (petsciiToScreenCode(box[i]) != want) boxCodes = false;
    }
    CHECK(boxCodes, "the kloten.d64 title box does not map to $55 $43.. $49");

    // The row below it, "| DEFEEST AT |": SHIFT+B is the vertical rule, and
    // the letters keep their places in the screen code table.
    uint8_t row[6]    = {0xC2, 0x20, 0x44, 0x45, 0x46, 0xC2};
    bool    rowCodes  = petsciiToScreenCode(row[0]) == 0x42 && petsciiToScreenCode(row[1]) == 0x20 &&
                       petsciiToScreenCode(row[2]) == 0x04 && petsciiToScreenCode(row[3]) == 0x05 &&
                       petsciiToScreenCode(row[4]) == 0x06 && petsciiToScreenCode(row[5]) == 0x42;
    CHECK(rowCodes, "letters and the vertical rule do not map to their screen codes");

    // The two blocks that repeat lower down, and the shifted space.
    CHECK(petsciiToScreenCode(0x40) == 0x00, "$40 is screen code $00");
    CHECK(petsciiToScreenCode(0x5A) == 0x1A, "$5a is screen code $1a");
    CHECK(petsciiToScreenCode(0x60) == 0x40, "$60 is screen code $40");
    CHECK(petsciiToScreenCode(0x7F) == 0x5F, "$7f is screen code $5f");
    CHECK(petsciiToScreenCode(0xA0) == 0x60, "the shifted space is screen code $60");
    CHECK(petsciiToScreenCode(0xBF) == 0x7F, "$bf is screen code $7f");
    CHECK(petsciiToScreenCode(0xDF) == 0x5F, "$df repeats $7f");
    CHECK(petsciiToScreenCode(0xE0) == 0x60, "$e0 repeats $a0");
    CHECK(petsciiToScreenCode(0xFF) == 0x7F, "$ff repeats $bf");

    // Nothing in the map may point past the unshifted half of the ROM, or a
    // name would draw whatever happens to follow it.
    bool inRange = true;
    for (int c = 0; c <= 0xFF; c++) {
        if (petsciiToScreenCode(static_cast<uint8_t>(c)) > 0x7F) inRange = false;
    }
    CHECK(inRange, "a screen code landed outside the unshifted charset");

    // Control codes have no glyph. A name must not be able to draw one.
    CHECK(petsciiToScreenCode(0x00) == 0x20, "$00 is not blank");
    CHECK(petsciiToScreenCode(0x1F) == 0x20, "$1f is not blank");
    CHECK(petsciiToScreenCode(0x80) == 0x20, "$80 is not blank");
    CHECK(petsciiToScreenCode(0x93) == 0x20, "clear screen is not blank");
    CHECK(petsciiToScreenCode(0x9F) == 0x20, "$9f is not blank");
}

static void testPetsciiRaw()
{
    printf("PETSCII kept as it was\n");

    uint8_t box[16] = {0xD5, 0xC3, 0xC3, 0xC3, 0xC3, 0xC3, 0xC3, 0xC3,
                       0xC3, 0xC3, 0xC3, 0xC3, 0xC3, 0xC3, 0xC3, 0xC9};
    std::string raw = petsciiRaw(box, 16);
    CHECK(raw.size() == 16, "raw name lost characters, got %zu", raw.size());
    CHECK(static_cast<uint8_t>(raw[0]) == 0xD5 && static_cast<uint8_t>(raw[15]) == 0xC9,
          "raw name was converted after all");

    // Padding goes, but only the trailing run: a $A0 inside a name is a
    // shifted space somebody drew with.
    uint8_t padded[16] = {0xC2, 0xA0, 0x41, 0xA0, 0xA0, 0xA0, 0xA0, 0xA0,
                          0xA0, 0xA0, 0xA0, 0xA0, 0xA0, 0xA0, 0xA0, 0xA0};
    std::string trimmed = petsciiRaw(padded, 16);
    CHECK(trimmed.size() == 3, "trailing padding not stripped, got %zu", trimmed.size());
    CHECK(static_cast<uint8_t>(trimmed[1]) == 0xA0, "an interior shifted space was eaten");
}

int main()
{
    testFormatDetection();
    testPetsciiNames();
    testPetsciiScreenCodes();
    testPetsciiRaw();
    testT64Basic();
    testT64BadEndAddress();
    testT64ZeroCounters();
    testT64AltMagicAndPadding();
    testT64Overflow();
    testT64Rejects();
    testD64SingleSector();
    testD64MultiSector();
    testD64Variants();
    testD64SplatAndRejects();
    testD64Overflow();
    testD64CircularChain();

    printf("\n%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
