#include <cstdint>

struct KbMatrixEntry {
    uint8_t    sentdc00;
    uint8_t    sentdc01;
    bool       implicit_shift;
};

static const KbMatrixEntry kb_matrix[128] = {
    { 0xff, 0xff, false},  // 00
    { 0x7f, 0x7f, false},  // 01 : ESC -> Run / stop
    { 0x7f, 0xfe, false},  // 02 : 1
    { 0x7f, 0xf7, false},  // 03 : 2
    { 0xfd, 0xfe, false},  // 04 : 3
    { 0xfd, 0xf7, false},  // 05 : 4
    { 0xfb, 0xfe, false},  // 06 : 5
    { 0xfb, 0xf7, false},  // 07 : 6
    { 0xf7, 0xfe, false},  // 08 : 7
    { 0xf7, 0xf7, false},  // 09 : 8
    { 0xef, 0xfe, false},  // 0a : 9
    { 0xef, 0xf7, false},  // 0b : 0
    { 0xdf, 0xf7, false},  // 0c : -
    { 0xbf, 0xdf, false},  // 0d : =/+
    { 0xfe, 0xfe, false},  // 0e : Backspace
    { 0xff, 0xff, false},  // 0f : Tab
    { 0x7f, 0xbf, false},  // 10 : Q
    { 0xfd, 0xfd, false},  // 11 : W
    { 0xfd, 0xbf, false},  // 12 : E
    { 0xfb, 0xfd, false},  // 13 : R
    { 0xfb, 0xbf, false},  // 14 : T
    { 0xf7, 0xfd, false},  // 15 : Y
    { 0xf7, 0xbf, false},  // 16 : U
    { 0xef, 0xfd, false},  // 17 : I
    { 0xef, 0xbf, false},  // 18 : O
    { 0xdf, 0xfd, false},  // 19 : P
    { 0xff, 0xff, false},  // 1a : [ {
    { 0xff, 0xff, false},  // 1b : ] }
    { 0xfe, 0xfd, false},  // 1c : Enter
    { 0x7f, 0xfb, false},  // 1d : Ctrl
    { 0xfd, 0xfb, false},  // 1e : A
    { 0xfd, 0xdf, false},  // 1f : S
    { 0xfb, 0xfb, false},  // 20 : D
    { 0xfb, 0xdf, false},  // 21 : F
    { 0xf7, 0xfb, false},  // 22 : G
    { 0xf7, 0xdf, false},  // 23 : H
    { 0xef, 0xfb, false},  // 24 : J
    { 0xef, 0xdf, false},  // 25 : K
    { 0xdf, 0xfb, false},  // 26 : L
    { 0xff, 0xff, false},  // 27 : ; :
    { 0xff, 0xff, false},  // 28 : ' "
    { 0xff, 0xff, false},  // 29 : ` ~
    { 0xfd, 0x7f, false},  // 2a : LShift
    { 0xff, 0xff, false},  // 2b : \ |
    { 0xfd, 0xef, false},  // 2c : Z
    { 0xfb, 0x7f, false},  // 2d : X
    { 0xfb, 0xef, false},  // 2e : C
    { 0xf7, 0x7f, false},  // 2f : V
    { 0xf7, 0xef, false},  // 30 : B
    { 0xef, 0x7f, false},  // 31 : N
    { 0xef, 0xef, false},  // 32 : M
    { 0xdf, 0x7f, false},  // 33 : , <
    { 0xdf, 0xef, false},  // 34 : . >
    { 0xbf, 0x7f, false},  // 35 : / ?
    { 0xbf, 0xef, false},  // 36 : RShift
    { 0xff, 0xff, false},  // 37 : Keypad * / prtscn
    { 0xff, 0xff, false},  // 38 : Left Alt
    { 0x7f, 0xef, false},  // 39 : Space
    { 0xff, 0xff, false},  // 3a : Caps Lock
    { 0xfe, 0xef, false},  // 3b : F1
    { 0xfe, 0xdf, false},  // 3c : F2 -> F3
    { 0xfe, 0xbf, false},  // 3d : F3 -> F5
    { 0xfe, 0x7f, false},  // 3e : F4 -> F7
    { 0xff, 0xff, false},  // 3f : F5
    { 0xff, 0xff, false},  // 40 : F6
    { 0xff, 0xff, false},  // 41 : F7
    { 0xff, 0xff, false},  // 42 : F8
    { 0xff, 0xff, false},  // 43 : F9
    { 0xff, 0xff, false},  // 44 : F10
    { 0xff, 0xff, false},  // 45 : Num Lock
    { 0xff, 0xff, false},  // 46 : Scroll Lock
    { 0xff, 0xff, false},  // 47 : 
    { 0xfe, 0x7f, true},  // 48 : Arrow Up
    { 0xff, 0xff, false},  // 49
    { 0xff, 0xff, false},  // 4a
    { 0xfe, 0xfb, false},  // 4b : Arrow Left
    { 0xff, 0xff, false},  // 4c
    { 0xfe, 0xfb, true},  // 4d : Arrow Right
    { 0xff, 0xff, false},  // 4e
    { 0xff, 0xff, false},  // 4f
    { 0xfe, 0x7f, false},  // 50 : Arrow Down
    { 0xff, 0xff, false},  // 51
    { 0xff, 0xff, false},  // 52
    { 0xff, 0xff, false},  // 53
    { 0xff, 0xff, false},  // 54
    { 0xff, 0xff, false},  // 55
    { 0xff, 0xff, false},  // 56
    { 0xff, 0xff, false},  // 57
    { 0xff, 0xff, false},  // 58
    { 0xff, 0xff, false},  // 59
    { 0xff, 0xff, false},  // 5a
    { 0x7f, 0xdf, false},  // 5b : Commodore
    { 0xff, 0xff, false},  // 5c : Right windows key 
    { 0xff, 0xff, false},  // 5d
    { 0xff, 0xff, false},  // 5e
    { 0xff, 0xff, false},  // 5f
    { 0xff, 0xff, false},  // 60
    { 0xff, 0xff, false},  // 61
    { 0xff, 0xff, false},  // 62
    { 0xff, 0xff, false},  // 63
    { 0xff, 0xff, false},  // 64
    { 0xff, 0xff, false},  // 65
    { 0xff, 0xff, false},  // 66
    { 0xff, 0xff, false},  // 67
    { 0xff, 0xff, false},  // 68
    { 0xff, 0xff, false},  // 69
    { 0xff, 0xff, false},  // 6a
    { 0xff, 0xff, false},  // 6b
    { 0xff, 0xff, false},  // 6c
    { 0xff, 0xff, false},  // 6d
    { 0xff, 0xff, false},  // 6e
    { 0xff, 0xff, false},  // 6f
    { 0xff, 0xff, false},  // 70
    { 0xff, 0xff, false},  // 71
    { 0xff, 0xff, false},  // 72
    { 0xff, 0xff, false},  // 73
    { 0xff, 0xff, false},  // 74
    { 0xff, 0xff, false},  // 75
    { 0xff, 0xff, false},  // 76
    { 0xff, 0xff, false},  // 77
    { 0xff, 0xff, false},  // 78
    { 0xff, 0xff, false},  // 79
    { 0xff, 0xff, false},  // 7a
    { 0xff, 0xff, false},  // 7b
    { 0xff, 0xff, false},  // 7c
    { 0xff, 0xff, false},  // 7d
    { 0xff, 0xff, false},  // 7e
    { 0xff, 0xff, false},  // 7f
};