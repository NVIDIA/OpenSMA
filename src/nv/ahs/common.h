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
#include "nv/gpio/common.h"
#include "sys/adc/adc.h"

namespace nv::nhp {

/**
 * @brief Pin configuration structure for e.1s drive output control
 *
 * This structure defines the GPIO pin assignments for controlling e.1s drive
 * output signals. It is used by the hot swap state machine (HSSM) to manage
 * power, clock, reset, and LED control for individual drives.
 */
struct E1sOutputPins
{
    /** @brief PCIe reset control pin - output (active low) */
    nv::gpio::GpioPort perst_l_port = nv::gpio::InvalidGpioPort;
    /** @brief PCIe reset control pin number */
    nv::gpio::GpioPin perst_l_pin = nv::gpio::InvalidGpioPin;

    /** @brief PCIe clock enable control pin - output (active low) */
    nv::gpio::GpioPort clk_en_l_port = nv::gpio::InvalidGpioPort;
    /** @brief PCIe clock enable control pin number */
    nv::gpio::GpioPin clk_en_l_pin = nv::gpio::InvalidGpioPin;

    /** @brief Internal drive power disable control pin - output (active high)*/
    nv::gpio::GpioPort pwrdis_port = nv::gpio::InvalidGpioPort;
    /** @brief Internal drive power disable control pin number */
    nv::gpio::GpioPin pwrdis_pin = nv::gpio::InvalidGpioPin;

    /** @brief 12V power enable control pin - output (active low) */
    nv::gpio::GpioPort pwren_l_port = nv::gpio::InvalidGpioPort;
    /** @brief 12V power enable control pin number */
    nv::gpio::GpioPin pwren_l_pin = nv::gpio::InvalidGpioPin;

    /** @brief LED tristate control pin - output
     *
     * Controls both amber and blue LEDs through tristate logic:
     * - High = both LEDs off
     * - Low = blue LED on
     * - High-Z = amber LED on
     */
    nv::gpio::GpioPort led_tristate_ctrl_port = nv::gpio::InvalidGpioPort;
    /** @brief LED tristate control pin number */
    nv::gpio::GpioPin led_tristate_ctrl_pin = nv::gpio::InvalidGpioPin;
};

/**
 * @brief Pin configuration structure for e.1s drive input monitoring
 *
 * This structure defines the GPIO pin assignments and ADC channels for monitoring
 * e.1s drive input signals. It is used by the NHP task to propagate input status
 * to the PCA9555 IO expander and hot swap state machine.
 */
struct E1sInputPins
{
    /** @brief Drive presence detection pin - input (active low) */
    nv::gpio::GpioPort prsnt_l_port = nv::gpio::InvalidGpioPort;
    /** @brief Drive presence detection pin number */
    nv::gpio::GpioPin prsnt_l_pin = nv::gpio::InvalidGpioPin;

    /** @brief PCIE PERST# pin - input (active low) */
    nv::gpio::GpioPort perst_l_port = nv::gpio::InvalidGpioPort;
    /** @brief PCIE PERST# pin number */
    nv::gpio::GpioPin perst_l_pin = nv::gpio::InvalidGpioPin;

    /** @brief ADC peripheral for power good monitoring */
    sys::adc::AdcPeripheral pgood_peripheral;
    /** @brief ADC channel for power good monitoring */
    sys::adc::AdcChannel pgood_channel;
};

}  // namespace nv::nhp

namespace nv::ahs {

/**
 * @brief AHS Control Register Bits and Constants
 *
 * This namespace contains constants and bit definitions for the AHS control registers
 * that are applied to the e.1s IO expander (PCA9555 emulator). These constants
 * define the pin assignments and functionality for different AHS mapping versions.
 */

// AHS Control Register Bits
// applied to e.1s io expander (pca9555 emulator)

/**
 * @brief SSDx_PRSNT0# pin bit position and mask
 *
 * Mapping Version 0, 1, 2 (Input)
 * This pin indicates the presence of an SSD drive. Active low signal.
 */
constexpr static uint8_t AhsPrsnt0LBit     = 0U;
constexpr static uint8_t AhsPrsnt0LBitmask = 1U << AhsPrsnt0LBit;

/**
 * @brief SSDx_ATTN_BTN# pin bit position and mask
 *
 * Mapping Version 1 (Input)
 * This pin receives attention button presses from the drive. Active low signal.
 */
constexpr static uint8_t AhsAttnBtnLBit     = 1U;
constexpr static uint8_t AhsAttnBtnLBitmask = 1U << AhsAttnBtnLBit;

/**
 * @brief SSDx_PWR_EN pin bit position and mask
 *
 * Mapping Version 1, 2 (Output)
 * Controls power enable for the drive. Note: On P3957, drive presence may
 * be controlling this instead of the AHS controller.
 */
constexpr static uint8_t AhsPwrEnBit     = 2U;
constexpr static uint8_t AhsPwrEnBitmask = 1U << AhsPwrEnBit;

/**
 * @brief SSDx_CLK_EN pin bit position and mask
 *
 * Mapping Version 2 (Output)
 * Controls clock enable for the drive.
 */
constexpr static uint8_t AhsClkEnBit     = 3U;
constexpr static uint8_t AhsClkEnBitmask = 1U << AhsClkEnBit;

/**
 * @brief SSDx_PERST# pin bit position and mask
 *
 * Mapping Version 2 (Output)
 * Controls PCIe reset signal for the drive. Active low signal.
 */
constexpr static uint8_t AhsForcePerstLBit     = 4U;
constexpr static uint8_t AhsForcePerstLBitMask = 1U << AhsForcePerstLBit;

/**
 * @brief SSDx_BLUELED# pin bit position and mask
 *
 * Mapping Version 2 (Output)
 * Controls the blue LED indicator. Blue is active low (amber is active high).
 */
constexpr static uint8_t AhsBlueLedLBit     = 5U;
constexpr static uint8_t AhsBlueLedLBitmask = 1U << AhsBlueLedLBit;

/**
 * @brief SSDx_AmberLED# pin bit position and mask
 *
 * Mapping Version 2 (Output)
 * Controls the amber LED indicator. Amber is active high (blue is active low).
 */
constexpr static uint8_t AhsAmberLedBit     = 6U;
constexpr static uint8_t AhsAmberLedBitmask = 1U << AhsAmberLedBit;

/**
 * @brief SSDx_PWR_GD pin bit position and mask
 *
 * Mapping Version 1, 2 (Input)
 * Power good signal from the drive. Note: On P3957, this may only indicate
 * a fault when both conditions are met:
 * 1. Drive is present and power is enabled (we know for sure power is enabled)
 * 2. Power good is low
 */
constexpr static uint8_t AhsPwrGdBit     = 7U;
constexpr static uint8_t AhsPwrGdBitmask = 1U << AhsPwrGdBit;

}  // namespace nv::ahs
