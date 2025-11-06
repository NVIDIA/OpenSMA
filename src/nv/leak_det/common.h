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
#include <array>

namespace nv::leak_detect {

constexpr uint16_t AdcVolVref        = 3317;  // ADC reference voltage in mV
constexpr uint16_t AdcResolutionBits = 16;    // ADC resolution (16-bit)
constexpr uint32_t AdcFullScale      = (1 << AdcResolutionBits) - 1;

constexpr auto MaxOneShotConvTime = 10;  // uint: 50ms -> 500ms

constexpr uint16_t inline to_adc_value(uint16_t threshhold)
{
    // coverity[cert_int31_c_violation] -- safe: no data loss
    return threshhold * AdcFullScale / AdcVolVref;
}

constexpr uint16_t inline to_vol_value(uint16_t adcreading)
{
    return static_cast<uint32_t>(adcreading) * static_cast<uint32_t>(AdcVolVref)
         / static_cast<uint32_t>(AdcFullScale);
}

constexpr uint16_t MinVol = 0;
constexpr uint16_t MaxVol = AdcVolVref;

using SensorId     = uint8_t;
using ThresholdNum = uint8_t;

/*********************************************
 *           Leak Detect Error Code          *
 *********************************************/
enum class Status : uint8_t
{
    // nominal code
    Ok = 0,

    // error code
    AdcOneShotConvTimeout,
    AdcIsrNoValidReading,
    AdcIsrNoValidSensorId,
    InvalidAdcInstance,
    InvalidSensorId,
    NoValidSensor,
    InvalidThreshold,
    InvalidSensorInfoSize,
    NoMatchedSensorId,
};

/*********************************************
 * Defines based on mcxn236/mcxn547 ADC Spec *
 *********************************************/
enum class AdcTriggerSrc : uint32_t
{
    _0 = 0,
    _1,
    _2,
    _3,
    Invalid
};

enum class AdcChannel : uint8_t
{
    _0 = 0,
    _1,
    _2,
    _3,
    _4,
    _5,
    _6,
    _7,
    _8,
    _9,
    _10,
    _11,
    _12,
    _13,
    _14,
    _15,
    _16,
    _17,
    _18,
    _19,
    _20,
    _21,
    _22,
    _23,
    _24,
    _25,
    _26,
    _27,
    _28,
    _29,
    _30,
    _31,
    Invalid
};

enum class AdcCommand : uint8_t
{
    None = 0,
    _1,
    _2,
    _3,
    _4,
    _5,
    _6,
    _7,
    _8,
    _9,
    _10,
    _11,
    _12,
    _13,
    _14,
    _15,
    Invalid
};

enum class AdcInstance : uint8_t
{
    _0 = 0,
    _1,
    Total,
    Invalid = Total
};

enum class AdcDataResultFifo : uint8_t
{
    _0 = 0,
    _1,
    Invalid
};

enum class AdcScanMode : uint8_t
{
    SingleEndedSideA       = 0,
    SingleEndedSideB       = 1,
    DifferentialBothSideAB = 2,
    Invalid
};

constexpr uint32_t Fifo0WatermarkInterruptEnable = 0x01;

/*********************************************
 *      Defines based on NSM Type 3 Spec     *
 *********************************************/
enum class State : uint8_t
{
    Nominal = 0,
    Leak    = 1,
    Short   = 2,
    Open    = 3,
    Invalid
};

using Threshold = uint16_t;
using Reading   = uint16_t;

/*********************************************
 *     Defines based on virtual gpio spec    *
 *********************************************/
constexpr size_t VirtualGpioNum = 2;

/***********************************************
 * Defines based on Leak Detect implementation *
 ***********************************************/
struct LeakDetectSensor
{
    State        state;  // Current sensor state
    SensorId     id;     // Sensor ID
    ThresholdNum thNum;  // Number of thresholds

    AdcInstance adcId;     // ADC instance number (0 ~ 1)
    AdcChannel  channel;   // ADC channel number (0 ~ 31)
    AdcScanMode scanMode;  // ADC scan mode (lpadc_sample_channel_mode_t enum value)

    AdcCommand    cmdScanning;  // ADC command number in command chaining for scanning mode
    AdcCommand    cmdNext;      // Next ADC command number in command chaining for scanning mode
    AdcCommand    cmdOneShot;   // ADC command number for one shot sampling mode
    AdcTriggerSrc cmdTriggerSrc;  // ADC trigger source number (0 ~ 3)

    Reading   reading;    // Current ADC reading
    Threshold minLeak;    // min leak threshold
    Threshold maxLeak;    // max leak threshold
    Threshold maxNormal;  // max normal threshold

    uint8_t                             ioxAddr;  // IOX address
    std::array<uint8_t, VirtualGpioNum> ioxPin;   // IOX pin numbers
};

enum class AdcMode : uint8_t
{
    Scanning,
    OneShot,
    Invalid
};

struct ThresholdConfig
{
    Threshold minLeak;    // min leak threshold
    Threshold maxLeak;    // max leak threshold
    Threshold maxNormal;  // max normal threshold
};

enum class VrGpioState : uint8_t
{                    // Virtual GPIO state
    Nominal = 0x00,  // Nominal (no leak)
    Leak    = 0x01,  // Leak detected
    Short   = 0x02,  // Sensor short fault
    Open    = 0x03   // Sensor open fault
};

enum class HwGpioState : uint8_t
{  // Hardware GPIO state
    Low  = 0x00,
    High = 0x01
};

enum class NsmEventType : uint8_t
{
    LeakDetected     = 0x01,  // Leak detection event ID
    SensorFaultOpen  = 0x02,  // Sensor fault open event ID
    SensorFaultShort = 0x03,  // Sensor fault short event ID
    ThresholdUpdate  = 0x04   // Threshold update event ID
};

constexpr AdcTriggerSrc AdcCmdTriggerSrc     = AdcTriggerSrc::_0;
constexpr AdcCommand    AdcCmdScanAllSensors = AdcCommand::_1;

static_assert(static_cast<uint8_t>(VrGpioState::Nominal)
                  == static_cast<uint8_t>(State::Nominal),
              "VrGpioState::Nominal must be State::Nominal");
static_assert(static_cast<uint8_t>(VrGpioState::Leak) == static_cast<uint8_t>(State::Leak),
              "VrGpioState::Leak must be State::Leak");
static_assert(static_cast<uint8_t>(VrGpioState::Short) == static_cast<uint8_t>(State::Short),
              "VrGpioState::Short must be State::Short");
static_assert(static_cast<uint8_t>(VrGpioState::Open) == static_cast<uint8_t>(State::Open),
              "VrGpioState::Open must be State::Open");
}  // namespace nv::leak_detect