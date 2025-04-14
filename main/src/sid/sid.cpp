// Based of cSID light - an attempt at a usable simple API

// cSID by Hermit (Mihaly Horvath), (Year 2017) http://hermit.sidrip.com
// (based on jsSID, this version has much lower CPU-usage, as mainloop runs at samplerate)
// License: WTF - Do what the fuck you want with this code, but please mention me as its original author.
#include "sid.hpp"
#include <cmath>
#include <cstdint>
#include "Config.hpp"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/idf_additions.h"
// #include "precalc.h"

typedef uint8_t byte;
// //----------------------------- SID emulation
// -----------------------------------------

// Arrays to support the emulation:

#define PERIOD0 CLOCK_RATIO_DEFAULT  // max(round(clock_ratio),9)
#define STEP0   3                    // ceil(PERIOD0/9.0)
uint32_t TriSaw_8580[4096], PulseSaw_8580[4096], PulseTriSaw_8580[4096];
float ADSRperiods[16] = {PERIOD0, 32, 63, 95, 149, 220, 267, 313, 392, 977, 1954, 3126, 3907, 11720, 19532, 31251};
byte  ADSRstep[16]    = {STEP0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};

const byte ADSR_exptable[256] = {
    1, 30, 30, 30, 30, 30, 30, 16, 16, 16, 16, 16, 16, 16, 16, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 4, 4, 4, 4,
    4,  // pos0:1  pos6:30  pos14:16
        // pos26:8
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1, 1,  // pos54:4 //pos93:2
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1};

SID::SID()
{
}

void SID::cSID_init(int32_t samplerate)
{
    int i;
    clock_ratio = C64_PAL_CPUCLK / samplerate;
    if (clock_ratio > 9) {
        ADSRperiods[0] = clock_ratio;
        ADSRstep[0]    = ceil(clock_ratio / 9.0);
    } else {
        ADSRperiods[0] = 9.0;
        ADSRstep[0]    = 1;
    }
    cutoff_ratio_8580 = -2 * 3.14 * (12500.0 / 2048) /
                        samplerate;  // -2 * 3.14 * ((82000/6.8) / 2048) / samplerate; //approx. 30Hz..12kHz according
                                     // to datasheet, but only for 6.8nF value, 22nF makes 9Hz...3.7kHz? wrong
    cap_6581_reciprocal = -1000000 / CAP_6581;  // lighten CPU-load in sample-callback
    cutoff_steepness_6581 =
        FILTER_DARKNESS_6581 *
        (2048.0 - VCR_FET_TRESHOLD);  // pre-scale for 0...2048 cutoff-value range //lighten CPU-load in sample-callback

    // cutoff_bottom_6581 = 1 - exp( -1 / (0.000000000470*1500000) / samplerate ); // 1 - exp( -2 * 3.14 *
    // (26000/pow(2,9)/0.47) / samplerate ); //around 140..220Hz cutoff is set by VCR-MOSFET limiter/shunt-resistor
    // (1.5MOhm) cutoff_top_6581 = 20000; //Hz // (26000/0.47);  // 1 - exp( -2 * 3.14 * (26000/0.47) / samplerate);

    // //cutoff range is 9 octaves stated by datasheet, but process variation might eliminate any filter spec.
    // cutoff_ratio_6581 = -2 * 3.14 * (cutoff_top_6581 / 2048) / samplerate;
    // //(cutoff_top_6581-cutoff_bottom_6581)/(2048.0-192.0); //datasheet: 30Hz..12kHz with 2.2pF -> 140Hz..56kHz with
    // 470pF?

    // createCombinedWF(TriSaw_8580, 0.8, 2.4, 0.64);
    // createCombinedWF(PulseSaw_8580, 1.4, 1.9, 0.68);
    // createCombinedWF(PulseTriSaw_8580, 0.8, 2.5, 0.64);

    for (i = 0; i < 9; i++) {
        ADSRstate[i]  = HOLDZERO_BITMASK;
        envcnt[i]     = 0;
        ratecnt[i]    = 0;
        phaseaccu[i]  = 0;
        prevaccu[i]   = 0;
        expcnt[i]     = 0;
        prevSR[i]     = 0;
        noise_LFSR[i] = 0x7FFFFF;
        prevwfout[i]  = 0;
    }

    for (i = 0; i < 3; i++) {
        sourceMSBrise[i] = 0;
        sourceMSB[i]     = 0;
        prevlowpass[i]   = 0;
        prevbandpass[i]  = 0;
    }
}

// registers: 0:freql1  1:freqh1  2:pwml1  3:pwmh1  4:ctrl1  5:ad1   6:sr1
// 7:freql2  8:freqh2  9:pwml2 10:pwmh2 11:ctrl2 12:ad2  13:sr 14:freql3
// 15:freqh3 16:pwml3 17:pwmh3 18:ctrl3 19:ad3  20:sr3
//            21:cutoffl 22:cutoffh 23:flsw_reso 24:vol_ftype 25:potX 26:potY
//            27:OSC3 28:ENV3
void SID::init(uint8_t* memory, AudioCallback audio_callback, int SID_model)
{
    // When more SID chips are emulated, scale down more
    // if (SIDamount == 2) {
    //     OUTPUT_SCALEDOWN /= 0.6;
    // } else if (SIDamount >= 3) {
    //     OUTPUT_SCALEDOWN /= 0.4;
    // }

    ESP_LOGI("SID", "Initializing SID with model %d", SID_model);
    cSID_init();
    // Set C64 memory pointer
    if (memory == nullptr) {
        ESP_LOGE("SID", "C64 memory not provided");
        return;
    }
    this->memory       = memory;
    // Setup the SID model
    this->SID_model[0] = this->SID_model[1] = this->SID_model[2] = SID_model;
    // Setup the sample out callback
    this->audio_callback                                         = audio_callback;

    // Internal SID state reset
    for (int i = 0; i < 9; i++) {
        ADSRstate[i] = HOLDZERO_BITMASK;
        ratecnt[i] = envcnt[i] = expcnt[i] = 0;
    }
}

// Run the SID cycles the correct amount te keep in sync with the scan lines
void SID::raster_line()
{
    scan_line_sync += (float)SAMPLES_PER_SCAN_LINE;
    // scan_line_sync    += nr_samples;
    while (scan_line_sync > 1.0) {
        sample_buffer[sample_buffer_pos++] = cycle(0, 0);

        // When the buffer is full, send the samples to the audio callback
        if (sample_buffer_pos == SAMPLE_BUFFER_SIZE) {
            audio_callback(sample_buffer, sample_buffer_pos);
            sample_buffer_pos = 0;
        }

        scan_line_sync -= 1.0;
    }
}

// My SID implementation is similar to what I worked out in a SwinSID variant during 3..4 months of development. (So
// jsSID only took 2 weeks armed with this experience.) I learned the workings of ADSR/WAVE/filter operations mainly
// from the quite well documented resid and resid-fp codes. (The SID reverse-engineering sites were also good sources.)
// Note that I avoided many internal/automatic variables from the SID function, assuming better speed this way. (Not
// using stack as much, but I'm not sure and it may depend on platform...) (The same is true for CPU emulation and
// player-code.)
// My SID implementation is similar to what I worked out in a SwinSID variant during 3..4 months of development. (So
// jsSID only took 2 weeks armed with this experience.) I learned the workings of ADSR/WAVE/filter operations mainly
// from the quite well documented resid and resid-fp codes. (The SID reverse-engineering sites were also good sources.)
// Note that I avoided many internal/automatic variables from the SID function, assuming better speed this way. (Not
// using stack as much, but I'm not sure and it may depend on platform...) (The same is true for CPU emulation and
// player-code.)
int SID::cycle(unsigned char num,
               uint32_t      baseaddr)  // the SID emulation itself ('num' is the number of SID to iterate (0..2)
{
    // better keep these variables static so they won't slow down the routine like if they were internal automatic
    // variables always recreated
    static byte         channel, ctrl, SR, prevgate, wf, test, *sReg, *vReg;
    static unsigned int accuadd, MSB, pw, wfout;
    static int          tmp, step, lim, nonfilt, filtin, filtout, output;
    static float        period, steep, rDS_VCR_FET, cutoff[3], resonance[3], ftmp;

    filtin = nonfilt = 0;
    sReg             = &memory[baseaddr];
    vReg             = sReg;

    // treating 2SID and 3SID channels uniformly (0..5 / 0..8), this probably avoids some extra code
    for (channel = num * SID_CHANNEL_AMOUNT; channel < (num + 1) * SID_CHANNEL_AMOUNT; channel++, vReg += 7) {
        ctrl = vReg[4];

        // ADSR envelope-generator:
        SR       = vReg[6];
        tmp      = 0;
        prevgate = (ADSRstate[channel] & GATE_BITMASK);
        if (prevgate != (ctrl & GATE_BITMASK)) {  // gatebit-change?
            if (prevgate)
                ADSRstate[channel] &= 0xFF - (GATE_BITMASK | ATTACK_BITMASK | DECAYSUSTAIN_BITMASK);
            else {  // falling edge
                ADSRstate[channel] =
                    (GATE_BITMASK | ATTACK_BITMASK | DECAYSUSTAIN_BITMASK);  // rising edge, also sets hold_zero_bit=0
                if ((SR & 0xF) > (prevSR[channel] & 0xF))
                    tmp =
                        1;  // assume SR->GATE write order: workaround to have crisp soundstarts by triggering delay-bug
            }  //(this is for the possible missed CTRL(GATE) vs SR register write order situations (1MHz CPU is cca 20
               // times faster than samplerate)
        }
        prevSR[channel]   = SR;  // if(SR&0xF) ratecnt[channel]+=5;  //assume SR->GATE write order: workaround to have
                                 // crisp soundstarts by triggering delay-bug
        ratecnt[channel] += clock_ratio;
        if (ratecnt[channel] >= 0x8000)
            ratecnt[channel] -= 0x8000;  // can wrap around (ADSR delay-bug: short 1st
                                         // frame)
        // set ADSR period that should be checked against rate-counter (depending on ADSR state
        // Attack/DecaySustain/Release)
        if (ADSRstate[channel] & ATTACK_BITMASK)
            step = vReg[5] >> 4;
        else if (ADSRstate[channel] & DECAYSUSTAIN_BITMASK)
            step = vReg[5] & 0xF;
        else
            step = SR & 0xF;
        period = ADSRperiods[step];
        step   = ADSRstep[step];
        if (ratecnt[channel] >= period && ratecnt[channel] < period + clock_ratio &&
            tmp == 0) {                  // ratecounter shot (matches rateperiod) (in genuine SID ratecounter is LFSR)
            ratecnt[channel] -= period;  // compensation for timing instead of simply setting 0 on rate-counter overload
            if ((ADSRstate[channel] & ATTACK_BITMASK) || ++expcnt[channel] == ADSR_exptable[envcnt[channel]]) {
                if (!(ADSRstate[channel] & HOLDZERO_BITMASK)) {
                    if (ADSRstate[channel] & ATTACK_BITMASK) {
                        envcnt[channel] += step;
                        if (envcnt[channel] >= 0xFF) {
                            envcnt[channel]     = 0xFF;
                            ADSRstate[channel] &= 0xFF - ATTACK_BITMASK;
                        }
                    } else if (!(ADSRstate[channel] & DECAYSUSTAIN_BITMASK) ||
                               envcnt[channel] > (SR & 0xF0) + (SR >> 4)) {
                        envcnt[channel] -= step;
                        if (envcnt[channel] <= 0 && envcnt[channel] + step != 0) {
                            envcnt[channel]     = 0;
                            ADSRstate[channel] |= HOLDZERO_BITMASK;
                        }
                    }
                }
                expcnt[channel] = 0;
            }
        }
        envcnt[channel] &= 0xFF;

        // WAVE-generation code (phase accumulator and waveform-selector):
        test    = ctrl & TEST_BITMASK;
        wf      = ctrl & 0xF0;
        accuadd = (vReg[0] + vReg[1] * 256) * clock_ratio;
        if (test || ((ctrl & SYNC_BITMASK) && sourceMSBrise[num]))
            phaseaccu[channel] = 0;
        else {
            phaseaccu[channel] += accuadd;
            if (phaseaccu[channel] > 0xFFFFFF) phaseaccu[channel] -= 0x1000000;
        }
        phaseaccu[channel] &= 0xFFFFFF;
        MSB                 = phaseaccu[channel] & 0x800000;
        sourceMSBrise[num]  = (MSB > (prevaccu[channel] & 0x800000)) ? 1 : 0;
        if (wf & NOISE_BITMASK) {  // noise waveform
            tmp = noise_LFSR[channel];
            if (((phaseaccu[channel] & 0x100000) != (prevaccu[channel] & 0x100000)) ||
                accuadd >= 0x100000)  // clock LFSR all time if clockrate exceeds observable at given samplerate
            {
                step                = (tmp & 0x400000) ^ ((tmp & 0x20000) << 5);
                tmp                 = ((tmp << 1) + (step ? 1 : test)) & 0x7FFFFF;
                noise_LFSR[channel] = tmp;
            }
            // we simply zero output when other waveform is mixed with noise. On real SID LFSR continuously gets filled
            // by zero and locks up. ($C1 waveform with pw<8 can keep it for a while...)
            wfout = (wf & 0x70) ? 0
                                : ((tmp & 0x100000) >> 5) + ((tmp & 0x40000) >> 4) + ((tmp & 0x4000) >> 1) +
                                      ((tmp & 0x800) << 1) + ((tmp & 0x200) << 2) + ((tmp & 0x20) << 5) +
                                      ((tmp & 0x04) << 7) + ((tmp & 0x01) << 8);
        } else if (wf & PULSE_BITMASK) {  // simple pulse
            pw  = (vReg[2] + (vReg[3] & 0xF) * 256) * 16;
            tmp = (int)accuadd >> 9;
            if (0 < pw && pw < tmp) pw = tmp;
            tmp ^= 0xFFFF;
            if (pw > tmp) pw = tmp;
            tmp = phaseaccu[channel] >> 8;
            if (wf == PULSE_BITMASK) {  // simple pulse, most often used waveform, make it sound as clean as possible
                                        // without oversampling
                // One of my biggest success with the SwinSID-variant was that I could clean the high-pitched and thin
                // sounds. (You might have faced with the unpleasant sound quality of high-pitched sounds without
                // oversampling. We need so-called 'band-limited' synthesis instead.
                //  There are a lot of articles about this issue on the internet. In a nutshell, the harsh edges produce
                //  harmonics that exceed the Nyquist frequency (samplerate/2) and they are folded back into hearable
                //  range, producing unvanted ringmodulation-like effect.)
                // After so many trials with dithering/filtering/oversampling/etc. it turned out I can't eliminate the
                // fukkin aliasing in time-domain, as suggested at pages. Oversampling (running the wave-generation 8
                // times more) was not a way at 32MHz SwinSID. It might be an option on PC but I don't prefer it in
                // JavaScript.) The only solution that worked for me in the end, what I came up with eventually: The
                // harsh rising and falling edges of the pulse are elongated making it a bit trapezoid. But not in
                // time-domain, but altering the transfer-characteristics. This had to be done in a frequency-dependent
                // way, proportionally to pitch, to keep the deep sounds crisp. The following code does this (my
                // favourite testcase is Robocop3 intro):
                step = (accuadd >= 255) ? 65535 / (accuadd / 256.0)
                                        : 0xFFFF;  // simple pulse, most often used waveform, make it sound as clean as
                                                   // possible without oversampling
                if (test)
                    wfout = 0xFFFF;
                else if (tmp < pw) {
                    lim = (0xFFFF - pw) * step;
                    if (lim > 0xFFFF) lim = 0xFFFF;
                    tmp   = lim - (pw - tmp) * step;
                    wfout = (tmp < 0) ? 0 : tmp;
                }  // rising edge
                else {
                    lim = pw * step;
                    if (lim > 0xFFFF) lim = 0xFFFF;
                    tmp   = (0xFFFF - tmp) * step - lim;
                    wfout = (tmp >= 0) ? 0xFFFF : tmp;
                }  // falling edge
            } else {  // combined pulse
                wfout = (tmp >= pw || test)
                            ? 0xFFFF
                            : 0;  //(this would be enough for a simple but aliased-at-high-pitches pulse)
                if (wf & TRI_BITMASK) {
                    if (wf & SAW_BITMASK) {
                        wfout = wfout ? combinedWF(num, channel, PulseTriSaw_8580, tmp >> 4, 1, vReg[1]) : 0;
                    }  // pulse+saw+triangle (waveform nearly identical to tri+saw)
                    else {
                        tmp   = phaseaccu[channel] ^ (ctrl & RING_BITMASK ? sourceMSB[num] : 0);
                        wfout = (wfout) ? combinedWF(num, channel, PulseSaw_8580,
                                                     (tmp ^ (tmp & 0x800000 ? 0xFFFFFF : 0)) >> 11, 0, vReg[1])
                                        : 0;
                    }
                }  // pulse+triangle
                else if (wf & SAW_BITMASK)
                    wfout = wfout ? combinedWF(num, channel, PulseSaw_8580, tmp >> 4, 1, vReg[1]) : 0;  // pulse+saw
            }
        } else if (wf & SAW_BITMASK) {        // saw
            wfout = phaseaccu[channel] >> 8;  // saw (this row would be enough for simple but aliased-at-high-pitch saw)
            // The anti-aliasing (cleaning) of high-pitched sawtooth wave works by the same principle as mentioned above
            // for the pulse, but the sawtooth has even harsher edge/transition, and as the falling edge gets longer,
            // tha rising edge should became shorter, and to keep the amplitude, it should be multiplied a little bit
            // (with reciprocal of rising-edge steepness). The waveform at the output essentially becomes an asymmetric
            // triangle, more-and-more approaching symmetric shape towards high frequencies. (If you check a recording
            // from the real SID, you can see a similar shape, the high-pitch sawtooth waves are triangle-like...) But
            // for deep sounds the sawtooth is really close to a sawtooth, as there is no aliasing there, but deep
            // sounds should be sharp...
            if (wf & TRI_BITMASK)
                wfout = combinedWF(num, channel, TriSaw_8580, wfout >> 4, 1, vReg[1]);  // saw+triangle
            else {  // simple cleaned (bandlimited) saw
                steep = (accuadd / 65536.0) / 288.0;
                if (steep == 0) steep = 1;  // avoid division by zero
                wfout += wfout * steep;
                if (wfout > 0xFFFF) wfout = 0xFFFF - (wfout - 0x10000) / steep;
            }
        } else if (wf & TRI_BITMASK) {  // triangle (this waveform has no harsh edges, so it doesn't suffer from strong
                                        // aliasing at high pitches)
            tmp   = phaseaccu[channel] ^ (ctrl & RING_BITMASK ? sourceMSB[num] : 0);
            wfout = (tmp ^ (tmp & 0x800000 ? 0xFFFFFF : 0)) >> 7;
        }
        wfout &= 0xFFFF;
        if (wf)
            prevwfout[channel] = wfout;
        else {
            wfout = prevwfout[channel];
        }  // emulate waveform 00 floating wave-DAC (on real SID waveform00 decays after 15s..50s depending on
           // temperature?)
        prevaccu[channel] = phaseaccu[channel];
        sourceMSB[num]    = MSB;  //(So the decay is not an exact value. Anyway, we just simply keep the value to avoid
                                  // clicks and support SounDemon digi later...)

        // routing the channel signal to either the filter or the unfiltered master output depending on filter-switch
        // SID-registers
        if (sReg[0x17] & FILTSW[channel])
            filtin += ((int)wfout - 0x8000) * envcnt[channel] / 256;
        else if ((FILTSW[channel] != 4) || !(sReg[0x18] & OFF3_BITMASK))
            nonfilt += ((int)wfout - 0x8000) * envcnt[channel] / 256;
    }
    // update readable SID1-registers (some SID tunes might use 3rd channel ENV3/OSC3 value as control)
    if (num == 0 && memory[1] & 3) {
        sReg[0x1B] = wfout >> 8;
        sReg[0x1C] = envcnt[3];
    }  // OSC3, ENV3 (some players rely on it)

    // FILTER: two integrator loop bi-quadratic filter, workings learned from resid code, but I kindof simplified the
    // equations The phases of lowpass and highpass outputs are inverted compared to the input, but bandpass IS in phase
    // with the input signal. The 8580 cutoff frequency control-curve is ideal (binary-weighted resistor-ladder VCRs),
    // while the 6581 has a treshold, and below that it outputs a constant ~200Hz cutoff frequency. (6581 uses MOSFETs
    // as VCRs to control cutoff causing nonlinearity and some 'distortion' due to resistance-modulation. There's a
    // cca. 1.53Mohm resistor in parallel with the MOSFET in 6581 which doesn't let the frequency go below 200..220Hz
    // Even if the MOSFET doesn't conduct at all. 470pF capacitors are small, so 6581 can't go below this
    // cutoff-frequency with 1.5MOhm.)
    cutoff[num] = sReg[0x16] * 8 + (sReg[0x15] & 7);
    if (SID_model[num] == 8580) {
        cutoff[num]    = (1 - exp((cutoff[num] + 2) * cutoff_ratio_8580));  // linear curve by resistor-ladder VCR
        resonance[num] = (pow(2, ((4 - (sReg[0x17] >> 4)) / 8.0)));
    } else {  // 6581
        cutoff[num] +=
            round(filtin * FILTER_DISTORTION_6581);  // MOSFET-VCR control-voltage-modulation (resistance-modulation aka
                                                     // 6581 filter distortion) emulation
        rDS_VCR_FET =
            cutoff[num] <= VCR_FET_TRESHOLD
                ? 100000000.0  // below Vth treshold Vgs control-voltage FET presents an open circuit
                : cutoff_steepness_6581 /
                      (cutoff[num] -
                       VCR_FET_TRESHOLD);  // rDS ~ (-Vth*rDSon) / (Vgs-Vth)  //above Vth FET drain-source resistance is
                                           // proportional to reciprocal of cutoff-control voltage
        cutoff[num] = (1 - exp(cap_6581_reciprocal / (VCR_SHUNT_6581 * rDS_VCR_FET / (VCR_SHUNT_6581 + rDS_VCR_FET)) /
                               samplerate));  // curve with 1.5MOhm VCR parallel Rshunt emulation
        resonance[num] = ((sReg[0x17] > 0x5F) ? 8.0 / (sReg[0x17] >> 4) : 1.41);
    }
    filtout = 0;
    ftmp    = filtin + prevbandpass[num] * resonance[num] + prevlowpass[num];
    if (sReg[0x18] & HIGHPASS_BITMASK) filtout -= ftmp;
    ftmp              = prevbandpass[num] - ftmp * cutoff[num];
    prevbandpass[num] = ftmp;
    if (sReg[0x18] & BANDPASS_BITMASK) filtout -= ftmp;
    ftmp             = prevlowpass[num] + ftmp * cutoff[num];
    prevlowpass[num] = ftmp;
    if (sReg[0x18] & LOWPASS_BITMASK) filtout += ftmp;

    // output stage for one SID
    // when it comes to $D418 volume-register digi playback, I made an AC / DC separation for $D418 value in the SwinSID
    // at low (20Hz or so) cutoff-frequency, and sent the AC (highpass) value to a 4th 'digi' channel mixed to the
    // master output, and set ONLY the DC (lowpass) value to the volume-control. This solved 2 issues: Thanks to the
    // lowpass filtering of the volume-control, SID tunes where digi is played together with normal SID channels, won't
    // sound distorted anymore, and the volume-clicks disappear when setting SID-volume. (This is useful for fade-in/out
    // tunes like Hades Nebula, where clicking ruins the intro.)
    output = (nonfilt + filtout) * (sReg[0x18] & 0xF) / OUTPUT_SCALEDOWN;
    // if (output>=32767) output=32767; else if (output<=-32768) output=-32768; //saturation logic on overload (not
    // needed if the callback handles it)
    return (int)output;  // master output
}

// The anatomy of combined waveforms: The resid source simply uses 4kbyte 8bit samples from wavetable arrays, says these
// waveforms are mystic due to the analog behaviour. It's true, the analog things inside SID play a significant role in
// how the combined waveforms look like, but process variations are not so huge that cause much differences in SIDs.
// After checking these waveforms by eyes, it turned out for me that these waveform are fractal-like, recursively
// approachable waveforms. My 1st thought and trial was to store only a portion of the waveforms in table, and magnify
// them depending on phase-accumulator's state. But I wanted to understand how these waveforms are produced. I felt from
// the waveform-diagrams that the bits of the waveforms affect each other, hence the recursive look. A short C code
// proved by assumption, I could generate something like a pulse+saw combined waveform. Recursive calculations were not
// feasible for MCU of SwinSID, but for jsSID I could utilize what I found out and code below generates the combined
// waveforms into wavetables. To approach the combined waveforms as much as possible, I checked out the SID schematic
// that can be found at some reverse-engineering sites... The SID's R-2R ladder WAVE DAC is driven by
// operation-amplifier like complementary FET output drivers, so that's not the place where I first thought the magic
// happens. These 'opamps' (for all 12 wave-bits) have single FETs as inputs, and they switch on above a certain level
// of input-voltage, causing 0 or 1 bit as R-2R DAC input. So the first keyword for the workings is TRESHOLD. These FET
// inputs are driven through serial switch FETs (wave-selector) that normally enables one waveform at a time. The
// phase-accumulator's output is brought to 3 kinds of circuitries for the 3 basic waveforms. The pulse simply drives
// all wave-selector inputs with a 0/1 depending on pulsewidth, the sawtooth has a XOR for triangle/ringmod generation,
// but what is common for all waveforms, they have an open-drain driver before the wave-selector, which has FETs towards
// GND and 'FET resistor' towards the power-supply rail. These outputs are clearly not designed to drive high loads, and
// normally they only have to drive the FETs input mentioned above. But when more of these output drivers are switched
// together by the switch-FETs in the wave-selector, they affect each other by loading each other. The pulse waveform,
// when selected, connects all of them together through a fairly strong connection, and its signal also affects the
// analog level (pulls below the treshold)... The farther a specific DAC bit driver is from the other, the less it
// affects its output. It turned out it's not powers of 2 but something else, that creates similar combined waveforms to
// that of real SID's... Note that combined waveforms never have values bigger than their sourcing sawtooth wave. The
// analog levels that get generated by the various bit drivers, that pull each other up/DOWN, depend on the resistances
// the components/wires inside the SID. And finally, what is output on the DAC depends on whether these analog levels
// are below or above the FET gate's treshold-level, That's how the combined waveform is generated. Maybe I couldn't
// explain well enough, but the code below is simple enough to understand the mechanism algoritmically. This simplified
// schematic exapmle might make it easier to understand sawtooth+pulse combination (must be observed with monospace
// fonts):
//                                _____            |-    .--------------.   /\/\--.
//  Vsupply                /  .----| |---------*---|-    /    Vsupply   !    R    !      As can be seen on this
//  schematic,
//   ------.       other   !  !   _____        !  TRES   \       \      !         /      the pulse wave-selector FETs
//         !       saw bit *--!----| |---------'  HOLD   /       !     |-     2R  \      connect the neighbouring
//         sawtooth /       output  !  !                          !      |------|-         /      outputs with a fairly
//         strong
//      Rd \              |-  !WAVEFORM-SELECTOR         *--*---|-      !    R    !      connection to each other
//      through
//         /              |-  !SWITCHING FETs            !  !    !      *---/\/\--*      their own wave-selector FETs.
//         ! saw-bit          !    _____                |-  !   ---     !         !      So the adjacent sawtooth
//         outputs
//         *------------------!-----| |-----------*-----|-  !          |-         /      pull each other lower (or maybe
//         a bit upper but not exceeding sawtooth line) ! (weak drive,so   !  saw switch       ! TRES-!  `----------|-
//         2R  \      depending on their low/high state and
//        |- can be shifted   !                   ! HOLD !              !         /      distance from each other,
//        causing
//   -----|- down (& up?)     !    _____          !      !              !     R   !      the resulting analog level that
//         ! by neighbours)   *-----| |-----------'     ---            ---   /\/\-*      will either turn the output on
//         or not.
//    GND ---                 !  pulse switch                                     !      (Depending on their relation to
//    treshold.)
//
//(As triangle waveform connects adjacent bits by default, the above explained effect becomes even stronger, that's why
// combined waveforms with thriangle are at 0 level most of the time.)

// in case you don't like these calculated combined waveforms it's easy to substitute the generated tables by
// pre-sampled 'exact' versions

unsigned int SID::combinedWF(uint8_t num, uint8_t channel, uint32_t* wfarray, int index, char differ6581, uint8_t freqh)
{
    static float addf;
    if (freqh == 0) freqh = 1;  // avoid division by zero
#ifdef NEW_COMBINED_WAVEFORMS
    addf = 0.4 + 0.6 / freqh;
#else
    addf = 0.6 + 0.4 / freqh;
#endif
    if (differ6581 && SID_model[num] == 6581) index &= 0x7FF;
    prevwavdata[channel] = wfarray[index] * addf + prevwavdata[channel] * (1.0 - addf);
    return prevwavdata[channel];
}

#ifdef NEW_COMBINED_WAVEFORMS
void SID::createCombinedWF(uint32_t* wfarray, float bitmul, float bitstrength, float treshold)
{
    int i, j, k;
    for (i = 0; i < 4096; i++) {
        wfarray[i] = 0;
        for (j = 0; j < 12; j++) {
            vTaskDelay(0);
            float bitlevel = ((i >> j) & 1);
            for (k = 0; k < 12; k++)
                if (!((i >> k) & 1)) bitlevel -= bitmul / pow(bitstrength, fabs(k - j));
            wfarray[i] += (bitlevel >= treshold) ? pow(2, j) : 0;
        }
        wfarray[i] *= 12;
    }
}
#else
void SID::createCombinedWF(unsigned int* wfarray, float bitmul, float bitstrength, float treshold)
{
    int i, j, k;
    for (i = 0; i < 4096; i++) {
        wfarray[i] = 0;
        for (j = 0; j < 12; j++) {
            float bitlevel = 0;
            for (k = 0; k < 12; k++) bitlevel += (bitmul / pow(bitstrength, std::abs(k - j))) * (((i >> k) & 1) - 0.5);
            wfarray[i] += (bitlevel >= treshold) ? pow(2, j) : 0;
        }
        wfarray[i] *= 12;
    }
}
#endif