#pragma once

// global constants and variables
#include <cstddef>
#include <cstdint>
#include "esp_attr.h"

typedef void (*AudioCallback)(int16_t* samples, size_t num_samples);

#define SIDMODEL_8580 8580
#define SIDMODEL_6581 6581

#define DEFAULT_SIDMODEL SIDMODEL_6581

#define C64_PAL_CPUCLK     985248.0
#define SID_CHANNEL_AMOUNT 3
#define LINES_PER_FRAME    312.5
#define PAL_FRAMERATE      50.1245419
// 50.06 // 50.0443427 //50.1245419 //(C64_PAL_CPUCLK/63/312.5), selected
// 50.06 // 50.0443427 //50.1245419 //(C64_PAL_CPUCLK/63/312.5), selected
// carefully otherwise some ADSR-sensitive tunes may suffer more:
// TODO: Higher sample frequency in the future, but for no we don't have the cycles.
#define DEFAULT_SAMPLERATE 22050.0
// 11025.0  //(Soldier of Fortune, 2nd Reality, Alliance, X-tra energy, Jackal,
// 16000.0 //(Soldier of Fortune, 2nd Reality, Alliance, X-tra energy, Jackal,
// Sanxion, Ultravox, Hard Track, Swing, Myth, LN3, etc.)
#define CLOCK_RATIO_DEFAULT \
    C64_PAL_CPUCLK / DEFAULT_SAMPLERATE  //(50.0567520: lowest framerate where Sanxion is fine,
                                         // and highest where Myth is almost fine)
#define VCR_SHUNT_6581 \
    1500  // kOhm //cca 1.5 MOhm Rshunt across VCR FET drain and source (causing
          // 220Hz bottom cutoff with 470pF integrator capacitors in old C64)
#define VCR_FET_TRESHOLD \
    192                 // Vth (on cutoff numeric range 0..2048) for the VCR cutoff-frequency
                        // control FET below which it doesn't conduct
#define CAP_6581 0.470  // nF //filter capacitor value for 6581
#define FILTER_DARKNESS_6581 \
    22.0  // the bigger the value, the darker the filter control is (that is,
          // cutoff frequency increases less with the same cutoff-value)
#define FILTER_DISTORTION_6581 \
    0.0016  // the bigger the value the more of resistance-modulation (filter
            // distortion) is applied for 6581 cutoff-control
#define SAMPLES_PER_SCAN_LINE 63 / (CLOCK_RATIO_DEFAULT)

#define SAMPLE_BUFFER_SIZE 32
#define US_PER_SAMPLE      (1000000.0 / DEFAULT_SAMPLERATE);
#define OUTPUT_SCALEDOWN   (SID_CHANNEL_AMOUNT * 16 + 26);
#define CUTOFF_RATIO_8580  (-2 * 3.14 * (12500.0 / 2048) / DEFAULT_SAMPLERATE)
#define CLOCK_RATIO        (C64_PAL_CPUCLK / DEFAULT_SAMPLERATE)

// //raw output divided by this after multiplied by main volume, this also
// compensates for filter-resonance emphasis to avoid distotion

enum {
    GATE_BITMASK         = 0x01,
    SYNC_BITMASK         = 0x02,
    RING_BITMASK         = 0x04,
    TEST_BITMASK         = 0x08,
    TRI_BITMASK          = 0x10,
    SAW_BITMASK          = 0x20,
    PULSE_BITMASK        = 0x40,
    NOISE_BITMASK        = 0x80,
    HOLDZERO_BITMASK     = 0x10,
    DECAYSUSTAIN_BITMASK = 0x40,
    ATTACK_BITMASK       = 0x80,
    LOWPASS_BITMASK      = 0x10,
    BANDPASS_BITMASK     = 0x20,
    HIGHPASS_BITMASK     = 0x40,
    OFF3_BITMASK         = 0x80
};

class SID {
   private:
    // SID-emulation variables:
    int32_t       SIDamount           = 1;
    int32_t       SID_model[3]        = {DEFAULT_SIDMODEL, DEFAULT_SIDMODEL, DEFAULT_SIDMODEL};
    int32_t       requested_SID_model = -1;
    uint8_t       ADSRstate[9], expcnt[9], prevSR[9], sourceMSBrise[9];
    int16_t       envcnt[9];
    uint32_t      prevwfout[9], prevwavdata[9], sourceMSB[3], noise_LFSR[9];
    int32_t       phaseaccu[9], prevaccu[9], prevlowpass[3], prevbandpass[3];
    float         ratecnt[9], cutoff_steepness_6581, cap_6581_reciprocal;
    float         clock_ratio = CLOCK_RATIO_DEFAULT;
    uint8_t* memory;
    float   scan_line_sync = 0.0;
    AudioCallback audio_callback = nullptr;

    int32_t combinedWF(uint8_t num, uint8_t channel, const uint32_t* wfarray, int index, char differ6581,
                       uint8_t freqh);

    // callback function to send out audio sample data
    // consists of a pointer and a number of samples to send

   public:
    SID();
    void cSID_init();
    void init(uint8_t* memory, AudioCallback sample_out_callback = nullptr, int sid_model = 8580);
    void raster_line();
    int  cycle(unsigned char num, uint32_t baseaddr);
};