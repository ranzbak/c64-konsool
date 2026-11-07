# Notes for Claude working in this repo

## Check whether the branch's pull request has already merged before pushing

Work here happens on one long lived branch. Twice in one session commits were
pushed onto that branch *after* its pull request had already merged, so the
work sat unmerged and invisible: not in `main`, not in any open pull request.
Once the merged pull request's description was even edited afterwards, so it
described code the pull request did not contain.

Before pushing, check:

```bash
git fetch origin main
git log --oneline origin/main..HEAD     # what is actually unmerged
```

If the previous pull request has merged, rebase the remaining commits onto the
new `main` and open a *new* pull request. A merged pull request cannot pick up
new commits, and editing its description does not change that.

## Building

The firmware needs ESP-IDF and components from the Espressif component
registry. Where that registry is unreachable, the components can be cloned
from GitHub into `components/` with `IDF_COMPONENT_MANAGER=0`, which is enough
to verify everything compiles. That setup needs local-only edits (pinning
`badge-bsp`, adding `pax-gfx` to `main`'s requires, stubbing a panel driver);
none of them belong in a commit, so check `git status` before staging.

`make build` on a normal machine is still the real check.

## Tests

Three host suites, no ESP-IDF and no hardware needed. Run them before pushing:

```bash
make -C main/src/images/test run          # .t64 and .d64 parsing
make -C main/src/drive/test run           # CBM DOS, GCR, the 1541 hardware
make -C main/src/test run                 # the 6502 itself
make -C components/hidhost/test_native test   # what a gamepad becomes as a joystick
```

They also run in CI on every pull request.

## Driving the emulator from the host

The harnesses built for this live in `~/Projects/c64-konsool-harness`, with a
README naming each one. They are kept out of `/tmp`, which a reboot wipes, and
out of the repo, since they are scratch. `kernal_probe.cpp` is the one to
reach for first.

**Test against real disks, not generated ones.** A sync bug hid for hours
because the .d64 generated here contains no byte aligned `$ff` at all, so every
host test passed while every real disk failed. `harness/disks/` holds real
ones.


Anything that goes wrong between the C64 and the drive can be reproduced on a
normal machine, which is far faster than reflashing to try an idea. Both sides
are plain C++ with no ESP-IDF in them, so a small harness can compile
`CPU6502`, `Drive1541`, `DiskController`, `Gcr`, `Via6522` and `D64Disk`
natively, put the kernal from `main/src/roms/kernal.h` on one side and a real
1541 DOS ROM on the other, and interleave the two the way `CPUC64` does:

```cpp
uint8_t before = c64.numofcycles;
c64.step();
unsigned spent = c64.numofcycles - before;
driveDebt += spent;      // the drive earns a cycle for every cycle the C64 spends
drive.countTimers(spent) // stepInstruction() does NOT advance the VIA timers
```

Two things that are easy to get wrong in such a harness and cost an evening
each: `stepInstruction()` runs the drive CPU but leaves its VIA timers alone,
so `countTimers()` has to be called separately or every timed handshake fails;
and the kernal times the bus with CIA 1 timer B (`$DC06`/`$DC07`/`$DC0F`,
underflow read back from `$DC0D`), so a stub C64 without it hangs in the EOI
turnaround rather than in anything real.

Ask the drive what it thinks rather than inferring from symptoms. Channel 15
is always open and reports the DOS's own view (`73,CBM DOS V2.6 1541,00,00`),
and the job queue in drive RAM is even more direct: `$00`-`$05` are the job
codes per buffer, `$06`-`$11` the track and sector each wants, and the code is
replaced by the result -- `$01` ok, `$02` header not found, `$03` no sync,
`$04` data block not found, `$05` data checksum, `$0b` id mismatch.

## Config that looks set but is not

Two ways a value in `sdkconfigs/` silently fails to reach the build, both of
which have cost real debugging time:

**A stale `sdkconfig` in the working directory wins.** ESP-IDF only applies
`SDKCONFIG_DEFAULTS` for symbols the existing `sdkconfig` does not already
mention, so a leftover from another branch pins whatever it happens to hold.
It is untracked here and survives a branch switch. Delete it and rebuild
before trusting any config value, and check `build/config/sdkconfig.h` for
what was actually compiled rather than the template.

**An invisible Kconfig symbol cannot be set at all.** A `config` with no
prompt always takes its `default`, whatever the defaults file says, and the
build does not warn. `CONFIG_USB_HOST_EXT_PORT_RESET_ATTEMPTS` is one of
these in IDF v5.5.1: it is `depends on IDF_EXPERIMENTAL_FEATURES`, marked
"Invisible config option", `default 1`. Setting it to 5 does nothing.
Check the symbol in `$IDF_PATH/components/*/Kconfig*` before believing a
config change had any effect.

## A USB drive needs a FATFS volume slot of its own

`CONFIG_FATFS_VOLUME_COUNT` must leave a slot free or a USB drive is turned
away before its filesystem is examined at all. The internal flash (`/int`) and
the SD card take one each, so the count has to be at least 3. The symptom is
that every disk fails identically, whatever it is or how it is formatted:

```
usb_msc: MSC device connected (addr=1)
usb_msc: msc_host_vfs_register failed: ESP_ERR_NOT_FOUND
```

`ESP_ERR_NOT_FOUND` there has one source: `msc_host_vfs_register()` begins
with `ff_diskio_get_drive()`, which reports it when every drive slot is taken.
Reading that error is what identifies the fault; guessing at USB host settings
does not, and there are several plausible-looking ones that are irrelevant.

## Flashing

Over badgelink (USB `16d0:0f9a`), not serial. Two identical
`303a:1001` JTAG devices enumerate and **which one is `ttyACM0` is not
stable**, so tell them apart by what they print: the radio coprocessor says
`Project name: tanmatsu-radio`, the P4 says `transport: Slave chip Id[12]`.
Both consoles are readable while the app runs, so reading a log needs no mode
switch -- but only until the app takes the USB port into host mode for a
gamepad or a drive, at which point the P4 console goes with it and its
`ttyACM` device disappears. Anything that has to be observed while a USB
device is attached needs logging built into the firmware, or has to be shown
on screen; the serial console will not be there. Decode a crash against the matching ELF; running one chip's addresses
through the other's `application.elf` resolves to plausible, entirely wrong
names.

## Formatting

`.clang-format` does not match how much of the existing code is written, so
running it over a whole pre-existing file produces a large diff of unrelated
changes. Format new files freely; for edits to existing ones, keep to the
local style of the surrounding code instead.

## 6502 timing

Codebase64 is the reference for this, at **https://codebase.c64.org/** -- see
`base:6510_instruction_timing`. Note that `codebase64.org` is a DIFFERENT
domain and now redirects to an unrelated site; it is not the wiki any more.

The rules that matter, because ordinary code never notices them and a fast
loader is made of nothing else:

- Absolute indexed and indirect indexed **reads** take one extra cycle when the
  index carries into the high byte. `LDA $nnnn,X` is 4 or 5; `LDA ($nn),Y` is 5
  or 6.
- **Writes** always take the higher count, fixed, because the chip always reads
  the address first: `STA $nnnn,X` is always 5, `STA ($nn),Y` always 6.
- **Read-modify-write** is likewise fixed: `INC $nnnn,X` and `ASL $nnnn,X` are
  always 7. Giving either of these a conditional cycle is a bug.
- Branches are 2 not taken, 3 taken, 4 taken across a page boundary.

Klaus Dormann's suite checks results, not timing, so it passes with every one
of these wrong. Only a fast loader notices, and it shows up as corrupt data
rather than as a crash.

## Emulator references

The drive emulation follows [Frodo](https://github.com/cebix/frodo4), and the
disk format details come from [VICE](https://vice-emu.sourceforge.io/). Both
are GPL v2 or later, compatible with this project's GPL v3. Where behaviour is
taken from either, the copyright notice travels with it. Prefer checking one of
them over reasoning from first principles: the polarity of the serial lines
differs between the two ends of the bus, and the status byte values are not
guessable.

The 1541 DOS ROM is copyrighted and deliberately not in this repo. It is read
from `/c64prg/1541.rom` on the SD card at runtime. A VICE install has a usable
copy in `/usr/share/vice/DRIVES/`: `dos1541-325302-01+901229-05.bin` is the
original two part ROM concatenated, which is the flat 16K the drive maps at
`$C000`.

Where a VICE checkout is to hand, `src/drive/rotation.c` is the reference for
how the head is meant to behave. It models rotation at the bit level with an
accumulator and treats sync as ten consecutive one bits, where this project is
byte granular; and it keeps `BRA_MOTOR_ON` and `BRA_BYTE_READY` apart, the
first turning the disk and the second only gating the signal to the CPU.

## The host build, and why it has to be lock stepped

`host/` builds the whole emulator for a normal machine. It is the only way to
check a change without a badge, and the gate it provides is "these demos render
byte identically before and after":

```bash
make -C host -j8
./host/c64host --frames 400 --prg demo.prg --autostart --screenshot out.ppm
./host/c64host --frames 900 --truedrive --drive-rom 1541.rom \
    --disk side1.d64 --type $'LOAD"*",8,1\rRUN\r' --screenshot out.ppm
```

That gate is worth nothing unless the runner is deterministic, and it very
nearly was not. The badge runs the emulation on one core and the display on the
other; the host keeps that shape, and the first version paced the two with a
sleep. The display thread then read the VIC's bitmap while the emulation was
still writing it, so **two runs of the same binary produced different
screenshots and different RAM**. That is not a flaky test, it is a broken
instrument: it reported a regression in kloten that did not exist, and a
correct bus fix was reverted because of a difference that turned out to be
noise.

It is fixed by waiting on the other thread's state rather than on a clock:
`hostWaitUntilParked()` blocks until the emulation is parked in
`xSemaphoreTake` at the end of its frame, and only then is the framebuffer
read. Exactly one emulated frame per drawn frame, no sleeps anywhere. **Do not
reintroduce a sleep here.** Before trusting any before/after comparison, run
the same binary twice and check it agrees with itself.

Disk runs are covered too: kloten loaded over the emulated 1541 comes back
identical in both the screenshot and all 64K.

## What the host build cannot check

It links no pax, so nothing in `main/src/menuoverlay/` or the boot screen is
verifiable here, and there are no tests for that layer either. Every menu
change is unverified until someone looks at the badge. Two PETSCII sizing
mistakes in one evening were caught that way and no other.

## The serial bus, and the polarities already checked

Frodo is the reference (`CLAUDE.md` says so above, and it holds). These four
were each checked against it and are right, so there is no need to go round
them again:

- **C64 output**: a line is released only by driving its bit with a zero. A one
  pulls it low, and so does leaving the bit an input, because the pin floats
  high and the inverting driver turns that into a pulled down line. Frodo:
  `inv_out = ~pra & ddra` (`src/CIA.cpp`, `write_pa`). A loader can therefore
  bit bang the bus from `$dd02` alone, and The Lab does.
- **Drive output**: DATA from VIA1 PB1, CLK from PB3, and the drive never
  drives ATN (`src/CPU1541.cpp`, `set_iec_lines`).
- **ATN acknowledge**: DATA is pulled low when the ATN line state equals the
  ATNA bit (`CalcIECLines`). Ours reads `dataOut || (atna != atnLow())`, which
  is the same thing.
- **Drive input**: ATN arrives on VIA1 PB7, set when ATN is asserted.

## Undocumented opcodes are results, not just cycles

`cycle_test.cpp` times LAX, SLO, SRE, RRA, RLA and DCP without checking what
they compute, and Klaus Dormann's suite does not walk them at all. A fast
loader is made of them: Sparkle's GCR decoder, in the drive's zero page, is
almost nothing but LAX, SAX, ALR and SBX. A wrong result there does not crash,
it decodes to the wrong byte and the loader spins for ever.

`cpu_test.cpp` now checks results and flags for eleven of them. They pass,
which rules the opcodes out as the cause of the Sparkle demos hanging and
leaves the disk side, where this emulation is byte granular and VICE is bit
granular.

## The Lab: what is actually known, and what was wrongly claimed

Reproduces on the host build. This section has been wrong twice, so the
corrections are kept: they are the useful part.

**Wrong claim 1: a bus deadlock at `$064c`.** It is not. The handshake works.
`$3848` writes `#$37` to `$dd02`, making ATN an input, and an input pulls its
line low, so ATN asserts and the drive moves on three instructions later:

    t=3277941  ATN -> LOW  (pra=c3 ddra=37)
    t=3277944  drive leaves the ATN wait, now at $0653

That came from a hot-PC histogram sampled at one moment, and a probe that
logged the first thirty ATN transitions and then every two hundredth, hiding
the working handshake in between.

**Wrong claim 2: the drive never sees the `$52` it hunts for.** It does. The
drivecode at `$05d1` waits for sync, takes the byte that was ready, waits for
the next, and requires `$52`:

```
05CF  A0 52     LDY #$52
05D1  2C 00 1C  BIT $1C00
05D4  30 FB     BMI $05D1      ; wait for SYNC
05D6  AD 01 1C  LDA $1C01
05D9  B8        CLV
05DA  50 FE     BVC $05DA      ; wait for the next byte
05DC  CC 01 1C  CPY $1C01
05DF  D0 EB     BNE $05CC
```

Logging the bytes it is handed:

    pc=$05d9 sync=1 headPos=6155 byte=$ff
    pc=$05df sync=0 headPos=6160 byte=$52     <- matches
    pc=$05df sync=0 headPos=6184 byte=$55
    pc=$05df sync=0 headPos=6522 byte=$52     <- matches

So it matches about half the time and falls through. The five byte gap between
the two reads is also correct, not a fault: BYTE READY is suppressed while the
head is on sync, so V fires on the first byte past a five byte sync run.

**What is actually true.** The drive spends its time at `$05d1` -- 720,917
samples against 42,822 for the byte-ready wait under it -- but as a *retry
loop*, not a stuck compare. It finds a header, goes on, something after
`$05e1` fails, and it comes back to hunt again for ever.

### Traced past `$05e1`: it is the header check that fails

The path after the header byte matches is short and always the same:

```
05E1  B7 B1     LAX $B1,Y
05E3  8E B0 05  STX $05B0
05E6  69 0F     ADC #$0F
05E8  50 FE     BVC $05E8      ; wait for one more byte
05EA  A6 93     LDX $93
05EC  41 57     EOR ($57,X)
05EE  48 68     PHA / PLA
05F0  87 B9     SAX $B9
05F2  4B C1     ALR #$C1
05F4  D0 D6     BNE $05CC      ; gives up, hunts again
```

Traced three times, identical each time: `$05e1 $05e3 $05e6 $05e8 x4 $05ea
$05ec $05ee $05ef $05f0 $05f2 $05f4 $05cc`. So it takes the header mark, pulls
one more GCR byte, folds the two together and requires the result to be zero.
It never is.

The bytes it is given:

    pos=6160 byte=$52   header mark, matches
    pos=6161 byte=$6e
    pos=6522 byte=$52
    pos=6523 byte=$6f

`$52` is right: the header mark `$08` encodes to `01010 01001`, whose first
byte is `$52`. The byte after it carries the checksum's high nibble, and
decoding `$6e` gives `10111`, which is `7`, for both headers sampled.

So the question is now narrow and entirely about what this encoder writes into
a header block: whether the byte following the mark is what a real disk carries,
and whether the checksum (`sector ^ track ^ id2 ^ id1`) and the ID bytes are
placed and encoded the way the surface really holds them. The DOS accepts these
headers, but the DOS decodes them through its own table and only checks the
result; this loader folds the raw GCR and demands an exact answer, so it is a
far stricter reader of the same bytes.

### The surface is not the difference

That comparison has now been made, and the encoder and the track layout are
identical to VICE's:

- `gcrConv4` against VICE's `gcr_convert_4bytes_to_GCR`, compiled side by side
  and fed the same input: **5145 headers and 65536 arbitrary quads, zero
  differing bytes**.
- Header block: `[$08, sector ^ track ^ id2 ^ id1, sector, track]` then
  `[id2, id1, $0f, $0f]`, the same order and the same checksum as
  `gcr_convert_sector_to_GCR`.
- Sync 5 bytes and header gap 9, matching `disk_image_sync_size` and
  `disk_image_header_gap_size` for a D64.
- Tail gaps `{9, 12, 17, 8}`, matching `gap_size_d64`.
- The whole track filled with `$55` before the sectors are written, as
  `fsimage-dxx.c` does, and the same stride: 354 plus the tail gap.

So the bytes on the emulated surface are right, and the header the loader reads
is the header VICE would give it.

The upload is not the problem either. Logging the byte the C64 loads at `$3851`
against the byte the drive stores at `$066a` shows the two sequences agreeing
exactly -- `ff ff ff 30 f0 60 b0 20 ff 40 80 00 ...` on both sides -- so the
`$dd02` transfer delivers the drivecode and its tables intact.

### The head is on the wrong track

What is actually wrong is where the head is. During the hunt:

    [track] head on track 1 (halfTrack 2) pos=5971

Track 1. The drivecode steps the head out from track 18 all the way to the stop
-- a bump, which is normal -- and then **never steps back in**. Counting the
steps after the upload: `in=0`, every one of them outward. So every header it
reads is a track 1 header, the check at `$05f2` compares it against what it
expects for the track it means to be on, and fails for ever. That is why the
`$52` is right and the fold still fails.

### The stepper model differs from VICE, but porting it breaks kloten

VICE (`src/drive/iecieee/via2d.c`, `store_prb`) works out the step from the
head's own position rather than a remembered phase, and ignores an ambiguous
two coil jump:

```c
new_stepper_position = byte & 3;
old_stepper_position = (current_half_track - 2) & 3;
step_count = (new - old) & 3;          /* 3 means -1 */
if (byte & 0x4) {                      /* only while the motor runs */
    if (step_count == 1 || step_count == -1) drive_move_head(step_count, drv);
}
```

`updateStepper` here keeps its own `lastStepperPhase`, has no motor check, and
treats anything that is not a single step forward as a step outward, so a two
coil jump moves the head where the hardware would leave it still.

Porting VICE's version verbatim was tried and **regressed kloten**: the five
.prg demos stayed byte identical but kloten stopped running, sitting at READY
instead of taking over the screen. So something else in this drive model is
built around the current behaviour -- the motor gate and the half track base
are the two candidates -- and the stepper cannot simply be replaced without
finding it. The change was reverted.

## VICE as an oracle, through the MCP build

The plain `x64sc` on this machine renders nothing headless: `-exitscreenshot`
comes back as an all but black frame with `-console`, with `SDL_VIDEODRIVER=dummy`,
under Xvfb, with `-default`, and with a monitor breakpoint driving `screenshot`.
Do not spend time on it again.

What does work is `~/Projects/vice-mcp`, a VICE build with an MCP server compiled
in. The binary is `vice/build-test-with-mcp/src/x64sc` and it needs a display, so
Xvfb is still required:

```bash
Xvfb :99 -screen 0 1024x768x24 &
DISPLAY=:99 x64sc -mcpserver -warp &      # serves http://127.0.0.1:6510/mcp
```

Then POST JSON-RPC 2.0 at `/mcp`: `tools/list` enumerates 64 tools, and
`tools/call` runs them. The useful ones here are `vice_autostart`,
`vice_checkpoint_add` (exec, load or store, so watchpoints too),
`vice_registers_get`, `vice_cia_get_state`, `vice_memory_read`,
`vice_disassemble` and `vice_display_screenshot`.

Three traps, each of which cost a run:

- **`sleep` is unavailable in this environment and `read -t 1 < /dev/zero`
  returns instantly**, so a wait loop built from it does not wait at all.
  `python3 -c "import time; time.sleep(3)"` does.
- **`pkill -f x64sc` matches the shell running it** and kills the caller. Use
  `pkill -x`, or put the commands in a script file.
- A checkpoint pauses the machine, and it stays paused. Poll `vice_ping` for
  `execution` and call `vice_execution_run` again, or nothing moves and every
  reading is of a halted machine.

Only the C64 is exposed: `vice_memory_banks` reports cpu, ram, rom, io and cart,
with no drive bank, so the 1541's RAM and PC cannot be read this way. Comparisons
have to be made on the C64 side.

### What it has already settled about The Lab

At `$3843`, the wait for DATA to go low, VICE reports `port_a=$c3 ddr_a=$3f` --
exactly what this emulator has there. So the C64's side of the handshake is set
up identically in both, and the divergence is after `$3846`, not before it.
VICE cycles through `$3843`/`$3846` repeatedly while the demo runs, and reaches
the demo proper; this emulator gets there too and then sticks at `$3992`.

The next experiment is to checkpoint `$3992` in VICE from before the autostart
and establish whether it is ever executed at all, then trace forward from
`$3846` in both and find the first instruction that differs.
