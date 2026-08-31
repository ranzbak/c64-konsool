#pragma once
#include <cstdint>
#include <chrono>
// Microseconds since the process started. The emulator only ever takes
// differences, so the origin does not matter.
static inline int64_t esp_timer_get_time()
{
    using namespace std::chrono;
    static const auto t0 = steady_clock::now();
    return duration_cast<microseconds>(steady_clock::now() - t0).count();
}
