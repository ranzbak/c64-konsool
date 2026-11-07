/*
 The serial port behaviour in VIA 1, in particular the gate that pulls DATA
 low when the attention acknowledge does not match the state of ATN, follows
 Frodo's CPU1541.cpp.

 Frodo (C) 1994-1997, 2002 Christian Bauer, GPL version 2 or later.
*/
#include "Drive1541.hpp"
#include <cstring>

// VIA 1 port B, the serial bus. The bus drivers invert, so a line held low
// reads back as a one here, and writing a one pulls a line low.
static const uint8_t VIA1_DATA_IN  = 0x01;
static const uint8_t VIA1_DATA_OUT = 0x02;
static const uint8_t VIA1_CLK_IN   = 0x04;
static const uint8_t VIA1_CLK_OUT  = 0x08;
static const uint8_t VIA1_ATNA     = 0x10;
static const uint8_t VIA1_ATN_IN   = 0x80;

// VIA 2 port B, the disk hardware.
static const uint8_t VIA2_STEP_MASK = 0x03;
static const uint8_t VIA2_MOTOR     = 0x04;
static const uint8_t VIA2_LED       = 0x08;
static const uint8_t VIA2_SYNC      = 0x80;

Drive1541::Drive1541()
{
    memset(ram, 0, sizeof(ram));
}

void Drive1541::setRom(uint8_t* romImage)
{
    rom       = romImage;
    romLoaded = (romImage != nullptr);
}

void Drive1541::reset()
{
    memset(ram, 0, sizeof(ram));
    via1.reset();
    via2.reset();
    // reset() flushes anything pending before parking the head.
    controller.reset();
    lastStepperPhase = 0;
    stepperAligned   = false;
    stepperSkew      = 0;
    byteReadyCycles  = 0;
    headReadThisByte = false;
    headByte         = 0;
    cpuhalted        = false;

    if (lines != nullptr) {
        lines->driveClk  = false;
        lines->driveData = false;
    }

    if (romLoaded) {
        // The reset vector lives at the very top of the ROM.
        pc = static_cast<uint16_t>(rom[ROM_SIZE - 4] | (rom[ROM_SIZE - 3] << 8));
    }
    sp    = 0xFF;
    iflag = true;
}

// Works out what the drive is pulling low, and pushes it onto the shared bus.
void Drive1541::updateIecOutputs()
{
    if (lines == nullptr) return;

    // Only bits configured as outputs drive the bus.
    uint8_t out = static_cast<uint8_t>(via1.prb & via1.ddrb);

    bool clkOut  = (out & VIA1_CLK_OUT) != 0;
    bool dataOut = (out & VIA1_DATA_OUT) != 0;
    bool atna    = (out & VIA1_ATNA) != 0;

    // The attention acknowledge gate: DATA is pulled low whenever the
    // acknowledge does not agree with the state of ATN. That is what holds
    // DATA down until the drive has noticed an ATN it was sent.
    lines->driveClk  = clkOut;
    lines->driveData = dataOut || (atna != lines->atnLow());

    // ATN also runs to VIA 1's CA1 pin. The gate above answers the bus on its
    // own, but it is this interrupt that gets the DOS out of its idle loop to
    // find out what it was called for.
    via1.setCa1(lines->atnLow());
}

// The stepper turns through four phases; which way it went tells the head
// whether to move in or out.
void Drive1541::updateStepper(uint8_t portB)
{
    // The stepper only turns while the motor is running, and its position is
    // the head's own position rather than a phase remembered from the last
    // write: on real hardware the two cannot disagree, because the head sits
    // wherever the energised coil holds it. VICE works it out the same way
    // (src/drive/iecieee/via2d.c, store_prb):
    //
    //     old  = (current_half_track - 2) & 3
    //     step = (new - old) & 3, where 3 means -1
    //
    // Energising the coil two places round is ambiguous -- the head is pulled
    // equally both ways -- so it stays where it is.
    //
    // That last rule needs the two to start out agreeing, which is the one
    // thing this emulator has to arrange and hardware gets for free. The head
    // is placed on track 18 at reset while the DOS holds phase zero, and those
    // disagree by exactly two, so every write would be the ambiguous case and
    // the head could never move at all. Line them up on the first write
    // instead of stepping on it.
    if ((portB & VIA2_MOTOR) == 0) return;

    int newPos = portB & VIA2_STEP_MASK;
    if (!stepperAligned) {
        stepperAligned = true;
        stepperSkew    = static_cast<uint8_t>((newPos - static_cast<int>(controller.currentHalfTrack() - 2)) & VIA2_STEP_MASK);
        return;
    }

    int oldPos = static_cast<int>(controller.currentHalfTrack() - 2 + stepperSkew) & VIA2_STEP_MASK;
    int step   = (newPos - oldPos) & VIA2_STEP_MASK;
    if (step == 3) step = -1;

    if (step == 1) {
        controller.moveHeadIn();
    } else if (step == -1) {
        controller.moveHeadOut();
    }
}

uint8_t Drive1541::readVia1(uint8_t reg)
{
    uint8_t input = 0;
    if (lines != nullptr) {
        if (lines->dataLow()) input |= VIA1_DATA_IN;
        if (lines->clkLow()) input |= VIA1_CLK_IN;
        if (lines->atnLow()) input |= VIA1_ATN_IN;
    }
    // Port B bits 5 and 6 are the device address jumpers; both low is
    // device 8.
    return via1.read(reg, 0xff, input);
}

uint8_t Drive1541::readVia2(uint8_t reg)
{
    switch (reg & 0x0f) {
        case Via6522::REG_PRA:
        case Via6522::REG_PRA_NH:
            // Port A is the head. The byte sits there until the next one
            // arrives, so reading the port again inside the same byte time
            // hands back the same byte rather than pulling the next one off
            // the track. Letting every read take a byte lets the head outrun
            // the disk: the DOS polls this port while it hunts for a sync
            // mark, far faster than bytes actually arrive, and the head then
            // races past every other sector.
            if (!headReadThisByte) {
                headReadThisByte = true;
                headByte         = controller.readGcrByte();
            }
            return headByte;

        case Via6522::REG_PRB: {
            // Sync is active low, and the write protect sense sits alongside.
            uint8_t input = static_cast<uint8_t>(controller.writeProtectBit());
            if (!controller.syncFound()) input |= VIA2_SYNC;
            return via2.read(reg, 0xff, input);
        }

        default:
            return via2.read(reg, 0xff, 0xff);
    }
}

void Drive1541::writeVia1(uint8_t reg, uint8_t value)
{
    via1.write(reg, value);
    uint8_t r = reg & 0x0f;
    if (r == Via6522::REG_PRB || r == Via6522::REG_DDRB) {
        updateIecOutputs();
    }
}

void Drive1541::writeVia2(uint8_t reg, uint8_t value)
{
    uint8_t r = reg & 0x0f;

    // Port A is the head. With the port driving, writing it puts a byte onto
    // the track; the drive only ever drives the whole port, since the head
    // reads or writes a byte at a time and nothing else is wired to it.
    if ((r == Via6522::REG_PRA || r == Via6522::REG_PRA_NH) && via2.ddra == 0xff) {
        controller.writeGcrByte(value);
    }

    via2.write(reg, value);

    if (r == Via6522::REG_PRB || r == Via6522::REG_DDRB) {
        updateStepper(via2.prb);

        // The motor stopping is the drive saying it has finished with this
        // track, so anything written gets pushed out rather than waiting for
        // the head to step.
        bool motorOn = (via2.prb & via2.ddrb & VIA2_MOTOR) != 0;
        if (!motorOn) {
            controller.flush();
        }
    }
}

uint8_t Drive1541::getMem(uint16_t addr)
{
    if (addr < 0x1800) {
        // 2K of RAM, mirrored through the bottom of the map.
        return ram[addr & (RAM_SIZE - 1)];
    }
    if (addr < 0x1c00) {
        return readVia1(static_cast<uint8_t>(addr & 0x0f));
    }
    if (addr < 0x2000) {
        return readVia2(static_cast<uint8_t>(addr & 0x0f));
    }
    if (addr >= 0xc000 && rom != nullptr) {
        return rom[addr - 0xc000];
    }
    // Nothing is mapped in the middle of the drive's address space.
    return 0;
}

void Drive1541::setMem(uint16_t addr, uint8_t val)
{
    if (addr < 0x1800) {
        ram[addr & (RAM_SIZE - 1)] = val;
        return;
    }
    if (addr < 0x1c00) {
        writeVia1(static_cast<uint8_t>(addr & 0x0f), val);
        return;
    }
    if (addr < 0x2000) {
        writeVia2(static_cast<uint8_t>(addr & 0x0f), val);
        return;
    }
    // ROM and unmapped space ignore writes.
}

void Drive1541::countTimers(unsigned int cycles)
{
    via1.countTimers(cycles);
    via2.countTimers(cycles);
}

void Drive1541::refreshIecOutputs()
{
    updateIecOutputs();
}

unsigned int Drive1541::stepInstruction()
{
    if (!romLoaded) return 1;

    // Sampled before the interrupt check, not after: taking an interrupt costs
    // seven cycles of its own and the head has to turn for those too.
    uint8_t before = numofcycles;

    // An interrupt from either VIA wakes the drive.
    if (!iflag && (via1.irqAsserted() || via2.irqAsserted())) {
        setPCToIntVec(static_cast<uint16_t>(getMem(0xfffe) | (getMem(0xffff) << 8)), false);
    }

    execute(getMem(pc++));
    uint8_t used = static_cast<uint8_t>(numofcycles - before);
    if (used == 0) used = 1;

    countByteReady(used);
    return used;
}

// The head shifts a bit at a time and the controller pulses BYTE READY on
// every eighth one. That line is wired to the 6502's SO pin, which sets the
// overflow flag from outside the instruction stream, and it is the only way
// the DOS knows a byte has arrived: it waits with "BVC *", clears the flag
// with CLV and then reads the head. Without it the drive sits in that branch
// forever the first time it touches the disk, which is exactly what a LOAD
// does once the command has been sent.
void Drive1541::countByteReady(unsigned int cycles)
{
    // The swap runs on wall clock, not on the head, so it keeps going whether
    // or not there is a disk to turn.
    controller.countChange(cycles);

    // Nothing turns under the head unless there is a disk and the motor is on.
    if (!controller.hasDisk()) return;
    if ((via2.prb & via2.ddrb & VIA2_MOTOR) == 0) return;


    // One byte is eight bit cells, and the bit rate is what the speed zone
    // selects: the outer tracks hold more, so their bytes come round sooner.
    unsigned int perByte = gcrZoneCyclesPerByte(controller.speedZone());

    byteReadyCycles += cycles;
    while (byteReadyCycles >= perByte) {
        byteReadyCycles -= perByte;
        // A byte the DOS never collected still passes under the head.
        if (!headReadThisByte) controller.rotate();
        headReadThisByte = false;

        // BYTE READY only reaches the SO pin while the DOS holds the gate
        // open. That gate is VIA 2's CA2, which the peripheral control
        // register drives high with bits 3 to 1 set; the DOS opens it around
        // the few instructions that take a byte off the head and shuts it
        // again straight after, because an overflow flag arriving unasked
        // ruins every other branch it makes. The gate stops the flag, not the
        // disk: the motor keeps turning either way, and stopping the rotation
        // with it means a sync mark never comes round and every read ends in
        // "no sync found".
        //
        // The hardware also holds the shift register while a sync mark is
        // passing, so no byte is ever handed over from inside one. That is
        // what makes the first byte after a sync the header mark the DOS
        // compares against.
        if ((via2.pcr & 0x0e) == 0x0e && !controller.syncFound()) vflag = true;
    }
}

unsigned int Drive1541::emulateCycles(unsigned int cycles)
{
    if (!romLoaded) return cycles;

    // numofcycles is only a byte, so the budget is counted separately: a
    // caller asking for more than 255 cycles would otherwise wrap and spin.
    unsigned int spent = 0;
    while (spent < cycles) {
        spent += stepInstruction();
    }
    return spent;
}

void Drive1541::run()
{
    // The drive is stepped from the C64's loop rather than running a loop of
    // its own, so there is nothing to do here.
}
