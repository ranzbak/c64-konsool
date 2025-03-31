# Template app project

## Prequisites

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
