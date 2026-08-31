#!/usr/bin/env bash
#
# Finds a 1541 DOS ROM in a local VICE install and puts a copy where the
# emulator wants it. The ROM is copyrighted and deliberately not in this repo,
# but anyone running the drive emulation already has VICE.
#
#   tools/get-drive-rom.sh [destination]
#
# The destination is either the c64prg directory on a mounted SD card, or a
# plain file. With no argument the ROM lands in ./1541.rom for copying by hand.

set -euo pipefail

ROM_SIZE=16384

# The original 1541 first: its two 8K parts joined is what the drive maps flat
# at $C000. The 1541-II is the same size and works too.
CANDIDATES=(
    "dos1541-325302-01+901229-05.bin"
    "dos1541ii-251968-03.bin"
    "dos1541"
)

# VICE_DRIVES overrides the search for an install in an unusual place.
SEARCH_DIRS=(
    "${VICE_DRIVES:-}"
    "/usr/share/vice/DRIVES"
    "/usr/local/share/vice/DRIVES"
    "/opt/vice/share/vice/DRIVES"
    "$HOME/.local/share/vice/DRIVES"
    "/Applications/vice-arm64-sdl2/share/vice/DRIVES"
)

found=""
for dir in "${SEARCH_DIRS[@]}"; do
    [ -n "$dir" ] && [ -d "$dir" ] || continue
    for name in "${CANDIDATES[@]}"; do
        if [ -f "$dir/$name" ]; then
            found="$dir/$name"
            break 2
        fi
    done
done

if [ -z "$found" ]; then
    echo "No 1541 ROM found. Looked for:" >&2
    printf '  %s\n' "${CANDIDATES[@]}" >&2
    echo "in:" >&2
    for dir in "${SEARCH_DIRS[@]}"; do [ -n "$dir" ] && printf '  %s\n' "$dir" >&2; done
    echo >&2
    echo "Install VICE, or point VICE_DRIVES at the directory holding the ROMs." >&2
    exit 1
fi

# Anything that is not exactly 16K is not the ROM the drive maps at \$C000.
actual=$(wc -c < "$found")
if [ "$actual" -ne "$ROM_SIZE" ]; then
    echo "$found is $actual bytes, expected $ROM_SIZE" >&2
    exit 1
fi

# Anything not named *.rom is taken as a directory to drop the ROM into, so a
# card path that does not exist yet does not quietly become a file called
# "c64prg".
dest="${1:-./1541.rom}"
case "$dest" in
    *.rom) ;;
    *)     dest="${dest%/}/1541.rom" ;;
esac

mkdir -p "$(dirname "$dest")"
cp "$found" "$dest"

echo "Copied $(basename "$found") -> $dest ($ROM_SIZE bytes)"
case "$dest" in
    ./1541.rom)
        echo
        echo "Put it in the c64prg directory on the SD card, then turn on"
        echo "'1541 emulation' in the main menu."
        ;;
esac
