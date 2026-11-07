#!/usr/bin/env python3
"""
Generate main/src/kbmatrix.hpp from a VICE GTK3 keyboard mapping (.vkm) file.

Usage:
    tools/gen_kbmatrix.py [path/to/keymap.vkm] [-o path/to/kbmatrix.hpp]

If no arguments are given, it reads "VICE-GTK3-Keymap v0.91 beta.vkm" from the
repo root and writes main/src/kbmatrix.hpp.

Background
----------
A .vkm file maps a *host* key (identified by a GDK/GTK keysym name, e.g. "a",
"exclam", "F1") to a position (row, column) in the C64 keyboard matrix, plus a
"shiftflag" describing how the C64 shift key should behave for that mapping.
See the comment header of any .vkm file (or
https://vice-emu.sourceforge.io/vice_toc.html, "3.9. Keyboard mapping files")
for the authoritative format description.

This tool re-targets that mapping onto the physical scancodes produced by the
Tanmatsu badge keyboard (BSP_INPUT_SCANCODE_* in
managed_components/badgeteam__badge-bsp/bsp/input.h) instead of GDK keysyms,
because that's what main/src/KonsoolKB.cpp actually receives at runtime.

Two lookups are involved for every physical key:
  - which .vkm keysym is produced when the key is pressed *without* a host
    shift held (the "base" character), and
  - which .vkm keysym is produced when the key is pressed *with* a host shift
    held (the "shifted" character).
These are looked up independently in the parsed .vkm table and turned into
one row in kb_matrix[] (base) and one row in kb_matrix_shift[] (shifted).

The KEY_TRANSLATION table below is therefore not a generic GDK-keysym-name
dictionary: it also encodes which *physical* Tanmatsu scancode a given pair
of base/shifted keysyms belongs to. The Tanmatsu keyboard has only F1-F4
usable for the C64 matrix (F5/F6 are reserved by the app itself, see
KonsoolKB.cpp) and only a single Ctrl key (no left/right pair), so a few
entries deliberately don't do a literal 1:1 keysym translation:

  - Physical F1..F4 are paired with the *next* vkm F-key so that four keys
    can reach all eight C64 function keys via shift (F1 -> C64 F1/F2,
    F2 -> C64 F3/F4, F3 -> C64 F5/F6, F4 -> C64 F7/F8). This exactly
    reproduces the mapping historically hand-written into kbmatrix.hpp.
  - The physical Ctrl key is wired to the vkm "Control_R" entry (the real
    C64 CTRL key) rather than "Control_L" (which this particular .vkm file
    maps to the C64 Commodore/CBM key for PETSCIIBOARD-style hardware with
    two Ctrl keys). The spare physical Meta/Fn key instead is wired to
    "Control_L", giving access to the Commodore key.

Keys with no physical presence on the Tanmatsu keyboard (numeric keypad,
Caps Lock, Home/Insert/Delete/Page Up/Page Down, a second Ctrl/Alt/Shift)
are intentionally left out of KEY_TRANSLATION, and negative-row .vkm entries
(joystick and RESTORE key mappings) are skipped entirely -- they don't
describe row/column matrix positions and aren't handled by this table.
"""
import argparse
import os
import re
import sys

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DEFAULT_VKM = os.path.join(REPO_ROOT, "VICE-GTK3-Keymap v0.91 beta.vkm")
DEFAULT_INPUT_H = os.path.join(
    REPO_ROOT, "managed_components", "badgeteam__badge-bsp", "bsp", "input.h"
)
DEFAULT_OUT = os.path.join(REPO_ROOT, "main", "src", "kbmatrix.hpp")

# shiftflag bits, straight from VICE's own enum shift_type (vice-3.10/src/keymap.h):
# VIRTUAL_SHIFT=1, LEFT_SHIFT=2, RIGHT_SHIFT=4, ALLOW_SHIFT=8, DESHIFT_SHIFT=16,
# ALLOW_OTHER=32, SHIFT_LOCK=64, ALT_MAP=256. KonsoolKB.cpp applies these bits
# itself at runtime (see keyboard_key_pressed_matrix / keyboard_latch_modifier_states
# in vice-3.10/src/keyboard.c) -- this generator no longer interprets them, it just
# carries the raw flag value through per (row, col) entry.

# (BSP_INPUT_SCANCODE_ suffix, base keysym, shifted keysym)
# See the module docstring for why F1-F4 and Ctrl/Meta don't translate 1:1.
KEY_TRANSLATION = [
    ("ESC", "Escape", "Escape"),
    ("1", "1", "exclam"),
    ("2", "2", "at"),
    ("3", "3", "numbersign"),
    ("4", "4", "dollar"),
    ("5", "5", "percent"),
    ("6", "6", "asciicircum"),
    ("7", "7", "ampersand"),
    ("8", "8", "asterisk"),
    ("9", "9", "parenleft"),
    ("0", "0", "parenright"),
    ("MINUS", "minus", "underscore"),
    ("EQUAL", "equal", "plus"),
    ("BACKSPACE", "BackSpace", "BackSpace"),
    ("Q", "q", "Q"),
    ("W", "w", "W"),
    ("E", "e", "E"),
    ("R", "r", "R"),
    ("T", "t", "T"),
    ("Y", "y", "Y"),
    ("U", "u", "U"),
    ("I", "i", "I"),
    ("O", "o", "O"),
    ("P", "p", "P"),
    ("LEFTBRACE", "bracketleft", "bracketleft"),
    ("RIGHTBRACE", "bracketright", "bracketright"),
    ("ENTER", "Return", "Return"),
    ("A", "a", "A"),
    ("S", "s", "S"),
    ("D", "d", "D"),
    ("F", "f", "F"),
    ("G", "g", "G"),
    ("H", "h", "H"),
    ("J", "j", "J"),
    ("K", "k", "K"),
    ("L", "l", "L"),
    ("SEMICOLON", "semicolon", "colon"),
    ("APOSTROPHE", "apostrophe", "quotedbl"),
    ("GRAVE", "grave", "asciitilde"),
    ("LEFTSHIFT", "Shift_L", "Shift_L"),
    ("BACKSLASH", "backslash", "bar"),
    ("Z", "z", "Z"),
    ("X", "x", "X"),
    ("C", "c", "C"),
    ("V", "v", "V"),
    ("B", "b", "B"),
    ("N", "n", "N"),
    ("M", "m", "M"),
    ("COMMA", "comma", "less"),
    ("DOT", "period", "greater"),
    ("SLASH", "slash", "question"),
    ("RIGHTSHIFT", "Shift_R", "Shift_R"),
    ("SPACE", "space", "space"),
    # Tanmatsu only exposes F1-F4 to the C64 matrix (F5/F6 are reserved by
    # KonsoolKB.cpp for joystick-port-swap / menu toggle); pair each with the
    # next vkm F-key so all eight C64 function keys stay reachable.
    ("F1", "F1", "F2"),
    ("F2", "F3", "F4"),
    ("F3", "F5", "F6"),
    ("F4", "F7", "F8"),
    ("ESCAPED_GREY_UP", "Up", "Up"),
    ("ESCAPED_GREY_DOWN", "Down", "Down"),
    ("ESCAPED_GREY_LEFT", "Left", "Left"),
    ("ESCAPED_GREY_RIGHT", "Right", "Right"),
    # Tanmatsu has a single physical Ctrl key -> wire it to the real C64 CTRL
    # key (vkm "Control_R"), and put the C64 Commodore key (vkm "Control_L")
    # on the spare physical Meta key instead.
    ("LEFTCTRL", "Control_R", "Control_R"),
    ("ESCAPED_LEFTMETA", "Control_L", "Control_L"),
]

NUM_SCANCODES = 128


class VkmParseError(Exception):
    pass


def parse_input_h(path):
    """Return {NAME: value} for every `BSP_INPUT_SCANCODE_NAME = value` line."""
    pat = re.compile(r"BSP_INPUT_SCANCODE_(\w+)\s*=\s*(0[xX][0-9A-Fa-f]+|\d+)")
    scancodes = {}
    with open(path, encoding="utf-8") as f:
        for line in f:
            m = pat.search(line)
            if m:
                name, val = m.groups()
                scancodes[name] = int(val, 0)
    return scancodes


def parse_vkm(path, table=None, meta=None, _seen=None):
    """Parse a .vkm file (and any !INCLUDE'd files) into {keysym: (row, col, flag)},
    plus a `meta` dict capturing the !LSHIFT/!RSHIFT/!VSHIFT directives (the matrix
    position of each real shift key, and which one virtual-shift keysyms target --
    see KonsoolKB.cpp for how these are applied at runtime).

    Only lines with row >= 0 carry (row, column) matrix positions; negative
    rows are joystick / RESTORE-key / 40-80-column / caps mappings, which
    aren't representable in a KbMatrixEntry table and are skipped.
    """
    if table is None:
        table = {}
    if meta is None:
        meta = {"lshift": None, "rshift": None, "vshift": None}
    if _seen is None:
        _seen = set()
    path = os.path.abspath(path)
    if path in _seen:
        raise VkmParseError(f"circular !INCLUDE of {path}")
    _seen.add(path)

    with open(path, encoding="utf-8") as f:
        for lineno, raw in enumerate(f, 1):
            line = raw.strip()
            if not line or line.startswith("#"):
                continue

            if line.startswith("!"):
                parts = line[1:].split()
                if not parts:
                    continue
                directive = parts[0].upper()
                args = parts[1:]
                if directive == "CLEAR":
                    table.clear()
                elif directive == "UNDEF":
                    if args:
                        table.pop(args[0], None)
                elif directive == "INCLUDE":
                    if args:
                        inc_path = args[0]
                        if not os.path.isabs(inc_path):
                            inc_path = os.path.join(os.path.dirname(path), inc_path)
                        parse_vkm(inc_path, table, meta, _seen)
                elif directive == "LSHIFT" and len(args) >= 2:
                    meta["lshift"] = (int(args[0]), int(args[1]))
                elif directive == "RSHIFT" and len(args) >= 2:
                    meta["rshift"] = (int(args[0]), int(args[1]))
                elif directive == "VSHIFT" and args:
                    meta["vshift"] = args[0].upper()
                # !SHIFTL, !VCBM etc. describe metadata this tool doesn't need
                # (shift-lock and CBM-key handling aren't reachable on Tanmatsu
                # hardware -- see tools/gen_kbmatrix.py module docstring).
                continue

            tokens = line.split(None, 4)
            if len(tokens) < 3:
                print(f"{path}:{lineno}: warning: malformed line, skipping: {raw!r}",
                      file=sys.stderr)
                continue
            # Joystick/RESTORE lines ('keysym row n') omit the shiftflag field.
            keysym, row_s, col_s = tokens[0], tokens[1], tokens[2]
            flag_s = tokens[3] if len(tokens) > 3 else "0"
            try:
                row, col, flag = int(row_s), int(col_s), int(flag_s)
            except ValueError:
                print(f"{path}:{lineno}: warning: non-numeric row/col/flag, skipping: {raw!r}",
                      file=sys.stderr)
                continue
            if row < 0:
                continue  # joystick / RESTORE / 40-80col / caps - not a matrix entry
            table[keysym] = (row, col, flag)

    _seen.discard(path)
    return table, meta


def matrix_entry(vkm_table, keysym):
    """Return the raw (row, col, shiftflag) vkm entry for one keysym, or None."""
    return vkm_table.get(keysym)


def build_tables(vkm_table, scancodes):
    unshifted = [None] * NUM_SCANCODES
    shifted = [None] * NUM_SCANCODES
    labels = [None] * NUM_SCANCODES

    for scancode_name, base_keysym, shifted_keysym in KEY_TRANSLATION:
        if scancode_name not in scancodes:
            print(f"warning: BSP_INPUT_SCANCODE_{scancode_name} not found in input.h, skipping",
                  file=sys.stderr)
            continue
        code = scancodes[scancode_name] & 0x7F
        if not (0 <= code < NUM_SCANCODES):
            print(f"warning: BSP_INPUT_SCANCODE_{scancode_name} out of range, skipping",
                  file=sys.stderr)
            continue

        base = matrix_entry(vkm_table, base_keysym)
        shift = matrix_entry(vkm_table, shifted_keysym)

        if base is None:
            print(f"warning: keysym {base_keysym!r} (base of {scancode_name}) "
                  f"not found in keymap", file=sys.stderr)
        else:
            unshifted[code] = base
        if shift is None:
            print(f"warning: keysym {shifted_keysym!r} (shifted variant of {scancode_name}) "
                  f"not found in keymap", file=sys.stderr)
        else:
            shifted[code] = shift

        label = scancode_name
        if base_keysym != shifted_keysym:
            label = f"{scancode_name} ({base_keysym}/{shifted_keysym})"
        labels[code] = label

    return unshifted, shifted, labels


def format_array(name, entries, labels):
    lines = [f"static const KbMatrixEntry {name}[{NUM_SCANCODES}] = {{"]
    for i, entry in enumerate(entries):
        if entry is None:
            row, col, flag = -1, -1, 0
        else:
            row, col, flag = entry
        comment = f"{i:02x}"
        if labels[i]:
            comment += f" : {labels[i]}"
        lines.append(
            f"    {{ {row:2d}, {col:2d}, 0x{flag:02x} }},  // {comment}"
        )
    lines.append("};")
    return "\n".join(lines)


def generate_header(vkm_path, input_h_path):
    scancodes = parse_input_h(input_h_path)
    vkm_table, meta = parse_vkm(vkm_path)
    unshifted, shifted, labels = build_tables(vkm_table, scancodes)

    if meta["lshift"] is None or meta["rshift"] is None or meta["vshift"] is None:
        raise VkmParseError(
            "keymap is missing !LSHIFT/!RSHIFT/!VSHIFT -- KonsoolKB.cpp's virtual-shift "
            "handling needs all three to know where the real shift keys live and which "
            "one absorbs a forced ('virtual') shift"
        )
    lshift_row, lshift_col = meta["lshift"]
    rshift_row, rshift_col = meta["rshift"]
    if meta["vshift"] not in ("LSHIFT", "RSHIFT"):
        raise VkmParseError(f"!VSHIFT {meta['vshift']} is not LSHIFT or RSHIFT")
    vshift_is_rshift = meta["vshift"] == "RSHIFT"

    vkm_name = os.path.basename(vkm_path)
    header = f"""// Generated by tools/gen_kbmatrix.py from "{vkm_name}".
// Do not edit by hand -- regenerate with:
//   tools/gen_kbmatrix.py "{vkm_name}"
#include <cstdint>

// row < 0 means "this scancode has no C64 matrix mapping". `shift` carries the
// raw vkm shiftflag bits (see VICE's enum shift_type in vice-3.10/src/keymap.h):
// bit 0 VIRTUAL_SHIFT, bit 1 LEFT_SHIFT, bit 2 RIGHT_SHIFT, bit 3 ALLOW_SHIFT,
// bit 4 DESHIFT_SHIFT, bit 6 SHIFT_LOCK. KonsoolKB.cpp applies these the same
// way VICE's keyboard.c does (see keyboard_key_pressed_matrix /
// keyboard_latch_modifier_states).
struct KbMatrixEntry {{
    int8_t  row;
    int8_t  col;
    uint8_t shift;
}};

// Matrix position of the two real shift keys, and which one absorbs a forced
// ("virtual") shift from another key's VIRTUAL_SHIFT bit -- from this keymap's
// "!LSHIFT {lshift_row} {lshift_col}", "!RSHIFT {rshift_row} {rshift_col}" and
// "!VSHIFT {meta['vshift']}" directives.
constexpr int8_t LSHIFT_ROW        = {lshift_row};
constexpr int8_t LSHIFT_COL        = {lshift_col};
constexpr int8_t RSHIFT_ROW        = {rshift_row};
constexpr int8_t RSHIFT_COL        = {rshift_col};
constexpr bool    VSHIFT_IS_RSHIFT = {"true" if vshift_is_rshift else "false"};

"""
    body = (
        format_array("kb_matrix", unshifted, labels)
        + "\n\n"
        + format_array("kb_matrix_shift", shifted, labels)
        + "\n"
    )
    return header + body


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                      formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("vkm", nargs="?", default=DEFAULT_VKM,
                         help="path to the .vkm keymap file")
    parser.add_argument("-i", "--input-h", default=DEFAULT_INPUT_H,
                         help="path to bsp/input.h")
    parser.add_argument("-o", "--output", default=DEFAULT_OUT,
                         help="path to write kbmatrix.hpp to")
    args = parser.parse_args()

    text = generate_header(args.vkm, args.input_h)
    with open(args.output, "w", encoding="utf-8") as f:
        f.write(text)
    print(f"wrote {args.output}", file=sys.stderr)


if __name__ == "__main__":
    main()
