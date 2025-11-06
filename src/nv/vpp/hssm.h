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

#include <cstdint>

#include "nv/nhp/common.h"

#include "nv/ipc/event.h"

namespace nv::vpp {

class E1sHotSwap
{
public:
    // Configs

    // Public Functions
    // ---------------------------------------------
    E1sHotSwap(nv::nhp::E1sOutputPins pin_config,
               bool                   pgood,
               bool                   prsntL,
               bool                   amberLed,
               bool                   blueLed,
               nv::ipc::Event*        hotSwapEvent,
               uint8_t                driveNum);
    E1sHotSwap(nv::nhp::E1sOutputPins pin_config,
               nv::ipc::Event*        hotSwapEvent,
               uint8_t                driveNum);
    E1sHotSwap();

    void updateStateMachine(
        bool pgood, bool prsntL, bool amberLed, bool blueLed, bool timerDone = false);

private:
    // Member Constants
    // ---------------------------------------------
    enum HotSwapState
    {
        DriveDisabled,  // (init state) drive not present
        WaitPgood,      // enables pwr, waits for pgood or times out and faults
        WaitClkStable,  // enables clocks, waits for stabilization time
        DriveOn,        // drive present and control given to host
        Fault           // fault of some kind, can retry after timeout
    };

    enum PinState
    {
        Low,
        High,
        HiZ
    };

    // Helper Functions
    // ---------------------------------------------
    // sets the tristate led pin based on the amber and blue led target register
    void set_leds(bool amberLed, bool blueLed);

    // sets the pins for the current state
    void update_pins(bool amberLed, bool blueLed);

    // initializes pins to default state
    void init_pins();

    // sets the pin to high, low, highz
    void set_pin(nv::gpio::GpioPort port, nv::gpio::GpioPin pin, PinState pin_state);

    void startTimer();

    // Member Variables
    // ---------------------------------------------
    // tracks the state
    HotSwapState state;

    // the pinout ports and pins for the drive
    nv::nhp::E1sOutputPins pinout;

    nv::ipc::Event* _hotSwapEvent;

    uint8_t _driveNum;
};

}  // namespace nv::vpp