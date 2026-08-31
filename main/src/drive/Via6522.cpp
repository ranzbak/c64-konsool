#include "Via6522.hpp"

void Via6522::reset()
{
    pra     = 0;
    ddra    = 0;
    prb     = 0;
    ddrb    = 0;
    t1c     = 0;
    t1l     = 0;
    t2c     = 0;
    t2ll    = 0;
    sr      = 0;
    acr     = 0;
    pcr     = 0;
    ifr     = 0;
    ier     = 0;
    t1Fired = false;
    ca1     = false;
}

void Via6522::setCa1(bool level)
{
    if (level == ca1) return;
    ca1 = level;

    // Only the edge the PCR asks for latches an interrupt.
    bool wantRising = (pcr & 0x01) != 0;
    if (level == wantRising) {
        ifr |= IRQ_CA1;
        updateIrqFlag();
    }
}

void Via6522::updateIrqFlag()
{
    if ((ifr & ier & 0x7f) != 0) {
        ifr |= IRQ_ANY;
    } else {
        ifr = static_cast<uint8_t>(ifr & 0x7f);
    }
}

void Via6522::countTimers(unsigned int cycles)
{
    // Timer 1. In free running mode it reloads from the latch and keeps
    // going; in one shot mode it only raises its flag the first time.
    if (t1c <= cycles) {
        bool freeRunning = (acr & 0x40) != 0;
        if (freeRunning) {
            uint16_t period = static_cast<uint16_t>(t1l + 1);
            if (period == 0) period = 1;
            unsigned int overshoot  = cycles - t1c;
            t1c                     = static_cast<uint16_t>(period - (overshoot % period));
            ifr                    |= IRQ_T1;
        } else {
            t1c = static_cast<uint16_t>(t1c - cycles);
            // One shot: the counter carries on wrapping, but the flag is only
            // raised the first time round.
            if (!t1Fired) {
                t1Fired  = true;
                ifr     |= IRQ_T1;
            }
        }
    } else {
        t1c = static_cast<uint16_t>(t1c - cycles);
    }

    // Timer 2 always counts down and only flags once per load.
    if (t2c <= cycles) {
        ifr |= IRQ_T2;
    }
    t2c = static_cast<uint16_t>(t2c - cycles);

    updateIrqFlag();
}

uint8_t Via6522::read(uint8_t reg, uint8_t portAInput, uint8_t portBInput)
{
    switch (reg & 0x0f) {
        case REG_PRB:
            // Output bits read back what was written, input bits read the pins.
            ifr = static_cast<uint8_t>(ifr & ~IRQ_CB1);
            updateIrqFlag();
            return static_cast<uint8_t>((prb & ddrb) | (portBInput & ~ddrb));

        case REG_PRA:
            ifr = static_cast<uint8_t>(ifr & ~IRQ_CA1);
            updateIrqFlag();
            return static_cast<uint8_t>((pra & ddra) | (portAInput & ~ddra));

        case REG_PRA_NH:
            return static_cast<uint8_t>((pra & ddra) | (portAInput & ~ddra));

        case REG_DDRB:
            return ddrb;
        case REG_DDRA:
            return ddra;

        case REG_T1CL:
            // Reading the low byte clears the timer 1 flag.
            ifr = static_cast<uint8_t>(ifr & ~IRQ_T1);
            updateIrqFlag();
            return static_cast<uint8_t>(t1c & 0xff);
        case REG_T1CH:
            return static_cast<uint8_t>(t1c >> 8);
        case REG_T1LL:
            return static_cast<uint8_t>(t1l & 0xff);
        case REG_T1LH:
            return static_cast<uint8_t>(t1l >> 8);

        case REG_T2CL:
            ifr = static_cast<uint8_t>(ifr & ~IRQ_T2);
            updateIrqFlag();
            return static_cast<uint8_t>(t2c & 0xff);
        case REG_T2CH:
            return static_cast<uint8_t>(t2c >> 8);

        case REG_SR:
            return sr;
        case REG_ACR:
            return acr;
        case REG_PCR:
            return pcr;
        case REG_IFR:
            return ifr;
        case REG_IER:
            return static_cast<uint8_t>(ier | 0x80);
        default:
            return 0;
    }
}

void Via6522::write(uint8_t reg, uint8_t value)
{
    switch (reg & 0x0f) {
        case REG_PRB:
            prb = value;
            ifr = static_cast<uint8_t>(ifr & ~IRQ_CB1);
            updateIrqFlag();
            break;

        case REG_PRA:
        case REG_PRA_NH:
            pra = value;
            ifr = static_cast<uint8_t>(ifr & ~IRQ_CA1);
            updateIrqFlag();
            break;

        case REG_DDRB:
            ddrb = value;
            break;
        case REG_DDRA:
            ddra = value;
            break;

        case REG_T1CL:
        case REG_T1LL:
            t1l = static_cast<uint16_t>((t1l & 0xff00) | value);
            break;

        case REG_T1CH:
            // Writing the high byte loads the counter from the latch and
            // clears the pending interrupt.
            t1l     = static_cast<uint16_t>((t1l & 0x00ff) | (value << 8));
            t1c     = t1l;
            t1Fired = false;
            ifr     = static_cast<uint8_t>(ifr & ~IRQ_T1);
            updateIrqFlag();
            break;

        case REG_T1LH:
            t1l = static_cast<uint16_t>((t1l & 0x00ff) | (value << 8));
            ifr = static_cast<uint8_t>(ifr & ~IRQ_T1);
            updateIrqFlag();
            break;

        case REG_T2CL:
            t2ll = value;
            break;

        case REG_T2CH:
            t2c = static_cast<uint16_t>((value << 8) | t2ll);
            ifr = static_cast<uint8_t>(ifr & ~IRQ_T2);
            updateIrqFlag();
            break;

        case REG_SR:
            sr = value;
            break;
        case REG_ACR:
            acr = value;
            break;
        case REG_PCR:
            pcr = value;
            break;

        case REG_IFR:
            // Writing a one clears that flag.
            ifr = static_cast<uint8_t>(ifr & ~(value & 0x7f));
            updateIrqFlag();
            break;

        case REG_IER:
            if (value & 0x80) {
                ier = static_cast<uint8_t>(ier | (value & 0x7f));
            } else {
                ier = static_cast<uint8_t>(ier & ~(value & 0x7f));
            }
            updateIrqFlag();
            break;

        default:
            break;
    }
}
