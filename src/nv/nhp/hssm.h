/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES.
 * All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#pragma once

#include "nv/gpio/common.h"
#include "nv/nhp/common.h"

namespace nv::nhp {

// Hot swap states controlled from 16bit pin control vector from PCA9555
class NhpE1sHotSwap
{
public:
    // Configs

    // Public Functions
    // ---------------------------------------------

    // intializes the pins and state machine
    // give the pinout for the drive
    // takes 16 bit vector for pin states based off NHP register protocol
    NhpE1sHotSwap(E1sOutputPins pin_config, const uint16_t nhp_pin_register);

    // default constructor
    NhpE1sHotSwap();

    // takes 16 bit vector for pin states based off NHP register protocol
    // updates state
    // sets pins
    void update_control_register(const uint16_t nhp_pin_register);

private:
    // Internal Constants
    // ---------------------------------------------

    enum HotSwapState
    {
        DriveDisabled,  // (default) drive not present and host not ready for control
        HostControl,    // drive present and control given to host
        PowerFault      // 12V faulted
    };

    enum PinState
    {
        Low,
        High,
        HiZ  // sets as input - not implemented yet
    };

    // Helper Functions
    // ---------------------------------------------
    // sets the tristate led pin based on the amber and blue led target register
    void set_leds(bool amberLed, bool blueLed);

    // sets the pins for the new state or control register change
    void update_pins(const uint16_t nhp_pin_register);

    // initializes pins to default state
    void init_pins();

    // sets the pin to high, low, highz
    void set_pin(nv::gpio::GpioPort port, nv::gpio::GpioPin pin, PinState pin_state);

    // Member Variables
    // ---------------------------------------------

    // tracks the state
    HotSwapState state;

    // the pinout ports and pins for the drive
    E1sOutputPins pinout;
};

}  // namespace nv::nhp