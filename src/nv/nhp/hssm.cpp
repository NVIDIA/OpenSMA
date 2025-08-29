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

#include "nv/nhp/hssm.h"

#include "nv/gpio/driver.h"
#include "nv/nv.h"

namespace nv::nhp {

NhpE1sHotSwap::NhpE1sHotSwap(E1sOutputPins pin_config, const uint16_t nhp_pin_register)
: state(DriveDisabled)
, pinout(pin_config)
{
    init_pins();

    update_control_register(nhp_pin_register);

    nv::info("Initialized NHP Drive Hot Swap State Machine\n");
}

NhpE1sHotSwap::NhpE1sHotSwap() : state(DriveDisabled), pinout()
{
    return;
}

void NhpE1sHotSwap::update_control_register(const uint16_t nhp_pin_register)
{
    // NOTE: On P3957 we may or may not be controlling pwren (PRSNT_L may be controlling)
    //     So pgood only tells us if a fault occurred if both:
    //         1. drive is present and pwr is enabled (we know for sure power is enabled)
    //         2. pgood is low

    // const bool PowerGood        = nhp_pin_register & NhpPwrGdBitmask;
    const bool PrsntL           = nhp_pin_register & NhpPrsnt0LBitmask;
    const bool HostTargetPerstL = nhp_pin_register & NhpForcePerstLBitMask;
    const bool HostTargetPwren  = nhp_pin_register & NhpPwrEnBitmask;
    const bool HostTargetClken  = nhp_pin_register & NhpClkEnBitmask;

    switch (state) {
        case DriveDisabled: {  // drive not present and host not ready for control
            if (!PrsntL && !HostTargetPerstL && !HostTargetPwren && HostTargetClken) {
                nv::info("Drive state changed to HostControl\n");
                state = HostControl;
            }
            break;
        }
        case HostControl: {  // drive present and control given to host
            if (PrsntL) {
                nv::info("Drive state changed to DriveDisabled\n");
                state = DriveDisabled;
            }
            break;
        }
        case PowerFault: {
            // 12V faulted never leave this state
            break;
        }
        default: {
            nv::error("Invalid HSSM state %d\n", state);
            break;
        }
    }

    update_pins(nhp_pin_register);
}

void NhpE1sHotSwap::update_pins(const uint16_t nhp_pin_register)
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
        case HostControl: {
            const bool HostTargetPerstL = nhp_pin_register & NhpForcePerstLBitMask;
            const bool HostTargetPwren  = nhp_pin_register & NhpPwrEnBitmask;
            const bool HostTargetClken  = nhp_pin_register & NhpClkEnBitmask;
            const bool PwrDis           = nhp_pin_register & NhpPwrdisBitmask;
            const bool BlueLedState     = !(nhp_pin_register & NhpBlueLedLBitmask);
            const bool AmberLedState    = nhp_pin_register & NhpAmberLedBitmask;

            set_pin(pinout.perst_l_port, pinout.perst_l_pin, (HostTargetPerstL) ? High : Low);
            // nhp register uses clock enable high and E.1s uses clock enable low
            set_pin(pinout.clk_en_l_port, pinout.clk_en_l_pin, (HostTargetClken) ? Low : High);
            set_pin(pinout.pwrdis_port, pinout.pwrdis_pin, (PwrDis) ? High : Low);
            // pwrEN uses active high internally but is active low on board
            set_pin(pinout.pwren_l_port, pinout.pwren_l_pin, (HostTargetPwren) ? Low : High);
            set_leds(AmberLedState, BlueLedState);
            break;
        }
        case PowerFault: {
            set_pin(pinout.perst_l_port, pinout.perst_l_pin, Low);
            set_pin(pinout.clk_en_l_port, pinout.clk_en_l_pin, High);
            set_pin(pinout.pwrdis_port, pinout.pwrdis_pin, Low);
            set_pin(pinout.pwren_l_port, pinout.pwren_l_pin, High);
            set_leds(false, false);
            break;
        }
        default: {
            nv::error("Invalid HSSM state %d\n", state);
            break;
        }
    }
}

void NhpE1sHotSwap::set_leds(bool amberLed, bool blueLed)
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

void NhpE1sHotSwap::init_pins()
{
    set_pin(pinout.perst_l_port, pinout.perst_l_pin, High);
    set_pin(pinout.clk_en_l_port, pinout.clk_en_l_pin, High);
    set_pin(pinout.pwrdis_port, pinout.pwrdis_pin, Low);
    set_pin(pinout.pwren_l_port, pinout.pwren_l_pin, Low);
    set_pin(pinout.led_tristate_ctrl_port, pinout.led_tristate_ctrl_pin, HiZ);
}

void NhpE1sHotSwap::set_pin(nv::gpio::GpioPort port, nv::gpio::GpioPin pin, PinState pin_state)
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

}  // namespace nv::nhp
