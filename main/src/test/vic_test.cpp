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

// What the VIC actually puts on the screen.
//
// Everything else that checks the VIC compares one build against another and
// can only say "this changed". That catches a regression and says nothing
// about whether either picture was right, which is how a wrong charset can
// survive every gate. These tests set the machine up by hand and assert the
// pixels from the chip's own rules instead.
//
// They assert structure, not colour: which pixels carry the foreground value
// and which the background. That is independent of the palette, so a change to
// the colour table cannot make a correct renderer look broken.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

#include "HeadlessDisplay.hpp"
#include "VIC.hpp"
#include "roms/charset.h"
#include "sid/sid.hpp"

static int checks   = 0;
static int failures = 0;

#define CHECK(cond, ...)                     \
    do {                                     \
        checks++;                            \
        if (!(cond)) {                       \
            failures++;                      \
            printf("  FAIL: ");              \
            printf(__VA_ARGS__);             \
            printf("\n");                    \
        }                                    \
    } while (0)

static uint8_t ram[65536];
static uint8_t sidregs[32];

// drawRasterline writes screen row (rasterline - 0x32), so this is the raster
// line that lands on row 0 of the picture.
static const int FIRST_LINE = 0x32;

struct Machine {
    VIC vic;
    SID sid;

    Machine()
    {
        memset(ram, 0, sizeof(ram));
        memset(sidregs, 0, sizeof(sidregs));
        // A no-op sink rather than nullptr: raster_line() calls the callback
        // without checking it.
        sid.init(sidregs, [](int16_t*, size_t) {}, 8580);
        vic.init(ram, charset_rom, &sid);
        vic.initVarsAndRegs();

        // Standard text mode, screen at $0400, charset from the character ROM,
        // 25 rows and 40 columns, display enabled, YSCROLL at its default 3.
        vic.vicreg[0x11]    = 0x1b;
        vic.vicreg[0x16]    = 0x08;
        vic.vicreg[0x18]    = 0x15;
        vic.vicreg[0x20]    = 0x0e;
        vic.vicreg[0x21]    = 0x06;
        vic.vicmem          = 0x0000;
        vic.screenmemstart  = 0x0400;
        vic.charset         = charset_rom;
        vic.screenblank     = false;
        vic.screenHeight();
    }

    // Draws a whole frame, raster line 0 to 311, the way the emulation does.
    // It has to be the whole frame: the VIC carries VC, VCBASE and RC across
    // lines, and they are only set up from the top, so starting part way down
    // the screen leaves the counters in a state the hardware never reaches.
    // perLine is called before each line with the raster number, which is how
    // a test reprograms a register mid-screen.
    void drawFrame(void (*perLine)(VIC&, int) = nullptr)
    {
        for (int r = 0; r < 312; r++) {
            vic.rasterline = static_cast<uint16_t>(r);
            if (perLine) perLine(vic, r);
            vic.drawRasterline();
        }
        // Hand the finished frame to the display, once, as the emulation does.
        vic.refresh(true);
    }

    // Read the picture the way the panel would: through the display driver,
    // after refresh() has handed the frame over. That exercises the real
    // output path rather than reaching into the VIC's own buffer.
    uint16_t px(int row, int col)
    {
        HeadlessDisplay* d = static_cast<HeadlessDisplay*>(vic.getDriver());
        return d->screen[row * 320 + col];
    }
};

// The eight pixels of one character cell on one row, as a bit pattern: a set
// bit means that pixel is not the background.
static uint8_t cellBits(Machine& m, int screenRow, int cell, uint16_t bg)
{
    uint8_t bits = 0;
    for (int i = 0; i < 8; i++) {
        if (m.px(screenRow, cell * 8 + i) != bg) bits |= (0x80 >> i);
    }
    return bits;
}

static void testStandardTextMode()
{
    printf("VIC: a character comes out as the bits in the character ROM\n");
    Machine m;

    // "A" is screen code 1, and light blue on whatever the background is.
    for (int i = 0; i < 40; i++) {
        ram[0x0400 + i]        = 0x01;
        m.vic.colormap[i]      = 0x0e;
    }
    m.drawFrame();

    // Row 0 of "A" is blank in the ROM, so its leftmost pixel is the
    // background colour: read it from there rather than assuming a palette.
    uint16_t bg = m.px(0, 0);

    bool anySet = false;
    for (int row = 0; row < 8; row++) {
        uint8_t want = charset_rom[0x01 * 8 + row];
        uint8_t got  = cellBits(m, row, 0, bg);
        if (want) anySet = true;
        CHECK(got == want, "row %d of 'A' drew %02x, the character ROM says %02x", row, got, want);
    }
    CHECK(anySet, "the glyph tested was blank on every row, the test proves nothing");
}

static void testEveryColumnIsFetched()
{
    printf("VIC: all forty columns come from consecutive screen memory\n");
    Machine m;

    // A different character in each cell, so a column drawn from the wrong
    // address shows up immediately.
    for (int i = 0; i < 40; i++) {
        ram[0x0400 + i]   = static_cast<uint8_t>(i + 1);
        m.vic.colormap[i] = 0x0e;
    }
    m.drawFrame();

    // Row 4 of the ROM's glyphs is where most characters have something.
    const int row = 4;
    // Take the background from a cell whose glyph row is empty, if there is
    // one; otherwise compare relative patterns only.
    uint16_t background = 0;
    bool     haveBg     = false;
    for (int c = 0; c < 40 && !haveBg; c++) {
        if (charset_rom[(c + 1) * 8 + row] == 0x00) {
            background = m.px(row, c * 8);
            haveBg     = true;
        }
    }
    CHECK(haveBg, "no empty glyph row to read the background from");
    if (!haveBg) return;

    int wrong = 0;
    for (int c = 0; c < 40; c++) {
        uint8_t want = charset_rom[(c + 1) * 8 + row];
        uint8_t got  = cellBits(m, row, c, background);
        if (got != want) wrong++;
    }
    CHECK(wrong == 0, "%d of 40 columns did not match the character ROM", wrong);
}

// The point of the badline model. With YSCROLL left alone the display advances
// a character row every eight lines; when a demo moves YSCROLL so the badline
// never comes, the VIC stops fetching and the row must not advance.
static void testRowAdvancesEveryEightLines()
{
    printf("VIC: the character row advances once every eight raster lines\n");
    Machine m;

    // Row 0 all "A", row 1 all "B", so which row is being drawn is readable
    // from the pixels.
    for (int i = 0; i < 40; i++) {
        ram[0x0400 + i]      = 0x01;  // A
        ram[0x0400 + 40 + i] = 0x02;  // B
        m.vic.colormap[i]      = 0x0e;
        m.vic.colormap[40 + i] = 0x0e;
    }
    m.drawFrame();

    uint16_t bg = m.px(0, 0);
    // Line 8 is the first line of the second character row, so it must show
    // row 0 of "B", not row 0 of "A" again.
    uint8_t want = charset_rom[0x02 * 8 + 0];
    uint8_t got  = cellBits(m, 8, 0, bg);
    CHECK(got == want, "line 8 drew %02x, expected row 0 of 'B' which is %02x", got, want);

    // And line 7 is still the last line of the first row.
    want = charset_rom[0x01 * 8 + 7];
    got  = cellBits(m, 7, 0, bg);
    CHECK(got == want, "line 7 drew %02x, expected row 7 of 'A' which is %02x", got, want);
}


// FLD: a demo moves YSCROLL every raster line so the bad line never fires. The
// VIC then stops fetching, RC freezes, VCBASE stops advancing and the display
// stretches. Before there were counters to freeze, YSCROLL only shifted the
// finished picture, so the row went on advancing regardless -- which is the
// whole reason this model exists.
static void testBadLineSuppressionStopsTheRow()
{
    printf("VIC: suppressing bad lines stops the character row advancing\n");
    Machine m;

    for (int i = 0; i < 40; i++) {
        ram[0x0400 + i]        = 0x01;  // A
        ram[0x0400 + 40 + i]   = 0x02;  // B
        m.vic.colormap[i]      = 0x0e;
        m.vic.colormap[40 + i] = 0x0e;
    }

    // Let the first character row start normally, then keep YSCROLL away from
    // the line the VIC actually compares against so no bad line can match
    // again. The model tests hwline, which is rasterline + 1, so the value has
    // to be chosen against that and not against the raster number itself.
    m.drawFrame([](VIC& v, int raster) {
        if (raster >= 0x39) {
            int hw = (raster + 1) & 7;
            v.vicreg[0x11] = static_cast<uint8_t>(0x18 | ((hw + 1) & 7));
        }
    });

    uint16_t bg   = m.px(0, 0);
    uint8_t  bRow = charset_rom[0x02 * 8 + 0];
    uint8_t  got  = cellBits(m, 8, 0, bg);

    CHECK(got != bRow, "line 8 drew row 0 of 'B' (%02x) even though no bad line fired, so the row advanced anyway",
          bRow);

    // And the first row, drawn before the suppression starts, must be intact:
    // this must not be a test that passes because the whole screen broke.
    uint8_t want = charset_rom[0x01 * 8 + 1];
    uint8_t r1   = cellBits(m, 1, 0, bg);
    CHECK(r1 == want, "row 1 of 'A' drew %02x before any suppression, expected %02x", r1, want);
}


// Sprites are positioned against the raster line and nothing else. YSCROLL
// scrolls the character display; there is no path from $d011 to the sprite
// sequencer. This one is here because there was such a path: the sprite Y
// comparison, the row of sprite data fetched and the row it was drawn on all
// took "rasterline + (vicreg[0x11] & 7) - 3", so every sprite moved when a
// demo scrolled the text. The default $d011 of $1b makes that term zero, which
// is why only demos ever noticed.
static void testSpritesIgnoreYScroll()
{
    printf("VIC: a sprite sits on the same lines whatever YSCROLL says\n");

    // Which screen rows a solid sprite covers, with YSCROLL set to yscroll.
    auto rowsCovered = [](int yscroll) {
        Machine m;
        // Solid 24x21 sprite: three bytes per row, all ones.
        for (int i = 0; i < 63; i++) ram[0x0800 + i] = 0xff;
        ram[0x0400 + 1016] = 0x20;  // sprite 0 data at $20 * 64 = $0800
        m.vic.vicreg[0x15] = 0x01;  // sprite 0 on
        m.vic.vicreg[0x00] = 0x40;  // x, minus the 24 pixel border offset
        m.vic.vicreg[0x01] = 0x60;  // y
        m.vic.vicreg[0x27] = 0x01;  // white
        m.vic.vicreg[0x11] = static_cast<uint8_t>(0x18 | (yscroll & 7));
        m.vic.vicreg[0x21] = 0x06;  // blue paper, and every cell is black ink,
        for (int i = 0; i < 1000; i++) m.vic.colormap[i] = 0x00;  // so only the
        m.drawFrame();                                            // sprite is white

        // Find the sprite by its own colour, not by "differs from the
        // background": changing YSCROLL changes how the characters are drawn
        // too, so anything looser measures the text rather than the sprite.
        const uint16_t white = HeadlessDisplay::colors[1];
        std::string    rows;
        for (int r = 0; r < 200; r++) {
            bool any = false;
            for (int c = 0; c < 320 && !any; c++) {
                if (m.px(r, c) == white) any = true;
            }
            rows += any ? '1' : '0';
        }
        return rows;
    };

    std::string at3 = rowsCovered(3);   // the default
    std::string at7 = rowsCovered(7);
    std::string at0 = rowsCovered(0);

    CHECK(at3.find('1') != std::string::npos, "the sprite did not appear at all, the test proves nothing");
    CHECK(at3 == at7, "moving YSCROLL from 3 to 7 moved the sprite");
    CHECK(at3 == at0, "moving YSCROLL from 3 to 0 moved the sprite");
}

// In the multicolour modes the bit pairs 00 and 01 are both background, and
// only 10 and 11 are foreground. That decides sprite priority and
// sprite-to-background collision. Calling 01 foreground hid sprites behind
// what should have been backdrop, and only in multicolour, which is why hires
// looked fine while multicolour did not.
static void testMulticolourZeroOneIsBackground()
{
    printf("VIC: bit pair 01 is background in multicolour, so a sprite shows through it\n");

    // Fill the screen with a character whose every bit pair is 01, put a solid
    // sprite over it with background priority, and see whether the sprite is
    // drawn. 01 is background, so it must be.
    Machine m;
    for (int i = 0; i < 8; i++) ram[0x0800 + i] = 0x55;  // 01 01 01 01
    for (int i = 0; i < 1000; i++) {
        ram[0x0400 + i]   = 0x00;   // character 0, whose glyph is the $55 above
        m.vic.colormap[i] = 0x0f;   // bit 3 set: this cell is multicolour
    }
    for (int i = 0; i < 63; i++) ram[0x0a00 + i] = 0xff;
    ram[0x0400 + 1016] = 0x28;      // sprite data at $28 * 64 = $0a00

    m.vic.vicreg[0x11] = 0x1b;
    m.vic.vicreg[0x16] = 0x18;      // multicolour text
    m.vic.vicreg[0x18] = 0x12;      // screen $0400, charset $0800
    m.vic.charset      = ram + 0x0800;
    m.vic.vicreg[0x22] = 0x02;
    m.vic.vicreg[0x23] = 0x03;
    m.vic.vicreg[0x15] = 0x01;      // sprite 0 on
    m.vic.vicreg[0x1b] = 0x01;      // sprite 0 BEHIND the foreground
    m.vic.vicreg[0x00] = 0x50;
    m.vic.vicreg[0x01] = 0x60;
    m.vic.vicreg[0x27] = 0x01;      // white
    m.drawFrame();

    // The sprite's colour must appear somewhere in the band it covers. If 01
    // counted as foreground the sprite would be hidden behind it everywhere.
    const uint16_t white = HeadlessDisplay::colors[1];
    int hits = 0;
    for (int r = 0; r < 200; r++)
        for (int c = 0; c < 320; c++)
            if (m.px(r, c) == white) hits++;
    CHECK(hits > 0, "a sprite with background priority was hidden by bit pair 01, which is not foreground");
}


// Multicolour text takes its four colours from four different places, and
// getting one of them from the wrong register is invisible until a demo uses
// it: 00 is the background, 01 is $d022, 10 is $d023 and 11 is the low three
// bits of colour RAM. Each pair is two pixels wide.
static void testMulticolourTextColours()
{
    printf("VIC: multicolour text takes 01 from $d022, 10 from $d023, 11 from colour RAM\n");
    Machine m;

    // One glyph byte covering all four pairs, in order: 00 01 10 11.
    for (int i = 0; i < 8; i++) ram[0x0800 + i] = 0x1b;
    for (int i = 0; i < 1000; i++) {
        ram[0x0400 + i]   = 0x00;
        m.vic.colormap[i] = 0x0f;  // bit 3 makes the cell multicolour, 7 is the colour
    }
    m.vic.charset      = ram + 0x0800;
    m.vic.vicreg[0x16] = 0x18;  // multicolour, 40 columns
    m.vic.vicreg[0x21] = 0x00;  // 00 -> black
    m.vic.vicreg[0x22] = 0x02;  // 01 -> red
    m.vic.vicreg[0x23] = 0x03;  // 10 -> cyan
    m.drawFrame();

    const uint16_t* pal  = HeadlessDisplay::colors;
    const uint16_t  want[4] = {pal[0], pal[2], pal[3], pal[7]};
    const char*     from[4] = {"$d021", "$d022", "$d023", "colour RAM"};

    for (int pair = 0; pair < 4; pair++) {
        // Two pixels per pair, so pair p covers columns 2p and 2p+1.
        for (int half = 0; half < 2; half++) {
            uint16_t got = m.px(0, pair * 2 + half);
            CHECK(got == want[pair], "multicolour pair %d pixel %d came out %04x, expected %04x from %s", pair, half,
                  got, want[pair], from[pair]);
        }
    }
}

// ECM and MCM set together is one of the VIC's invalid modes. The chip outputs
// black. Drawing nothing instead left whatever the previous frame had put in
// that row of the buffer, so an invalid mode showed a stale picture.
static void testInvalidModeIsBlack()
{
    printf("VIC: the invalid ECM+MCM mode outputs black, not the last frame\n");
    Machine m;

    // Draw a normal screenful first, so there is something to leave behind.
    for (int i = 0; i < 1000; i++) {
        ram[0x0400 + i]   = 0x01;
        m.vic.colormap[i] = 0x0e;
    }
    m.drawFrame();
    bool drewSomething = false;
    for (int c = 0; c < 320 && !drewSomething; c++)
        if (m.px(4, c) != m.px(4, 0)) drewSomething = true;
    CHECK(drewSomething, "the first frame drew nothing, so the test cannot show the second one replaced it");

    // Now the invalid combination: ECM in $d011 and MCM in $d016.
    m.vic.vicreg[0x11] = 0x5b;
    m.vic.vicreg[0x16] = 0x18;
    m.drawFrame();

    const uint16_t black = HeadlessDisplay::colors[0];
    int wrong = 0;
    for (int c = 0; c < 320; c++)
        if (m.px(4, c) != black) wrong++;
    CHECK(wrong == 0, "%d of 320 pixels on an invalid-mode line were not black", wrong);
}


// Hires bitmap takes both its colours from the video matrix byte and none from
// colour RAM: a set bit is the high nibble, a clear bit the low one. That is
// the one mode where colour RAM is not read at all.
static void testHiresBitmapColours()
{
    printf("VIC: hires bitmap takes both colours from the video matrix byte\n");
    Machine m;

    m.vic.vicreg[0x11] = 0x3b;      // bitmap mode, display on, 25 rows
    m.vic.vicreg[0x16] = 0x08;      // hires, 40 columns
    m.vic.bitmapstart  = 0x2000;
    m.vic.screenmemstart = 0x0400;

    ram[0x2000] = 0xaa;             // 1010 1010, alternating
    for (int i = 0; i < 1000; i++) {
        ram[0x0400 + i]   = 0x71;   // 7 where set, 1 where clear
        m.vic.colormap[i] = 0x0c;   // must be ignored entirely
    }
    m.drawFrame();

    const uint16_t* pal = HeadlessDisplay::colors;
    for (int x = 0; x < 8; x++) {
        bool     set  = (0xaa >> (7 - x)) & 1;
        uint16_t want = set ? pal[7] : pal[1];
        uint16_t got  = m.px(0, x);
        CHECK(got == want, "hires bitmap pixel %d came out %04x, expected %04x from the %s nibble", x, got, want,
              set ? "high" : "low");
    }
}

// Multicolour bitmap uses all three sources: 00 is $d021, 01 the high nibble of
// the video matrix byte, 10 the low nibble, and 11 colour RAM.
static void testMulticolourBitmapColours()
{
    printf("VIC: multicolour bitmap takes 01 and 10 from the matrix nibbles, 11 from colour RAM\n");
    Machine m;

    m.vic.vicreg[0x11] = 0x3b;      // bitmap mode
    m.vic.vicreg[0x16] = 0x18;      // multicolour, 40 columns
    m.vic.vicreg[0x21] = 0x00;      // 00 -> black
    m.vic.bitmapstart  = 0x2000;
    m.vic.screenmemstart = 0x0400;

    ram[0x2000] = 0x1b;             // 00 01 10 11
    for (int i = 0; i < 1000; i++) {
        ram[0x0400 + i]   = 0x21;   // 01 -> 2, 10 -> 1
        m.vic.colormap[i] = 0x05;   // 11 -> 5
    }
    m.drawFrame();

    const uint16_t* pal     = HeadlessDisplay::colors;
    const uint16_t  want[4] = {pal[0], pal[2], pal[1], pal[5]};
    const char*     from[4] = {"$d021", "the high nibble", "the low nibble", "colour RAM"};

    for (int pair = 0; pair < 4; pair++) {
        for (int half = 0; half < 2; half++) {
            uint16_t got = m.px(0, pair * 2 + half);
            CHECK(got == want[pair], "mc bitmap pair %d pixel %d came out %04x, expected %04x from %s", pair, half,
                  got, want[pair], from[pair]);
        }
    }
}


// A multicolour sprite draws in pairs, two pixels wide: 00 is transparent, 01
// is $d025, 10 is the sprite's own colour, and 11 is $d026. Edge of Disgrace
// leans on these, and none of it was covered.
static void testMulticolourSpriteColours()
{
    printf("VIC: a multicolour sprite takes 01 from $d025, 10 from its own colour, 11 from $d026\n");
    Machine m;

    // A blank screen in a known background, so "transparent" is checkable.
    for (int i = 0; i < 1000; i++) {
        ram[0x0400 + i]   = 0x20;  // space
        m.vic.colormap[i] = 0x00;
    }
    for (int r = 0; r < 21; r++) ram[0x0800 + r * 3] = 0x1b;  // 00 01 10 11
    ram[0x0400 + 1016] = 0x20;                                // sprite 0 data at $0800

    m.vic.vicreg[0x21] = 0x06;  // background blue
    m.vic.vicreg[0x15] = 0x01;  // sprite 0 on
    m.vic.vicreg[0x1c] = 0x01;  // sprite 0 multicolour
    m.vic.vicreg[0x00] = 0x40;  // x = 64 - 24 = 40
    m.vic.vicreg[0x01] = 0x60;  // y = 96, so screen rows 46..66
    m.vic.vicreg[0x25] = 0x02;  // 01
    m.vic.vicreg[0x26] = 0x03;  // 11
    m.vic.vicreg[0x27] = 0x01;  // 10, the sprite's own colour
    m.drawFrame();

    const uint16_t* pal     = HeadlessDisplay::colors;
    const uint16_t  want[4] = {pal[6], pal[2], pal[1], pal[3]};
    const char*     from[4] = {"the background, being transparent", "$d025", "the sprite's own colour", "$d026"};

    const int row = 50;  // inside 46..66
    for (int pair = 0; pair < 4; pair++) {
        for (int half = 0; half < 2; half++) {
            uint16_t got = m.px(row, 40 + pair * 2 + half);
            CHECK(got == want[pair], "mc sprite pair %d pixel %d came out %04x, expected %04x from %s", pair, half,
                  got, want[pair], from[pair]);
        }
    }
}

// Y expansion doubles a sprite's height, 21 lines becoming 42, with each row of
// data drawn twice.
static void testSpriteYExpansion()
{
    printf("VIC: Y expansion makes a sprite twice as tall\n");

    auto height = [](bool expanded) {
        Machine m;
        for (int i = 0; i < 63; i++) ram[0x0800 + i] = 0xff;
        ram[0x0400 + 1016] = 0x20;
        for (int i = 0; i < 1000; i++) m.vic.colormap[i] = 0x00;
        m.vic.vicreg[0x21] = 0x06;
        m.vic.vicreg[0x15] = 0x01;
        m.vic.vicreg[0x17] = expanded ? 0x01 : 0x00;  // Y expand sprite 0
        m.vic.vicreg[0x00] = 0x40;
        m.vic.vicreg[0x01] = 0x60;
        m.vic.vicreg[0x27] = 0x01;  // white
        m.drawFrame();

        const uint16_t white = HeadlessDisplay::colors[1];
        int rows = 0;
        for (int r = 0; r < 200; r++) {
            for (int c = 0; c < 320; c++) {
                if (m.px(r, c) == white) { rows++; break; }
            }
        }
        return rows;
    };

    int plain    = height(false);
    int expanded = height(true);
    CHECK(plain == 21, "an unexpanded sprite covered %d lines, expected 21", plain);
    CHECK(expanded == 42, "a Y expanded sprite covered %d lines, expected 42", expanded);
}

int main()
{
    printf("VIC rendering\n\n");
    testStandardTextMode();
    testEveryColumnIsFetched();
    testRowAdvancesEveryEightLines();
    testBadLineSuppressionStopsTheRow();
    testSpritesIgnoreYScroll();
    testMulticolourZeroOneIsBackground();
    testMulticolourTextColours();
    testInvalidModeIsBlack();
    testHiresBitmapColours();
    testMulticolourBitmapColours();
    testMulticolourSpriteColours();
    testSpriteYExpansion();
    printf("\n%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
