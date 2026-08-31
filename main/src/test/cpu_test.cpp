/*
 Runs the emulated 6502 against Klaus Dormann's functional test suite.

     make -C main/src/test run

 The suite is the standard way to check a 6502: it walks every documented
 opcode and addressing mode, the flag behaviour of each, and decimal mode
 arithmetic, trapping on the spot when something is wrong. Both the C64 and
 the emulated 1541 run on this same core, so a fault here would show up as
 software misbehaving in ways that are very hard to pin down on hardware.

 The test image is Copyright (C) 2012-2015 Klaus Dormann, distributed under
 the GNU General Public License version 3 or later, the same licence as this
 project.
*/

#include <fcntl.h>
#include <unistd.h>
#include <cstdio>
#include <cstring>
#include <string>
#include "CPU6502.hpp"

// Where the image is loaded, where execution starts, and the address the
// suite spins on once every test has passed. All three come from the
// assembler listing that ships with the suite.
static const uint16_t TEST_LOAD_ADDR    = 0x0000;
static const uint16_t TEST_START_ADDR   = 0x0400;
static const uint16_t TEST_SUCCESS_ADDR = 0x3469;

// The suite runs tens of millions of instructions. This is generous enough to
// finish and small enough to notice a runaway.
static const long MAX_INSTRUCTIONS = 500L * 1000L * 1000L;

// A bare 6502 with 64K of RAM and nothing else attached.
class TestCpu : public CPU6502 {
   public:
    uint8_t mem[65536];

    TestCpu()
    {
        memset(mem, 0, sizeof(mem));
    }

    void run() override
    {
    }
    uint8_t getMem(uint16_t addr) override
    {
        return mem[addr];
    }
    void setMem(uint16_t addr, uint8_t val) override
    {
        mem[addr] = val;
    }

    void begin(uint16_t addr)
    {
        pc = addr;
        sp = 0xfd;
        a = x = y   = 0;
        cflag       = false;
        zflag       = false;
        iflag       = true;
        dflag       = false;
        bflag       = false;
        vflag       = false;
        nflag       = false;
        cpuhalted   = false;
        numofcycles = 0;
    }

    // Runs one instruction. Returns the address it started at, so the caller
    // can spot a jump to itself.
    uint16_t stepOne()
    {
        uint16_t at = pc;
        execute(getMem(pc++));
        return at;
    }

    uint16_t currentPc() const
    {
        return pc;
    }
    bool isHalted() const
    {
        return cpuhalted;
    }

    // Handles for the targeted tests below.
    void setPc(uint16_t v)
    {
        pc = v;
    }
    uint8_t getSp() const
    {
        return sp;
    }
    void setSp(uint8_t v)
    {
        sp = v;
    }
    bool getI() const
    {
        return iflag;
    }
    void setI(bool v)
    {
        iflag = v;
    }
    bool getD() const
    {
        return dflag;
    }
    void setD(bool v)
    {
        dflag = v;
    }
    bool getC() const
    {
        return cflag;
    }
    void setC(bool v)
    {
        cflag = v;
    }
    void enterInterrupt(uint16_t vec, bool fromBrk)
    {
        setPCToIntVec(vec, fromBrk);
    }
    void setA(uint8_t v)
    {
        a = v;
    }
    uint8_t getA() const
    {
        return a;
    }
    void setX(uint8_t v)
    {
        x = v;
    }
    uint8_t getX() const
    {
        return x;
    }
    void setY(uint8_t v)
    {
        y = v;
    }
    bool getZ() const
    {
        return zflag;
    }
    bool getN() const
    {
        return nflag;
    }
};

static int checks   = 0;
static int failures = 0;

#define CHECK(cond, ...)                                  \
    do {                                                  \
        checks++;                                         \
        if (!(cond)) {                                    \
            failures++;                                   \
            printf("  FAIL %s:%d: ", __FILE__, __LINE__); \
            printf(__VA_ARGS__);                          \
            printf("\n");                                 \
        }                                                 \
    } while (0)

// The functional test suite deliberately leaves interrupts alone, since a
// bare 6502 has nothing to raise them. Both the C64 and the drive lean on
// this path constantly, so it is worth checking separately.
static void testInterruptEntry()
{
    printf("6502: interrupt entry pushes the right state\n");
    TestCpu cpu;
    cpu.begin(0x1000);
    cpu.setSp(0xff);
    cpu.setI(false);
    cpu.setD(true);
    cpu.setC(true);

    cpu.enterInterrupt(0x9000, false);

    CHECK(cpu.currentPc() == 0x9000, "did not jump to the vector, pc is $%04x", cpu.currentPc());
    CHECK(cpu.getI(), "the interrupt disable flag was not set on entry");
    CHECK(!cpu.getD(), "decimal mode was not cleared on entry");

    // The return address and status sit on the stack, high byte first.
    CHECK(cpu.getSp() == 0xfc, "three bytes should have been pushed, sp is $%02x", cpu.getSp());
    CHECK(cpu.mem[0x01ff] == 0x10, "return address high byte was $%02x", cpu.mem[0x01ff]);
    CHECK(cpu.mem[0x01fe] == 0x00, "return address low byte was $%02x", cpu.mem[0x01fe]);

    uint8_t pushed = cpu.mem[0x01fd];
    CHECK((pushed & 0x10) == 0, "the break flag was set on a hardware interrupt");
    CHECK((pushed & 0x20) != 0, "the unused status bit was not set");
    CHECK((pushed & 0x01) != 0, "carry was lost from the pushed status");
    CHECK((pushed & 0x08) != 0, "decimal was lost from the pushed status");
}

static void testBrkSetsBreakFlag()
{
    printf("6502: BRK marks the pushed status and skips the padding byte\n");
    TestCpu cpu;
    cpu.begin(0x2000);
    cpu.setSp(0xff);
    cpu.setI(false);

    cpu.mem[0xfffe] = 0x34;
    cpu.mem[0xffff] = 0x12;
    cpu.mem[0x2000] = 0x00;  // BRK
    cpu.mem[0x2001] = 0xea;  // the byte BRK skips over

    cpu.stepOne();

    CHECK(cpu.currentPc() == 0x1234, "BRK did not take the vector, pc is $%04x", cpu.currentPc());
    CHECK((cpu.mem[0x01fd] & 0x10) != 0, "BRK did not set the break flag in the pushed status");
    // BRK is one byte but pushes the address after the byte that follows it.
    uint16_t pushedPc = static_cast<uint16_t>(cpu.mem[0x01fe] | (cpu.mem[0x01ff] << 8));
    CHECK(pushedPc == 0x2002, "BRK pushed $%04x, expected $2002", pushedPc);
}

static void testRtiRestores()
{
    printf("6502: RTI puts back the flags and the return address\n");
    TestCpu cpu;
    cpu.begin(0x3000);
    cpu.setSp(0xff);
    cpu.setI(false);
    cpu.setC(true);
    cpu.setD(false);

    cpu.mem[0xfffe] = 0x00;
    cpu.mem[0xffff] = 0x80;
    cpu.mem[0x3000] = 0x00;  // BRK
    cpu.mem[0x8000] = 0x40;  // RTI at the handler

    cpu.stepOne();  // BRK
    CHECK(cpu.getI(), "interrupts were not disabled by BRK");

    cpu.setC(false);  // the handler clobbers a flag
    cpu.stepOne();    // RTI

    CHECK(cpu.currentPc() == 0x3002, "RTI returned to $%04x, expected $3002", cpu.currentPc());
    CHECK(cpu.getC(), "RTI did not restore carry");
    CHECK(!cpu.getI(), "RTI did not restore the interrupt flag");
    CHECK(cpu.getSp() == 0xff, "RTI did not unwind the stack, sp is $%02x", cpu.getSp());
}

static void testStackWraps()
{
    printf("6502: the stack pointer wraps inside page one\n");
    TestCpu cpu;
    cpu.begin(0x4000);
    cpu.setSp(0x00);

    cpu.mem[0x4000] = 0x48;  // PHA
    cpu.stepOne();
    CHECK(cpu.getSp() == 0xff, "sp did not wrap to $ff, it is $%02x", cpu.getSp());

    cpu.mem[0x4001] = 0x68;  // PLA
    cpu.stepOne();
    CHECK(cpu.getSp() == 0x00, "sp did not wrap back to $00, it is $%02x", cpu.getSp());
}

// Klaus Dormann's suite only walks the documented opcodes. Loaders reach for
// the undocumented ones constantly, and SBX in particular is how a loader
// counts a block down: it subtracts without a carry to worry about.
static void testSbx()
{
    printf("6502: SBX subtracts the operand from A AND X\n");
    TestCpu cpu;

    // $a0 - $e0 borrows: the answer is $c0 and the carry comes out clear.
    cpu.begin(0x4000);
    cpu.setA(0xa0);
    cpu.setX(0xa0);
    cpu.mem[0x4000] = 0xcb;  // SBX #
    cpu.mem[0x4001] = 0xe0;
    cpu.stepOne();
    CHECK(cpu.getX() == 0xc0, "SBX #$e0 on A=X=$a0 left $%02x, expected $c0", cpu.getX());
    CHECK(!cpu.getC(), "SBX set the carry on a borrow");
    CHECK(cpu.getN(), "SBX did not set N from the result");
    CHECK(!cpu.getZ(), "SBX set Z on a result of $c0");
    CHECK(cpu.getA() == 0xa0, "SBX changed the accumulator");

    // No borrow: the carry stays set, as after a compare that was not less.
    cpu.begin(0x4000);
    cpu.setA(0xff);
    cpu.setX(0xff);
    cpu.mem[0x4000] = 0xcb;
    cpu.mem[0x4001] = 0x01;
    cpu.stepOne();
    CHECK(cpu.getX() == 0xfe, "SBX #$01 on A=X=$ff left $%02x, expected $fe", cpu.getX());
    CHECK(cpu.getC(), "SBX cleared the carry without a borrow");

    // Equal values leave zero with the carry set.
    cpu.begin(0x4000);
    cpu.setA(0x3c);
    cpu.setX(0xff);
    cpu.mem[0x4000] = 0xcb;
    cpu.mem[0x4001] = 0x3c;
    cpu.stepOne();
    CHECK(cpu.getX() == 0x00, "SBX of equal values left $%02x, expected $00", cpu.getX());
    CHECK(cpu.getZ(), "SBX did not set Z on a result of zero");
    CHECK(cpu.getC(), "SBX cleared the carry on equal values");

    // The AND happens before the subtraction, not after it.
    cpu.begin(0x4000);
    cpu.setA(0x0f);
    cpu.setX(0xf0);
    cpu.mem[0x4000] = 0xcb;
    cpu.mem[0x4001] = 0x01;
    cpu.stepOne();
    CHECK(cpu.getX() == 0xff, "SBX did the AND after the subtraction, X is $%02x", cpu.getX());
}

// The pointer for (zp),y lives in page zero and does not walk out of it.
static void testIndirectYWrapsInZeroPage()
{
    printf("6502: an indirect pointer at $ff takes its high byte from $00\n");
    TestCpu cpu;
    cpu.begin(0x4000);
    cpu.setY(0x00);
    cpu.mem[0x00ff] = 0x34;
    cpu.mem[0x0000] = 0x12;  // the high byte the real chip reads
    cpu.mem[0x0100] = 0x99;  // what it would read if the pointer ran on
    cpu.mem[0x1234] = 0x5a;
    cpu.mem[0x9934] = 0xa5;

    cpu.mem[0x4000] = 0xb1;  // LDA (zp),Y
    cpu.mem[0x4001] = 0xff;
    cpu.stepOne();
    CHECK(cpu.getA() == 0x5a, "read through $%04x, expected $1234 (a is $%02x)",
          cpu.getA() == 0xa5 ? 0x9934 : 0, cpu.getA());

    // Indexing still crosses out of the pointed-at page as usual.
    cpu.begin(0x4000);
    cpu.setY(0x10);
    cpu.mem[0x1244] = 0x77;
    cpu.mem[0x4000] = 0xb1;
    cpu.mem[0x4001] = 0xff;
    cpu.stepOne();
    CHECK(cpu.getA() == 0x77, "indexing off the wrapped pointer gave $%02x, expected $77",
          cpu.getA());
}

static bool loadImage(TestCpu& cpu, const std::string& path)
{
    int fd = open(path.c_str(), O_RDONLY);
    if (fd < 0) return false;

    size_t total = 0;
    while (total < sizeof(cpu.mem)) {
        ssize_t got = read(fd, cpu.mem + TEST_LOAD_ADDR + total, sizeof(cpu.mem) - TEST_LOAD_ADDR - total);
        if (got <= 0) break;
        total += static_cast<size_t>(got);
    }
    close(fd);
    return total > 0;
}

// Klaus Dormann's suite walks the documented opcodes only, and the cycle tests
// next door check how long these take without ever checking what they compute.
// A fast loader is made of them: Sparkle's GCR decoder, which runs in the
// drive's zero page, is almost nothing but LAX, SAX, ALR and SBX. A wrong
// result here does not crash, it silently decodes to the wrong byte, and the
// loader spins looking for a block it will never recognise.
static void testUndocumented()
{
    printf("6502: the undocumented opcodes a loader is built from\n");
    TestCpu cpu;

    // LAX zp: loads A and X with the same byte.
    cpu.begin(0x4000);
    cpu.mem[0x0010]  = 0x80;
    cpu.mem[0x4000]  = 0xa7;  // LAX $10
    cpu.mem[0x4001]  = 0x10;
    cpu.stepOne();
    CHECK(cpu.getA() == 0x80, "LAX $10 left A=$%02x, expected $80", cpu.getA());
    CHECK(cpu.getX() == 0x80, "LAX $10 left X=$%02x, expected $80", cpu.getX());
    CHECK(cpu.getN(), "LAX did not take N from the byte it loaded");
    CHECK(!cpu.getZ(), "LAX set Z on $80");

    // LAX abs, the form the decoder uses to read a table.
    cpu.begin(0x4000);
    cpu.mem[0x1234] = 0x00;
    cpu.mem[0x4000] = 0xaf;  // LAX $1234
    cpu.mem[0x4001] = 0x34;
    cpu.mem[0x4002] = 0x12;
    cpu.stepOne();
    CHECK(cpu.getA() == 0x00 && cpu.getX() == 0x00, "LAX $1234 did not load zero into both");
    CHECK(cpu.getZ(), "LAX did not set Z on zero");

    // LAX (zp),Y.
    cpu.begin(0x4000);
    cpu.setY(0x04);
    cpu.mem[0x0020] = 0x00;
    cpu.mem[0x0021] = 0x30;  // pointer to $3000
    cpu.mem[0x3004] = 0x5a;
    cpu.mem[0x4000] = 0xb3;  // LAX ($20),Y
    cpu.mem[0x4001] = 0x20;
    cpu.stepOne();
    CHECK(cpu.getA() == 0x5a && cpu.getX() == 0x5a, "LAX ($20),Y left A=$%02x X=$%02x, expected $5a",
          cpu.getA(), cpu.getX());

    // SAX zp: stores A AND X, and touches no flag.
    cpu.begin(0x4000);
    cpu.setA(0xf0);
    cpu.setX(0x3c);
    cpu.setC(true);
    cpu.mem[0x4000] = 0x87;  // SAX $30
    cpu.mem[0x4001] = 0x30;
    cpu.stepOne();
    CHECK(cpu.mem[0x0030] == 0x30, "SAX stored $%02x, expected $f0 AND $3c = $30", cpu.mem[0x0030]);
    CHECK(cpu.getA() == 0xf0 && cpu.getX() == 0x3c, "SAX changed a register");
    CHECK(cpu.getC(), "SAX cleared the carry, it must not touch flags");

    // ALR: AND then shift right, with the carry coming out of the bit shifted off.
    cpu.begin(0x4000);
    cpu.setA(0xff);
    cpu.mem[0x4000] = 0x4b;  // ALR #$0f
    cpu.mem[0x4001] = 0x0f;
    cpu.stepOne();
    CHECK(cpu.getA() == 0x07, "ALR #$0f on A=$ff left $%02x, expected $07", cpu.getA());
    CHECK(cpu.getC(), "ALR did not put the shifted out bit into the carry");
    CHECK(!cpu.getN(), "ALR set N, a right shift cannot");

    // ANC: AND, then copy bit 7 into the carry.
    cpu.begin(0x4000);
    cpu.setA(0xff);
    cpu.mem[0x4000] = 0x0b;  // ANC #$80
    cpu.mem[0x4001] = 0x80;
    cpu.stepOne();
    CHECK(cpu.getA() == 0x80, "ANC #$80 on A=$ff left $%02x, expected $80", cpu.getA());
    CHECK(cpu.getN(), "ANC did not set N");
    CHECK(cpu.getC(), "ANC did not copy bit 7 into the carry");

    // DCP: decrement, then compare against A.
    cpu.begin(0x4000);
    cpu.setA(0x05);
    cpu.mem[0x0040] = 0x05;
    cpu.mem[0x4000] = 0xc7;  // DCP $40
    cpu.mem[0x4001] = 0x40;
    cpu.stepOne();
    CHECK(cpu.mem[0x0040] == 0x04, "DCP left $%02x in memory, expected $04", cpu.mem[0x0040]);
    CHECK(cpu.getC(), "DCP cleared the carry comparing $05 against $04");
    CHECK(!cpu.getZ(), "DCP set Z on an unequal compare");
    CHECK(cpu.getA() == 0x05, "DCP changed the accumulator");

    // ISC: increment, then subtract.
    cpu.begin(0x4000);
    cpu.setA(0x20);
    cpu.setC(true);
    cpu.mem[0x0050] = 0x0f;
    cpu.mem[0x4000] = 0xe7;  // ISC $50
    cpu.mem[0x4001] = 0x50;
    cpu.stepOne();
    CHECK(cpu.mem[0x0050] == 0x10, "ISC left $%02x in memory, expected $10", cpu.mem[0x0050]);
    CHECK(cpu.getA() == 0x10, "ISC left A=$%02x, expected $20 - $10 = $10", cpu.getA());

    // SLO: shift left, then OR into A.
    cpu.begin(0x4000);
    cpu.setA(0x00);
    cpu.mem[0x0060] = 0x81;
    cpu.mem[0x4000] = 0x07;  // SLO $60
    cpu.mem[0x4001] = 0x60;
    cpu.stepOne();
    CHECK(cpu.mem[0x0060] == 0x02, "SLO left $%02x in memory, expected $02", cpu.mem[0x0060]);
    CHECK(cpu.getA() == 0x02, "SLO left A=$%02x, expected $02", cpu.getA());
    CHECK(cpu.getC(), "SLO did not shift bit 7 into the carry");

    // SRE: shift right, then EOR into A. This is the one a GCR decoder leans on.
    cpu.begin(0x4000);
    cpu.setA(0xff);
    cpu.mem[0x0070] = 0x03;
    cpu.mem[0x4000] = 0x47;  // SRE $70
    cpu.mem[0x4001] = 0x70;
    cpu.stepOne();
    CHECK(cpu.mem[0x0070] == 0x01, "SRE left $%02x in memory, expected $01", cpu.mem[0x0070]);
    CHECK(cpu.getA() == 0xfe, "SRE left A=$%02x, expected $ff EOR $01 = $fe", cpu.getA());
    CHECK(cpu.getC(), "SRE did not shift bit 0 into the carry");

    // RLA: rotate left through the carry, then AND into A.
    cpu.begin(0x4000);
    cpu.setA(0xff);
    cpu.setC(true);
    cpu.mem[0x0080] = 0x80;
    cpu.mem[0x4000] = 0x27;  // RLA $80
    cpu.mem[0x4001] = 0x80;
    cpu.stepOne();
    CHECK(cpu.mem[0x0080] == 0x01, "RLA left $%02x in memory, expected $01", cpu.mem[0x0080]);
    CHECK(cpu.getA() == 0x01, "RLA left A=$%02x, expected $01", cpu.getA());
    CHECK(cpu.getC(), "RLA did not rotate bit 7 into the carry");

    // RRA: rotate right through the carry, then add to A.
    cpu.begin(0x4000);
    cpu.setA(0x00);
    cpu.setC(true);
    cpu.mem[0x0090] = 0x01;
    cpu.mem[0x4000] = 0x67;  // RRA $90
    cpu.mem[0x4001] = 0x90;
    cpu.stepOne();
    CHECK(cpu.mem[0x0090] == 0x80, "RRA left $%02x in memory, expected $80", cpu.mem[0x0090]);
    CHECK(cpu.getA() == 0x81, "RRA left A=$%02x, expected $80 plus the carry it rotated out", cpu.getA());
}

int main(int argc, char** argv)
{
    std::string path = (argc > 1) ? argv[1] : "6502_functional_test.bin";

    TestCpu cpu;
    if (!loadImage(cpu, path)) {
        printf("cannot read %s\n", path.c_str());
        printf("the suite is vendored next to this test; pass a path to use another copy\n");
        return 1;
    }

    printf("6502: Klaus Dormann functional test suite\n");
    cpu.begin(TEST_START_ADDR);

    long     executed = 0;
    uint16_t trapAt   = 0;
    bool     trapped  = false;

    while (executed < MAX_INSTRUCTIONS) {
        uint16_t at = cpu.stepOne();
        executed++;

        // Every trap in the suite, pass or fail, is a jump to itself.
        if (cpu.currentPc() == at) {
            trapAt  = at;
            trapped = true;
            break;
        }
        if (cpu.isHalted()) {
            printf("  FAIL: hit an illegal opcode at $%04x after %ld instructions\n", at, executed);
            return 1;
        }
    }

    if (!trapped) {
        printf("  FAIL: ran %ld instructions without reaching a trap\n", executed);
        printf("        the suite never finished, so something is looping\n");
        return 1;
    }

    checks++;
    if (trapAt == TEST_SUCCESS_ADDR) {
        printf("  passed, %ld instructions executed\n", executed);
    } else {
        failures++;
        printf("  FAIL: trapped at $%04x after %ld instructions\n", trapAt, executed);
        printf("        that address is where the suite gave up; look it up in\n");
        printf("        6502_functional_test.lst to see which test failed\n");
    }

    testInterruptEntry();
    testBrkSetsBreakFlag();
    testRtiRestores();
    testStackWraps();
    testSbx();
    testUndocumented();
    testIndirectYWrapsInZeroPage();

    printf("\n%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
