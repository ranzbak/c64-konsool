# Konsool 64

## Introduction

This application is a functional C64 emulator that run on the [Konsool/Tanmatsu](https://badge.team/docs/badges/konsool/) device.

On the Konsool device, the emulator both outputs audio and video and accepts keyboard input.

## Usage

### Accessing the menu 

Press the 'purple diamond' button on the device at any time to open or close the menu.

### Navigation

The following keys are used:

| Key            | Description                 |
| -------------- | --------------------------- |
| Purple diamond | Show or hide menu           |
| Up/Down        | Move up and down the menus. |
| Enter          | Activate a menu item        |
| ESC            | Go back one menu up         |


Since the menu structure is still being developed, I'm going to not document more details at this time.

## Games / Sofware

### Loading prg files

At this time only the loading of **.PRG** files is supported, .d64 files are in the planning.

- To load .prg files, put them on in a directory named 'c64prg' SD card file system.

- By Opening the menu and select 'Load PRG', a .prg file can be selected and loaded.

- When the C64 screen shows again, type the command 'run' and press enter.

### Joystick emulation

The original commodore 64 had two joystick ports namely '1' and '2'.
Because some games use port one and others two, switching the joystick between ports is needed.

In order to enable the Joystick, the 'keyboard joystick' option in the main menu needs to be set to 'yes'.

Selecting the joystick port is done using the 'F5' or 'blue tri-lobe' key.

Indicators of joystick status and port will be added to the software in the future.

#### Joystick key bindings

| key               | Joystick function                |
| ----------------- | -------------------------------- |
| Arrow up          | UP                               |
| Arrow down        | DOWN                             |
| Arrow left        | LEFT                             |
| Arrow right       | RIGHT                            |
| Left SHIFT        | FIRE button                      |
| F5 / Blue diamond | Switch joystick between port 1/2 |

## Perquisites

### Install the build dependencies

#### Debian / Ubuntu: 

```bash
sudo apt-get install git wget flex bison gperf python3 python3-pip python3-venv cmake ninja-build ccache libffi-dev libssl-dev dfu-util libusb-1.0-0
```

#### Arch

```bash
sudo pacman -S --needed gcc git make flex bison gperf python cmake ninja ccache dfu-util libusb
``` 

### Setup the the build environment

```bash
make prepare
```

## Build the project

```bash
make build
```

## Upload to the Tanmatsu

Fast and easiest way to upload the build

```bash
tools/badgelink.py appfs upload "c64-emu" "C64 Emulator" 0 <project_root>/build/application.bin
```

## Configure clangd

The esp-idf cross compiler has built in include paths, not using this cross compiler will result in clangd complaining about missing include files.
The CMake project project by default already makes a compile_commands.json file, but clangd will not accept any cross compiler without it being white listed.

In order to white list a compiler for clangd to extract the include paths:

```
--query-driver=/**/riscv32-esp-elf/bin/riscv32-esp-elf-gcc
```

In VsCodium this can be done using the following statement in the settings.json

```json
"clangd.arguments": [
  "--query-driver=/**/riscv32-esp-elf/bin/riscv32-esp-elf-gcc"
]
```

## Many many credits for the person who wrote the emulator this is based on

[retrolec](https://github.com/retroelec/T-HMI-C64/commits?author=retroelec)

[T-HMI-C64](https://github.com/retroelec/T-HMI-C64)

## License & Copyright


Modified 2025 Ranzbak Badge.Team

Copyright (C) 2024 retroelec <retroelec42@gmail.com>

This program is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation; either version 3 of the License, or (at your
option) any later version.
