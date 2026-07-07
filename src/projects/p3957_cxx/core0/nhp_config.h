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

#include <chrono>

#include "nv/gpio/common.h"
#include "nv/ahs/ahs.h"
#include "nv/ahs/task.h"
#include NV_IPC_CONFIG_H

namespace nv::nhp {

// AHS PROTOCOL CONFIG

// When true, firmware drives the PERST_L / CLK_EN_L / PWR_EN_L / PWR_DIS
// pins from the HSSM state machine. When false, the platform delegates
// that pin sequencing to external hardware (CPLD / backplane) and
// firmware only manages LED indication.
constexpr bool EnableFwHssmPinControl = false;

// Schematic Note: (this would seem to apply to A02, B00, C00, C01 and C02)
//
// J2 - MCIO0 --+-- J6 (SSD0) : VPP0
//              +-- J7 (SSD1) : VPP0
// J3 - MCIO1 --+-- J8 (SSD2) : VPP0b
//              +-- J9 (SSD3) : VPP0b
// J4 - MCIO2 --+- J10 (SSD4) : VPP1
//              +- J11 (SSD5) : VPP1
// J5 - MCIO3 --+- J12 (SSD6) : VPP1b
//              +- J13 (SSD7) : VPP1b
//
// From the physical system/backplane perspective, the SSD drive pairs are swapped
// from how they are labeled on the schematic.  The following table is how they
// should be mapped in firmware to the AHS configuration, using the physical
// perspective of the SSD drive pairs.
// J2 - MCIO0 --+-- J6 (SSD1) : VPP0  / AHS 0, Drive 1 : GPIOEXP 0, Bank 1
//              +-- J7 (SSD0) : VPP0  / AHS 0, Drive 0 : GPIOEXP 0, Bank 0
// J3 - MCIO1 --+-- J8 (SSD3) : VPP0b / AHS 0, Drive 3 : GPIOEXP 1, Bank 1
//              +-- J9 (SSD2) : VPP0b / AHS 0, Drive 2 : GPIOEXP 1, Bank 0
// J4 - MCIO2 --+- J10 (SSD5) : VPP1  / AHS 1, Drive 1 : GPIOEXP 0, Bank 1
//              +- J11 (SSD4) : VPP1  / AHS 1, Drive 0 : GPIOEXP 0, Bank 0
// J5 - MCIO3 --+- J12 (SSD7) : VPP1b / AHS 1, Drive 3 : GPIOEXP 1, Bank 1
//              +- J13 (SSD6) : VPP1b / AHS 1, Drive 2 : GPIOEXP 1, Bank 0

// The drive configurations below are based on the current schematic symbols and names.

// Drive 0 Configuration (J6-SSD0 on schematic)
// -----------------------------
constexpr nv::nhp::E1sOutputPins Drive0OutputPins{
    .perst_l_port           = ipc::MCU_SSD0_PERST_L_PORT,
    .perst_l_pin            = ipc::MCU_SSD0_PERST_L_PIN,
    .clk_en_l_port          = ipc::MCU_CLK_SSD0_EN_N_PORT,
    .clk_en_l_pin           = ipc::MCU_CLK_SSD0_EN_N_PIN,
    .pwrdis_port            = ipc::SSD0_PWRDIS_PORT,
    .pwrdis_pin             = ipc::SSD0_PWRDIS_PIN,
    .pwren_l_port           = ipc::MCU_V_SSD0_DIS_PORT,
    .pwren_l_pin            = ipc::MCU_V_SSD0_DIS_PIN,
    .led_tristate_ctrl_port = ipc::SSD0_LED_PORT,
    .led_tristate_ctrl_pin  = ipc::SSD0_LED_PIN
    //
};
constexpr nv::nhp::E1sInputPins Drive0InputPins{
    .prsnt_l_port     = ipc::SSD0_PRSNT_L_PORT,
    .prsnt_l_pin      = ipc::SSD0_PRSNT_L_PIN,
    .perst_l_port     = ipc::PCIE_SSD0_PERST_L_PORT,
    .perst_l_pin      = ipc::PCIE_SSD0_PERST_L_PIN,
    .pgood_peripheral = ipc::MCU_VMON_12V_SSD0_DIV_PERIPH,
    .pgood_channel    = ipc::MCU_VMON_12V_SSD0_DIV_CMD
    //
};

// Drive 1 Configuration (J7-SSD1 on schematic)
// -----------------------------
constexpr nv::nhp::E1sOutputPins Drive1OutputPins{
    .perst_l_port           = ipc::MCU_SSD1_PERST_L_PORT,
    .perst_l_pin            = ipc::MCU_SSD1_PERST_L_PIN,
    .clk_en_l_port          = ipc::MCU_CLK_SSD1_EN_N_PORT,
    .clk_en_l_pin           = ipc::MCU_CLK_SSD1_EN_N_PIN,
    .pwrdis_port            = ipc::SSD1_PWRDIS_PORT,
    .pwrdis_pin             = ipc::SSD1_PWRDIS_PIN,
    .pwren_l_port           = ipc::MCU_V_SSD1_DIS_PORT,
    .pwren_l_pin            = ipc::MCU_V_SSD1_DIS_PIN,
    .led_tristate_ctrl_port = ipc::SSD1_LED_PORT,
    .led_tristate_ctrl_pin  = ipc::SSD1_LED_PIN
    //
};
constexpr nv::nhp::E1sInputPins Drive1InputPins{
    .prsnt_l_port     = ipc::SSD1_PRSNT_L_PORT,
    .prsnt_l_pin      = ipc::SSD1_PRSNT_L_PIN,
    .perst_l_port     = ipc::PCIE_SSD1_PERST_L_PORT,
    .perst_l_pin      = ipc::PCIE_SSD1_PERST_L_PIN,
    .pgood_peripheral = ipc::MCU_VMON_12V_SSD1_DIV_PERIPH,
    .pgood_channel    = ipc::MCU_VMON_12V_SSD1_DIV_CMD
    //
};

// Drive 2 Configuration (J8-SSD2 on schematic)
// -----------------------------
constexpr nv::nhp::E1sOutputPins Drive2OutputPins{
    .perst_l_port           = ipc::MCU_SSD2_PERST_L_PORT,
    .perst_l_pin            = ipc::MCU_SSD2_PERST_L_PIN,
    .clk_en_l_port          = ipc::MCU_CLK_SSD2_EN_N_PORT,
    .clk_en_l_pin           = ipc::MCU_CLK_SSD2_EN_N_PIN,
    .pwrdis_port            = ipc::SSD2_PWRDIS_PORT,
    .pwrdis_pin             = ipc::SSD2_PWRDIS_PIN,
    .pwren_l_port           = ipc::MCU_V_SSD2_DIS_PORT,
    .pwren_l_pin            = ipc::MCU_V_SSD2_DIS_PIN,
    .led_tristate_ctrl_port = ipc::SSD2_LED_PORT,
    .led_tristate_ctrl_pin  = ipc::SSD2_LED_PIN
    //
};
constexpr nv::nhp::E1sInputPins Drive2InputPins{
    .prsnt_l_port     = ipc::SSD2_PRSNT_L_PORT,
    .prsnt_l_pin      = ipc::SSD2_PRSNT_L_PIN,
    .perst_l_port     = ipc::PCIE_SSD2_PERST_L_PORT,
    .perst_l_pin      = ipc::PCIE_SSD2_PERST_L_PIN,
    .pgood_peripheral = ipc::MCU_VMON_12V_SSD2_DIV_PERIPH,
    .pgood_channel    = ipc::MCU_VMON_12V_SSD2_DIV_CMD
    //
};

// Drive 3 Configuration (J9-SSD3 on schematic)
// -----------------------------
constexpr nv::nhp::E1sOutputPins Drive3OutputPins{
    .perst_l_port           = ipc::MCU_SSD3_PERST_L_PORT,
    .perst_l_pin            = ipc::MCU_SSD3_PERST_L_PIN,
    .clk_en_l_port          = ipc::MCU_CLK_SSD3_EN_N_PORT,
    .clk_en_l_pin           = ipc::MCU_CLK_SSD3_EN_N_PIN,
    .pwrdis_port            = ipc::SSD3_PWRDIS_PORT,
    .pwrdis_pin             = ipc::SSD3_PWRDIS_PIN,
    .pwren_l_port           = ipc::MCU_V_SSD3_DIS_PORT,
    .pwren_l_pin            = ipc::MCU_V_SSD3_DIS_PIN,
    .led_tristate_ctrl_port = ipc::SSD3_LED_PORT,
    .led_tristate_ctrl_pin  = ipc::SSD3_LED_PIN
    //
};
constexpr nv::nhp::E1sInputPins Drive3InputPins{
    .prsnt_l_port     = ipc::SSD3_PRSNT_L_PORT,
    .prsnt_l_pin      = ipc::SSD3_PRSNT_L_PIN,
    .perst_l_port     = ipc::PCIE_SSD3_PERST_L_PORT,
    .perst_l_pin      = ipc::PCIE_SSD3_PERST_L_PIN,
    .pgood_peripheral = ipc::MCU_VMON_12V_SSD3_DIV_PERIPH,
    .pgood_channel    = ipc::MCU_VMON_12V_SSD3_DIV_CMD
    //
};

// Drive 4 Configuration (J10-SSD4 on schematic)
// -----------------------------
constexpr nv::nhp::E1sOutputPins Drive4OutputPins{
    .perst_l_port           = ipc::MCU_SSD4_PERST_L_PORT,
    .perst_l_pin            = ipc::MCU_SSD4_PERST_L_PIN,
    .clk_en_l_port          = ipc::MCU_CLK_SSD4_EN_N_PORT,
    .clk_en_l_pin           = ipc::MCU_CLK_SSD4_EN_N_PIN,
    .pwrdis_port            = ipc::SSD4_PWRDIS_PORT,
    .pwrdis_pin             = ipc::SSD4_PWRDIS_PIN,
    .pwren_l_port           = ipc::MCU_V_SSD4_DIS_PORT,
    .pwren_l_pin            = ipc::MCU_V_SSD4_DIS_PIN,
    .led_tristate_ctrl_port = ipc::SSD4_LED_PORT,
    .led_tristate_ctrl_pin  = ipc::SSD4_LED_PIN
    //
};
constexpr nv::nhp::E1sInputPins Drive4InputPins{
    .prsnt_l_port     = ipc::SSD4_PRSNT_L_PORT,
    .prsnt_l_pin      = ipc::SSD4_PRSNT_L_PIN,
    .perst_l_port     = ipc::PCIE_SSD4_PERST_L_PORT,
    .perst_l_pin      = ipc::PCIE_SSD4_PERST_L_PIN,
    .pgood_peripheral = ipc::MCU_VMON_12V_SSD4_DIV_PERIPH,
    .pgood_channel    = ipc::MCU_VMON_12V_SSD4_DIV_CMD
    //
};

// Drive 5 Configuration (J11-SSD5 on schematic)
// -----------------------------
constexpr nv::nhp::E1sOutputPins Drive5OutputPins{
    .perst_l_port           = ipc::MCU_SSD5_PERST_L_PORT,
    .perst_l_pin            = ipc::MCU_SSD5_PERST_L_PIN,
    .clk_en_l_port          = ipc::MCU_CLK_SSD5_EN_N_PORT,
    .clk_en_l_pin           = ipc::MCU_CLK_SSD5_EN_N_PIN,
    .pwrdis_port            = ipc::SSD5_PWRDIS_PORT,
    .pwrdis_pin             = ipc::SSD5_PWRDIS_PIN,
    .pwren_l_port           = ipc::MCU_V_SSD5_DIS_PORT,
    .pwren_l_pin            = ipc::MCU_V_SSD5_DIS_PIN,
    .led_tristate_ctrl_port = ipc::SSD5_LED_PORT,
    .led_tristate_ctrl_pin  = ipc::SSD5_LED_PIN
    //
};
constexpr nv::nhp::E1sInputPins Drive5InputPins{
    .prsnt_l_port     = ipc::SSD5_PRSNT_L_PORT,
    .prsnt_l_pin      = ipc::SSD5_PRSNT_L_PIN,
    .perst_l_port     = ipc::PCIE_SSD5_PERST_L_PORT,
    .perst_l_pin      = ipc::PCIE_SSD5_PERST_L_PIN,
    .pgood_peripheral = ipc::MCU_VMON_12V_SSD5_DIV_PERIPH,
    .pgood_channel    = ipc::MCU_VMON_12V_SSD5_DIV_CMD
    //
};

// Drive 6 Configuration (J12-SSSD6 on schematic)
// -----------------------------
constexpr nv::nhp::E1sOutputPins Drive6OutputPins{
    .perst_l_port           = ipc::MCU_SSD6_PERST_L_PORT,
    .perst_l_pin            = ipc::MCU_SSD6_PERST_L_PIN,
    .clk_en_l_port          = ipc::MCU_CLK_SSD6_EN_N_PORT,
    .clk_en_l_pin           = ipc::MCU_CLK_SSD6_EN_N_PIN,
    .pwrdis_port            = ipc::SSD6_PWRDIS_PORT,
    .pwrdis_pin             = ipc::SSD6_PWRDIS_PIN,
    .pwren_l_port           = ipc::MCU_V_SSD6_DIS_PORT,
    .pwren_l_pin            = ipc::MCU_V_SSD6_DIS_PIN,
    .led_tristate_ctrl_port = ipc::SSD6_LED_PORT,
    .led_tristate_ctrl_pin  = ipc::SSD6_LED_PIN
    //
};
constexpr nv::nhp::E1sInputPins Drive6InputPins{
    .prsnt_l_port     = ipc::SSD6_PRSNT_L_PORT,
    .prsnt_l_pin      = ipc::SSD6_PRSNT_L_PIN,
    .perst_l_port     = ipc::PCIE_SSD6_PERST_L_PORT,
    .perst_l_pin      = ipc::PCIE_SSD6_PERST_L_PIN,
    .pgood_peripheral = ipc::MCU_VMON_12V_SSD6_DIV_PERIPH,
    .pgood_channel    = ipc::MCU_VMON_12V_SSD6_DIV_CMD
    //
};

// Drive 7 Configuration (J13-SSD7 on schematic)
// -----------------------------
constexpr nv::nhp::E1sOutputPins Drive7OutputPins{
    .perst_l_port           = ipc::MCU_SSD7_PERST_L_PORT,
    .perst_l_pin            = ipc::MCU_SSD7_PERST_L_PIN,
    .clk_en_l_port          = ipc::MCU_CLK_SSD7_EN_N_PORT,
    .clk_en_l_pin           = ipc::MCU_CLK_SSD7_EN_N_PIN,
    .pwrdis_port            = ipc::SSD7_PWRDIS_PORT,
    .pwrdis_pin             = ipc::SSD7_PWRDIS_PIN,
    .pwren_l_port           = ipc::MCU_V_SSD7_DIS_PORT,
    .pwren_l_pin            = ipc::MCU_V_SSD7_DIS_PIN,
    .led_tristate_ctrl_port = ipc::SSD7_LED_PORT,
    .led_tristate_ctrl_pin  = ipc::SSD7_LED_PIN
    //
};
constexpr nv::nhp::E1sInputPins Drive7InputPins{
    .prsnt_l_port     = ipc::SSD7_PRSNT_L_PORT,
    .prsnt_l_pin      = ipc::SSD7_PRSNT_L_PIN,
    .perst_l_port     = ipc::PCIE_SSD7_PERST_L_PORT,
    .perst_l_pin      = ipc::PCIE_SSD7_PERST_L_PIN,
    .pgood_peripheral = ipc::MCU_VMON_12V_SSD7_DIV_PERIPH,
    .pgood_channel    = ipc::MCU_VMON_12V_SSD7_DIV_CMD
    //
};

// The drive mappings below are using definitions from the schematic, but
// are swapped per drive pair to match the physical system/backplane perspective.

// MCIO0 Configuration (J2-MCIO0 on schematic)
// -----------------------------
constexpr nv::gpio::GpioPort Nhp0InterruptPort = ipc::VPP_CPU0_ALERT_L_PORT;
constexpr nv::gpio::GpioPin  Nhp0InterruptPin  = ipc::VPP_CPU0_ALERT_L_PIN;

constexpr std::array<E1sOutputPins, NumE1sDrives> Nhp0driveOutputPins = {
    Drive1OutputPins, Drive0OutputPins, Drive3OutputPins, Drive2OutputPins};
constexpr std::array<E1sInputPins, NumE1sDrives> Nhp0driveInputPins = {
    Drive1InputPins, Drive0InputPins, Drive3InputPins, Drive2InputPins};

// MCIO2 Configuration (J4-MCIO2 on schematic)
// -----------------------------
constexpr nv::gpio::GpioPort Nhp1InterruptPort = ipc::VPP_CPU1_ALERT_L_PORT;
constexpr nv::gpio::GpioPin  Nhp1InterruptPin  = ipc::VPP_CPU1_ALERT_L_PIN;

constexpr std::array<E1sOutputPins, NumE1sDrives> Nhp1driveOutputPins = {
    Drive5OutputPins, Drive4OutputPins, Drive7OutputPins, Drive6OutputPins};
constexpr std::array<E1sInputPins, NumE1sDrives> Nhp1driveInputPins = {
    Drive5InputPins, Drive4InputPins, Drive7InputPins, Drive6InputPins};

// AHS Address Definitions
// -----------------------------
constexpr uint8_t AhsEepromAddress0   = 0x50;
constexpr uint8_t AhsExpanderAddress0 = 0x20;
constexpr uint8_t AhsInstanceNum0     = 0;

constexpr uint8_t AhsEepromAddress1   = 0x54;
constexpr uint8_t AhsExpanderAddress1 = 0x24;
constexpr uint8_t AhsInstanceNum1     = 1;

// -------------------------------------
// Hotplug Configuration Specific to AHS Mode
constexpr static nv::ahs::AHS::AhsConfig ahsConfig0 = {
    .i2c_bus             = ipc::VPP_CPU0_I2C_PORT,
    .output_pins         = nhp::Nhp0driveOutputPins,
    .input_pins          = nhp::Nhp0driveInputPins,
    .interrupt_port      = nhp::Nhp0InterruptPort,
    .interrupt_pin       = nhp::Nhp0InterruptPin,
    .eeprom_address      = nhp::AhsEepromAddress0,
    .io_expander_address = nhp::AhsExpanderAddress0,
    .ahsInstanceNum      = nhp::AhsInstanceNum0
    //
};

constexpr static nv::ahs::AHS::AhsConfig ahsConfig1 = {
    .i2c_bus             = ipc::VPP_CPU1_I2C_PORT,
    .output_pins         = nhp::Nhp1driveOutputPins,
    .input_pins          = nhp::Nhp1driveInputPins,
    .interrupt_port      = nhp::Nhp1InterruptPort,
    .interrupt_pin       = nhp::Nhp1InterruptPin,
    .eeprom_address      = nhp::AhsEepromAddress1,
    .io_expander_address = nhp::AhsExpanderAddress1,
    .ahsInstanceNum      = nhp::AhsInstanceNum1
    //
};

constexpr static nv::ahs::TaskConfig ahs_task_config = {
    // NOLINTNEXTLINE: tidy not seeing ipc config header macro
    .task_id        = nv::ipc::TaskId::NHP, // NOLINT: not seeing ipc config header macro
    .task_name      = "AHS",
    .ahs_configs    = {ahsConfig0, ahsConfig1},
    .hotSwapEventId = nv::ipc::EventId::Nhp, // NOLINT: not seeing ipc config header macro
    .delay_time     = std::chrono::microseconds(7LL)  // 1.708us per ADC conversion (4 samples)
                                                      // two conversions per trigger event
                                                      // 10us delay between trigger events
};

}  // namespace nv::nhp
