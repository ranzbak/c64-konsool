#include "CbmImage.hpp"
#include <cctype>

ImageFormat imageFormatFromName(const std::string& filename)
{
    size_t dot = filename.find_last_of('.');
    if (dot == std::string::npos) {
        return ImageFormat::UNKNOWN;
    }
    std::string ext = filename.substr(dot + 1);
    for (size_t i = 0; i < ext.size(); i++) {
        ext[i] = static_cast<char>(tolower(static_cast<unsigned char>(ext[i])));
    }
    if (ext == "prg") return ImageFormat::PRG;
    if (ext == "t64") return ImageFormat::T64;
    if (ext == "d64") return ImageFormat::D64;
    return ImageFormat::UNKNOWN;
}

// PETSCII file names use $41-$5A for the letters, and the shifted charset
// repeats them at $C1-$DA. Everything printable is passed through as is,
// anything else becomes an underscore so a corrupt name cannot smuggle
// control characters into the menu.
static char petToAscii(uint8_t c)
{
    // PETSCII repeats its graphics rather than its letters: $c0-$df draws the
    // same shapes as $60-$7f, and $e0-$ff the same as $a0-$bf. A directory is
    // listed in the unshifted charset, where $c3 is SHIFT+C, a horizontal
    // rule, and not the letter C. Folding these onto $41-$5a is what turned
    // the box a disk draws around its title into a row of capitals.
    if (c >= 0xC0 && c <= 0xDF) {
        c = static_cast<uint8_t>(c - 0x60);
    } else if (c >= 0xE0) {
        c = static_cast<uint8_t>(c - 0x40);
    }
    if (c >= 0x41 && c <= 0x5A) return static_cast<char>(c);  // A-Z
    if (c >= 0x20 && c <= 0x40) return static_cast<char>(c);  // space, digits, punctuation
    if (c == 0x5B || c == 0x5D) return static_cast<char>(c);  // [ ]

    // A control code in a name is not art, it is either junk or an attempt to
    // do something to the screen, so keep it obvious rather than dressing it
    // up as a graphic.
    if (c < 0x20 || (c >= 0x80 && c <= 0x9F)) return '_';

    // $a0 is the shifted space, which draws as a blank, and the rest of that
    // block is the solid and shaded blocks.
    if (c == 0xA0) return ' ';
    if (c >= 0xA1 && c <= 0xBF) return '#';

    // Disk names and directory art lean on the block graphics, and turning all
    // of them into the same character makes a border look like a mistake.
    // These are only stand-ins for a font that has no PETSCII in it, but they
    // keep the shape of what was drawn.
    switch (c) {
        case 0x60:  // horizontal bar, and the line the shifted set draws with
        case 0x40:
        case 0x63:
        case 0x64:
        case 0x77:
        case 0x78:
            return '-';
        case 0x5C:  // vertical bar
        case 0x62:
        case 0x65:
        case 0x67:
        case 0x74:
            return '|';
        case 0x69:  // corners and junctions, including the rounded ones that
        case 0x6A:  // SHIFT+I/J/K/U draw around a directory title
        case 0x6B:
        case 0x75:
        case 0x6C:
        case 0x6E:
        case 0x6F:
        case 0x70:
        case 0x72:
        case 0x73:
        case 0x7A:
        case 0x7B:
        case 0x7D:
            return '+';
        case 0x61:  // solid and shaded blocks
        case 0x66:
        case 0x76:
        case 0x79:
        case 0x7C:
        case 0x7E:
        case 0x7F:
            return '#';
        default:
            break;
    }
    return '.';
}


// Tape records pad with $20, disk directory slots pad with $A0, and a slot
// that was never written is left at $00. Only the trailing run goes: a $A0 in
// the middle of a name is a shifted space somebody drew with.
static size_t petsciiTrim(const uint8_t* petscii, size_t len)
{
    while (len > 0 && (petscii[len - 1] == 0xA0 || petscii[len - 1] == 0x20 || petscii[len - 1] == 0x00)) {
        len--;
    }
    return len;
}


// PETSCII repeats its graphics rather than its letters, so the two upper
// blocks fold back onto $40-$7f. See petToAscii() above for why that matters.
uint8_t petsciiToScreenCode(uint8_t c)
{
    if (c < 0x20) return 0x20;                              // control code, no glyph
    if (c < 0x40) return c;                                 // space, digits, punctuation
    if (c < 0x60) return static_cast<uint8_t>(c - 0x40);    // @ A-Z [ ]
    if (c < 0x80) return static_cast<uint8_t>(c - 0x20);    // the graphics block
    if (c < 0xA0) return 0x20;                              // control code, no glyph
    if (c < 0xC0) return static_cast<uint8_t>(c - 0x40);    // shifted space and the blocks
    if (c < 0xE0) return static_cast<uint8_t>(c - 0x80);    // repeat of $60-$7f
    return static_cast<uint8_t>(c - 0x80);                  // repeat of $a0-$bf
}


std::string petsciiRaw(const uint8_t* petscii, size_t len)
{
    len = petsciiTrim(petscii, len);
    return std::string(reinterpret_cast<const char*>(petscii), len);
}


std::string petsciiToDisplay(const uint8_t* petscii, size_t len)
{
    len = petsciiTrim(petscii, len);
    std::string out;
    out.reserve(len);
    for (size_t i = 0; i < len; i++) {
        out.push_back(petToAscii(petscii[i]));
    }
    return out;
}
