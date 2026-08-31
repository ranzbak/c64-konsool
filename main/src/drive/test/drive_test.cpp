/*
 Host side tests for the 1541's VIAs, the bus lines and the disk controller.

     make -C main/src/drive/test drive

 Covers the VIA registers and timers, the wired-and behaviour of the serial
 lines, the attention acknowledge gate and the head stepping, and boots the
 drive CPU itself against a small hand assembled ROM so the real DOS ROM,
 which cannot be shipped, is not needed.
*/

#include <cstdio>
#include <cstring>
#include <vector>
#include "../D64Disk.hpp"
#include "../DiskController.hpp"
#include "../Drive1541.hpp"
#include "../IecLines.hpp"
#include "../Via6522.hpp"

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

class MemDisk : public DiskImage {
   public:
    std::vector<uint8_t> bytes;
    unsigned int         trackCount = 35;
    bool                 readOnly   = true;

    MemDisk()
    {
        size_t blocks = 0;
        for (unsigned int t = 1; t <= trackCount; t++) blocks += sectorsPerTrack(t);
        bytes.assign(blocks * CBM_SECTOR_SIZE, 0);
        for (unsigned int t = 1; t <= trackCount; t++) {
            for (unsigned int s = 0; s < sectorsPerTrack(t); s++) {
                size_t off = offset(t, s);
                for (unsigned int i = 0; i < CBM_SECTOR_SIZE; i++) {
                    bytes[off + i] = static_cast<uint8_t>((t * 5 + s * 3 + i) & 0xff);
                }
            }
        }
        // A plausible disk id in the BAM, which ends up in every sector header.
        size_t bam        = offset(18, 0);
        bytes[bam + 0xA2] = 'I';
        bytes[bam + 0xA3] = 'D';
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
    bool writeSector(unsigned int track, unsigned int sector, const uint8_t* buf) override
    {
        if (readOnly) return false;
        if (track < 1 || track > trackCount || sector >= sectorsPerTrack(track)) return false;
        memcpy(&bytes[offset(track, sector)], buf, CBM_SECTOR_SIZE);
        return true;
    }
    bool writable() const override
    {
        return !readOnly;
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

/* ------------------------------------------------------------------ VIA -- */

static void testViaPorts()
{
    printf("VIA: output bits read back, input bits read the pins\n");
    Via6522 via;
    via.reset();

    via.write(Via6522::REG_DDRB, 0x0f);  // low nibble out, high nibble in
    via.write(Via6522::REG_PRB, 0xaa);

    uint8_t got = via.read(Via6522::REG_PRB, 0xff, 0x50);
    CHECK((got & 0x0f) == 0x0a, "output bits read back as $%02x", got & 0x0f);
    CHECK((got & 0xf0) == 0x50, "input bits read as $%02x, expected $50", got & 0xf0);

    CHECK(via.read(Via6522::REG_DDRB, 0, 0) == 0x0f, "DDRB did not read back");
}

static void testViaTimer1()
{
    printf("VIA: timer 1 flags on underflow, reloads only when free running\n");
    Via6522 via;
    via.reset();

    // One shot: writing the high byte loads the counter and clears the flag.
    via.write(Via6522::REG_ACR, 0x00);
    via.write(Via6522::REG_T1CL, 100);
    via.write(Via6522::REG_T1CH, 0);
    CHECK((via.ifr & Via6522::IRQ_T1) == 0, "flag set straight after loading");

    via.countTimers(50);
    CHECK((via.ifr & Via6522::IRQ_T1) == 0, "flag set before the timer ran out");
    via.countTimers(60);
    CHECK((via.ifr & Via6522::IRQ_T1) != 0, "flag missing after the timer ran out");

    // Reading the low byte clears it.
    via.read(Via6522::REG_T1CL, 0, 0);
    CHECK((via.ifr & Via6522::IRQ_T1) == 0, "reading T1CL did not clear the flag");

    // Free running: it reloads and keeps flagging.
    via.reset();
    via.write(Via6522::REG_ACR, 0x40);
    via.write(Via6522::REG_T1CL, 10);
    via.write(Via6522::REG_T1CH, 0);
    via.countTimers(20);
    CHECK((via.ifr & Via6522::IRQ_T1) != 0, "free running timer did not flag");
    via.read(Via6522::REG_T1CL, 0, 0);
    via.countTimers(20);
    CHECK((via.ifr & Via6522::IRQ_T1) != 0, "free running timer did not flag again");
}

static void testViaTimer1OneShotFiresOnce()
{
    printf("VIA: a one shot timer 1 flags once, not every time it wraps\n");
    Via6522 via;
    via.reset();

    via.write(Via6522::REG_ACR, 0x00);  // one shot
    via.write(Via6522::REG_T1CL, 10);
    via.write(Via6522::REG_T1CH, 0);

    via.countTimers(20);
    CHECK((via.ifr & Via6522::IRQ_T1) != 0, "the timer did not flag when it ran out");

    via.read(Via6522::REG_T1CL, 0, 0);  // acknowledge
    CHECK((via.ifr & Via6522::IRQ_T1) == 0, "the flag did not clear");

    // The counter keeps running and wraps all the way round. On real hardware
    // that does not raise the flag again until the timer is reloaded, and an
    // extra interrupt here would look like a phantom one to the drive.
    for (int i = 0; i < 300; i++) {
        via.countTimers(255);
    }
    CHECK((via.ifr & Via6522::IRQ_T1) == 0, "the one shot timer flagged again after wrapping");

    // Reloading arms it once more.
    via.write(Via6522::REG_T1CH, 0);
    via.countTimers(20);
    CHECK((via.ifr & Via6522::IRQ_T1) != 0, "reloading did not re-arm the timer");
}

static void testViaInterruptGating()
{
    printf("VIA: an interrupt only asserts once it is enabled\n");
    Via6522 via;
    via.reset();

    via.write(Via6522::REG_ACR, 0x00);
    via.write(Via6522::REG_T1CL, 5);
    via.write(Via6522::REG_T1CH, 0);
    via.countTimers(10);

    CHECK((via.ifr & Via6522::IRQ_T1) != 0, "flag missing");
    CHECK(!via.irqAsserted(), "interrupt asserted while disabled");

    via.write(Via6522::REG_IER, 0x80 | Via6522::IRQ_T1);  // set enable
    CHECK(via.irqAsserted(), "interrupt not asserted once enabled");
    CHECK((via.read(Via6522::REG_IFR, 0, 0) & Via6522::IRQ_ANY) != 0, "IFR bit 7 not set");

    via.write(Via6522::REG_IFR, Via6522::IRQ_T1);  // writing a one clears
    CHECK(!via.irqAsserted(), "interrupt still asserted after clearing the flag");

    via.write(Via6522::REG_IER, Via6522::IRQ_T1);  // bit 7 clear disables
    CHECK((via.ier & Via6522::IRQ_T1) == 0, "IER bit did not clear");
}

/* ---------------------------------------------------------------- lines -- */

static void testLinesAreWiredAnd()
{
    printf("Lines: any participant pulling low wins\n");
    IecLines lines;
    lines.reset();

    CHECK(!lines.clkLow() && !lines.dataLow() && !lines.atnLow(), "lines not released after reset");

    lines.c64Clk = true;
    CHECK(lines.clkLow(), "the C64 could not pull CLK low");
    lines.c64Clk   = false;
    lines.driveClk = true;
    CHECK(lines.clkLow(), "the drive could not pull CLK low");

    // Both pulling, then one releasing, still leaves the line low.
    lines.c64Clk = true;
    CHECK(lines.clkLow(), "both pulling should be low");
    lines.c64Clk = false;
    CHECK(lines.clkLow(), "line went high while the drive was still pulling");
    lines.driveClk = false;
    CHECK(!lines.clkLow(), "line stayed low with nobody pulling");

    // ATN is driven by the C64 alone.
    lines.c64Atn = true;
    CHECK(lines.atnLow(), "ATN did not follow the C64");
}

/* -------------------------------------------------- the acknowledge gate -- */

// Pokes VIA 1 through the drive's own memory map, which is how the ROM does it.
static void setVia1PortB(Drive1541& drive, uint8_t ddrb, uint8_t prb)
{
    drive.setMem(0x1802, ddrb);  // DDRB
    drive.setMem(0x1800, prb);   // PRB
}

static void testAtnAcknowledgeGate()
{
    printf("Drive: DATA is held low until the drive acknowledges ATN\n");
    IecLines  lines;
    Drive1541 drive;
    lines.reset();
    drive.setLines(&lines);

    // Bits 1, 3 and 4 are the outputs the drive drives: DATA, CLK and ATNA.
    const uint8_t DDRB = 0x1a;

    // Nothing asserted anywhere: the bus is free.
    setVia1PortB(drive, DDRB, 0x00);
    CHECK(!lines.dataLow(), "DATA low with nothing going on");

    // The C64 asserts ATN. With the acknowledge still low, the gate pulls
    // DATA down, which is how a drive says "I am here".
    lines.c64Atn = true;
    drive.refreshIecOutputs();
    CHECK(lines.dataLow(), "DATA not pulled low when ATN was asserted");

    // The drive acknowledges by raising ATNA, releasing DATA again.
    setVia1PortB(drive, DDRB, 0x10);
    CHECK(!lines.dataLow(), "DATA still low after the drive acknowledged");

    // With ATN gone but the acknowledge left set, the gate pulls DATA low
    // again; the two have to agree.
    lines.c64Atn = false;
    drive.refreshIecOutputs();
    CHECK(lines.dataLow(), "the gate did not react to ATN being released");

    setVia1PortB(drive, DDRB, 0x00);
    CHECK(!lines.dataLow(), "DATA not released once both agreed");

    // The drive can also pull DATA low on its own, whatever ATN is doing.
    setVia1PortB(drive, DDRB, 0x02);
    CHECK(lines.dataLow(), "the drive could not pull DATA low itself");
    setVia1PortB(drive, DDRB, 0x08);
    CHECK(lines.clkLow(), "the drive could not pull CLK low");
    CHECK(!lines.dataLow(), "DATA low when only CLK was asserted");
}

static void testDriveReadsBusState()
{
    printf("Drive: port B reports the bus, low lines reading as ones\n");
    IecLines  lines;
    Drive1541 drive;
    lines.reset();
    drive.setLines(&lines);

    // All of port B as inputs, so the pins show through.
    drive.setMem(0x1802, 0x00);

    uint8_t idle = drive.getMem(0x1800);
    CHECK((idle & 0x01) == 0, "DATA read as low on an idle bus");
    CHECK((idle & 0x04) == 0, "CLK read as low on an idle bus");
    CHECK((idle & 0x80) == 0, "ATN read as low on an idle bus");

    lines.c64Data = true;
    lines.c64Clk  = true;
    lines.c64Atn  = true;
    uint8_t busy  = drive.getMem(0x1800);
    CHECK((busy & 0x01) != 0, "DATA low did not read as a one");
    CHECK((busy & 0x04) != 0, "CLK low did not read as a one");
    CHECK((busy & 0x80) != 0, "ATN low did not read as a one");
}

/* ------------------------------------------------------------ head/disk -- */

static void testHeadStepping()
{
    printf("Controller: the stepper walks the head a half track at a time\n");
    MemDisk        disk;
    DiskController controller;
    controller.setDisk(&disk);

    CHECK(controller.currentTrack() == 18, "head did not start on track 18");

    unsigned int start = controller.currentHalfTrack();
    controller.moveHeadIn();
    controller.moveHeadIn();
    CHECK(controller.currentHalfTrack() == start + 2, "two half steps did not move two");
    CHECK(controller.currentTrack() == 19, "two half steps did not reach the next track");

    controller.moveHeadOut();
    controller.moveHeadOut();
    CHECK(controller.currentTrack() == 18, "stepping back did not return to track 18");

    // The head cannot walk off the inside edge.
    for (int i = 0; i < 200; i++) controller.moveHeadOut();
    CHECK(controller.currentTrack() >= 1, "head walked past track 1");

    for (int i = 0; i < 400; i++) controller.moveHeadIn();
    CHECK(controller.currentTrack() <= 42, "head walked past the last track");
}

static void testControllerReadsTrack()
{
    printf("Controller: the head reads back the GCR of the track it is on\n");
    MemDisk        disk;
    DiskController controller;
    controller.setDisk(&disk);

    // Track 18 sector 0 should be recoverable from the byte stream the head
    // hands over, which is what the drive's own read routines do.
    std::vector<uint8_t> seen;
    for (unsigned int i = 0; i < controller.trackLength(); i++) {
        seen.push_back(controller.readGcrByte());
    }

    uint8_t back[CBM_SECTOR_SIZE];
    CHECK(gcrDecodeSector(seen.data(), back), "the first sector under the head did not decode");

    uint8_t expect[CBM_SECTOR_SIZE];
    disk.readSector(18, 0, expect);
    CHECK(memcmp(back, expect, CBM_SECTOR_SIZE) == 0, "the head returned the wrong sector");

    // The track wraps rather than running off the end.
    uint8_t first = controller.readGcrByte();
    CHECK(first == seen[0], "the track did not wrap round");

    // Sync marks are found where the encoder put them.
    DiskController fresh;
    fresh.setDisk(&disk);
    CHECK(fresh.syncFound(), "no sync at the start of a track");

    // With no disk the head finds gap, not stale data.
    DiskController empty;
    CHECK(!empty.hasDisk(), "an empty controller claims to have a disk");
    CHECK(empty.readGcrByte() == GCR_GAP_BYTE, "an empty drive returned something other than gap");
    CHECK(!empty.syncFound(), "an empty drive found a sync mark");
}

static void testResetKeepsTheDisk()
{
    printf("Controller: a reset parks the head without losing the disk\n");
    MemDisk        disk;
    DiskController controller;
    controller.setDisk(&disk);

    // Move away from where a reset should leave the head.
    for (int i = 0; i < 6; i++) controller.moveHeadIn();
    CHECK(controller.currentTrack() != 18, "the head did not move");

    controller.reset();
    CHECK(controller.currentTrack() == 18, "a reset did not park the head on track 18");

    // The disk is still in the drive, so there must be a track under the head
    // rather than gap. Getting this wrong looks exactly like an unformatted
    // disk and would leave the drive hunting forever.
    CHECK(controller.syncFound(), "no sync under the head after a reset");
    uint8_t first = controller.readGcrByte();
    CHECK(first == 0xff, "the head read $%02x after a reset, expected a sync byte", first);
}

static void testWriteProtectSense()
{
    printf("Controller: write protect reflects whether the image can be written\n");
    MemDisk        disk;
    DiskController controller;

    disk.readOnly = true;
    controller.setDisk(&disk);
    CHECK(controller.writeProtectBit() == 0x00, "a read only image did not report protected");

    disk.readOnly = false;
    controller.setDisk(&disk);
    CHECK(controller.writeProtectBit() == 0x10, "a writable image reported protected");

    DiskController empty;
    CHECK(empty.writeProtectBit() == 0x00, "an empty drive did not report protected");
}

static void testHeadWritesReachTheImage()
{
    printf("Controller: bytes written by the head land in the image\n");
    MemDisk disk;
    disk.readOnly = false;

    DiskController controller;
    controller.setDisk(&disk);

    // Build the sector the drive would lay down: same header, new data.
    uint8_t replacement[CBM_SECTOR_SIZE];
    for (unsigned int i = 0; i < CBM_SECTOR_SIZE; i++) {
        replacement[i] = static_cast<uint8_t>(0xa0 + (i & 0x0f));
    }
    uint8_t encoded[GCR_SECTOR_SIZE_MAX];
    gcrEncodeSector(replacement, 18, 0, 'I', 'D', encoded);

    // The head starts at the beginning of the track, so writing the encoded
    // sector straight through lands it on sector 0.
    for (unsigned int i = 0; i < gcrSectorSizeForTrack(18); i++) {
        controller.writeGcrByte(encoded[i]);
    }

    // Nothing reaches the image until the head is done with the track.
    uint8_t before[CBM_SECTOR_SIZE];
    disk.readSector(18, 0, before);
    CHECK(memcmp(before, replacement, CBM_SECTOR_SIZE) != 0, "the image changed before any flush");

    controller.flush();

    uint8_t after[CBM_SECTOR_SIZE];
    CHECK(disk.readSector(18, 0, after), "could not read the sector back");
    CHECK(memcmp(after, replacement, CBM_SECTOR_SIZE) == 0, "the written sector did not reach the image");

    // Other sectors on the track are left alone.
    uint8_t neighbour[CBM_SECTOR_SIZE];
    disk.readSector(18, 1, neighbour);
    bool untouched = false;
    for (unsigned int i = 0; i < CBM_SECTOR_SIZE; i++) {
        if (neighbour[i] != replacement[i]) {
            untouched = true;
            break;
        }
    }
    CHECK(untouched, "writing one sector changed its neighbour");
}

static void testHeadWriteStepsFlush()
{
    printf("Controller: stepping away writes the track back on the way out\n");
    MemDisk disk;
    disk.readOnly = false;

    DiskController controller;
    controller.setDisk(&disk);

    uint8_t replacement[CBM_SECTOR_SIZE];
    memset(replacement, 0x5c, sizeof(replacement));
    uint8_t encoded[GCR_SECTOR_SIZE_MAX];
    gcrEncodeSector(replacement, 18, 0, 'I', 'D', encoded);
    for (unsigned int i = 0; i < gcrSectorSizeForTrack(18); i++) {
        controller.writeGcrByte(encoded[i]);
    }

    // Two half steps take the head to the next track, and the changes have to
    // go with it rather than being dropped.
    controller.moveHeadIn();
    controller.moveHeadIn();

    uint8_t after[CBM_SECTOR_SIZE];
    disk.readSector(18, 0, after);
    CHECK(memcmp(after, replacement, CBM_SECTOR_SIZE) == 0, "stepping away lost the written data");
}

static void testWriteRefusedOnReadOnlyImage()
{
    printf("Controller: a read only image is never written to\n");
    MemDisk disk;
    disk.readOnly = true;

    DiskController controller;
    controller.setDisk(&disk);

    uint8_t original[CBM_SECTOR_SIZE];
    disk.readSector(18, 0, original);

    uint8_t replacement[CBM_SECTOR_SIZE];
    memset(replacement, 0x77, sizeof(replacement));
    uint8_t encoded[GCR_SECTOR_SIZE_MAX];
    gcrEncodeSector(replacement, 18, 0, 'I', 'D', encoded);
    for (unsigned int i = 0; i < gcrSectorSizeForTrack(18); i++) {
        controller.writeGcrByte(encoded[i]);
    }
    controller.flush();

    uint8_t after[CBM_SECTOR_SIZE];
    disk.readSector(18, 0, after);
    CHECK(memcmp(after, original, CBM_SECTOR_SIZE) == 0, "a read only image was written to");
}

static void testGarbledTrackIsNotWrittenBack()
{
    printf("Controller: a garbled track does not scribble over the image\n");
    MemDisk disk;
    disk.readOnly = false;

    DiskController controller;
    controller.setDisk(&disk);

    uint8_t original[CBM_SECTOR_SIZE];
    disk.readSector(18, 0, original);

    // Write noise over the start of the track, wrecking the header and the
    // data block. Nothing recognisable is there, so nothing should be written
    // back rather than a wrong guess landing on a real sector.
    for (unsigned int i = 0; i < gcrSectorSizeForTrack(18); i++) {
        controller.writeGcrByte(static_cast<uint8_t>(0x33));
    }
    controller.flush();

    uint8_t after[CBM_SECTOR_SIZE];
    disk.readSector(18, 0, after);
    CHECK(memcmp(after, original, CBM_SECTOR_SIZE) == 0, "a garbled track overwrote a real sector");
}

static void testSpeedZones()
{
    printf("Controller: speed zone follows the track number\n");
    MemDisk        disk;
    DiskController controller;
    controller.setDisk(&disk);

    struct {
        unsigned int track;
        uint8_t      zone;
    } expected[] = {{1, 3}, {17, 3}, {18, 2}, {24, 2}, {25, 1}, {30, 1}, {31, 0}, {35, 0}};

    for (unsigned int i = 0; i < sizeof(expected) / sizeof(expected[0]); i++) {
        // Walk the head to the wanted track.
        while (controller.currentTrack() > expected[i].track) controller.moveHeadOut();
        while (controller.currentTrack() < expected[i].track) controller.moveHeadIn();
        CHECK(controller.speedZone() == expected[i].zone, "track %u gave zone %u, expected %u", expected[i].track,
              controller.speedZone(), expected[i].zone);
    }
}

static void testDriveMemoryMap()
{
    printf("Drive: RAM mirrors, and the VIAs sit where the ROM expects\n");
    Drive1541 drive;
    IecLines  lines;
    drive.setLines(&lines);

    drive.setMem(0x0000, 0x42);
    CHECK(drive.getMem(0x0000) == 0x42, "RAM did not hold a byte");
    // 2K of RAM appears again every 2K up to the VIAs.
    CHECK(drive.getMem(0x0800) == 0x42, "RAM did not mirror at $0800");
    CHECK(drive.getMem(0x1000) == 0x42, "RAM did not mirror at $1000");

    drive.setMem(0x07ff, 0x99);
    CHECK(drive.getMem(0x0fff) == 0x99, "the top of RAM did not mirror");

    // VIA 1 answers across its whole quarter, VIA 2 across the next.
    drive.setMem(0x1803, 0x5a);  // DDRA
    CHECK(drive.getMem(0x1803) == 0x5a, "VIA 1 DDRA did not read back");
    CHECK(drive.getMem(0x1903) == 0x5a, "VIA 1 did not mirror at $1903");

    drive.setMem(0x1c03, 0x3c);
    CHECK(drive.getMem(0x1c03) == 0x3c, "VIA 2 DDRA did not read back");
    CHECK(drive.getMem(0x1d03) == 0x3c, "VIA 2 did not mirror at $1d03");

    // With no ROM the drive reports itself unusable rather than running wild.
    CHECK(!drive.ready(), "a drive with no ROM claimed to be ready");
    CHECK(drive.getMem(0xc000) == 0, "unmapped ROM space returned something");
}

/* ------------------------------------------------- running the drive CPU -- */

// The real DOS ROM is copyrighted and not shipped, so the drive CPU could not
// be run at all in the tests. It does not need the real one: a handful of
// hand assembled instructions in a ROM image is enough to drive the whole
// stack, the reset vector, the memory map, both VIAs, the bus lines and the
// head.
//
// The program below waits for ATN, acknowledges it, reads four bytes off the
// head into zero page, then parks itself:
//
//      C000  78         sei
//      C001  a2 ff      ldx #$ff
//      C003  9a         txs
//      C004  a9 1a      lda #$1a       ; DATA, CLK and ATNA are outputs
//      C006  8d 02 18   sta $1802      ; VIA 1 DDRB
//      C009  a9 00      lda #$00
//      C00b  8d 00 18   sta $1800      ; release everything
//      C00e  ad 00 18   lda $1800      ; wait_atn: read port B
//      C011  29 80      and #$80       ; ATN reads as a one when low
//      C013  f0 f9      beq wait_atn
//      C015  a9 10      lda #$10       ; acknowledge by raising ATNA
//      C017  8d 00 18   sta $1800
//      C01a  a2 00      ldx #$00
//      C01c  ad 01 1c   lda $1c01      ; read_loop: VIA 2 port A is the head
//      C01f  9d 00 00   sta $0000,x
//      C022  e8         inx
//      C023  e0 04      cpx #$04
//      C025  d0 f5      bne read_loop
//      C027  a9 42      lda #$42       ; leave a marker so the test can tell
//      C029  8d 10 00   sta $0010
//      C02c  4c 2c c0   done: jmp done
static void buildTestRom(uint8_t* rom)
{
    memset(rom, 0xff, Drive1541::ROM_SIZE);

    static const uint8_t program[] = {
        0x78,              // sei
        0xa2, 0xff,        // ldx #$ff
        0x9a,              // txs
        0xa9, 0x1a,        // lda #$1a
        0x8d, 0x02, 0x18,  // sta $1802
        0xa9, 0x00,        // lda #$00
        0x8d, 0x00, 0x18,  // sta $1800
        0xad, 0x00, 0x18,  // lda $1800
        0x29, 0x80,        // and #$80
        0xf0, 0xf9,        // beq -7
        0xa9, 0x10,        // lda #$10
        0x8d, 0x00, 0x18,  // sta $1800
        0xa2, 0x00,        // ldx #$00
        0xad, 0x01, 0x1c,  // lda $1c01
        0x9d, 0x00, 0x00,  // sta $0000,x
        0xe8,              // inx
        0xe0, 0x04,        // cpx #$04
        0xd0, 0xf5,        // bne -11
        0xa9, 0x42,        // lda #$42
        0x8d, 0x10, 0x00,  // sta $0010
        0x4c, 0x2c, 0xc0,  // jmp $c02c
    };
    memcpy(rom, program, sizeof(program));

    // Reset vector at $fffc, which is the top of the ROM.
    rom[Drive1541::ROM_SIZE - 4] = 0x00;
    rom[Drive1541::ROM_SIZE - 3] = 0xc0;
}

static void testDriveCpuRuns()
{
    printf("Drive: the CPU boots, answers ATN and reads the head\n");

    static uint8_t rom[Drive1541::ROM_SIZE];
    buildTestRom(rom);

    MemDisk   disk;
    IecLines  lines;
    Drive1541 drive;

    lines.reset();
    drive.setLines(&lines);
    drive.setRom(rom);
    drive.setDisk(&disk);
    CHECK(drive.ready(), "the drive did not accept the ROM");

    drive.reset();

    // It should be spinning on the ATN check, not off in the weeds.
    drive.emulateCycles(200);
    CHECK(drive.getMem(0x0010) != 0x42, "the program finished before ATN was ever asserted");

    // Once the drive has set its direction register, releasing everything
    // leaves the bus alone.
    CHECK(!lines.dataLow(), "the drive held DATA low while idle");
    CHECK(!lines.clkLow(), "the drive held CLK low while idle");

    // The C64 asserts ATN. The acknowledge gate should pull DATA low straight
    // away, before the drive has even noticed.
    lines.c64Atn = true;
    drive.refreshIecOutputs();
    CHECK(lines.dataLow(), "DATA was not pulled low when ATN went active");

    // Now let it run: it should see ATN, acknowledge, and read the head.
    for (int i = 0; i < 100 && drive.getMem(0x0010) != 0x42; i++) {
        drive.emulateCycles(100);
    }
    CHECK(drive.getMem(0x0010) == 0x42, "the drive program never reached its marker");

    // Acknowledging released DATA again, which is how a drive reports itself
    // present and ready.
    CHECK(!lines.dataLow(), "DATA stayed low after the drive acknowledged");

    // The four bytes it read are the start of track 18, which begins with the
    // sync mark the encoder wrote.
    for (int i = 0; i < 4; i++) {
        uint8_t got = drive.getMem(static_cast<uint16_t>(i));
        CHECK(got == 0xff, "byte %d off the head was $%02x, expected a sync byte", i, got);
    }
}

// A second test ROM, this one putting the head into write mode and laying two
// bytes down:
//
//      C000  78         sei
//      C001  a2 ff      ldx #$ff
//      C003  9a         txs
//      C004  a9 ff      lda #$ff
//      C006  8d 03 1c   sta $1c03      ; VIA 2 DDRA, port A drives, so writing
//      C009  a9 ab      lda #$ab
//      C00b  8d 01 1c   sta $1c01      ; onto the track
//      C00e  a9 cd      lda #$cd
//      C010  8d 01 1c   sta $1c01
//      C013  a9 42      lda #$42
//      C015  8d 10 00   sta $0010      ; marker
//      C018  4c 18 c0   done: jmp done
static void buildWriteRom(uint8_t* rom)
{
    memset(rom, 0xff, Drive1541::ROM_SIZE);

    static const uint8_t program[] = {
        0x78,              // sei
        0xa2, 0xff,        // ldx #$ff
        0x9a,              // txs
        0xa9, 0xff,        // lda #$ff
        0x8d, 0x03, 0x1c,  // sta $1c03
        0xa9, 0xab,        // lda #$ab
        0x8d, 0x01, 0x1c,  // sta $1c01
        0xa9, 0xcd,        // lda #$cd
        0x8d, 0x01, 0x1c,  // sta $1c01
        0xa9, 0x42,        // lda #$42
        0x8d, 0x10, 0x00,  // sta $0010
        0x4c, 0x18, 0xc0,  // jmp $c018
    };
    memcpy(rom, program, sizeof(program));
    rom[Drive1541::ROM_SIZE - 4] = 0x00;
    rom[Drive1541::ROM_SIZE - 3] = 0xc0;
}

static void testDriveCpuWritesTheHead()
{
    printf("Drive: the CPU can put the head into write mode and lay bytes down\n");

    static uint8_t rom[Drive1541::ROM_SIZE];
    buildWriteRom(rom);

    MemDisk disk;
    disk.readOnly = false;

    IecLines  lines;
    Drive1541 drive;
    lines.reset();
    drive.setLines(&lines);
    drive.setRom(rom);
    drive.setDisk(&disk);
    drive.reset();

    for (int i = 0; i < 50 && drive.getMem(0x0010) != 0x42; i++) {
        drive.emulateCycles(50);
    }
    CHECK(drive.getMem(0x0010) == 0x42, "the write program never finished");

    // The head is now two bytes along. Reading the rest of the way round
    // brings it back to where the writes landed.
    DiskController& controller = drive.disk();
    for (unsigned int i = 0; i < controller.trackLength() - 2; i++) {
        controller.readGcrByte();
    }
    uint8_t first  = controller.readGcrByte();
    uint8_t second = controller.readGcrByte();
    CHECK(first == 0xab, "the first byte written was read back as $%02x", first);
    CHECK(second == 0xcd, "the second byte written was read back as $%02x", second);
}

static void testDriveCpuCycleAccounting()
{
    printf("Drive: instructions report the cycles they spent\n");

    static uint8_t rom[Drive1541::ROM_SIZE];
    buildTestRom(rom);

    IecLines  lines;
    Drive1541 drive;
    lines.reset();
    drive.setLines(&lines);
    drive.setRom(rom);
    drive.reset();

    // Every instruction has to claim at least one cycle, or the interleaving
    // against the C64 would spin without making progress.
    for (int i = 0; i < 20; i++) {
        unsigned int used = drive.stepInstruction();
        CHECK(used >= 1, "an instruction claimed %u cycles", used);
        CHECK(used <= 8, "an instruction claimed %u cycles, which is too many", used);
    }

    // Asking for a budget consumes roughly that much, give or take the
    // instruction it was in the middle of.
    unsigned int spent = drive.emulateCycles(100);
    CHECK(spent >= 100 && spent < 110, "a budget of 100 cycles spent %u", spent);
}

static void testLargeCycleBudget()
{
    printf("Drive: a cycle budget larger than a byte does not wrap\n");

    static uint8_t rom[Drive1541::ROM_SIZE];
    buildTestRom(rom);

    IecLines  lines;
    Drive1541 drive;
    lines.reset();
    drive.setLines(&lines);
    drive.setRom(rom);
    drive.reset();

    // The CPU's own cycle counter is a byte. Counting a budget in it would
    // wrap and spin forever the moment a caller asked for more than 255.
    unsigned int spent = drive.emulateCycles(5000);
    CHECK(spent >= 5000 && spent < 5010, "a budget of 5000 cycles spent %u", spent);
}

static void testDriveWithoutRom()
{
    printf("Drive: with no ROM it stays inert rather than running wild\n");

    IecLines  lines;
    Drive1541 drive;
    lines.reset();
    drive.setLines(&lines);
    drive.reset();

    CHECK(!drive.ready(), "a drive with no ROM claimed to be ready");
    // Asking it to run must not execute anything or touch the bus.
    unsigned int spent = drive.emulateCycles(50);
    CHECK(spent == 50, "a drive with no ROM did not just consume the budget");
    CHECK(!lines.dataLow() && !lines.clkLow(), "a drive with no ROM drove the bus");
}

int main()
{
    testViaPorts();
    testViaTimer1();
    testViaTimer1OneShotFiresOnce();
    testViaInterruptGating();
    testLinesAreWiredAnd();
    testAtnAcknowledgeGate();
    testDriveReadsBusState();
    testHeadStepping();
    testControllerReadsTrack();
    testResetKeepsTheDisk();
    testWriteProtectSense();
    testHeadWritesReachTheImage();
    testHeadWriteStepsFlush();
    testWriteRefusedOnReadOnlyImage();
    testGarbledTrackIsNotWrittenBack();
    testSpeedZones();
    testDriveMemoryMap();
    testDriveCpuRuns();
    testDriveCpuWritesTheHead();
    testDriveCpuCycleAccounting();
    testLargeCycleBudget();
    testDriveWithoutRom();

    printf("\n%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
