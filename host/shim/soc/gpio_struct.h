#pragma once
// The joystick reads the GPIO registers directly on the badge. Nothing is
// wired up here and the host runner never enables it.
#include <cstdint>
struct { struct { uint32_t val; } in; struct { uint32_t val; } in1; } GPIO;
