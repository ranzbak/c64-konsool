#pragma once
/*
 Copyright (C) 2025 Konsool 64 contributors

 This program is free software; you can redistribute it and/or modify it
 under the terms of the GNU General Public License as published by the
 Free Software Foundation; either version 3 of the License, or (at your
 option) any later version.

 This program is distributed in the hope that it will be useful, but
 WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 for more details.

 For the complete text of the GNU General Public License see
 http://www.gnu.org/licenses/.
*/

#include <cstddef>
#include <cstdint>
#include "DiskImage.hpp"

// GCR encoding, which is how bytes actually sit on a 1541 disk surface.
//
// A .d64 holds decoded 256 byte sectors, but a real drive's read routines pull
// raw GCR off the head and decode it themselves. Anything with its own loader
// does exactly that, so the drive has to be handed a GCR track rather than
// tidy sectors.
//
// The layout follows Frodo's 1541job.cpp: per sector a sync, the header, a
// gap, another sync, the data block and a second gap. Frodo writes a single
// sync byte where hardware writes five; five is used here since a full track
// still has room for it.
//
// Frodo (C) 1994-1997, 2002 Christian Bauer, GPL version 2 or later.

static const unsigned int GCR_SYNC_BYTES   = 5;
static const unsigned int GCR_HEADER_BYTES = 10;  // 8 decoded bytes
static const unsigned int GCR_HEADER_GAP   = 9;
static const unsigned int GCR_DATA_BYTES   = 325;  // 260 decoded bytes

// A sector without its tail gap. The gap is what differs between speed zones.
static const unsigned int GCR_SECTOR_BODY =
    GCR_SYNC_BYTES + GCR_HEADER_BYTES + GCR_HEADER_GAP + GCR_SYNC_BYTES + GCR_DATA_BYTES;  // 354

// Speed zones, numbered as the drive numbers them: 3 is the outermost and
// fastest, 0 the innermost. The disk turns at a fixed 300 rpm, so an outer
// track passes more bytes under the head per revolution than an inner one.
static const unsigned int GCR_ZONE_COUNT = 4;

// Which zone a track belongs to. The same split the DOS uses when it picks
// the density bits.
inline unsigned int gcrSpeedZone(unsigned int track)
{
    if (track <= 17) return 3;
    if (track <= 24) return 2;
    if (track <= 30) return 1;
    return 0;
}

// Bytes between the start of one sector and the start of the next. The
// format routine pads the tail gap out per zone so the sectors fill the
// track evenly rather than leaving all the slack in one lump at the end.
// A lump is fatal: the DOS gives up hunting for a sync mark after about 730
// byte times, so no gap-only stretch may come near that.
// VICE diskimage.c, gap_size_d64.
inline unsigned int gcrSectorSize(unsigned int zone)
{
    static const unsigned int tailGap[GCR_ZONE_COUNT] = {9, 12, 17, 8};
    return GCR_SECTOR_BODY + tailGap[zone % GCR_ZONE_COUNT];
}

// How many bytes go past the head in one revolution of a track in this zone.
// VICE diskimage.c, raw_track_size_d64.
inline unsigned int gcrZoneTrackBytes(unsigned int zone)
{
    static const unsigned int trackBytes[GCR_ZONE_COUNT] = {6250, 6666, 7142, 7692};
    return trackBytes[zone % GCR_ZONE_COUNT];
}

// How long one byte takes to pass under the head, in drive cycles. The disk
// turns at a fixed 300 rpm whatever zone the head is over, so this and
// gcrZoneTrackBytes() are two sides of the same number: their product is one
// revolution, 200000 cycles at 1 MHz.
inline unsigned int gcrZoneCyclesPerByte(unsigned int zone)
{
    static const unsigned int perByte[GCR_ZONE_COUNT] = {32, 30, 28, 26};
    return perByte[zone % GCR_ZONE_COUNT];
}

inline unsigned int gcrSectorSizeForTrack(unsigned int track)
{
    return gcrSectorSize(gcrSpeedZone(track));
}

inline unsigned int gcrTrackBytes(unsigned int track)
{
    return gcrZoneTrackBytes(gcrSpeedZone(track));
}

// Buffer sizes, for callers that need a fixed allocation. A track buffer has
// to hold the longest zone; only the first gcrTrackBytes(track) of it are on
// the disk surface.
static const unsigned int GCR_TRACK_SIZE      = 7692;
static const unsigned int GCR_SECTOR_SIZE_MAX = GCR_SECTOR_BODY + 17;

// The byte that fills gaps and any unused tail of a track. It cannot be
// mistaken for a sync mark.
static const uint8_t GCR_GAP_BYTE = 0x55;

// Encodes one 256 byte sector, header included, into
// gcrSectorSizeForTrack(track) bytes.
void gcrEncodeSector(const uint8_t* block, unsigned int track, unsigned int sector, uint8_t id1, uint8_t id2,
                     uint8_t* dest);

// Encodes a whole track off `disk` into `dest`, which must hold
// GCR_TRACK_SIZE bytes. Anything past the last sector, up to the track's own
// length, is filled with gap. Returns false if the track could not be read.
bool gcrEncodeTrack(DiskImage& disk, unsigned int track, uint8_t id1, uint8_t id2, uint8_t* dest);

// Decodes five GCR bytes back into four. Returns false on an invalid symbol.
bool gcrDecode5(const uint8_t* gcr, uint8_t* out);

// Decodes the data block of one encoded sector back into 256 bytes, checking
// the data mark and checksum.
bool gcrDecodeSector(const uint8_t* sectorGcr, uint8_t* block);
