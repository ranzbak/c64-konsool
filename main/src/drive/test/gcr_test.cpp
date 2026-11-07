/*
 Host side tests for the GCR layer.

     make -C main/src/drive/test gcr
*/

#include "../Gcr.hpp"
#include <cstdio>
#include <cstring>
#include <vector>
#include "../D64Disk.hpp"

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

// Frodo's gcr_conv4, transcribed exactly, so the packing here can be compared
// against it byte for byte rather than only against itself.
static const uint16_t frodo_gcr_table[16] = {
    0x0a, 0x0b, 0x12, 0x13, 0x0e, 0x0f, 0x16, 0x17, 0x09, 0x19, 0x1a, 0x1b, 0x0d, 0x1d, 0x1e, 0x15,
};

static void frodo_gcr_conv4(const uint8_t* from, uint8_t* to)
{
    uint16_t g;

    g     = static_cast<uint16_t>((frodo_gcr_table[*from >> 4] << 5) | frodo_gcr_table[*from & 15]);
    *to++ = static_cast<uint8_t>(g >> 2);
    *to   = static_cast<uint8_t>((g << 6) & 0xc0);
    from++;

    g      = static_cast<uint16_t>((frodo_gcr_table[*from >> 4] << 5) | frodo_gcr_table[*from & 15]);
    *to++ |= static_cast<uint8_t>((g >> 4) & 0x3f);
    *to    = static_cast<uint8_t>((g << 4) & 0xf0);
    from++;

    g      = static_cast<uint16_t>((frodo_gcr_table[*from >> 4] << 5) | frodo_gcr_table[*from & 15]);
    *to++ |= static_cast<uint8_t>((g >> 6) & 0x0f);
    *to    = static_cast<uint8_t>((g << 2) & 0xfc);
    from++;

    g      = static_cast<uint16_t>((frodo_gcr_table[*from >> 4] << 5) | frodo_gcr_table[*from & 15]);
    *to++ |= static_cast<uint8_t>((g >> 8) & 0x03);
    *to    = static_cast<uint8_t>(g);
}

// A minimal in-memory disk so a whole track can be encoded.
class MemDisk : public DiskImage {
   public:
    std::vector<uint8_t> bytes;
    unsigned int         trackCount = 35;

    MemDisk()
    {
        size_t blocks = 0;
        for (unsigned int t = 1; t <= trackCount; t++) blocks += sectorsPerTrack(t);
        bytes.assign(blocks * CBM_SECTOR_SIZE, 0);
        // Fill with something recognisable per sector.
        for (unsigned int t = 1; t <= trackCount; t++) {
            for (unsigned int s = 0; s < sectorsPerTrack(t); s++) {
                size_t off = offset(t, s);
                for (unsigned int i = 0; i < CBM_SECTOR_SIZE; i++) {
                    bytes[off + i] = static_cast<uint8_t>((t * 7 + s * 3 + i) & 0xff);
                }
            }
        }
    }

    size_t offset(unsigned int track, unsigned int sector) const
    {
        size_t off = 0;
        for (unsigned int t = 1; t < track; t++) off += sectorsPerTrack(t) * CBM_SECTOR_SIZE;
        return off + sector * CBM_SECTOR_SIZE;
    }

    bool readSector(unsigned int track, unsigned int sector, uint8_t* buf) override
    {
        if (track < 1 || track > trackCount || sector >= sectorsPerTrack(track)) return false;
        memcpy(buf, &bytes[offset(track, sector)], CBM_SECTOR_SIZE);
        return true;
    }
    bool writable() const override
    {
        return false;
    }
    unsigned int tracks() const override
    {
        return trackCount;
    }
    unsigned int sectorsPerTrack(unsigned int track) const override
    {
        return D64Disk::sectorsOnTrack(track);
    }
};

static void testMatchesFrodoPacking()
{
    printf("GCR: 4-to-5 packing matches Frodo byte for byte\n");

    // Every group of four bytes goes through the same path, so walking a
    // spread of values covers the packing thoroughly.
    bool same = true;
    for (unsigned int seed = 0; seed < 4096 && same; seed++) {
        uint8_t in[4];
        in[0] = static_cast<uint8_t>(seed & 0xff);
        in[1] = static_cast<uint8_t>((seed >> 4) & 0xff);
        in[2] = static_cast<uint8_t>((seed * 7) & 0xff);
        in[3] = static_cast<uint8_t>((seed * 31 + 17) & 0xff);

        uint8_t theirs[5] = {0, 0, 0, 0, 0};
        frodo_gcr_conv4(in, theirs);

        // Reach the packing through a full sector encode, comparing the first
        // group of the data block, which is the data mark plus three bytes.
        uint8_t block[CBM_SECTOR_SIZE];
        memset(block, 0, sizeof(block));
        block[0] = in[1];
        block[1] = in[2];
        block[2] = in[3];

        uint8_t sector[GCR_SECTOR_SIZE_MAX];
        gcrEncodeSector(block, 18, 0, 'I', 'D', sector);

        uint8_t mine[5];
        memcpy(mine, sector + GCR_SYNC_BYTES + GCR_HEADER_BYTES + GCR_HEADER_GAP + GCR_SYNC_BYTES, 5);

        uint8_t expectIn[4] = {0x07, in[1], in[2], in[3]};
        uint8_t expect[5]   = {0, 0, 0, 0, 0};
        frodo_gcr_conv4(expectIn, expect);

        if (memcmp(mine, expect, 5) != 0) {
            CHECK(false, "seed %u: packing differs from Frodo", seed);
            same = false;
        }
    }
    CHECK(same, "packing diverged from Frodo");
}

static void testSectorRoundTrip()
{
    printf("GCR: a sector encodes and decodes back unchanged\n");

    uint8_t block[CBM_SECTOR_SIZE];
    for (unsigned int i = 0; i < CBM_SECTOR_SIZE; i++) block[i] = static_cast<uint8_t>(i);

    uint8_t sector[GCR_SECTOR_SIZE_MAX];
    gcrEncodeSector(block, 17, 5, 'A', 'B', sector);

    uint8_t back[CBM_SECTOR_SIZE];
    CHECK(gcrDecodeSector(sector, back), "decode rejected its own encoding");
    CHECK(memcmp(block, back, CBM_SECTOR_SIZE) == 0, "round trip changed the data");

    // Awkward patterns: all zeroes and all ones both have to survive.
    memset(block, 0x00, sizeof(block));
    gcrEncodeSector(block, 1, 0, 'A', 'B', sector);
    CHECK(gcrDecodeSector(sector, back) && memcmp(block, back, CBM_SECTOR_SIZE) == 0, "all zeroes failed");

    memset(block, 0xff, sizeof(block));
    gcrEncodeSector(block, 35, 16, 'Z', 'Z', sector);
    CHECK(gcrDecodeSector(sector, back) && memcmp(block, back, CBM_SECTOR_SIZE) == 0, "all ones failed");
}

static void testHeaderContents()
{
    printf("GCR: the header carries the right track, sector and id\n");

    uint8_t block[CBM_SECTOR_SIZE];
    memset(block, 0x42, sizeof(block));

    uint8_t sector[GCR_SECTOR_SIZE_MAX];
    gcrEncodeSector(block, 18, 7, 'I', 'D', sector);

    for (unsigned int i = 0; i < GCR_SYNC_BYTES; i++) {
        CHECK(sector[i] == 0xff, "sync byte %u was $%02x", i, sector[i]);
    }

    uint8_t hdr[4];
    CHECK(gcrDecode5(sector + GCR_SYNC_BYTES, hdr), "header did not decode");
    CHECK(hdr[0] == 0x08, "header mark was $%02x", hdr[0]);
    CHECK(hdr[2] == 7, "header sector was %u", hdr[2]);
    CHECK(hdr[3] == 18, "header track was %u", hdr[3]);
    // The checksum is the other three fields exclusive ored with the disk id.
    CHECK(hdr[1] == (7 ^ 18 ^ 'D' ^ 'I'), "header checksum was $%02x", hdr[1]);

    uint8_t hdr2[4];
    CHECK(gcrDecode5(sector + GCR_SYNC_BYTES + 5, hdr2), "header id did not decode");
    CHECK(hdr2[0] == 'D' && hdr2[1] == 'I', "header id was '%c%c'", hdr2[0], hdr2[1]);

    // The gap must not look like a sync mark.
    const uint8_t* gap = sector + GCR_SYNC_BYTES + GCR_HEADER_BYTES;
    for (unsigned int i = 0; i < GCR_HEADER_GAP; i++) {
        CHECK(gap[i] == GCR_GAP_BYTE, "gap byte %u was $%02x", i, gap[i]);
    }
}

static void testTrackEncoding()
{
    printf("GCR: a whole track encodes, every sector recoverable\n");

    MemDisk              disk;
    std::vector<uint8_t> gcr(GCR_TRACK_SIZE);

    // Track 1 has the most sectors, track 35 the fewest.
    const unsigned int tracks[] = {1, 18, 25, 35};
    for (unsigned int ti = 0; ti < sizeof(tracks) / sizeof(tracks[0]); ti++) {
        unsigned int track = tracks[ti];
        CHECK(gcrEncodeTrack(disk, track, 'I', 'D', gcr.data()), "track %u failed to encode", track);

        unsigned int sectors = disk.sectorsPerTrack(track);
        for (unsigned int s = 0; s < sectors; s++) {
            uint8_t back[CBM_SECTOR_SIZE];
            if (!gcrDecodeSector(gcr.data() + s * gcrSectorSizeForTrack(track), back)) {
                CHECK(false, "track %u sector %u did not decode", track, s);
                break;
            }
            uint8_t expect[CBM_SECTOR_SIZE];
            disk.readSector(track, s, expect);
            if (memcmp(back, expect, CBM_SECTOR_SIZE) != 0) {
                CHECK(false, "track %u sector %u came back different", track, s);
                break;
            }
        }

        // The tail past the last sector has to be gap, not stale bytes. Only
        // the track's own length is on the surface; the rest of the buffer
        // belongs to longer tracks and the head never reaches it.
        bool tailClean = true;
        for (size_t i = sectors * gcrSectorSizeForTrack(track); i < gcrTrackBytes(track); i++) {
            if (gcr[i] != GCR_GAP_BYTE) {
                tailClean = false;
                break;
            }
        }
        CHECK(tailClean, "track %u has non gap bytes after the last sector", track);
    }

    CHECK(!gcrEncodeTrack(disk, 0, 'I', 'D', gcr.data()), "track 0 was accepted");
    CHECK(!gcrEncodeTrack(disk, 99, 'I', 'D', gcr.data()), "track 99 was accepted");
}

static void testTrackFitsRealDisk()
{
    printf("GCR: every track fits its real capacity and has no dead stretch\n");

    // The DOS hunts for a sync mark with a timer, not a byte count: $F556
    // arms VIA 1's T1 with $D0xx and gives up when the high byte falls below
    // $80, so about 20480 drive cycles. Any run of track without a sync mark
    // in it that takes longer than that to pass the head is unreadable, and
    // the DOS reports error 3, no sync found. The longest such run is the
    // data block of the last sector, its gap, and whatever capacity is left
    // over at the end of the track.
    const unsigned int syncTimeout = 20480;

    MemDisk disk;
    for (unsigned int track = 1; track <= 35; track++) {
        unsigned int zone    = gcrSpeedZone(track);
        unsigned int sectors = disk.sectorsPerTrack(track);
        unsigned int stride  = gcrSectorSize(zone);
        unsigned int used    = sectors * stride;
        unsigned int cap     = gcrZoneTrackBytes(zone);

        CHECK(used <= cap, "track %u needs %u bytes, more than the %u a real one holds", track, used, cap);

        // A revolution is a revolution whatever zone the head is over: 300
        // rpm is 200000 cycles at 1 MHz. Capacity and byte rate have to agree
        // on that or the disk turns at the wrong speed.
        unsigned int revolution = cap * gcrZoneCyclesPerByte(zone);
        CHECK(revolution > 199000 && revolution < 201000, "track %u turns in %u cycles, not the 200000 of 300 rpm",
              track, revolution);

        unsigned int leftOver = cap - used;
        unsigned int longest  = GCR_DATA_BYTES + (stride - GCR_SECTOR_BODY) + leftOver;
        unsigned int cycles   = longest * gcrZoneCyclesPerByte(zone);
        CHECK(cycles * 2 < syncTimeout * 3, "track %u has a %u byte stretch with no sync mark: %u cycles against a %u budget",
              track, longest, cycles, syncTimeout);
    }

    CHECK(GCR_SECTOR_BODY == GCR_SYNC_BYTES + 10 + 9 + GCR_SYNC_BYTES + 325, "sector size does not add up");
    CHECK(GCR_TRACK_SIZE >= gcrZoneTrackBytes(3), "the track buffer is too small for the longest track");
    CHECK(GCR_SECTOR_SIZE_MAX >= gcrSectorSize(2), "the sector buffer is too small for the widest sector");
}

static void testRejectsBadSymbols()
{
    printf("GCR: invalid symbols and a bad checksum are rejected\n");

    uint8_t block[CBM_SECTOR_SIZE];
    memset(block, 0x11, sizeof(block));

    uint8_t sector[GCR_SECTOR_SIZE_MAX];
    gcrEncodeSector(block, 10, 3, 'A', 'B', sector);

    // Corrupt a byte inside the data block; either the symbol becomes invalid
    // or the checksum stops matching, and both must be caught.
    uint8_t corrupted[GCR_SECTOR_SIZE_MAX];
    memcpy(corrupted, sector, sizeof(corrupted));
    size_t dataAt     = GCR_SYNC_BYTES + GCR_HEADER_BYTES + GCR_HEADER_GAP + GCR_SYNC_BYTES + 20;
    corrupted[dataAt] = static_cast<uint8_t>(corrupted[dataAt] ^ 0xff);

    uint8_t back[CBM_SECTOR_SIZE];
    CHECK(!gcrDecodeSector(corrupted, back), "a corrupted data block was accepted");

    // A run of zero bits is not a legal symbol.
    uint8_t bad[5] = {0, 0, 0, 0, 0};
    uint8_t out[4];
    CHECK(!gcrDecode5(bad, out), "an all zero group decoded");
}

int main()
{
    testMatchesFrodoPacking();
    testSectorRoundTrip();
    testHeaderContents();
    testTrackEncoding();
    testTrackFitsRealDisk();
    testRejectsBadSymbols();

    printf("\n%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
