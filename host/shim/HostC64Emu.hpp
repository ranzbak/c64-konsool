#pragma once
// Shadows the board version. CPUC64 only ever reaches into C64Emu for the
// keyboard, so the host build supplies just that and skips the menu overlay,
// the SID output stage and the BSP entirely.
#include <cstdint>
#include <cstring>

// A C64 keyboard matrix that can be typed into from the command line.
class HostKeyboard {
   public:
    // Eight rows of eight keys, a bit clear meaning held down.
    uint8_t rows[8];

    HostKeyboard() { memset(rows, 0xff, sizeof(rows)); }

    void clear() { memset(rows, 0xff, sizeof(rows)); }

    void press(int row, int col) { rows[row] &= ~(1 << col); }

    // What the C64 reads back on $dc01 for whatever it selected on $dc00.
    uint8_t getdc01(uint8_t dc00, bool)
    {
        uint8_t v = 0xff;
        for (int r = 0; r < 8; r++) {
            if (!(dc00 & (1 << r))) v &= rows[r];
        }
        return v;
    }
    uint8_t getKBJoyValue() { return 0xff; }
    uint8_t getGamepadJoyValue() { return 0xff; }
    void    setKbcodes(uint8_t, uint8_t) {}
};

class C64Emu {
   public:
    HostKeyboard konsoolkb;
};
