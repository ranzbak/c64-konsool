#pragma once
#include <cstdint>
#include <thread>
#include <chrono>
#include "portmacro.h"

typedef int      BaseType_t;
typedef unsigned UBaseType_t;
typedef uint32_t TickType_t;
#define pdTRUE  1
#define pdFALSE 0
#define pdMS_TO_TICKS(ms) (ms)

// The runner drives the emulation itself and never actually sleeps, but the
// emulator calls this between loads to let the kernal settle. Sleeping for
// real would only make a headless run slower, so it is a no-op and the caller
// keeps stepping the machine.
static inline void vTaskDelay(TickType_t) {}

// The badge runs the emulation on one core and the display on the other, and
// throttles the first to the second through a binary semaphore. The host
// keeps that shape, so this has to be a real semaphore rather than a no-op:
// without it the two threads race over the VIC's bitmap and a screenshot
// tears.
#include <mutex>
#include <condition_variable>

struct HostSemaphore {
    std::mutex              m;
    std::condition_variable cv;
    std::condition_variable arrived;    // fires when someone parks in take()
    bool                    signalled = false;
    int                     waiters   = 0;
};
typedef HostSemaphore* SemaphoreHandle_t;

static inline SemaphoreHandle_t xSemaphoreCreateBinary() { return new HostSemaphore(); }
static inline SemaphoreHandle_t xSemaphoreCreateMutex() { return new HostSemaphore(); }

static inline BaseType_t xSemaphoreTake(SemaphoreHandle_t s, TickType_t ticks)
{
    if (s == nullptr) return pdTRUE;
    std::unique_lock<std::mutex> lk(s->m);
    s->waiters++;
    s->arrived.notify_all();
    // No timeout. The emulation parks here at the end of every frame and the
    // runner releases it exactly once per drawn frame; letting the wait expire
    // instead lets the emulation run a frame nobody asked for, and the run
    // stops being reproducible. A headless run has no deadline to miss.
    s->cv.wait(lk, [s] { return s->signalled; });
    s->waiters--;
    s->signalled = false;
    return pdTRUE;
}

static inline BaseType_t xSemaphoreGive(SemaphoreHandle_t s)
{
    if (s == nullptr) return pdTRUE;
    {
        std::lock_guard<std::mutex> lk(s->m);
        s->signalled = true;
    }
    s->cv.notify_one();
    return pdTRUE;
}

// Blocks until a thread is parked inside xSemaphoreTake on this semaphore.
// The emulation parks there at the end of every frame, so this is how the
// runner knows the frame is complete and the framebuffer can be read without
// racing the thread that writes it. Waiting on the thread's own state rather
// than on a sleep is what makes a headless run reproducible: two runs of the
// same binary have to produce the same screenshot, or a pixel comparison
// between two builds means nothing.
static inline void hostWaitUntilParked(SemaphoreHandle_t s)
{
    if (s == nullptr) return;
    std::unique_lock<std::mutex> lk(s->m);
    s->arrived.wait(lk, [s] { return s->waiters > 0 && !s->signalled; });
}
