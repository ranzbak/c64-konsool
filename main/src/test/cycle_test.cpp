/*
 Checks that the emulated 6502 charges the right number of cycles.

     make -C main/src/test run

 Klaus Dormann's functional suite next door validates results and says nothing
 at all about timing, so a cycle that is charged twice, or not at all, sails
 straight through it. Ordinary code never notices either: the kernal and the
 1541 DOS talk to each other with handshakes, so each side waits however long
 the other takes. Fast loaders do not. They bit-bang the serial bus with both
 sides counting cycles, so the drive has to put a bit on the line exactly when
 the C64 looks for it. One cycle out anywhere in the loop and the data comes
 back shifted or garbled.

 The rules pinned down here come from the masswerk.at 6502 instruction set
 reference, https://www.masswerk.at/6502/6502_instruction_set.html, illegal
 opcodes included:

   - A branch costs 2 when it is not taken, 3 when it is taken, and 4 when it
     is taken and the target lies in a different page from the instruction
     that follows the branch.
   - An indexed READ (abs,X, abs,Y, (zp),Y) costs one cycle more when the
     index carries out of the low byte, because the chip reads once with the
     stale high byte and then again with the corrected one.
   - An indexed WRITE or read-modify-write never varies. The chip always
     performs that dummy read, so the higher count is the only count, and
     charging those a page-cross penalty would be just as wrong as not
     charging the reads.

 Every count below was taken from the reference, not from the emulator.
*/

#include <cstdio>
#include <cstring>
#include "CPU6502.hpp"

// A bare 6502 with 64K of RAM and nothing else attached, plus the handles
// needed to set up one instruction at a time and read the cycles back.
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

    void setA(uint8_t v)
    {
        a = v;
    }
    void setX(uint8_t v)
    {
        x = v;
    }
    void setY(uint8_t v)
    {
        y = v;
    }
    void setC(bool v)
    {
        cflag = v;
    }
    void setZ(bool v)
    {
        zflag = v;
    }
    void setN(bool v)
    {
        nflag = v;
    }
    void setV(bool v)
    {
        vflag = v;
    }
    uint8_t getA() const
    {
        return a;
    }
    uint16_t currentPc() const
    {
        return pc;
    }
    bool jammed() const
    {
        return cpuhalted;
    }
    void enterInterrupt(uint16_t vec, bool fromBrk)
    {
        setPCToIntVec(vec, fromBrk);
    }

    // Runs the one instruction at pc and reports what it cost.
    uint8_t step()
    {
        numofcycles = 0;
        execute(getMem(pc++));
        return numofcycles;
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

// Where the instruction under test is assembled, and two data addresses well
// away from it. An index of $10 lands inside the page from $2000 and steps
// over into the next one from $20f8.
static const uint16_t CODE    = 0x1000;
static const uint16_t DATA    = 0x2000;
static const uint16_t DATAEND = 0x20f8;
static const uint8_t  IDX     = 0x10;
static const uint8_t  PTR     = 0x80;  // zero page slot holding the (zp),Y pointer

// ---------------------------------------------------------------- branches

struct BranchCase {
    uint8_t     opcode;
    const char *name;
    void (TestCpu::*setFlag)(bool);
    bool takenValue;  // the value of that flag that makes this branch jump
};

static const BranchCase BRANCHES[] = {
    {0x10, "bpl", &TestCpu::setN, false}, {0x30, "bmi", &TestCpu::setN, true},
    {0x50, "bvc", &TestCpu::setV, false}, {0x70, "bvs", &TestCpu::setV, true},
    {0x90, "bcc", &TestCpu::setC, false}, {0xb0, "bcs", &TestCpu::setC, true},
    {0xd0, "bne", &TestCpu::setZ, false}, {0xf0, "beq", &TestCpu::setZ, true},
};

// Assembles one branch at `at` with the given offset, runs it, and reports
// both what it cost and where it went.
static uint8_t runBranch(const BranchCase &b, bool taken, uint16_t at, int8_t offset, uint16_t *landed = nullptr)
{
    TestCpu cpu;
    cpu.begin(at);
    (cpu.*b.setFlag)(taken ? b.takenValue : !b.takenValue);
    cpu.mem[at]     = b.opcode;
    cpu.mem[at + 1] = static_cast<uint8_t>(offset);
    uint8_t cost    = cpu.step();
    if (landed) *landed = cpu.currentPc();
    return cost;
}

static void testBranchCycles()
{
    printf("cycles: a branch costs 2, 3 when taken, 4 when taken across a page\n");

    for (unsigned i = 0; i < sizeof(BRANCHES) / sizeof(BRANCHES[0]); i++) {
        const BranchCase &b = BRANCHES[i];
        uint8_t           got;

        // Not taken: the offset is read and thrown away, two cycles, and
        // whether the target would have crossed a page makes no difference.
        got = runBranch(b, false, CODE, 0x10);
        CHECK(got == 2, "%s not taken cost %u, want 2", b.name, got);
        got = runBranch(b, false, 0x10f0, 0x20);
        CHECK(got == 2, "%s not taken over a page cost %u, want 2", b.name, got);

        // Taken inside the page: $1000 + 2 = $1002, + $10 = $1012, still $10.
        got = runBranch(b, true, CODE, 0x10);
        CHECK(got == 3, "%s taken inside a page cost %u, want 3", b.name, got);

        // Taken forwards over the boundary: $10f0 + 2 = $10f2, + $20 = $1112.
        got = runBranch(b, true, 0x10f0, 0x20);
        CHECK(got == 4, "%s taken forward across a page cost %u, want 4", b.name, got);

        // Taken backwards over the boundary: $1005 + 2 = $1007, - $20 = $0fe7.
        got = runBranch(b, true, 0x1005, -0x20);
        CHECK(got == 4, "%s taken back across a page cost %u, want 4", b.name, got);

        // The boundary itself. From $10fd the next instruction is $10ff, so an
        // offset of 0 stays in the page and an offset of 1 does not. One byte
        // apart, one cycle apart.
        got = runBranch(b, true, 0x10fd, 0x00);
        CHECK(got == 3, "%s taken to the last byte of the page cost %u, want 3", b.name, got);
        got = runBranch(b, true, 0x10fd, 0x01);
        CHECK(got == 4, "%s taken to the first byte of the next page cost %u, want 4", b.name, got);
    }

    // The jump still goes where it should, cycles aside.
    uint16_t landed = 0;
    runBranch(BRANCHES[6], true, 0x10f0, 0x20, &landed);  // bne
    CHECK(landed == 0x1112, "the branch landed at $%04x, want $1112", landed);

    // A backwards branch from the bottom of memory wraps rather than going
    // negative, the same as the chip does.
    uint8_t cost = runBranch(BRANCHES[6], true, 0x0002, -128, &landed);
    CHECK(cost == 4, "a branch wrapping past $0000 cost %u, want 4", cost);
    CHECK(landed == 0xff84, "the wrapping branch landed at $%04x, want $ff84", landed);
}

// The emulator decides the page cross by testing one bit, which works because
// the two high bytes can only ever be equal or one apart. That is a shortcut,
// so check it the long way instead: every offset from every interesting place
// in a page, against a target worked out separately.
static void testBranchTargetsExhaustively()
{
    printf("cycles: every branch offset from every corner of a page\n");

    static const uint16_t bases[] = {0x0000, 0x0001, 0x0002, 0x007e, 0x1000, 0x1040,
                                     0x10fd, 0x10fe, 0x10ff, 0xfffd, 0xff00, 0xff80};
    int                   wrong   = 0;
    int                   crossed = 0;

    for (unsigned bi = 0; bi < sizeof(bases) / sizeof(bases[0]); bi++) {
        for (int off = -128; off <= 127; off++) {
            const BranchCase &b = BRANCHES[6];  // bne, they all share the code

            // What the chip does: the offset applies to the address of the
            // instruction after the branch, and the answer wraps in 16 bits.
            uint16_t next   = static_cast<uint16_t>(bases[bi] + 2);
            uint16_t target = static_cast<uint16_t>(next + off);
            bool     cross  = (next >> 8) != (target >> 8);
            if (cross) crossed++;

            uint16_t landed = 0;
            uint8_t  got    = runBranch(b, true, bases[bi], static_cast<int8_t>(off), &landed);
            uint8_t  want   = cross ? 4 : 3;
            if (got != want || landed != target) {
                if (++wrong <= 8) {
                    printf("  FAIL %s:%d: bne at $%04x offset %+d cost %u landing at $%04x, "
                           "want %u landing at $%04x\n",
                           __FILE__, __LINE__, bases[bi], off, got, landed, want, target);
                }
            }

            // Not taken is always two, wherever it would have gone.
            if (runBranch(b, false, bases[bi], static_cast<int8_t>(off)) != 2) {
                if (++wrong <= 8) {
                    printf("  FAIL %s:%d: bne not taken at $%04x offset %+d did not cost 2\n", __FILE__, __LINE__,
                           bases[bi], off);
                }
            }
        }
    }

    checks++;
    if (wrong) {
        failures++;
        printf("  %d of %u branch offsets were wrong\n", wrong,
               (unsigned)(sizeof(bases) / sizeof(bases[0])) * 256 * 2);
    }
    // A sanity check on the check: if nothing crossed a page, the loop above
    // was not testing what it claims to.
    CHECK(crossed > 100, "only %d of the offsets crossed a page, so the sweep proves little", crossed);
}

// -------------------------------------------------- indexed addressing modes

enum Mode { ABSX, ABSY, INDY };

struct IndexedCase {
    uint8_t     opcode;
    const char *name;
    Mode        mode;
    uint8_t     base;     // cycles when the index does not carry
    bool        penalty;  // whether a carry costs one more
};

// Every opcode in the emulator that uses abs,X, abs,Y or (zp),Y, with the
// counts from the reference. The reads take the penalty; the writes and the
// read-modify-writes never do, because the chip always performs the dummy
// read and so always pays for it.
static const IndexedCase INDEXED[] = {
    // abs,X reads: 4 (+1)
    {0x1d, "ora abs,x", ABSX, 4, true},
    {0x3d, "and abs,x", ABSX, 4, true},
    {0x5d, "eor abs,x", ABSX, 4, true},
    {0x7d, "adc abs,x", ABSX, 4, true},
    {0xbc, "ldy abs,x", ABSX, 4, true},
    {0xbd, "lda abs,x", ABSX, 4, true},
    {0xdd, "cmp abs,x", ABSX, 4, true},
    {0xfd, "sbc abs,x", ABSX, 4, true},
    {0x1c, "nop abs,x", ABSX, 4, true},
    {0x3c, "nop abs,x", ABSX, 4, true},
    {0x5c, "nop abs,x", ABSX, 4, true},
    {0x7c, "nop abs,x", ABSX, 4, true},
    {0xdc, "nop abs,x", ABSX, 4, true},
    {0xfc, "nop abs,x", ABSX, 4, true},

    // abs,Y reads: 4 (+1)
    {0x19, "ora abs,y", ABSY, 4, true},
    {0x39, "and abs,y", ABSY, 4, true},
    {0x59, "eor abs,y", ABSY, 4, true},
    {0x79, "adc abs,y", ABSY, 4, true},
    {0xb9, "lda abs,y", ABSY, 4, true},
    {0xbe, "ldx abs,y", ABSY, 4, true},
    {0xbf, "lax abs,y", ABSY, 4, true},
    {0xd9, "cmp abs,y", ABSY, 4, true},
    {0xf9, "sbc abs,y", ABSY, 4, true},
    {0xbb, "las abs,y", ABSY, 4, true},

    // (zp),Y reads: 5 (+1)
    {0x11, "ora (zp),y", INDY, 5, true},
    {0x31, "and (zp),y", INDY, 5, true},
    {0x51, "eor (zp),y", INDY, 5, true},
    {0x71, "adc (zp),y", INDY, 5, true},
    {0xb1, "lda (zp),y", INDY, 5, true},
    {0xb3, "lax (zp),y", INDY, 5, true},
    {0xd1, "cmp (zp),y", INDY, 5, true},
    {0xf1, "sbc (zp),y", INDY, 5, true},

    // abs,X writes and read-modify-writes: flat
    {0x9d, "sta abs,x", ABSX, 5, false},
    {0x9c, "shy abs,x", ABSX, 5, false},
    {0x1e, "asl abs,x", ABSX, 7, false},
    {0x3e, "rol abs,x", ABSX, 7, false},
    {0x5e, "lsr abs,x", ABSX, 7, false},
    {0x7e, "ror abs,x", ABSX, 7, false},
    {0xde, "dec abs,x", ABSX, 7, false},
    {0xfe, "inc abs,x", ABSX, 7, false},
    {0x1f, "slo abs,x", ABSX, 7, false},
    {0x3f, "rla abs,x", ABSX, 7, false},
    {0x5f, "sre abs,x", ABSX, 7, false},
    {0x7f, "rra abs,x", ABSX, 7, false},
    {0xdf, "dcp abs,x", ABSX, 7, false},
    {0xff, "isb abs,x", ABSX, 7, false},

    // abs,Y writes and read-modify-writes: flat
    {0x99, "sta abs,y", ABSY, 5, false},
    {0x9b, "tas abs,y", ABSY, 5, false},
    {0x9e, "shx abs,y", ABSY, 5, false},
    {0x9f, "sha abs,y", ABSY, 5, false},
    {0x1b, "slo abs,y", ABSY, 7, false},
    {0x3b, "rla abs,y", ABSY, 7, false},
    {0x5b, "sre abs,y", ABSY, 7, false},
    {0x7b, "rra abs,y", ABSY, 7, false},
    {0xdb, "dcp abs,y", ABSY, 7, false},
    {0xfb, "isb abs,y", ABSY, 7, false},

    // (zp),Y writes and read-modify-writes: flat
    {0x91, "sta (zp),y", INDY, 6, false},
    {0x13, "slo (zp),y", INDY, 8, false},
    {0x33, "rla (zp),y", INDY, 8, false},
    {0x53, "sre (zp),y", INDY, 8, false},
    {0x73, "rra (zp),y", INDY, 8, false},
    {0xd3, "dcp (zp),y", INDY, 8, false},
    {0xf3, "isb (zp),y", INDY, 8, false},
};

// Assembles one indexed instruction at CODE against `target` and runs it. The
// index is always $10, so a target of $2000 stays inside the page and a target
// of $20f8 carries into $2100.
static uint8_t runIndexed(const IndexedCase &c, uint16_t target)
{
    TestCpu cpu;
    cpu.begin(CODE);
    cpu.setA(0x55);
    cpu.setX(IDX);
    cpu.setY(IDX);
    cpu.mem[CODE] = c.opcode;
    if (c.mode == INDY) {
        cpu.mem[CODE + 1] = PTR;
        cpu.mem[PTR]      = target & 0xff;
        cpu.mem[PTR + 1]  = target >> 8;
    } else {
        cpu.mem[CODE + 1] = target & 0xff;
        cpu.mem[CODE + 2] = target >> 8;
    }
    return cpu.step();
}

static void testIndexedCycles()
{
    printf("cycles: an indexed read pays for a carry, a write never does\n");

    for (unsigned i = 0; i < sizeof(INDEXED) / sizeof(INDEXED[0]); i++) {
        const IndexedCase &c       = INDEXED[i];
        uint8_t            inPage  = runIndexed(c, DATA);
        uint8_t            crossed = runIndexed(c, DATAEND);
        uint8_t            want    = c.base + (c.penalty ? 1 : 0);

        CHECK(inPage == c.base, "$%02x %s inside a page cost %u, want %u", c.opcode, c.name, inPage, c.base);
        CHECK(crossed == want, "$%02x %s across a page cost %u, want %u", c.opcode, c.name, crossed, want);
        if (!c.penalty) {
            CHECK(inPage == crossed,
                  "$%02x %s changed cost across a page (%u then %u); writes and read-modify-writes "
                  "always do the dummy read, so they never vary",
                  c.opcode, c.name, inPage, crossed);
        }
    }

    // The penalty follows the carry out of the low byte, not whether the two
    // addresses happen to look different. $20fe + 1 stays, $20ff + 1 does not.
    TestCpu cpu;
    for (int i = 0; i < 2; i++) {
        uint16_t target = i ? 0x20ff : 0x20fe;
        cpu.begin(CODE);
        cpu.setX(1);
        cpu.mem[CODE]     = 0xbd;  // lda abs,x
        cpu.mem[CODE + 1] = target & 0xff;
        cpu.mem[CODE + 2] = target >> 8;
        uint8_t got       = cpu.step();
        CHECK(got == (i ? 5 : 4), "lda $%04x,x with x=1 cost %u, want %u", target, got, i ? 5 : 4);
    }

    // A carry changes what the instruction costs, never where it reads.
    cpu.begin(CODE);
    cpu.setX(IDX);
    cpu.mem[CODE]     = 0xbd;  // lda abs,x
    cpu.mem[CODE + 1] = DATAEND & 0xff;
    cpu.mem[CODE + 2] = DATAEND >> 8;
    cpu.mem[0x2108]   = 0x42;
    uint8_t got       = cpu.step();
    CHECK(got == 5, "lda $20f8,x with x=$10 cost %u, want 5", got);
    CHECK(cpu.getA() == 0x42, "it read $%02x, so it did not fetch from $2108", cpu.getA());

    // An index big enough to wrap the address space still costs only the one
    // extra cycle.
    cpu.begin(CODE);
    cpu.setY(0xff);
    cpu.mem[CODE]     = 0xb9;  // lda abs,y
    cpu.mem[CODE + 1] = 0xff;
    cpu.mem[CODE + 2] = 0xff;
    got               = cpu.step();
    CHECK(got == 5, "lda $ffff,y with y=$ff cost %u, want 5", got);

    // The flag is per instruction, not sticky: an instruction that crosses a
    // page must not leave the next one paying for it.
    cpu.begin(CODE);
    cpu.setX(IDX);
    cpu.mem[CODE]     = 0xbd;  // lda $20f8,x  -> crosses
    cpu.mem[CODE + 1] = DATAEND & 0xff;
    cpu.mem[CODE + 2] = DATAEND >> 8;
    cpu.mem[CODE + 3] = 0xbd;  // lda $2000,x  -> does not
    cpu.mem[CODE + 4] = DATA & 0xff;
    cpu.mem[CODE + 5] = DATA >> 8;
    CHECK(cpu.step() == 5, "the crossing read did not cost 5");
    got = cpu.step();
    CHECK(got == 4, "the read after a crossing one cost %u, want 4; the flag is stale", got);
}

// ------------------------------------------------------- flat count repairs

static void testFlatCounts()
{
    printf("cycles: the counts that were simply wrong\n");
    TestCpu cpu;
    uint8_t got;

    // php was charging nothing at all. On the C64 side that made it free,
    // while the drive's "every instruction spends at least one cycle" clamp
    // charged it one, so the same instruction cost the two machines different
    // amounts and they drifted apart. Both sides of a handshake have to agree.
    cpu.begin(CODE);
    cpu.mem[CODE] = 0x08;
    got           = cpu.step();
    CHECK(got == 3, "php cost %u, want 3", got);

    cpu.begin(CODE);
    cpu.mem[CODE]     = 0x05;  // ora zp
    cpu.mem[CODE + 1] = 0x40;
    got               = cpu.step();
    CHECK(got == 3, "ora zp cost %u, want 3", got);

    cpu.begin(CODE);
    cpu.mem[CODE]     = 0x09;  // ora #
    cpu.mem[CODE + 1] = 0x0f;
    got               = cpu.step();
    CHECK(got == 2, "ora # cost %u, want 2", got);

    // Every other instruction of the same two shapes, to show the pair above
    // were isolated typos rather than a pattern.
    const uint8_t zpops[] = {0x25, 0x45, 0x65, 0xa5, 0xc5, 0xe5};  // and eor adc lda cmp sbc
    for (unsigned i = 0; i < sizeof(zpops); i++) {
        cpu.begin(CODE);
        cpu.mem[CODE]     = zpops[i];
        cpu.mem[CODE + 1] = 0x40;
        got               = cpu.step();
        CHECK(got == 3, "$%02x zp cost %u, want 3", zpops[i], got);
    }
    const uint8_t immops[] = {0x29, 0x49, 0x69, 0xa9, 0xc9, 0xe9};
    for (unsigned i = 0; i < sizeof(immops); i++) {
        cpu.begin(CODE);
        cpu.mem[CODE]     = immops[i];
        cpu.mem[CODE + 1] = 0x0f;
        got               = cpu.step();
        CHECK(got == 2, "$%02x # cost %u, want 2", immops[i], got);
    }

    // The other stack instructions, which php sits between.
    struct {
        uint8_t op;
        uint8_t cost;
        const char *name;
    } stack[] = {{0x48, 3, "pha"}, {0x68, 4, "pla"}, {0x28, 4, "plp"}};
    for (unsigned i = 0; i < sizeof(stack) / sizeof(stack[0]); i++) {
        cpu.begin(CODE);
        cpu.mem[CODE] = stack[i].op;
        got           = cpu.step();
        CHECK(got == stack[i].cost, "%s cost %u, want %u", stack[i].name, got, stack[i].cost);
    }

    // Nothing except the JAM opcodes may be free. A zero cost instruction lets
    // the interleaving between the C64 and the drive spin without either
    // machine making progress, which is what the drive's clamp was hiding.
    for (int op = 0; op < 256; op++) {
        cpu.begin(CODE);
        memset(cpu.mem + CODE, 0, 4);
        cpu.mem[CODE]    = static_cast<uint8_t>(op);
        cpu.mem[PTR]     = DATA & 0xff;
        cpu.mem[PTR + 1] = DATA >> 8;
        got              = cpu.step();
        if (got == 0) {
            CHECK(cpu.jammed(), "opcode $%02x cost nothing and did not jam", op);
        } else {
            CHECK(got >= 2 && got <= 8, "opcode $%02x cost %u, which is outside 2..8", op, got);
        }
    }
}

// -------------------------------------------------------------- interrupts

static void testInterruptCycles()
{
    printf("cycles: entering an interrupt costs 7, and brk still costs 7\n");
    TestCpu cpu;

    // Pushing the return address and the status and reading the vector takes
    // seven cycles whether a hardware line raised it or brk did. Charging
    // nothing shifted the phase of every raster and CIA interrupt.
    cpu.begin(CODE);
    cpu.numofcycles = 0;
    cpu.enterInterrupt(0x9000, false);
    CHECK(cpu.numofcycles == 7, "a hardware interrupt entry cost %u, want 7", cpu.numofcycles);

    // brk goes through that same routine, so it must not add its own seven on
    // top or it would come to fourteen.
    cpu.begin(CODE);
    cpu.mem[CODE]   = 0x00;
    cpu.mem[0xfffe] = 0x00;
    cpu.mem[0xffff] = 0x90;
    uint8_t got     = cpu.step();
    CHECK(got == 7, "brk cost %u, want 7", got);
    CHECK(cpu.currentPc() == 0x9000, "brk went to $%04x, want $9000", cpu.currentPc());
}

// ---------------------------------------------------------- jmp (indirect)

static void testJmpIndirectWrap()
{
    printf("cycles: jmp (indirect) costs 5 and wraps inside its own page\n");
    TestCpu cpu;
    uint8_t got;

    cpu.begin(CODE);
    cpu.mem[CODE]     = 0x6c;
    cpu.mem[CODE + 1] = 0x00;
    cpu.mem[CODE + 2] = 0x30;
    cpu.mem[0x3000]   = 0x34;
    cpu.mem[0x3001]   = 0x12;
    got               = cpu.step();
    CHECK(got == 5, "jmp (indirect) cost %u, want 5", got);
    CHECK(cpu.currentPc() == 0x1234, "jmp ($3000) landed at $%04x, want $1234", cpu.currentPc());

    // The chip's own bug: a pointer at $xxff reads the low byte from $30ff and
    // the high byte from $3000, not from $3100, because the increment never
    // leaves the page. The emulator reproduces it and code in the wild leans
    // on it, so pin it down before someone tidies it away.
    cpu.begin(CODE);
    cpu.mem[CODE]     = 0x6c;
    cpu.mem[CODE + 1] = 0xff;
    cpu.mem[CODE + 2] = 0x30;
    cpu.mem[0x30ff]   = 0x78;
    cpu.mem[0x3000]   = 0x56;  // where the chip really looks
    cpu.mem[0x3100]   = 0xaa;  // where a fixed chip would look
    got               = cpu.step();
    CHECK(got == 5, "jmp ($30ff) cost %u, want 5", got);
    CHECK(cpu.currentPc() == 0x5678, "jmp ($30ff) landed at $%04x, want $5678; the page did not wrap",
          cpu.currentPc());
}

// ------------------------------------------------------------------ totals

// A short run of real code, counted by hand against the reference. A whole
// loop catches an error that the per-instruction checks would each let past,
// and this is the shape a loader uses: it crosses a page on the read and it
// branches back over one.
static void testLoopTotal()
{
    printf("cycles: a whole loop adds up\n");
    TestCpu cpu;
    cpu.begin(0x10f0);

    //   $10f0  ldy #$00      2
    //   $10f2  lda $20f8,y   4, and 5 once $f8 + y carries
    //   $10f5  sta $3000,y   5 always, carry or not
    //   $10f8  iny           2
    //   $10f9  cpy #$10      2
    //   $10fb  bne $10f2     3 taken; $10fd back to $10f2 stays in page $10
    //                        2 on the pass that falls through
    uint8_t code[] = {0xa0, 0x00, 0xb9, 0xf8, 0x20, 0x99, 0x00, 0x30, 0xc8, 0xc0, 0x10, 0xd0, 0xf5};
    memcpy(cpu.mem + 0x10f0, code, sizeof(code));
    for (int i = 0; i < 16; i++) {
        cpu.mem[0x20f8 + i] = 0xa0 + i;
    }

    unsigned total = 0;
    int      steps = 0;
    while (cpu.currentPc() != 0x10fd && steps < 200) {
        total += cpu.step();
        steps++;
    }

    // ldy is 2. Then sixteen passes of lda + sta + iny + cpy + bne, which is
    // 4 + 5 + 2 + 2 + 3 = 16 each. y runs 0 to 15 at the lda and $f8 + y
    // carries from y = 8, so eight of those passes pay one more. The last bne
    // falls through, which is 2 rather than 3.
    unsigned want = 2 + 16 * 16 + 8 - 1;
    CHECK(steps == 1 + 16 * 5, "the loop ran %d instructions, want %d", steps, 1 + 16 * 5);
    CHECK(total == want, "the loop cost %u cycles, want %u", total, want);
    for (int i = 0; i < 16; i++) {
        CHECK(cpu.mem[0x3000 + i] == 0xa0 + i, "the loop copied $%02x to $%04x, want $%02x",
              cpu.mem[0x3000 + i], 0x3000 + i, 0xa0 + i);
    }
}

int main()
{
    printf("6502 cycle counts, checked against masswerk.at/6502/6502_instruction_set.html\n\n");

    testBranchCycles();
    testBranchTargetsExhaustively();
    testIndexedCycles();
    testFlatCounts();
    testInterruptCycles();
    testJmpIndirectWrap();
    testLoopTotal();

    printf("\n%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
