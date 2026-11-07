# Konsool 64

## Introduction

This application is a functional C64 emulator that run on the [Konsool/Tanmatsu](https://badge.team/docs/badges/konsool/) device.

On the Konsool device, the emulator both outputs audio and video and accepts keyboard input.

## Usage

### Accessing the menu 

Press the 'purple diamond' button on the device at any time to open or close the menu.

### Navigation

The following keys are used:

| Key            | Description                 |
| -------------- | --------------------------- |
| Purple diamond | Show or hide menu           |
| Up/Down        | Move up and down the menus. |
| Enter          | Activate a menu item        |
| ESC            | Go back one menu up         |


Since the menu structure is still being developed, I'm going to not document more details at this time.

## Games / Sofware

### Supported file formats

| Format   | Support                                                          |
| -------- | ---------------------------------------------------------------- |
| **.PRG** | Full. Loaded straight into memory.                               |
| **.T64** | Full. Pick a program from the tape container.                    |
| **.D64** | Mountable as drive 8, with loading and saving, or a real 1541.   |
| .TAP     | Not supported, needs datasette emulation.                        |
| .CRT     | Not supported, needs cartridge ROM banking.                      |

### Loading files

- Put your files in a directory named `c64prg` on the SD card. The directory is
  created for you the first time the emulator starts with a card inserted. The
  card mounts at `/sd`, so that path reads as `/sd/c64prg` in the log.

- Open the menu, select 'Load file', and pick a file. Choosing a .t64 or .d64
  opens a second menu listing the programs inside it; press ESC to go back. The
  listing is sorted and paged; the page markers say which page you are on and
  wrap around, so the end of a long listing is one step back from the first page.

- A USB disk works the same way, through 'Load file from USB disk', which reads
  the root of the disk rather than a `c64prg` directory.

- When the C64 screen shows again, type the command 'run' and press enter.

### Two ways to use a .d64

Selecting a disk image gives you a choice.

**Mount as drive 8** hands the whole disk to the C64, which then loads from it
itself:

```
LOAD"$",8        list the directory
LOAD"NAME",8     load a program
LOAD"*",8,1      load the first program
```

This is the one to use for anything that loads more than one part, because the
C64 stays in charge of the loading. The disk stays mounted until you mount a
different one, the same way a real drive stays plugged in across a reset.

**Picking a program from the list** copies that one program straight into
memory, the same way a .prg is loaded. It is quicker for something that loads
in one go, and it is the only option for a .t64. Nothing can be saved back
this way; mount the disk if you want to save.

Files that were never closed properly on the original disk are listed with a
trailing `*`, exactly as a real directory listing marks them, and will usually
fail to load.

### How the drive emulation works, and what it will not do

There is no serial bus hardware in this emulator. Instead the Kernal's own
serial routines are replaced: `LISTEN`, `TALK`, `SECOND`, `TKSA`, `CIOUT`,
`ACPTR`, `UNTLK` and `UNLSN` each get a JAM opcode patched over their first
byte, and the emulator services the call from the mounted image. Their
addresses are read out of the Kernal jump table rather than hardcoded. The
traps are only in place while a disk is mounted; unmount and the ROM is exactly
as it was.

Because every one of the Kernal's higher level routines is built on those eight
primitives, implementing them once covers `LOAD`, `SAVE`, `OPEN`, `CHRIN`,
directory listings and sequential files alike.

Saving works too, so `SAVE"NAME",8` writes a real file to the image, blocks
are taken from and given back to the BAM, and the directory entry is only
marked closed once the last sector is written. An interrupted save therefore
shows up as a splat file, exactly as it would on real hardware. The command
channel handles scratch and rename:

```
OPEN 15,8,15,"S0:NAME":CLOSE 15       delete a file
OPEN 15,8,15,"R0:NEW=OLD":CLOSE 15    rename a file
SAVE"@0:NAME",8                       save over an existing file
```

What this does **not** cover:

- **Fast loaders.** A fast loader bit-bangs the serial lines directly and
  uploads its own code into the drive, so it never calls the Kernal routines
  that are trapped. For those, turn on 1541 emulation below.
- **Formatting and copying** (`N` and `C` on the command channel), which
  answer `31,SYNTAX ERROR`.
- **Appending** to an existing file (`,A`), which answers `30,SYNTAX ERROR`.
- **REL files**, which need side sectors.
- **Growing the directory** past its 144 entries; a full directory answers
  `72,DISK FULL`.

If the card or the file will not open for writing, the image is mounted read
only and everything above answers `26,WRITE PROTECT ON` instead.

## 1541 emulation

Turning on '1541 emulation' in the main menu replaces all of the above with an
actual drive: a second 6502, its 2K of RAM, the DOS ROM, both 6522s and the
disk surface as GCR. Fast loaders work because there is now a drive for them
to upload their code into.

### You need to supply the ROM

The 1541 DOS ROM is a 16K copyrighted binary and is not shipped with this
firmware. Put your own copy on the card next to the disk images:

```
/c64prg/1541.rom      exactly 16384 bytes
```

A VICE install already has one, in `/usr/share/vice/DRIVES/`, and there is a
target that finds it, checks the size and puts it where it belongs:

```bash
make drive-rom                                   # drops ./1541.rom to copy by hand
make drive-rom DEST=/run/media/you/SD/c64prg     # straight onto a mounted card
```

Set `VICE_DRIVES` if VICE is installed somewhere unusual. To do it by hand,
either of these is the flat 16K the drive maps at `$C000`; copy one across and
rename it:

| File | What it is |
| --- | --- |
| `dos1541-325302-01+901229-05.bin` | The original 1541, its two 8K parts joined. This is the one to start with. |
| `dos1541ii-251968-03.bin` | The 1541-II, one 16K part, DOS 2.6 with the later fixes. Worth trying if a fast loader misbehaves. |

Anything that is not exactly 16384 bytes is rejected, with the size it found in
the log.

Without the ROM the menu option turns itself back off and the Kernal traps stay
in charge, which is a perfectly good way to run most software.

The rest of the emulator needs nothing on the card: the C64's own KERNAL, BASIC
and character ROMs are compiled into the firmware, in `main/src/roms/`. The
1541 DOS ROM is the only one read at runtime.

### What it does

The C64's `$DD00` now drives the serial bus for real, rather than the lines
being stubbed out. Both machines pull ATN, CLK and DATA the way open collector
hardware does, so a line is low when either end pulls it and only floats high
when both let go. The drive answers on the bus through VIA 1, including the
gate that holds DATA low until it has acknowledged an ATN.

The two CPUs are interleaved an instruction at a time, following Frodo:
whichever has fallen behind on cycles runs next. That is not cycle exact, but
it keeps them close enough for loaders that hand bytes back and forth.

Sectors are GCR encoded on the fly into a track image, and the head advances
one byte each time the drive reads port A of VIA 2, which is Frodo's model
too.

Writing goes the same way round. With port A driving, a byte written to it
lands on the track under the head. When the head steps away, the motor stops
or the disk is taken out, the track is decoded back into sectors and written
to the image. Only sectors whose header still reads correctly are written
back, so a garbled track cannot scribble over a real one. Frodo does not do
this at all: it patches the drive ROM so writes bypass GCR entirely. Whether
the real DOS ROM's write routine drives this the way it expects has not been
confirmed on hardware yet, so treat saving through the 1541 as the least
proven part of the emulator and keep a spare copy of any image you care
about.

### What it still will not do

- **Copy protection.** Schemes that measure sync lengths, look for weak bits
  or depend on exact rotation need bit level emulation with real timing, which
  VICE does and this does not. It would also need G64 images: a .d64 has
  already thrown away the information that protection relies on, so no
  emulator can recover it from one.
- **Anything but a 1541.** No 1571, no 1581, one drive only, always device 8.

### Loading a .prg from BASIC

With no disk mounted, `LOAD"NAME",8` at the C64 prompt still loads `NAME.PRG`
straight from the `c64prg` directory on the card. Mounting a disk takes that
over: while one is mounted, drive 8 is the disk.

### Joystick emulation

The original commodore 64 had two joystick ports namely '1' and '2'.
Because some games use port one and others two, switching the joystick between ports is needed.

In order to enable the Joystick, the 'keyboard joystick' option in the main menu needs to be set to 'yes'.

Selecting the joystick port is done using the 'F5' or 'blue tri-lobe' key.

Indicators of joystick status and port will be added to the software in the future.

#### Joystick key bindings

| key               | Joystick function                |
| ----------------- | -------------------------------- |
| Arrow up          | UP                               |
| Arrow down        | DOWN                             |
| Arrow left        | LEFT                             |
| Arrow right       | RIGHT                            |
| Left SHIFT        | FIRE button                      |
| F5 / Blue diamond | Switch joystick between port 1/2 |

## Perquisites

### Install the build dependencies

#### Debian / Ubuntu: 

```bash
sudo apt-get install git wget flex bison gperf python3 python3-pip python3-venv cmake ninja-build ccache libffi-dev libssl-dev dfu-util libusb-1.0-0
```

#### Arch

```bash
sudo pacman -S --needed gcc git make flex bison gperf python cmake ninja ccache dfu-util libusb
``` 

### Setup the the build environment

```bash
make prepare
```

## Build the project

```bash
make build
```

## Running the tests

The image readers and the drive emulation depend on nothing but POSIX file
calls, so they can be built and run on a normal machine without ESP-IDF:

```bash
make -C main/src/images/test run    # .t64 and .d64 parsing
make -C main/src/drive/test run     # CBM DOS, GCR and the 1541 hardware
make -C main/src/test run           # the 6502 itself
```

The first builds synthetic images, including ones carrying the malformed
headers that turn up in the wild, and checks the right bytes end up at the
right addresses. The second drives the emulated drive through the same bus
commands the Kernal issues, and checks that what comes back is what a real 1541
would return, down to the directory column layout and the status message
format. It also boots the 1541's own CPU against a small hand assembled test
ROM, so the reset vector, memory map, VIAs, bus lines and head are all
exercised without needing the DOS ROM.

The third runs the CPU against [Klaus Dormann's functional test
suite](https://github.com/Klaus2m5/6502_65C02_functional_tests), which walks
every documented opcode, addressing mode and flag rule including decimal
arithmetic, and traps the moment one is wrong. Both the C64 and the emulated
1541 run on that same core, so a fault there would surface as software
misbehaving in ways that are painful to track down on hardware. The suite
image is vendored in `main/src/test`; it is Copyright (C) 2012-2015 Klaus
Dormann under the GPL version 3, the same licence as this project.

## Upload to the Tanmatsu

Fast and easiest way to upload the build

```bash
tools/badgelink.py appfs upload "c64-emu" "C64 Emulator" 0 <project_root>/build/application.bin
```

## Configure clangd

The esp-idf cross compiler has built in include paths, not using this cross compiler will result in clangd complaining about missing include files.
The CMake project project by default already makes a compile_commands.json file, but clangd will not accept any cross compiler without it being white listed.

In order to white list a compiler for clangd to extract the include paths:

```
--query-driver=/**/riscv32-esp-elf/bin/riscv32-esp-elf-gcc
```

In VsCodium this can be done using the following statement in the settings.json

```json
"clangd.arguments": [
  "--query-driver=/**/riscv32-esp-elf/bin/riscv32-esp-elf-gcc"
]
```

## Credits

The drive emulation follows [Frodo](https://github.com/cebix/frodo4) by
Christian Bauer, both halves of it: the Kernal trap approach and the real
1541. The Kernal status byte values come from its `IEC.h`, the GCR tables and
sector layout from `1541job.cpp`, and the serial port behaviour and CPU
interleaving from `CPU1541.cpp` and `C64.cpp`.

The directory listing layout, the status message format and the CBM DOS error
texts are ported from [VICE](https://vice-emu.sourceforge.io/), and the
handling of malformed .t64 containers follows
[t64fix](https://github.com/Compyx/t64fix).

Frodo and VICE are both GPL version 2 or later, which is compatible with this
project's GPL version 3.

## Many many credits for the person who wrote the emulator this is based on

[retrolec](https://github.com/retroelec/T-HMI-C64/commits?author=retroelec)

[T-HMI-C64](https://github.com/retroelec/T-HMI-C64)

## License & Copyright


Modified 2025 Ranzbak Badge.Team

Copyright (C) 2024 retroelec <retroelec42@gmail.com>

This program is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation; either version 3 of the License, or (at your
option) any later version.
