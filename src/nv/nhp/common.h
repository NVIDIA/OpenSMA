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
#include "sys/adc/adc.h"

namespace nv::nhp {

// meant for hssm to manage output values
struct E1sOutputPins
{
    // perst low - input
    nv::gpio::GpioPort perst_l_port;
    nv::gpio::GpioPin  perst_l_pin;

    // pcie clock enable - output
    nv::gpio::GpioPort clk_en_l_port;
    nv::gpio::GpioPin  clk_en_l_pin;

    // internal drive power disable - output
    nv::gpio::GpioPort pwrdis_port;
    nv::gpio::GpioPin  pwrdis_pin;

    // 12v power enable for that drive - output
    nv::gpio::GpioPort pwren_l_port;
    nv::gpio::GpioPin  pwren_l_pin;

    // amber/blue led control - output
    // high = both off
    // low = blue on
    // hiZ = amber on
    nv::gpio::GpioPort led_tristate_ctrl_port;
    nv::gpio::GpioPin  led_tristate_ctrl_pin;
};

// means for nhp task to propagate inputs to pca9555 and hssm
struct E1sInputPins
{
    // drive present low - input
    nv::gpio::GpioPort prsnt_l_port;
    nv::gpio::GpioPin  prsnt_l_pin;

    // monitor 12V comparator - input
    sys::adc::AdcPeripheral pgood_peripheral;
    sys::adc::AdcChannel    pgood_channel;
};

// NHP Control Register Bits
// applied to e.1s io expander (pca9555 emulator)
constexpr static uint16_t NhpForcePerstLBit     = 0U;
constexpr static uint16_t NhpForcePerstLBitMask = 1U << NhpForcePerstLBit;

constexpr static uint16_t NhpClkEnBit     = 1U;
constexpr static uint16_t NhpClkEnBitmask = 1U << NhpClkEnBit;

constexpr static uint16_t NhpPrsnt0LBit     = 2U;
constexpr static uint16_t NhpPrsnt0LBitmask = 1U << NhpPrsnt0LBit;

constexpr static uint16_t NhpActivityBit     = 3U;                    // unused
constexpr static uint16_t NhpActivityBitmask = 1U << NhpActivityBit;  // unused

// blue is active low (amber is active high)
constexpr static uint16_t NhpBlueLedLBit     = 4U;
constexpr static uint16_t NhpBlueLedLBitmask = 1U << NhpBlueLedLBit;

constexpr static uint16_t NhpClkreqLBit     = 5U;                   // unused
constexpr static uint16_t NhpClkreqLBitmask = 1U << NhpClkreqLBit;  // unused

// NOTE: On P3957 we may or may not be controlling pwren (drive present may be controlling)
constexpr static uint16_t NhpPwrEnBit     = 6U;
constexpr static uint16_t NhpPwrEnBitmask = 1U << NhpPwrEnBit;

constexpr static uint16_t NhpPwrdisBit     = 7U;
constexpr static uint16_t NhpPwrdisBitmask = 1U << NhpPwrdisBit;

// NOTE: On P3957 we may or may not be controlling pwren (drive present may be controlling)
//     So pgood only tells us if a fault occurred if both:
//         1. drive is present and pwr is enabled (we know for sure power is enabled)
//         2. pgood is low
constexpr static uint16_t NhpPwrGdBit     = 8U;
constexpr static uint16_t NhpPwrGdBitmask = 1U << NhpPwrGdBit;

constexpr static uint16_t NhpPrsnt1LBit     = 9U;                   // unused
constexpr static uint16_t NhpPrsnt1LBitmask = 1U << NhpPrsnt1LBit;  // unused

constexpr static uint16_t NhpPrsnt2LBit     = 10U;                  // unused
constexpr static uint16_t NhpPrsnt2LBitmask = 1U << NhpPrsnt2LBit;  // unused

// amber is active high (blue is active low)
constexpr static uint16_t NhpAmberLedBit     = 11U;
constexpr static uint16_t NhpAmberLedBitmask = 1U << NhpAmberLedBit;

constexpr static uint16_t NhpAttnBtnLBit     = 12U;                   // unused
constexpr static uint16_t NhpAttnBtnLBitmask = 1U << NhpAttnBtnLBit;  // unused

}  // namespace nv::nhp
