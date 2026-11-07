#pragma once
#include <cstdint>
#include <cstring>
#include "DisplayDriver.hpp"

// Keeps the picture in memory instead of pushing it at a panel. The VIC hands
// over 320x208 of screen and a colour per border line, which is all a
// screenshot needs.
class HeadlessDisplay : public DisplayDriver {
   public:
    static const int W = 320, H = 208, BORDER = 26;

    uint16_t screen[W * H];
    uint16_t border[260];
    long     frames = 0;

    HeadlessDisplay() { memset(screen, 0, sizeof(screen)); memset(border, 0, sizeof(border)); }

    void init() override {}
    void drawBitmap(uint16_t* bitmap) override { memcpy(screen, bitmap, sizeof(screen)); }
    void drawFrame(uint16_t* frameColors) override {
        memcpy(border, frameColors, sizeof(border));
        frames++;
    }
    const uint16_t* getC64Colors() const override { return colors; }

    // The sixteen C64 colours as RGB565, the same order the emulator uses.
    // Defined here rather than in the runner so that anything including this
    // header links, tests included.
    static constexpr uint16_t colors[16] = {
        0x0000, 0xFFFF, 0x8000, 0x07FF, 0x8010, 0x0400, 0x0010, 0xFFE0,
        0xFC00, 0x8200, 0xFA10, 0x4208, 0x8410, 0x07E0, 0x421F, 0xA534,
    };
};
