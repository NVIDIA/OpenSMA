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
#include <cstddef>

namespace nv::volt_mon {

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

using SensorId                     = uint8_t;
constexpr SensorId SensorIdNotUsed = ~0;

/******************************************************************************************
 *                         Volt Monitor Error Code Defines                                *
 ******************************************************************************************/
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
    AdcOneShotConversionOnGoing,
};

/******************************************************************************************
 *                          ADC Trigger Source Defines                                    *
 ******************************************************************************************/
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

/******************************************************************************************
 *                        NSM Type 3 Spec State Defines                                   *
 ******************************************************************************************/
enum class State : uint8_t
{
    /* shared states for all sensors */
    Nominal = 0,
    /* leak detect states */
    Leak  = 1,
    Short = 2,
    Open  = 3,
    /* bus bar temp states */
    LowTemp  = 4,
    HighTemp = 5,
    /* pgood volt states */
    PgoodLow  = 6,
    PgoodHigh = 7,
    /* invalid state */
    Invalid
};

using Threshold = uint16_t;
using Reading   = uint16_t;

constexpr Threshold ThresholdNotUsed = ~0;

/******************************************************************************************
 *                          Volt Monitor Defines                                          *
 ******************************************************************************************/

enum class Sensor : uint8_t
{
    LeakDetect,
    BusBarTemp,
    BusBarVolt,
    McuInternalTemp,
    PgoodVolt,
    Invalid
};

/******************************************************************************************
 *                      Voltage Monitor Base Structure (Common ADC Fields)                *
 ******************************************************************************************/
struct VoltMon
{
    AdcInstance   adcId;        // ADC instance number (0 ~ 1)
    AdcChannel    channel;      // ADC channel number (0 ~ 31)
    AdcScanMode   scanMode;     // ADC scan mode (lpadc_sample_channel_mode_t enum value)
    AdcCommand    cmdScanning;  // ADC command number in command chaining for scanning mode
    AdcCommand    cmdNext;      // Next ADC command number in command chaining for scanning mode
    AdcCommand    cmdOneShot;   // ADC command number for one shot sampling mode
    AdcTriggerSrc cmdTriggerSrc;  // ADC trigger source number (0 ~ 3)
    Reading       reading;        // Current ADC reading
};

/******************************************************************************************
 *                      Pgood Voltage Sensor Structure                                    *
 ******************************************************************************************/
struct PgoodVoltSensor : VoltMon
{
    Sensor   sensor;  // Sensor type (Sensor::PgoodVolt)
    SensorId id;      // Sensor ID (e.g., SSD drive number)
    State    state;   // Sensor state

    Threshold pgoodMin;  // ADC hw compare low threshold
    Threshold pgoodMax;  // ADC hw compare high threshold
};

/******************************************************************************************
 *                      Leak Detect Sensor Structure                                      *
 ******************************************************************************************/
struct LeakDetectSensor : VoltMon
{
    Sensor   sensor;  // Sensor type (Sensor::LeakDetect)
    SensorId id;      // Sensor ID
    State    state;   // Sensor state

    Threshold minLeak;    // min leak threshold
    Threshold maxLeak;    // max leak threshold
    Threshold maxNormal;  // max normal threshold

    // IOX GPIO configuration (2 pins for 2-bit state encoding)
    uint8_t                ioxAddr;  // IOX I2C address
    std::array<uint8_t, 2> ioxPin;   // 2 pins: [0]=bit0, [1]=bit1
};

/******************************************************************************************
 *                      BusBar Temperature Sensor Structure                               *
 ******************************************************************************************/
struct BusBarTempSensor : VoltMon
{
    Sensor   sensor;  // Sensor type (Sensor::BusBarTemp)
    SensorId id;      // Sensor ID
    State    state;   // Sensor state

    // NTC: High temp → Low ADC, Low temp → High ADC
    Threshold busBarHighTemp;  // High temperature threshold (low ADC value)
    Threshold busBarLowTemp;   // Low temperature threshold (high ADC value), usually 0xFFFF

    // IOX GPIO configuration (1 pin for simple on/off state)
    uint8_t                ioxAddr;  // IOX I2C address
    std::array<uint8_t, 1> ioxPin;   // 1 pin: 0=nominal, 1=fault
};

/******************************************************************************************
 *                      BusBar Voltage Sensor Structure                                   *
 ******************************************************************************************/
struct BusBarVoltSensor : VoltMon
{
    Sensor   sensor;  // Sensor type (Sensor::BusBarVolt)
    SensorId id;      // Sensor ID
    State    state;   // Sensor state

    Threshold busBarHighVolt;  // High voltage threshold
    Threshold busBarLowVolt;   // Low voltage threshold

    // IOX GPIO configuration (if needed in the future)
    // uint8_t                ioxAddr;
    // std::array<uint8_t, N> ioxPin;
};

/******************************************************************************************
 *                      MCU Internal Temperature Sensor Structure                         *
 ******************************************************************************************/
// Inherits from VoltMon! Uses independent trigger configuration (Trigger 1, FIFO 1)
// Different from leak/busbar: does NOT participate in command chain (on-demand sampling)
// Special behavior: reads 2 consecutive results from FIFO (vbe1, vbe8) for temperature
// calculation
struct McuInternalTempSensor : VoltMon
{
    Sensor sensor;       // Sensor type (Sensor::McuInternalTemp)
    float  tempCelsius;  // Current temperature in Celsius
};

enum class AdcMode : uint8_t
{
    Scanning,
    OneShot,
    Invalid
};

struct ThresholdLeakDet
{
    Threshold minLeak;    // min leak threshold
    Threshold maxLeak;    // max leak threshold
    Threshold maxNormal;  // max normal threshold
};

struct ThresholdBusbar
{
    // NTC: High temp → Low ADC, Low temp → High ADC
    Threshold busBarHighTemp;  // High temperature threshold (low ADC value)
    Threshold busBarLowTemp;   // Low temperature threshold (high ADC value), usually 0xFFFF
};

enum class VrGpioState : uint8_t
{
    // leak detect states (last 2 bits)
    Nominal = 0x00,  // Nominal (no leak)
    Leak    = 0x01,  // Leak detected
    Short   = 0x02,  // Sensor short fault
    Open    = 0x03,  // Sensor open fault

    // busbar temp/volt states (last 2 bits)
    LowTemp  = 0x04,  // Low temperature
    HighTemp = 0x05,  // High temperature
    LowVolt  = 0x06,  // Low voltage
    HighVolt = 0x07,  // High voltage
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

// Scanning mode: Trigger 0 → CMD 1 (leak detect/busbar) → FIFO 0
constexpr AdcTriggerSrc AdcCmdTriggerSrc     = AdcTriggerSrc::_0;
constexpr AdcCommand    AdcCmdScanAllSensors = AdcCommand::_1;

// Temperature sensor: Independent trigger configuration
// - Trigger 1 → CMD 15 → END → FIFO 1 (on-demand, does not participate in cmd chain)
constexpr AdcTriggerSrc AdcTempTriggerSrc = AdcTriggerSrc::_1;
constexpr uint8_t       AdcTempFifoSelect = 1;

static_assert(static_cast<uint8_t>(VrGpioState::Nominal)
                  == static_cast<uint8_t>(State::Nominal),
              "VrGpioState::Nominal must be State::Nominal");
static_assert(static_cast<uint8_t>(VrGpioState::Leak) == static_cast<uint8_t>(State::Leak),
              "VrGpioState::Leak must be State::Leak");
static_assert(static_cast<uint8_t>(VrGpioState::Short) == static_cast<uint8_t>(State::Short),
              "VrGpioState::Short must be State::Short");
static_assert(static_cast<uint8_t>(VrGpioState::Open) == static_cast<uint8_t>(State::Open),
              "VrGpioState::Open must be State::Open");
}  // namespace nv::volt_mon

// clang-format off
/******************************************************************************************
 *                      Macros for Projects Without Voltage Monitor Features              *
 ******************************************************************************************/

/**
 * @brief Disable leak detection feature
 *
 * Use this macro in voltage_monitor_config namespace for projects that don't use
 * leak detection. It provides all required definitions with zero sensors.
 *
 * @example
 * namespace voltage_monitor_config {
 *     VOLTAGE_MONITOR_DISABLE_LEAK_DETECT
 *     // ... other config ...
 * }
 */
#define VOLTAGE_MONITOR_DISABLE_LEAK_DETECT                                                    \
    constexpr size_t         LeakDetectSensorNum                     = 0;                      \
    constexpr SensorId       LeakDetectSensorId[LeakDetectSensorNum] = {};                     \
    constexpr gpio::GpioPort AlertGpioPort = nv::gpio::InvalidGpioPort;                        \
    constexpr gpio::GpioPin  AlertGpioPin  = nv::gpio::InvalidGpioPin;                         \
    template<size_t Index>                                                                     \
    constexpr nv::volt_mon::LeakDetectSensor leak_detect_get_sensor_config()                   \
    {                                                                                          \
        static_assert(Index < LeakDetectSensorNum, "Sensor index out of range");               \
        return {};                                                                             \
    }

/**
 * @brief Disable Power Good voltage monitoring feature
 *
 * Use this macro in voltage_monitor_config namespace for projects that don't use
 * Power Good voltage monitoring. It provides all required definitions with zero sensors.
 *
 * @example
 * namespace voltage_monitor_config {
 *     VOLTAGE_MONITOR_DISABLE_PGOOD_VOLT
 *     // ... other config ...
 * }
 */
#define VOLTAGE_MONITOR_DISABLE_PGOOD_VOLT                                                     \
    constexpr size_t   PgoodVoltSensorNum                    = 0;                              \
    constexpr SensorId PgoodVoltSensorDefault                = 0;                              \
    constexpr SensorId PgoodVoltSensorId[PgoodVoltSensorNum] = {};                             \
    template<size_t Index>                                                                     \
    constexpr nv::volt_mon::PgoodVoltSensor pgood_volt_get_sensor_config()                     \
    {                                                                                          \
        static_assert(Index < PgoodVoltSensorNum, "Sensor index out of range");                \
        return {};                                                                             \
    }

/**
 * @brief Disable busbar temperature monitoring feature
 *
 * Use this macro in voltage_monitor_config namespace for projects that don't use
 * busbar temperature monitoring. It provides all required definitions with zero sensors.
 *
 * @example
 * namespace voltage_monitor_config {
 *     VOLTAGE_MONITOR_DISABLE_BUSBAR_TEMP
 *     // ... other config ...
 * }
 */
#define VOLTAGE_MONITOR_DISABLE_BUSBAR_TEMP                                                    \
    constexpr size_t   BusBarTempSensorNum                     = 0;                            \
    constexpr SensorId BusBarTempSensorDefault                 = 0;                            \
    constexpr SensorId BusBarTempSensorId[BusBarTempSensorNum] = {};                           \
    template<size_t Index>                                                                     \
    constexpr nv::volt_mon::BusBarTempSensor bus_bar_temp_get_sensor_config()                  \
    {                                                                                          \
        static_assert(Index < BusBarTempSensorNum, "Sensor index out of range");               \
        return {};                                                                             \
    }

/**
 * @brief Disable MCU internal temperature monitoring feature
 *
 * Use this macro in voltage_monitor_config namespace for projects that don't use
 * MCU internal temperature monitoring. It provides a stub configuration function.
 *
 * Note: Unlike leak_detect and busbar_temp, MCU internal temperature uses singleton pattern
 * (McuInternalTemp::inst()), so no init_sensors() function is needed.
 *
 * @example
 * namespace voltage_monitor_config {
 *     VOLTAGE_MONITOR_DISABLE_MCU_INTERNAL_TEMP
 *     // ... other config ...
 * }
 */
#define VOLTAGE_MONITOR_DISABLE_MCU_INTERNAL_TEMP                                              \
    constexpr nv::volt_mon::McuInternalTempSensor mcu_internal_temp_get_sensor_config()        \
    {                                                                                          \
        return {                                                                               \
            {   /* VoltMon base - all invalid/unused */                                        \
                nv::volt_mon::AdcInstance::Invalid,   /* adcId */                              \
                nv::volt_mon::AdcChannel::Invalid,    /* channel */                            \
                nv::volt_mon::AdcScanMode::Invalid,   /* scanMode */                           \
                nv::volt_mon::AdcCommand::None,       /* cmdScanning */                        \
                nv::volt_mon::AdcCommand::None,       /* cmdNext */                            \
                nv::volt_mon::AdcCommand::None,       /* cmdOneShot */                         \
                nv::volt_mon::AdcTriggerSrc::Invalid, /* cmdTriggerSrc */                      \
                0,                                    /* reading (not used) */                 \
            },                                                                                 \
            nv::volt_mon::Sensor::Invalid,            /* sensor */                             \
            0.0f                                      /* tempCelsius */                        \
        };                                                                                     \
    }

/**
 * @brief Enable only MCU internal temperature monitoring
 *
 * Use this macro for projects that only need MCU internal temperature monitoring.
 * It disables leak detection and busbar temperature, and provides default MCU
 * internal temperature sensor configuration (ADC0, CH26, Differential, CMD15, Trigger 1).
 *
 * @example
 * namespace voltage_monitor_config {
 *     using namespace nv::volt_mon;
 *     VOLTAGE_MONITOR_ENABLE_MCU_INTERNAL_TEMP
 * }
 */
#define VOLTAGE_MONITOR_ENABLE_MCU_INTERNAL_TEMP                                               \
    constexpr bool EnableDbgInfo = false;                                                      \
    constexpr bool SensorOnAdc0  = false;                                                      \
    constexpr bool SensorOnAdc1  = false;                                                      \
    VOLTAGE_MONITOR_DISABLE_LEAK_DETECT                                                        \
    VOLTAGE_MONITOR_DISABLE_BUSBAR_TEMP                                                        \
    inline constexpr nv::volt_mon::McuInternalTempSensor                                       \
    mcu_internal_temp_get_sensor_config()                                                      \
    {                                                                                          \
        return {                                                                               \
            {                                                                                  \
                nv::volt_mon::AdcInstance::_0,                     /* adcId */                 \
                nv::volt_mon::AdcChannel::_26,                    /* channel (internal temp) */\
                nv::volt_mon::AdcScanMode::DifferentialBothSideAB, /* scanMode */              \
                nv::volt_mon::AdcCommand::None,                    /* cmdScanning (not used) */\
                nv::volt_mon::AdcCommand::None,                    /* cmdNext (no chain) */    \
                nv::volt_mon::AdcCommand::_15,                     /* cmdOneShot (CMD 15) */   \
                nv::volt_mon::AdcTempTriggerSrc,                   /* cmdTriggerSrc */         \
                0,                                                 /* reading */               \
            },                                                                                 \
            nv::volt_mon::Sensor::McuInternalTemp,                /* sensor */                 \
            0.0f                                                  /* tempCelsius */            \
        };                                                                                     \
    }

/**
 * @brief Enable MCU internal temperature via legacy sys::sensor API
 *
 * Use this macro for projects that need MCU internal temperature monitoring
 * but use the legacy sys::sensor::Driver instead of the volt_mon module.
 * It disables leak detection, busbar temperature, and provides an invalid
 * MCU temp sensor config so that shared code falls back to the legacy API
 * via if constexpr.
 *
 * @example
 * namespace voltage_monitor_config {
 *     using namespace nv::volt_mon;
 *     MCU_INTERNAL_TEMP_USE_LEGACY_API
 * }
 */
#define MCU_INTERNAL_TEMP_USE_LEGACY_API                                                       \
    constexpr bool EnableDbgInfo = false;                                                      \
    constexpr bool SensorOnAdc0  = false;                                                      \
    constexpr bool SensorOnAdc1  = false;                                                      \
    VOLTAGE_MONITOR_DISABLE_LEAK_DETECT                                                        \
    VOLTAGE_MONITOR_DISABLE_BUSBAR_TEMP                                                        \
    VOLTAGE_MONITOR_DISABLE_MCU_INTERNAL_TEMP

/**
 * @brief Completely disable all voltage monitor features
 *
 * Use this macro for projects that don't need any voltage monitoring at all,
 * including MCU internal temperature. It disables leak detection, busbar
 * temperature, and MCU internal temperature.
 *
 * @example
 * namespace voltage_monitor_config {
 *     using namespace nv::volt_mon;
 *     VOLTAGE_MONITOR_DISABLED
 * }
 */
#define VOLTAGE_MONITOR_DISABLED                                                               \
    constexpr bool EnableDbgInfo = false;                                                      \
    constexpr bool SensorOnAdc0  = false;                                                      \
    constexpr bool SensorOnAdc1  = false;                                                      \
    VOLTAGE_MONITOR_DISABLE_LEAK_DETECT                                                        \
    VOLTAGE_MONITOR_DISABLE_BUSBAR_TEMP                                                        \
    VOLTAGE_MONITOR_DISABLE_MCU_INTERNAL_TEMP
// clang-format on
