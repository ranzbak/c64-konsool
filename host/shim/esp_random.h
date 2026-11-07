#pragma once
#include <cstdint>
// Deterministic on purpose: a run has to be reproducible to be worth
// debugging. hostSeedRandom() is called from the runner.
extern uint32_t hostRandomState;
static inline uint32_t esp_random()
{
    hostRandomState ^= hostRandomState << 13;
    hostRandomState ^= hostRandomState >> 17;
    hostRandomState ^= hostRandomState << 5;
    return hostRandomState;
}
