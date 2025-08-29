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

#include "nv/vpp/hssm.h"
#include "nv/gpio/driver.h"

namespace nv::vpp {

E1sHotSwap::E1sHotSwap(nv::nhp::E1sOutputPins pin_config,
                       bool                   pgood,
                       bool                   prsntL,
                       bool                   amberLed,
                       bool                   blueLed,
                       nv::ipc::Event*        hotSwapEvent,
                       uint8_t                driveNum)
: state(DriveDisabled)
, pinout(pin_config)
, _hotSwapEvent(hotSwapEvent)
, _driveNum(driveNum)
{
    init_pins();

    updateStateMachine(pgood, prsntL, amberLed, blueLed);

    nv::info("Initialized drive %d hot swap controller.\n", _driveNum);
}

E1sHotSwap::E1sHotSwap(nv::nhp::E1sOutputPins pin_config,
                       nv::ipc::Event*        hotSwapEvent,
                       uint8_t                driveNum)
: state(DriveDisabled)
, pinout(pin_config)
, _hotSwapEvent(hotSwapEvent)
, _driveNum(driveNum)
{
    init_pins();

    updateStateMachine(false, true, false, false);

    nv::info("Initialized drive %d hot swap controller.\n", _driveNum);
}

E1sHotSwap::E1sHotSwap() : state(), pinout(), _hotSwapEvent(), _driveNum()
{
    //
}

void E1sHotSwap::updateStateMachine(
    bool pgood, bool prsntL, bool amberLed, bool blueLed, bool timerDone)
{
    switch (state) {
        case DriveDisabled: {
            if (!prsntL && !pgood) {
                state = WaitPgood;
                // uncomment this to enable power fault state
                // startTimer();
            }
            else if (!prsntL && pgood) {
                state = WaitClkStable;
                startTimer();
            }
            else if (prsntL) {
                // stay
            }
            else {
                // coverity[dead_error_line] leaving this in case state conditions change
                nv::error("Unexpected state change\n");
            }
            break;
        }
        case WaitPgood: {
            // remove this to enable power fault state
            timerDone = false;
            if (prsntL) {
                state = DriveDisabled;
            }
            else if (!prsntL && pgood) {
                state = WaitClkStable;
                startTimer();
            }
            else if (!prsntL && !pgood && timerDone) {
                // coverity[dead_error_line] leaving this if we want to enable fault
                state = Fault;
            }
            else if (!prsntL && !pgood && !timerDone) {
                // stay
            }
            else {
                // coverity[dead_error_line] leaving this in case state conditions change
                nv::error("Unexpected state change\n");
            }
            break;
        }
        case WaitClkStable: {
            if (prsntL) {
                state = DriveDisabled;
            }
            else if (!prsntL && pgood && timerDone) {
                state = DriveOn;
            }
            else if (!prsntL && !pgood && timerDone) {
                state = Fault;
            }
            else if (!prsntL && !timerDone) {
                // stay
            }
            else {
                // coverity[dead_error_line] leaving this in case state conditions change
                nv::error("Unexpected state change\n");
            }
            break;
        }
        case DriveOn: {
            if (prsntL) {
                state = DriveDisabled;
            }
            else if (!prsntL && !pgood) {
                state = Fault;
            }
            else if (!prsntL && pgood) {
                // stay
            }
            else {
                // coverity[dead_error_line] leaving this in case state conditions change
                nv::error("Unexpected state change\n");
            }
            break;
        }
        case Fault: {
            // Currently stays here forever

            // Could implement retry timer
            /*
            if (prsntL && retryAfterFaultTimerDone) {
                state = DriveDisabled;
            }
            else if (!prsntL && pgood && retryAfterFaultTimerDone) {
                state = WaitClkStable;
                startTimer();
            }
            else if (!prsntL && !pgood && retryAfterFaultTimerDone) {
                state = WaitPgood;
                // startTimer();
            }
            else if (!prsntL && !retryAfterFaultTimerDone) {
                // stay
            }
            else {
                // coverity[dead_error_line] leaving this in case state conditions change
                nv::error("Unexpected state change\n");
            }
            */
            break;
        }
        default: {
            nv::error("Invalid HSSM state %d\n", state);
            break;
        }
    }

    // nv::info("Drive %d state: %x\n", _driveNum, state);  // DEBUG
    update_pins(amberLed, blueLed);
}

void E1sHotSwap::update_pins(bool amberLed, bool blueLed)
{
    switch (state) {
        case DriveDisabled: {
            // hold everything off
            set_pin(pinout.perst_l_port, pinout.perst_l_pin, Low);
            set_pin(pinout.clk_en_l_port, pinout.clk_en_l_pin, High);
            set_pin(pinout.pwrdis_port, pinout.pwrdis_pin, Low);
            set_pin(pinout.pwren_l_port, pinout.pwren_l_pin, High);
            set_leds(false, false);
            break;
        }
        case WaitPgood: {
            // just power enabled
            set_pin(pinout.perst_l_port, pinout.perst_l_pin, Low);
            set_pin(pinout.clk_en_l_port, pinout.clk_en_l_pin, High);
            set_pin(pinout.pwrdis_port, pinout.pwrdis_pin, Low);
            set_pin(pinout.pwren_l_port, pinout.pwren_l_pin, Low);
            // set_leds(amberLed, blueLed);
            set_leds(true, false);
            break;
        }
        case WaitClkStable: {
            // power and clock enabled
            set_pin(pinout.perst_l_port, pinout.perst_l_pin, Low);
            set_pin(pinout.clk_en_l_port, pinout.clk_en_l_pin, Low);
            set_pin(pinout.pwrdis_port, pinout.pwrdis_pin, Low);
            set_pin(pinout.pwren_l_port, pinout.pwren_l_pin, Low);
            set_leds(amberLed, blueLed);
            break;
        }
        case DriveOn: {
            // pwr, clk, and pcie enabled
            set_pin(pinout.perst_l_port, pinout.perst_l_pin, High);
            set_pin(pinout.clk_en_l_port, pinout.clk_en_l_pin, Low);
            set_pin(pinout.pwrdis_port, pinout.pwrdis_pin, Low);
            set_pin(pinout.pwren_l_port, pinout.pwren_l_pin, High);
            set_leds(amberLed, blueLed);
            break;
        }
        case Fault: {
            // hold everything off
            set_pin(pinout.perst_l_port, pinout.perst_l_pin, Low);
            set_pin(pinout.clk_en_l_port, pinout.clk_en_l_pin, High);
            set_pin(pinout.pwrdis_port, pinout.pwrdis_pin, Low);
            set_pin(pinout.pwren_l_port, pinout.pwren_l_pin, High);
            set_leds(amberLed, blueLed);
            break;
        }
        default: {
            nv::error("Invalid HSSM state %d\n", state);
            break;
        }
    }
}

void E1sHotSwap::set_leds(bool amberLed, bool blueLed)
{
    // convert 2-pin to tri state
    // (for some reason amber is active high blue is active Low)
    if (blueLed && !amberLed) {
        // blue on amber off - Low
        set_pin(pinout.led_tristate_ctrl_port, pinout.led_tristate_ctrl_pin, Low);
    }
    else if (!blueLed && amberLed) {
        // blue off amber on - high
        set_pin(pinout.led_tristate_ctrl_port, pinout.led_tristate_ctrl_pin, High);
    }
    else if (!blueLed && !amberLed) {
        // both off - hiZ
        set_pin(pinout.led_tristate_ctrl_port, pinout.led_tristate_ctrl_pin, HiZ);
    }
    else {
        // both on - not allowed by E.1s spec (throws error)
        // guess turn them off?
        set_pin(pinout.led_tristate_ctrl_port, pinout.led_tristate_ctrl_pin, HiZ);
        nv::warn("Host VIOLATED E.1s spec by attempting to drive both LEDs.\n");
    }
}

void E1sHotSwap::init_pins()
{
    set_pin(pinout.perst_l_port, pinout.perst_l_pin, High);
    set_pin(pinout.clk_en_l_port, pinout.clk_en_l_pin, High);
    set_pin(pinout.pwrdis_port, pinout.pwrdis_pin, Low);
    set_pin(pinout.pwren_l_port, pinout.pwren_l_pin, Low);
    set_pin(pinout.led_tristate_ctrl_port, pinout.led_tristate_ctrl_pin, HiZ);
}

void E1sHotSwap::set_pin(nv::gpio::GpioPort port, nv::gpio::GpioPin pin, PinState pin_state)
{
    switch (pin_state) {
        case Low: {
            const nv::gpio::Status gpio_status = nv::gpio::Driver::init_pin(
                port, pin, nv::gpio::Direction::Output, nv::gpio::GpioState::Low);
            if (gpio_status != nv::gpio::Status::Ok) {
                nv::error("Error setting GPIO%d_%d to %d in HSSM\n", port, pin, pin_state);
            }
            break;
        }
        case High: {
            const nv::gpio::Status gpio_status = nv::gpio::Driver::init_pin(
                port, pin, nv::gpio::Direction::Output, nv::gpio::GpioState::High);
            if (gpio_status != nv::gpio::Status::Ok) {
                nv::error("Error setting GPIO%d_%d to %d in HSSM\n", port, pin, pin_state);
            }
            break;
        }
        case HiZ: {
            const nv::gpio::Status gpio_status = nv::gpio::Driver::init_pin(
                port, pin, nv::gpio::Direction::Input, nv::gpio::GpioState::Low);
            if (gpio_status != nv::gpio::Status::Ok) {
                nv::error("Error setting GPIO%d_%d to %d in HSSM\n", port, pin, pin_state);
            }
            break;
        }
        default: {
            nv::error("Setting invalid pin state %d for GPIO%d_%d\n", pin_state, port, pin);
            break;
        }
    }
}

// Could use ctimer interrupt for this instead
void E1sHotSwap::startTimer()
{
    if (_hotSwapEvent->set(static_cast<nv::ipc::Event::Bits>(0x1U << _driveNum), true)
        != nv::ipc::Event::Status::Ok) {
        nv::error("Error setting drive timer event bits\n");
    }
}

}  // namespace nv::vpp