/*
 The 4-to-5 bit table and the sector layout below are taken from Frodo's
 1541job.cpp, so what the drive's own read routines see off the "surface" is
 the same as on hardware.

 Frodo (C) 1994-1997, 2002 Christian Bauer, GPL version 2 or later.
*/
#include "Gcr.hpp"
#include <cstring>

// Each nibble becomes a five bit symbol chosen so no run of more than two
// zeroes appears, which is what keeps the read head in sync.
static const uint8_t kGcrTable[16] = {
    0x0a, 0x0b, 0x12, 0x13, 0x0e, 0x0f, 0x16, 0x17, 0x09, 0x19, 0x1a, 0x1b, 0x0d, 0x1d, 0x1e, 0x15,
};

// Reverse of the above, built once. 0xff marks a symbol that cannot occur.
static uint8_t kGcrReverse[32];
static bool    kGcrReverseReady = false;

static void buildReverseTable()
{
    if (kGcrReverseReady) return;
    memset(kGcrReverse, 0xff, sizeof(kGcrReverse));
    for (uint8_t i = 0; i < 16; i++) {
        kGcrReverse[kGcrTable[i]] = i;
    }
    kGcrReverseReady = true;
}

// Packs four bytes into five, as eight five bit symbols laid end to end.
static void gcrConv4(const uint8_t* from, uint8_t* to)
{
    uint32_t g;

    g     = static_cast<uint32_t>((kGcrTable[from[0] >> 4] << 5) | kGcrTable[from[0] & 15]);
    g     = (g << 10) | static_cast<uint32_t>((kGcrTable[from[1] >> 4] << 5) | kGcrTable[from[1] & 15]);
    to[0] = static_cast<uint8_t>(g >> 12);
    to[1] = static_cast<uint8_t>(g >> 4);

    uint32_t h = static_cast<uint32_t>((kGcrTable[from[2] >> 4] << 5) | kGcrTable[from[2] & 15]);
    h          = (h << 10) | static_cast<uint32_t>((kGcrTable[from[3] >> 4] << 5) | kGcrTable[from[3] & 15]);

    to[2] = static_cast<uint8_t>(((g & 0x0f) << 4) | ((h >> 16) & 0x0f));
    to[3] = static_cast<uint8_t>(h >> 8);
    to[4] = static_cast<uint8_t>(h);
}

bool gcrDecode5(const uint8_t* gcr, uint8_t* out)
{
    buildReverseTable();

    // Rebuild the 40 bit run, then take it apart five bits at a time.
    uint64_t bits = 0;
    for (int i = 0; i < 5; i++) {
        bits = (bits << 8) | gcr[i];
    }

    uint8_t nibbles[8];
    for (int i = 0; i < 8; i++) {
        uint8_t symbol = static_cast<uint8_t>((bits >> (35 - i * 5)) & 0x1f);
        uint8_t value  = kGcrReverse[symbol];
        if (value == 0xff) return false;
        nibbles[i] = value;
    }

    for (int i = 0; i < 4; i++) {
        out[i] = static_cast<uint8_t>((nibbles[i * 2] << 4) | nibbles[i * 2 + 1]);
    }
    return true;
}

void gcrEncodeSector(const uint8_t* block, unsigned int track, unsigned int sector, uint8_t id1, uint8_t id2,
                     uint8_t* dest)
{
    uint8_t* p = dest;
    uint8_t  buf[4];

    // Header: which track and sector the head is passing over.
    memset(p, 0xff, GCR_SYNC_BYTES);
    p += GCR_SYNC_BYTES;

    buf[0] = 0x08;  // header mark
    buf[1] = static_cast<uint8_t>(sector ^ track ^ id2 ^ id1);
    buf[2] = static_cast<uint8_t>(sector);
    buf[3] = static_cast<uint8_t>(track);
    gcrConv4(buf, p);

    buf[0] = id2;
    buf[1] = id1;
    buf[2] = 0x0f;
    buf[3] = 0x0f;
    gcrConv4(buf, p + 5);
    p += GCR_HEADER_BYTES;

    memset(p, GCR_GAP_BYTE, GCR_HEADER_GAP);
    p += GCR_HEADER_GAP;

    // Data: the 256 bytes, preceded by a mark and followed by a checksum.
    memset(p, 0xff, GCR_SYNC_BYTES);
    p += GCR_SYNC_BYTES;

    uint8_t sum;
    buf[0] = 0x07;  // data mark
    sum = buf[1]  = block[0];
    sum ^= buf[2]  = block[1];
    sum ^= buf[3] = block[2];
    gcrConv4(buf, p);
    p += 5;

    for (int i = 3; i < 255; i += 4) {
        sum ^= buf[0]  = block[i];
        sum ^= buf[1]  = block[i + 1];
        sum ^= buf[2]  = block[i + 2];
        sum ^= buf[3] = block[i + 3];
        gcrConv4(buf, p);
        p += 5;
    }

    sum ^= buf[0] = block[255];
    buf[1]        = sum;
    buf[2]        = 0;
    buf[3]        = 0;
    gcrConv4(buf, p);
    p += 5;

    // The tail gap runs to the start of the next sector. Its length is what
    // spreads the track's spare capacity evenly between the sectors.
    memset(p, GCR_GAP_BYTE, gcrSectorSizeForTrack(track) - GCR_SECTOR_BODY);
}

bool gcrEncodeTrack(DiskImage& disk, unsigned int track, uint8_t id1, uint8_t id2, uint8_t* dest)
{
    unsigned int sectors = disk.sectorsPerTrack(track);
    if (sectors == 0) return false;

    // Anything past the last sector stays gap, so the head reads something
    // harmless rather than stale bytes from a longer track. Only the track's
    // own length is written; the rest of the buffer belongs to longer tracks
    // and the head never reaches it.
    memset(dest, GCR_GAP_BYTE, gcrTrackBytes(track));

    unsigned int stride = gcrSectorSizeForTrack(track);
    uint8_t      block[CBM_SECTOR_SIZE];
    for (unsigned int sector = 0; sector < sectors; sector++) {
        if (!disk.readSector(track, sector, block)) return false;
        gcrEncodeSector(block, track, sector, id1, id2, dest + sector * stride);
    }
    return true;
}

bool gcrDecodeSector(const uint8_t* sectorGcr, uint8_t* block)
{
    const uint8_t* p = sectorGcr + GCR_SYNC_BYTES + GCR_HEADER_BYTES + GCR_HEADER_GAP + GCR_SYNC_BYTES;

    uint8_t buf[4];
    if (!gcrDecode5(p, buf)) return false;
    if (buf[0] != 0x07) return false;  // not a data block

    uint8_t sum  = buf[1];
    block[0]     = buf[1];
    sum ^= block[1]  = buf[2];
    sum ^= block[2]  = buf[3];
    p               += 5;

    for (int i = 3; i < 255; i += 4) {
        if (!gcrDecode5(p, buf)) return false;
        sum ^= block[i]  = buf[0];
        sum ^= block[i + 1]  = buf[1];
        sum ^= block[i + 2]  = buf[2];
        sum ^= block[i + 3]  = buf[3];
        p                   += 5;
    }

    if (!gcrDecode5(p, buf)) return false;
    sum ^= block[255] = buf[0];
    return sum == buf[1];
}
