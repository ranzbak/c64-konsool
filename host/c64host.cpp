// Runs the whole emulator on a normal machine, with no panel, so a demo can be
// started and its screen written to a file. The badge runs CPUC64::run() on one
// core and the display on the other; this keeps that shape, with the display
// loop here throttling the emulation exactly as the badge does.
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <string>
#include <thread>
#include <atomic>
#include "CPUC64.hpp"
#include "VIC.hpp"
#include "HeadlessDisplay.hpp"
#include "HostC64Emu.hpp"
#include "roms/charset.h"
#include "sid/sid.hpp"
#include "drive/D64Disk.hpp"

uint32_t hostRandomState = 0x12345678u;

// The sixteen C64 colours as RGB565, matching the board driver's table.
static uint8_t  ram[65536];
static CPUC64   cpu;
static VIC      vic;
static SID      sid;
static C64Emu   emu;

static std::atomic<bool> running{true};
static D64Disk           disk;
static uint8_t           driveRom[Drive1541::ROM_SIZE];

// Disks to be swapped in later, for the multi load demos that stop and wait for
// the next one. Each is its own D64Disk with its own fd, held for the whole run
// so the drive never reads through a descriptor that has just been closed.
// D64Disk closes its fd in the destructor and has no copy constructor worth
// having, so this is a plain array rather than a vector.
static const int MAX_SWAPS = 8;
static D64Disk    swapDisks[MAX_SWAPS];
static const char* swapPath[MAX_SWAPS];
static long        swapFrame[MAX_SWAPS];
static int         numSwapDisks  = 0;
static int         numSwapFrames = 0;

// Types into the kernal's keyboard buffer rather than the matrix: the buffer
// is what the matrix feeds anyway, and it needs no scancode table.
static std::string pendingKeys;
static void feedKeyboard()
{
    if (pendingKeys.empty()) return;
    // $c6 is how many characters are waiting, $0277 is the buffer itself.
    if (ram[0x00c6] != 0) return;  // let the machine catch up
    size_t n = pendingKeys.size() < 8 ? pendingKeys.size() : 8;
    for (size_t i = 0; i < n; i++) {
        char c = pendingKeys[i];
        // RETURN on a C64 is $0d; a newline typed on the command line is $0a.
        ram[0x0277 + i] = (uint8_t)((c == '\n') ? 0x0d : c);
    }
    ram[0x00c6] = (uint8_t)n;
    pendingKeys.erase(0, n);
}

// Puts a .prg where its first two bytes say it goes, the way the menu does.
static bool injectPrg(const char* path)
{
    FILE* f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "cannot open %s\n", path); return false; }
    uint8_t hdr[2];
    if (fread(hdr, 1, 2, f) != 2) { fclose(f); return false; }
    uint16_t addr = hdr[0] | (hdr[1] << 8);
    size_t   pos  = addr, n;
    while (pos < 65536 && (n = fread(ram + pos, 1, 65536 - pos, f)) > 0) pos += n;
    fclose(f);
    fprintf(stderr, "injected %s at $%04x..$%04x\n", path, addr, (unsigned)pos - 1);
    // Point BASIC at the end so RUN works for a BASIC program.
    ram[0x2d] = (uint8_t)(pos & 0xff);
    ram[0x2e] = (uint8_t)(pos >> 8);
    return true;
}

// Almost every demo is machine code behind a one line BASIC stub that does
// SYS <address>. Injecting the bytes does not relink BASIC's line pointers the
// way a real LOAD does, so RUN falls straight through; reading the address out
// of the stub and jumping there is what SYS would have done anyway.
static uint16_t findSysAddress()
{
    for (uint16_t a = 0x0801; a < 0x0830; a++) {
        if (ram[a] != 0x9e) continue;  // the SYS token
        uint16_t n = 0;
        uint16_t p = a + 1;
        while (ram[p] == 0x20) p++;  // SYS 2061 and SYS2061 are both written
        if (ram[p] < '0' || ram[p] > '9') continue;
        while (ram[p] >= '0' && ram[p] <= '9') n = (uint16_t)(n * 10 + (ram[p++] - '0'));
        return n;
    }
    return 0;
}

static void cpuThread()
{
    cpu.run();  // never returns; the process exits when the frames are done
}

// A .ppm is trivial to write and every image tool reads it.
static void writePPM(const char* path, HeadlessDisplay* d)
{
    FILE* f = fopen(path, "wb");
    if (!f) { fprintf(stderr, "cannot write %s\n", path); return; }
    const int W = HeadlessDisplay::W, H = HeadlessDisplay::H;
    fprintf(f, "P6\n%d %d\n255\n", W, H);
    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
            uint16_t c = d->screen[y * W + x];
            // RGB565 back out to eight bits a channel.
            uint8_t px[3] = {static_cast<uint8_t>(((c >> 11) & 0x1f) << 3),
                             static_cast<uint8_t>(((c >> 5) & 0x3f) << 2),
                             static_cast<uint8_t>((c & 0x1f) << 3)};
            fwrite(px, 1, 3, f);
        }
    }
    fclose(f);
    fprintf(stderr, "wrote %s (%dx%d)\n", path, W, H);
}

int main(int argc, char** argv)
{
    long        frames    = 200;
    const char* shot      = "screen.ppm";
    const char* diskPath  = nullptr;
    const char* prgPath   = nullptr;
    const char* romPath   = nullptr;
    bool        trueDrive = false;
    uint16_t    sysAddr   = 0;
    bool        autostart = false;
    const char* memDump   = nullptr;

    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        if (a == "--frames" && i + 1 < argc) frames = atol(argv[++i]);
        else if (a == "--screenshot" && i + 1 < argc) shot = argv[++i];
        else if (a == "--seed" && i + 1 < argc) hostRandomState = (uint32_t)atol(argv[++i]);
        else if (a == "--disk" && i + 1 < argc) diskPath = argv[++i];
        else if (a == "--disk2" && i + 1 < argc) {
            if (numSwapDisks == MAX_SWAPS) {
                fprintf(stderr, "at most %d --disk2 images\n", MAX_SWAPS);
                return 2;
            }
            swapPath[numSwapDisks++] = argv[++i];
        }
        else if (a == "--swap-at" && i + 1 < argc) {
            if (numSwapFrames == MAX_SWAPS) {
                fprintf(stderr, "at most %d --swap-at frames\n", MAX_SWAPS);
                return 2;
            }
            swapFrame[numSwapFrames++] = atol(argv[++i]);
        }
        else if (a == "--prg" && i + 1 < argc) prgPath = argv[++i];
        else if (a == "--drive-rom" && i + 1 < argc) romPath = argv[++i];
        else if (a == "--truedrive") trueDrive = true;
        else if (a == "--type" && i + 1 < argc) pendingKeys = argv[++i];
        else if (a == "--sys" && i + 1 < argc) sysAddr = (uint16_t)strtol(argv[++i], nullptr, 0);
        else if (a == "--autostart") autostart = true;
        else if (a == "--dumpmem" && i + 1 < argc) memDump = argv[++i];
        else {
            fprintf(stderr,
                    "usage: %s [--frames N] [--screenshot out.ppm] [--seed N]\n"
                    "          [--disk d.d64] [--truedrive] [--drive-rom 1541.rom]\n"
                    "          [--disk2 next.d64 --swap-at N] ...\n"
                    "          [--prg p.prg] [--type \"LOAD*,8,1\\rRUN\\r\"]\n"
                    "          [--sys addr] [--autostart] [--dumpmem out.bin]\n"
                    "\n"
                    "  --disk2/--swap-at may be repeated up to %d times; the n'th\n"
                    "  --disk2 goes in at the n'th --swap-at. A swap needs about 70\n"
                    "  frames to finish, so leave that much before --frames.\n",
                    argv[0], MAX_SWAPS);
            return 2;
        }
    }

    memset(ram, 0, sizeof(ram));
    vic.init(ram, charset_rom, &sid);
    cpu.init(ram, charset_rom, &vic, &emu);

    // The VIC ticks the SID once a rasterline, so it has to be set up even
    // though nothing here listens. The samples go nowhere.
    sid.init(cpu.getSidRegs(), [](int16_t*, size_t) {}, 8580);

    // Put a disk in the drive before anything starts running.
    if (diskPath != nullptr) {
        if (!disk.open(diskPath)) { fprintf(stderr, "cannot read %s\n", diskPath); return 2; }
        if (trueDrive) {
            if (romPath == nullptr) { fprintf(stderr, "--truedrive needs --drive-rom\n"); return 2; }
            FILE* rf = fopen(romPath, "rb");
            if (!rf || fread(driveRom, 1, sizeof(driveRom), rf) != sizeof(driveRom)) {
                fprintf(stderr, "%s is not a %u byte 1541 rom\n", romPath,
                        (unsigned)sizeof(driveRom));
                return 2;
            }
            fclose(rf);
            if (!cpu.enableTrueDrive(driveRom, &disk)) {
                fprintf(stderr, "could not start the drive\n");
                return 2;
            }
            fprintf(stderr, "1541 emulation on, %s in the drive\n", diskPath);
        } else {
            fprintf(stderr, "%s attached (kernal traps)\n", diskPath);
        }
    }

    // The later disks, opened now rather than at the swap, so a bad path fails
    // before the run starts instead of half way through a demo.
    if (numSwapDisks != numSwapFrames) {
        fprintf(stderr, "%d --disk2 image(s) but %d --swap-at frame(s); each disk needs one frame\n",
                numSwapDisks, numSwapFrames);
        return 2;
    }
    if (numSwapDisks > 0) {
        // Without --truedrive nothing is attached to swap for: no CbmDos is
        // constructed here, so --disk alone is just a filename that gets printed.
        if (!trueDrive || diskPath == nullptr) {
            fprintf(stderr, "--disk2 needs --disk and --truedrive\n");
            return 2;
        }
        for (int s = 0; s < numSwapDisks; s++) {
            if (!swapDisks[s].open(swapPath[s])) {
                fprintf(stderr, "cannot read %s\n", swapPath[s]);
                return 2;
            }
            // A swap takes 1.2M drive cycles, about 61 PAL frames, for the
            // write protect line to finish going away and coming back. Ending
            // the run inside that window means the DOS never sees the new disk.
            if (swapFrame[s] + 70 > frames) {
                fprintf(stderr,
                        "warning: --swap-at %ld leaves under 70 frames of the %ld asked for; "
                        "the drive may not finish noticing the change\n",
                        swapFrame[s], frames);
            }
            fprintf(stderr, "%s ready to go in at frame %ld\n", swapPath[s], swapFrame[s]);
        }
    }

    HeadlessDisplay* display = static_cast<HeadlessDisplay*>(vic.getDriver());

    std::thread th(cpuThread);

    // The display side: draw a frame, then let the emulation run the next one.
    for (long i = 0; i < frames; i++) {
        // Everything in this loop reaches into the running machine: injectPrg
        // writes RAM, setPC moves the program counter, the swap changes the
        // disk under the head, and refresh reads the framebuffer. So park the
        // emulation first and hold it there for all of it. Doing the injection
        // before this wait raced the emulation mid-frame, and the program
        // occasionally never started at all -- which showed up as a run that
        // was reproducible most of the time, the worst kind.
        hostWaitUntilParked(cpu.getFrameRateMutex());

        // Give the machine a moment to reach the READY prompt before typing,
        // and let an injected program be started the same way a person would.
        if (i == 100 && prgPath != nullptr) {
            injectPrg(prgPath);
            if (autostart && sysAddr == 0) sysAddr = findSysAddress();
        }
        if (i == 105 && sysAddr != 0) {
            fprintf(stderr, "starting at $%04x\n", sysAddr);
            cpu.setPC(sysAddr);
        }
        if (i > 100) feedKeyboard();
        for (int s = 0; s < numSwapDisks; s++) {
            if (i != swapFrame[s]) continue;
            fprintf(stderr, "frame %ld: swapping in %s\n", i, swapPath[s]);
            // The emulation thread steps the drive, so it has to stop before
            // the GCR track under the head is rewritten. The badge does the
            // same around anything that touches the drive from the menu task.
            // Deliberately swapDisk(), not enableTrueDrive() or setDisk():
            // swapDisk leaves the drive CPU alone, so a loader that has
            // uploaded its own code survives, and it works the write protect
            // line the way a disk coming out and another going in does, which
            // is the only way the DOS can tell.
            cpu.drive.swapDisk(&swapDisks[s]);
        }
        vic.refresh(true);
        xSemaphoreGive(cpu.getFrameRateMutex());
    }

    // The loop above ends by letting the emulation go, so it is drawing
    // another frame right now. Everything below reads the machine -- the VIC
    // registers, the screen, the framebuffer, all of RAM -- so wait for it to
    // park once more first. Without this the capture races that last frame,
    // and the run is reproducible only about five times in six, which is far
    // worse than never: it makes a before-and-after comparison look solid
    // until the one time it quietly is not.
    hostWaitUntilParked(cpu.getFrameRateMutex());

    // Where the VIC is fetching from. A demo that draws the wrong glyphs is
    // usually pointed at the wrong place rather than holding wrong data.
    {
        uint8_t d011 = vic.vicreg[0x11], d016 = vic.vicreg[0x16], d018 = vic.vicreg[0x18];
        bool    bmm = d011 & 0x20, ecm = d011 & 0x40, mcm = d016 & 0x10;
        fprintf(stderr,
                "VIC: $d011=%02x $d016=%02x $d018=%02x  %s%s%s\n"
                "     vicmem=$%04x screenmem=$%04x bitmap=$%04x charset=%s(+$%04x)\n",
                d011, d016, d018, bmm ? "bitmap " : "text ", ecm ? "ecm " : "", mcm ? "multicolour" : "",
                vic.vicmem, vic.screenmemstart, vic.bitmapstart,
                (vic.charset >= charset_rom && vic.charset < charset_rom + 4096) ? "CHARROM" : "ram",
                (unsigned)(vic.charset >= charset_rom && vic.charset < charset_rom + 4096
                               ? vic.charset - charset_rom
                               : vic.charset - ram));
    }

    // The text screen says more than the pixels do: if the machine booted, the
    // banner is sitting in screen RAM at $0400 as screen codes.
    printf("--- screen memory at $0400 ---\n");
    for (int row = 0; row < 25; row++) {
        char line[41];
        for (int col = 0; col < 40; col++) {
            uint8_t sc = ram[0x0400 + row * 40 + col];
            char    ch;
            if (sc == 0x20) ch = ' ';
            else if (sc <= 0x1a) ch = (char)('@' + sc);       // @ and A-Z
            else if (sc >= 0x30 && sc <= 0x39) ch = (char)sc; // digits
            else if (sc == 0x2e) ch = '.';
            else if (sc == 0x2c) ch = ',';
            else if (sc == 0x2f) ch = '/';
            else if (sc == 0x1b) ch = '[';
            else if (sc == 0x1d) ch = ']';
            else if (sc == 0x3a) ch = ':';
            else ch = (sc == 0) ? ' ' : '.';
            line[col] = ch;
        }
        line[40] = 0;
        printf("|%s|\n", line);
    }

    if (memDump != nullptr) {
        FILE* mf = fopen(memDump, "wb");
        if (mf) { fwrite(ram, 1, sizeof(ram), mf); fclose(mf); fprintf(stderr, "wrote %s\n", memDump); }
    }

    writePPM(shot, display);
    fprintf(stderr, "%ld frames asked for, display drew %ld\n", frames, display->frames);
    fflush(nullptr);   // _exit skips this, and the screen dump is buffered
    th.detach();
    _exit(0);
}
