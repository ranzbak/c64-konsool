#pragma once

using namespace std;
/*
 Copyright (C) 2024 retroelec <retroelec42@gmail.com>

 This program is free software; you can redistribute it and/or modify it
 under the terms of the GNU General Public License as published by the
 Free Software Foundation; either version 3 of the License, or (at your
 option) any later version.

 This program is distributed in the hope that it will be useful, but
 WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 for more details.

 For the complete text of the GNU General Public License see
 http://www.gnu.org/licenses/.
*/

#include "BoardDriver.hpp"
#include "Config.hpp"
#include "HardwareInitializationException.h"
#include <driver/gpio.h>
#include <soc/gpio_struct.h>

class T_HMI : public BoardDriver {
public:
  void init() override;
};