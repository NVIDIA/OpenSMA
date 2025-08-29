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
#include "nv/vpp/vpp.h"
#include "nv/vpp/task.h"
#include NV_IPC_CONFIG_H

namespace nv::nhp {

// NHP PROTOCOL CONFIG
// -------------------------------------
// How many NHP instances are managed by this MCU
static_assert((NumNhpInstances == 4), "This VPP Project must have 4 NHP instances");

// Number of Drives For Each NHP Channel
static_assert(NumE1sDrives % 2 == 0, "NHP can only have only pairs of drives");
static_assert(NumE1sDrives <= 8, "NHP has 8 drives max");
static_assert(NumE1sDrives == 2, "This VPP Project must have 2 drives per NHP instance");

// NHP Mapping Version - 3 bits
static_assert(MappingVersion <= 7U, "MappingVersion must be 3 bits max");

// 2 bits max - 8/4/2/1 drives = 3/2/1/0
static_assert(NumOfPartitions <= 0b11,
              "Partition Register over 2 bits (NumE1sDrives too high)");

// Drive 0 Configuration
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
    .pgood_peripheral = ipc::MCU_VMON_12V_SSD0_DIV_PERIPH,
    .pgood_channel    = ipc::MCU_VMON_12V_SSD0_DIV_CMD
    //
};

// Drive 1 Configuration
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
    .pgood_peripheral = ipc::MCU_VMON_12V_SSD1_DIV_PERIPH,
    .pgood_channel    = ipc::MCU_VMON_12V_SSD1_DIV_CMD
    //
};

// Drive 2 Configuration
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
    .pgood_peripheral = ipc::MCU_VMON_12V_SSD2_DIV_PERIPH,
    .pgood_channel    = ipc::MCU_VMON_12V_SSD2_DIV_CMD
    //
};

// Drive 3 Configuration
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
    .pgood_peripheral = ipc::MCU_VMON_12V_SSD3_DIV_PERIPH,
    .pgood_channel    = ipc::MCU_VMON_12V_SSD3_DIV_CMD
    //
};

// Drive 4 Configuration
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
    .pgood_peripheral = ipc::MCU_VMON_12V_SSD4_DIV_PERIPH,
    .pgood_channel    = ipc::MCU_VMON_12V_SSD4_DIV_CMD
    //
};

// Drive 5 Configuration
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
    .pgood_peripheral = ipc::MCU_VMON_12V_SSD5_DIV_PERIPH,
    .pgood_channel    = ipc::MCU_VMON_12V_SSD5_DIV_CMD
    //
};

// Drive 6 Configuration
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
    .pgood_peripheral = ipc::MCU_VMON_12V_SSD6_DIV_PERIPH,
    .pgood_channel    = ipc::MCU_VMON_12V_SSD6_DIV_CMD
    //
};

// Drive 7 Configuration
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
    .pgood_peripheral = ipc::MCU_VMON_12V_SSD7_DIV_PERIPH,
    .pgood_channel    = ipc::MCU_VMON_12V_SSD7_DIV_CMD
    //
};

// MCI0 Configuration
// -----------------------------
constexpr nv::gpio::GpioPort Nhp0InterruptPort = ipc::VPP_CPU0_ALERT_L_PORT;
constexpr nv::gpio::GpioPin  Nhp0InterruptPin  = ipc::VPP_CPU0_ALERT_L_PIN;

constexpr std::array<E1sOutputPins, NumE1sDrives> Nhp0driveOutputPins = {Drive0OutputPins,
                                                                         Drive1OutputPins};
constexpr std::array<E1sInputPins, NumE1sDrives>  Nhp0driveInputPins  = {Drive0InputPins,
                                                                         Drive1InputPins};

// MCI1 Configuration
// -----------------------------
constexpr nv::gpio::GpioPort Nhp1InterruptPort = ipc::VPP_CPU0B_ALERT_L_PORT;
constexpr nv::gpio::GpioPin  Nhp1InterruptPin  = ipc::VPP_CPU0B_ALERT_L_PIN;

constexpr std::array<E1sOutputPins, NumE1sDrives> Nhp1driveOutputPins = {Drive2OutputPins,
                                                                         Drive3OutputPins};
constexpr std::array<E1sInputPins, NumE1sDrives>  Nhp1driveInputPins  = {Drive2InputPins,
                                                                         Drive3InputPins};

// MCI2 Configuration
// -----------------------------
constexpr nv::gpio::GpioPort Nhp2InterruptPort = ipc::VPP_CPU1_ALERT_L_PORT;
constexpr nv::gpio::GpioPin  Nhp2InterruptPin  = ipc::VPP_CPU1_ALERT_L_PIN;

constexpr std::array<E1sOutputPins, NumE1sDrives> Nhp2driveOutputPins = {Drive4OutputPins,
                                                                         Drive5OutputPins};
constexpr std::array<E1sInputPins, NumE1sDrives>  Nhp2driveInputPins  = {Drive4InputPins,
                                                                         Drive5InputPins};

// MCI3 Configuration
// -----------------------------
constexpr nv::gpio::GpioPort Nhp3InterruptPort = ipc::VPP_CPU1B_ALERT_L_PORT;
constexpr nv::gpio::GpioPin  Nhp3InterruptPin  = ipc::VPP_CPU1B_ALERT_L_PIN;

constexpr std::array<E1sOutputPins, NumE1sDrives> Nhp3driveOutputPins = {Drive6OutputPins,
                                                                         Drive7OutputPins};
constexpr std::array<E1sInputPins, NumE1sDrives>  Nhp3driveInputPins  = {Drive6InputPins,
                                                                         Drive7InputPins};

// -------------------------------------
// Hotplug Configuration Specific to VPP Mode
constexpr static nv::vpp::VPP::VppConfig vppConfig0 = {
    .i2c_bus        = static_cast<i2c::Port>(ipc::VPP_CPU0_I2C_PORT),
    .output_pins    = nhp::Nhp0driveOutputPins,
    .input_pins     = nhp::Nhp0driveInputPins,
    .interrupt_port = nhp::Nhp0InterruptPort,
    .interrupt_pin  = nhp::Nhp0InterruptPin,
    .vppInstanceNum = 0
    //
};

constexpr static nv::vpp::VPP::VppConfig vppConfig1 = {
    .i2c_bus        = static_cast<i2c::Port>(ipc::VPP_CPU0B_I2C_PORT),
    .output_pins    = nhp::Nhp1driveOutputPins,
    .input_pins     = nhp::Nhp1driveInputPins,
    .interrupt_port = nhp::Nhp1InterruptPort,
    .interrupt_pin  = nhp::Nhp1InterruptPin,
    .vppInstanceNum = 1
    //
};

constexpr static nv::vpp::VPP::VppConfig vppConfig2 = {
    .i2c_bus        = static_cast<i2c::Port>(ipc::VPP_CPU1_I2C_PORT),
    .output_pins    = nhp::Nhp2driveOutputPins,
    .input_pins     = nhp::Nhp2driveInputPins,
    .interrupt_port = nhp::Nhp2InterruptPort,
    .interrupt_pin  = nhp::Nhp2InterruptPin,
    .vppInstanceNum = 2
    //
};

constexpr static nv::vpp::VPP::VppConfig vppConfig3 = {
    .i2c_bus        = static_cast<i2c::Port>(ipc::VPP_CPU1B_I2C_PORT),
    .output_pins    = nhp::Nhp3driveOutputPins,
    .input_pins     = nhp::Nhp3driveInputPins,
    .interrupt_port = nhp::Nhp3InterruptPort,
    .interrupt_pin  = nhp::Nhp3InterruptPin,
    .vppInstanceNum = 3
    //
};

constexpr static nv::vpp::TaskConfig vpp_task_config = {
    // NOLINTNEXTLINE: tidy not seeing ipc config header macro
    .task_id        = nv::ipc::TaskId::NHP, // NOLINT: not seeing ipc config header macro
    .task_name      = "VPP",
    .vpp_configs    = {vppConfig0, vppConfig1, vppConfig2, vppConfig3},
    .hotSwapEventId = nv::ipc::EventId::Nhp  // NOLINT: not seeing ipc config header macro
};

}  // namespace nv::nhp
