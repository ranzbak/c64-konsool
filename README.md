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

TODO: Describe upload using AppFS upload

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

## License & Copyright

Copyright 2025 Nicolai Electronics

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so.

THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Feel free to remove this file but be sure to add your own license for your app.
If you don't know what license to choose: we recommend releasing your project under terms of the MIT license.

