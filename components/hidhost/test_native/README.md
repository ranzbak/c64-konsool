# Gamepad tests

`hid_gamepad.c` is plain C with no ESP-IDF dependencies beyond logging, so it can be built and run
on a host against report descriptors captured from real devices. These tests check what a gamepad
turns into as a C64 joystick byte.

```
make test
```

The report descriptor parser it decodes through, and the descriptors themselves, come from the
[badgeteam/hid-host](https://github.com/badgeteam/esp32-component-hid-host) component. Build the
project once so the component manager fetches it, or point `HID_HOST` at a checkout:

```
make test HID_HOST=../../../../esp32-component-hid-host
```

The descriptors come from actual hardware. The mice, the Stadia controller and the DualShock 4
clone were captured for [konsool-HID](https://github.com/annejan/konsool-HID/pull/2), the
DualShock 3 and the Competition Pro on a Tanmatsu and on a Linux host.
