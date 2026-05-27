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

#include <cstddef>
#include <cstdint>
#include <cstring>

#include "config.h"

#include "nv/i2c/emc1812.h"
#include "nv/i2c/nct75.h"
#include "nv/i2c/sensor.h"
#include "nv/i2c/tmp1075.h"
#include "nv/i2c/tmp461.h"
#include "nv/ipc/supervisor.h"
#include "nv/logger/log.h"
#include "nv/mctp/enums.h"
#include "nv/mctp/nsm.h"
#include "nv/mctp/task.h"
#include "nv/telemetry/utils.h"
#include "nv/volt_mon/busbar_temp.h"
#include "nv/volt_mon/ntc_table.h"
#include "nv/volt_mon/leak_detect.h"
#include "nv/volt_mon/mcu_internal_temp.h"
#include "sys/sensor/sensor.h"
using namespace nv;
using namespace mctp;

namespace nv::mctp {

constexpr auto
    BusBarTempSensorDefault = nv::ipc::voltage_monitor_config::BusBarTempSensorDefault;
// Default low temperature threshold voltage (high ADC, rarely triggered)
constexpr nv::volt_mon::Threshold LOW_TEMP_THRESHOLD_MV = nv::volt_mon::AdcVolVref;

/** A map indexed by Type3PowerSensors which has corresponding
 *  tag in nv::telemetry::TelemId */
constexpr std::array<nv::telemetry::TelemId, 3> cachePowerSensorsMap = {
    nv::telemetry::TelemId::Gpu1Power,    // PowerGpu1
    nv::telemetry::TelemId::Gpu2Power,    // PowerGpu2
    nv::telemetry::TelemId::ModulePower,  // PowerModule
};

uint8_t findTemperatureSensorIndex(uint8_t sensorId)
{
    if (!nv::mctp::I2cTempSensorList.empty()) {
        for (uint8_t i = 0; i < nv::mctp::I2cTempSensorList.size(); ++i) {
            const auto& sensor = nv::mctp::I2cTempSensorList.at(i);
            if (sensor.sensor_id == sensorId) {
                // coverity[DEADCODE:dead_error_line]
                if (sensor.port != nv::i2c::Port::End && sensor.identified_addr != 0) {
                    return i;
                }
                else {
                    return Type3TemperatureSensors::TempSensor_End;
                }
            }
        }
    }
    return Type3TemperatureSensors::TempSensor_End;  // Not found
}

namespace nsm_type3 {

using TelemetryItem                        = nv::telemetry::TelemId;
auto constexpr InvalidSensorId             = nv::telemetry::TelemId::MaxItem;
static constexpr auto TelemetryInvalidData = static_cast<TelemetryValue>(
    nv::telemetry::Cache::InvalidData);
static auto constexpr ConversionFactor_NvS24_8 = 256;

/**
 * @brief set_temperature_NvS24_8
 *              Moves a temperature value from 'temperature' to 24 bits in 'temp_NvS4_8'
 * @param [in]  temperature
 * @param [out] temp_NvS4_8
 */
void set_temperature_NvS24_8(TelemetryValue temperature, int32_t& temp_NvS4_8)
{
    if (temperature == TelemetryInvalid || temperature == TelemetryInvalidData) {
        auto invalid = TelemetryInvalid;
        std::memcpy(&temp_NvS4_8, &invalid, sizeof(int32_t));
        return;
    }
    temp_NvS4_8 = static_cast<int32_t>(temperature) * ConversionFactor_NvS24_8;
}

/**
 * @brief set_temperature_NvS24_8
 * @param [in] temperature
 * @param [out temp_NvS4_8
 */
void set_temperature_NvS24_8(TelemetryValue temperature, uint32_t& temp_NvS4_8)
{
    int32_t signed_value{0};
    set_temperature_NvS24_8(temperature, signed_value);
    std::memcpy(&temp_NvS4_8, &signed_value, sizeof(int32_t));
}

/**
 * @brief getValidCachedPowerDrawSensor
 *      - The tagId needs to match 2 tables:
 *       #- cachePowerSensorsMap where the TagId is the index
 *       #- mcuPowerSensors List of valid sensors for the MCU
 *
 * @return The corresponding sensor Id if the tagId is valid (match 2 tables)
 */
TelemetryItem getValidCachedPowerDrawSensor(const uint8_t tagId)
{
    if constexpr (nv::mctp::mcuPowerSensorsSize > 0) {
        if (tagId < cachePowerSensorsMap.size()) {
            for (auto sensorId : nv::mctp::mcuPowerSensors) {
                if (sensorId == tagId) {
                    return cachePowerSensorsMap.at(tagId);
                }
            }
        }
    }
    return InvalidSensorId;
}

//
uint32_t read_i2c_power_sensor(uint8_t sensorId)
{
    uint32_t power_value = 0;
    switch (sensorId) {
        case Type3PowerSensors::Power_Gpu1  : power_value = TelemetryInvalid; break;
        case Type3PowerSensors::Power_Gpu2  : power_value = TelemetryInvalid; break;
        case Type3PowerSensors::PowerHsc    :
        case Type3PowerSensors::PowerHsc_2  :
        case Type3PowerSensors::PowerHsc_3  :
        case Type3PowerSensors::PowerHsc_4  :
        case Type3PowerSensors::PowerHsc_Sby: {
            const int8_t device_index = get_power_sensor_mgr().find_hsc_index_by_power_sensor(
                static_cast<Type3PowerSensors>(sensorId));
            if (device_index >= 0) {
                auto status = get_power_sensor_mgr().hsc_read_input_power(device_index,
                                                                          power_value);
                if (status != nv::i2c::I2cStatus::Ok) {
                    power_value = TelemetryInvalid;
                }
            }
            else {
                power_value = TelemetryInvalid;
            }
            break;
        }
        case Type3PowerSensors::PowerHscc:
        case Type3PowerSensors::PowerHscc_2: {
            const int8_t device_index = get_power_sensor_mgr().find_hscc_index_by_power_sensor(
                static_cast<Type3PowerSensors>(sensorId));
            if (device_index >= 0) {
                auto status = get_power_sensor_mgr().hscc_read_input_power(device_index,
                                                                           power_value);
                if (status != nv::i2c::I2cStatus::Ok) {
                    power_value = TelemetryInvalid;
                }
            }
            else {
                power_value = TelemetryInvalid;
            }
            break;
        }
        default: power_value = TelemetryInvalid; break;
    }
    return power_value;
}

uint32_t read_i2c_voltage_sensor(uint8_t sensorId)
{
    uint32_t voltage_value = 0;
    switch (sensorId) {
        case T3Voltage::Voltage_Hsc_Vout:
        case T3Voltage::Voltage_Hsc_Vout_2:
        case T3Voltage::Voltage_Hsc_Vout_3:
        case T3Voltage::Voltage_Hsc_Vout_4:
        case T3Voltage::Voltage_Hsc_Sby_Vout: {
            const int8_t device_index = get_power_sensor_mgr().find_hsc_index_by_vout_sensor(
                static_cast<T3Voltage>(sensorId));
            if (device_index >= 0) {
                auto status = get_power_sensor_mgr().hsc_read_vout(device_index, voltage_value);
                if (status != nv::i2c::I2cStatus::Ok) {
                    voltage_value = TelemetryInvalid;
                }
            }
            else {
                voltage_value = TelemetryInvalid;
            }
            break;
        }
        case T3Voltage::Voltage_Hsc_Vin:
        case T3Voltage::Voltage_Hsc_Vin_2:
        case T3Voltage::Voltage_Hsc_Vin_3:
        case T3Voltage::Voltage_Hsc_Vin_4:
        case T3Voltage::Voltage_Hsc_Sby_Vin: {
            const int8_t device_index = get_power_sensor_mgr().find_hsc_index_by_vin_sensor(
                static_cast<T3Voltage>(sensorId));
            if (device_index >= 0) {
                auto status = get_power_sensor_mgr().hsc_read_vin(device_index, voltage_value);
                if (status != nv::i2c::I2cStatus::Ok) {
                    voltage_value = TelemetryInvalid;
                }
            }
            else {
                voltage_value = TelemetryInvalid;
            }
            break;
        }
        case T3Voltage::Voltage_Hscc_Vout:
        case T3Voltage::Voltage_Hscc_Vout_2: {
            const int8_t device_index = get_power_sensor_mgr().find_hscc_index_by_vout_sensor(
                static_cast<T3Voltage>(sensorId));
            if (device_index >= 0) {
                auto status = get_power_sensor_mgr().hscc_read_vout(device_index,
                                                                    voltage_value);
                if (status != nv::i2c::I2cStatus::Ok) {
                    voltage_value = TelemetryInvalid;
                }
            }
            else {
                voltage_value = TelemetryInvalid;
            }
            break;
        }
        case T3Voltage::Voltage_Hscc_Vin:
        case T3Voltage::Voltage_Hscc_Vin_2: {
            const int8_t device_index = get_power_sensor_mgr().find_hscc_index_by_vin_sensor(
                static_cast<T3Voltage>(sensorId));
            if (device_index >= 0) {
                auto status = get_power_sensor_mgr().hscc_read_vin(device_index, voltage_value);
                if (status != nv::i2c::I2cStatus::Ok) {
                    voltage_value = TelemetryInvalid;
                }
            }
            else {
                voltage_value = TelemetryInvalid;
            }
            break;
        }
        default: voltage_value = TelemetryInvalid; break;
    }
    return voltage_value;
}

TelemetryValue get_internal_sensor_temperature()
{
    TelemetryValue temperature = TelemetryInvalid;
    float          floatTemp   = 0.0;
    constexpr auto
         mcuTempCfg = nv::ipc::voltage_monitor_config::mcu_internal_temp_get_sensor_config();
    bool gotTemperature = false;
    if constexpr (mcuTempCfg.sensor != nv::volt_mon::Sensor::Invalid) {
        gotTemperature = (nv::mcu_internal_temp::McuInternalTemp::inst()
                              .get_temperature_celsius(floatTemp)
                          == nv::volt_mon::Status::Ok);
    }
    else {
        gotTemperature = sys::sensor::Driver::get_current_temperature(floatTemp);
    }
    if (true == gotTemperature) {
        temperature = static_cast<TelemetryValue>(floatTemp);
    }
    return temperature;
}

TelemetryValue get_cx8_sensor_temperature(uint8_t sensorId)
{
    TelemetryValue      temperature = TelemetryInvalid;
    const TelemetryItem telemetryId = nv::telemetry::getTelemIdFromTempSensorId(sensorId);
    if (telemetryId == nv::telemetry::TelemId::MaxItem) {
        return TelemetryInvalid;
    }
    if (nv::ipc::ModuleTempSensorSize > 0) {
        for (const auto& cx8_sensor : nv::ipc::ModuleTempSensorList) {
            auto telemetry = std::get<2>(cx8_sensor);
            if (telemetry == telemetryId) {
                uint8_t temp{0};
                auto    port    = std::get<0>(cx8_sensor);
                auto    address = std::get<1>(cx8_sensor);
                auto status = nv::i2c::TempSensor(port, address, telemetry).get_CX8_temp(temp);
#if 0  // change here to save persistent log on MCU
                auto    logEvent = nv::logger::Event::T3Cx8TemperatureDebug;
                nv::logger::info_wait(logEvent,
                                      {static_cast<uint8_t>(port),
                                       static_cast<uint8_t>(address),
                                       static_cast<uint8_t>(telemetry),
                                       static_cast<uint8_t>(status),
                                       static_cast<uint8_t>(temp)});
#endif
                if (status == nv::i2c::I2cStatus::Ok) {
                    temperature = static_cast<TelemetryValue>(temp);
                }
                break;
            }
        }
    }
    return temperature;
}

TelemetryValue read_i2c_temp_sensor(uint8_t sensorId)
{
    // Find sensor index using the extracted function
    const int sensorIndex = nv::mctp::findTemperatureSensorIndex(sensorId);
    if (sensorIndex == Type3TemperatureSensors::TempSensor_End) {
        return TelemetryInvalid;
    }

    // Get sensor configuration from array using the index
    const auto&         sensorConfig = nv::mctp::I2cTempSensorList.at(sensorIndex);
    const nv::i2c::Port i2cPort      = sensorConfig.port;
    const uint8_t       i2cAddr      = sensorConfig.identified_addr;
    const uint8_t       id           = sensorConfig.sensor_model;

    if (id == nv::i2c::SensorModel::Sensor_Tmp461) {
        nv::i2c::Tmp461 tmp461(i2cPort, i2cAddr);
        int8_t          temp   = 0;
        auto            status = tmp461.get_remote_high_temp(temp);
        if (status == nv::i2c::I2cStatus::Ok) {
            return static_cast<TelemetryValue>(temp);
        }
        else {
            // TODO: Add to flash log
            return TelemetryInvalid;
        }
    }

    else if (id == nv::i2c::SensorModel::Sensor_Emc1812) {
        nv::i2c::Emc1812 emc1812(i2cPort, i2cAddr);
        int8_t           temp   = 0;
        auto             status = emc1812.get_remote_high_temp(temp);
        if (status == nv::i2c::I2cStatus::Ok) {
            return static_cast<TelemetryValue>(temp);
        }
        else {
            // TODO: Add to flash log
            return TelemetryInvalid;
        }
    }
    else if (id == nv::i2c::SensorModel::Sensor_Tmp1075) {
        nv::i2c::Tmp1075 tmp1075(i2cPort, i2cAddr);
        int8_t           temp   = 0;
        auto             status = tmp1075.read_temperature(temp);
        if (status == nv::i2c::I2cStatus::Ok) {
            return static_cast<TelemetryValue>(temp);
        }
    }
    else if (id == nv::i2c::SensorModel::Sensor_Nct75) {
        nv::i2c::Nct75 nct75(i2cPort, i2cAddr);
        int8_t         temp   = 0;
        auto           status = nct75.read_temperature(temp);
        if (status == nv::i2c::I2cStatus::Ok) {
            return static_cast<TelemetryValue>(temp);
        }
    }
    // TODO: Support NCT 70 sensor driver
    else if (id == nv::i2c::SensorModel::Sensor_Nct70) {
        return TelemetryInvalid;
    }

    return TelemetryInvalid;
}

/**
 * @brief read_busbar_temperature_info() does NOT convert temperatures to NvS24.8 format
 * @param temperature_info - the data structure
 * @return
 */
Ccode read_busbar_temperature_info(nv::mctp::NsmT3BusBarInfoTemperature& temperature_info)
{
    constexpr auto BusBarTempSensorNum = nv::ipc::voltage_monitor_config::BusBarTempSensorNum;
    if constexpr (BusBarTempSensorNum > 0) {
        std::array<nv::volt_mon::BusBarTempSensor, BusBarTempSensorNum> sensors{};
        auto& busbarTemperature = nv::busbar_temp::BusbarTemp::inst();
        if (busbarTemperature.get_sensor_info(sensors) == nv::volt_mon::Status::Ok) {
            for (const auto& sensor : sensors) {
                if (sensor.id == BusBarTempSensorDefault) {
                    // Convert ADC reading to voltage (mV), then to temperature (0.1°C)
                    auto voltageMv = volt_mon::to_vol_value(sensor.reading);
                    auto tempRaw   = volt_mon::ntc_voltage_to_temperature(voltageMv);
                    // Convert from 0.1°C to °C (integer)
                    auto temperature = (tempRaw != volt_mon::NtcTempInvalid)
                                         ? (tempRaw / volt_mon::NtcTempScale)
                                         : 0;

                    // High temp threshold
                    auto thresholdVoltageMv = volt_mon::to_vol_value(sensor.busBarHighTemp);
                    auto thresholdTempRaw   = volt_mon::ntc_voltage_to_temperature(
                        thresholdVoltageMv);
                    auto threshold = (thresholdTempRaw != volt_mon::NtcTempInvalid)
                                       ? (thresholdTempRaw / volt_mon::NtcTempScale)
                                       : volt_mon::NtcTempMax;

                    temperature_info.temperature           = temperature;
                    temperature_info.temperature_threshold = threshold;
                    nv::info("%s() voltage=%umV temperature=%d°C threshold=%d°C\n",
                             __func__,
                             voltageMv,
                             temperature,
                             threshold);
                    break;
                }
            }
        }
        else {
            return Ccode::ErrorGeneral;
        }
    }
    return Ccode::Success;
}

/** @brief in order to have the temperature in the format NvS24.0
 *      call getTemperatureTelemetry(BusBar_Temp)
 *
 * @returns the raw temperature from BusBar
 */
TelemetryValue read_busbar_temperature()
{
    NsmT3BusBarInfoTemperature temp_info{};
    TelemetryValue             temperature = TelemetryInvalid;
    if (read_busbar_temperature_info(temp_info) == Ccode::Success) {
        temperature = temp_info.temperature;
    }
    return temperature;
}

/**
 * @brief Sets the BusBar sensor Temperature Threshold in the format NvS24.0
 * @returns Ccode::Success if could get BusBar sensor Temperature Threshold
 */
Ccode read_busbar_temperature_threshold(uint32_t& threshold)
{
    NsmT3BusBarInfoTemperature temp_info{};
    if (read_busbar_temperature_info(temp_info) == Ccode::Success) {
        threshold = temp_info.temperature_threshold;
        return Ccode::Success;
    }
    return Ccode::ErrorGeneral;
}

Ccode set_busbar_temperature_threshold(const NsmT3SetThermalParameterReq& request)
{
    if constexpr (nv::ipc::voltage_monitor_config::BusBarTempSensorNum > 0) {
        auto& busbarTemperature = nv::busbar_temp::BusbarTemp::inst();

        // Convert temperature (°C) to ADC threshold value
        // threshold is temperature in °C (uint8_t: 0-255, but valid range is -40~125)
        auto tempCelsius = static_cast<int16_t>(request.threshold);

        // Temperature → NTC resistance (Ω)
        uint32_t resistanceOhm = volt_mon::ntc_temperature_to_resistance(tempCelsius);
        if (resistanceOhm == 0) {
            // Invalid temperature, use default max temp (125°C)
            resistanceOhm = volt_mon::ntc_temperature_to_resistance(volt_mon::NtcTempMax);
        }

        // Resistance → Voltage: V = Vcc × R / (R + Rpullup)
        uint32_t voltageMv = volt_mon::NtcVref * resistanceOhm
                           / (resistanceOhm + volt_mon::NtcPullupResistor);

        nv::info("%s() temp=%d°C resistance=%luΩ voltage=%lumV\n",
                 __func__,
                 tempCelsius,
                 resistanceOhm,
                 voltageMv);

        // set_thresholds() expects voltage in mV, it converts to ADC internally
        const nv::volt_mon::ThresholdBusbar temp_threshold{
            static_cast<uint16_t>(voltageMv),  // busBarHighTemp (high temp threshold)
            LOW_TEMP_THRESHOLD_MV              // busBarLowTemp (fixed, rarely used)
        };
        auto rc = busbarTemperature.set_thresholds(BusBarTempSensorDefault, temp_threshold);
        if (rc != nv::volt_mon::Status::Ok) {
            return Ccode::ErrorGeneral;
        }

        /** @note after setting thresholds, update sensor state/vrgpio state by querying
         *  the sensor info (one-shot read → update state/GPIO → restart ADC scanning) */
        std::array<nv::volt_mon::BusBarTempSensor,
                   nv::ipc::voltage_monitor_config::BusBarTempSensorNum>
            sensor_info{};
        if (busbarTemperature.get_sensor_info(sensor_info) != nv::volt_mon::Status::Ok) {
            return Ccode::ErrorGeneral;
        }
    }
    return Ccode::Success;
}

TelemetryValue getThermalTelemetry(const uint8_t sensorId)
{
    TelemetryValue telemetry_value = TelemetryInvalid;

    // Check if the T3 Tag Id is in GB Telemetry range
    if (sensorId < GB_TempSensor_End) {
        auto cacheId = nv::telemetry::getTelemIdFromTempSensorId(sensorId);
        if (cacheId == nv::telemetry::TelemId::MaxItem) {
            return TelemetryInvalid;
        }
        nv::telemetry::Cache& telemetry = nv::telemetry::Cache::inst();
        (void)telemetry.get_table();
        // gets corresponding sensor id in Cache
        telemetry_value = telemetry.get_cache(cacheId);
        if (telemetry_value != TelemetryInvalid && telemetry_value != TelemetryInvalidData) {
            switch (cacheId) {  // NOLINT
                case nv::telemetry::TelemId::InternalTemp:
                    telemetry_value = get_internal_sensor_temperature();
                    break;
                case nv::telemetry::TelemId::CX8_1_Temp:  // NOLINT
                    [[fallthrough]];
                case nv::telemetry::TelemId::CX8_2_Temp:  // NOLINT
                    telemetry_value = get_cx8_sensor_temperature(sensorId);
                    break;
                default: telemetry_value = TelemetryInvalid; break;
            }
        }
        return telemetry_value;
    }

    // Directly read temperature from sensor
    else {
        switch (sensorId) {
            case Type3TemperatureSensors::SMA_Internal:
                telemetry_value = get_internal_sensor_temperature();
                break;
            case Type3TemperatureSensors::SMA_External:
            case Type3TemperatureSensors::CPU1_Die:
            case Type3TemperatureSensors::CPU1_SoC:
            case Type3TemperatureSensors::CPU2_Die:
            case Type3TemperatureSensors::CPU2_SoC:
            case Type3TemperatureSensors::PCB_Temp_1:
            case Type3TemperatureSensors::PCB_Temp_2:
            case Type3TemperatureSensors::PCB_Temp_3:
            case Type3TemperatureSensors::PCB_Temp_4:
            case Type3TemperatureSensors::GPU1_Die_A:
            case Type3TemperatureSensors::GPU1_Die_B:
            case Type3TemperatureSensors::GPU2_Die_A:
            case Type3TemperatureSensors::GPU2_Die_B:
            case Type3TemperatureSensors::NvLink_Temp:
            case Type3TemperatureSensors::CPLD_Temp:
                telemetry_value = read_i2c_temp_sensor(sensorId);
                break;
            case Type3TemperatureSensors::QM4_1_Temp:
            case Type3TemperatureSensors::QM4_2_Temp:
                telemetry_value = get_cx8_sensor_temperature(sensorId);
                break;
            case Type3TemperatureSensors::HSCC_Temp:
            case Type3TemperatureSensors::HSCC_Temp_2: {
                const int8_t
                    device_index = get_power_sensor_mgr().find_hscc_index_by_temp_sensor(
                        static_cast<Type3TemperatureSensors>(sensorId));
                if (device_index >= 0) {
                    uint16_t temp   = 0;
                    auto     status = get_power_sensor_mgr().hscc_read_temperature(device_index,
                                                                               temp);
                    if (status == nv::i2c::I2cStatus::Ok) {
                        telemetry_value = static_cast<TelemetryValue>(temp);
                    }
                }
                else {
                    telemetry_value = TelemetryInvalid;
                }
                break;
            }
            case Type3TemperatureSensors::HSC_Temp:
            case Type3TemperatureSensors::HSC_Temp_2:
            case Type3TemperatureSensors::HSC_Temp_3:
            case Type3TemperatureSensors::HSC_Temp_4:
            case Type3TemperatureSensors::HSC_Sby_Temp: {
                const int8_t
                    device_index = get_power_sensor_mgr().find_hsc_index_by_temp_sensor(
                        static_cast<Type3TemperatureSensors>(sensorId));
                if (device_index >= 0) {
                    uint16_t temp   = 0;
                    auto     status = get_power_sensor_mgr().hsc_read_temperature(device_index,
                                                                              temp);
                    if (status == nv::i2c::I2cStatus::Ok) {
                        telemetry_value = static_cast<TelemetryValue>(temp);
                    }
                }
                else {
                    telemetry_value = TelemetryInvalid;
                }
                break;
            }
            case Type3TemperatureSensors::BusBar_Temp:
                telemetry_value = read_busbar_temperature();
                break;
            default: telemetry_value = TelemetryInvalid; break;
        }
    }

    return telemetry_value;
}

TelemetryValue getPowerTelemetry(const uint8_t sensorId)
{
    TelemetryValue telemetry_value = TelemetryInvalid;

    // Check if the T3 Tag Id is in GB Telemetry range
    if (sensorId < GB_PowerSensor_End) {
        auto cacheId = nv::telemetry::getTelemIdFromPowerSensorId(sensorId);
        if (cacheId == nv::telemetry::TelemId::MaxItem) {
            return TelemetryInvalid;
        }
        nv::telemetry::Cache& telemetry = nv::telemetry::Cache::inst();
        (void)telemetry.get_table();
        // gets corresponding sensor id in Cache
        telemetry_value = telemetry.get_cache(cacheId);

        return telemetry_value;
    }

    // Directly read temperature from sensor
    else {
        switch (sensorId) {
            case Type3PowerSensors::Power_Gpu1:
            case Type3PowerSensors::Power_Gpu2: {
                auto cacheId = nv::telemetry::getTelemIdFromPowerSensorId(sensorId);
                if (cacheId == nv::telemetry::TelemId::MaxItem) {
                    return TelemetryInvalid;
                }
                nv::telemetry::Cache& telemetry = nv::telemetry::Cache::inst();
                (void)telemetry.get_table();
                // gets corresponding sensor id in Cache
                telemetry_value = telemetry.get_cache(cacheId);
                break;
            }
            case Type3PowerSensors::PowerHsc:
            case Type3PowerSensors::PowerHsc_2:
            case Type3PowerSensors::PowerHsc_3:
            case Type3PowerSensors::PowerHsc_4:
            case Type3PowerSensors::PowerHsc_Sby:
            case Type3PowerSensors::PowerHscc:
            case Type3PowerSensors::PowerHscc_2:
                telemetry_value = read_i2c_power_sensor(sensorId);
                break;

            default: telemetry_value = TelemetryInvalid; break;
        }
    }

    return telemetry_value;
}

TelemetryValue getVoltageTelemetry(const uint8_t sensorId)
{
    TelemetryValue telemetry_value = TelemetryInvalid;

    switch (sensorId) {
        case T3Voltage::Voltage_Hsc_Vout:
        case T3Voltage::Voltage_Hsc_Vout_2:
        case T3Voltage::Voltage_Hsc_Vout_3:
        case T3Voltage::Voltage_Hsc_Vout_4:
        case T3Voltage::Voltage_Hsc_Sby_Vout:
        case T3Voltage::Voltage_Hsc_Vin:
        case T3Voltage::Voltage_Hsc_Vin_2:
        case T3Voltage::Voltage_Hsc_Vin_3:
        case T3Voltage::Voltage_Hsc_Vin_4:
        case T3Voltage::Voltage_Hsc_Sby_Vin:
        case T3Voltage::Voltage_Hscc_Vout:
        case T3Voltage::Voltage_Hscc_Vout_2:
        case T3Voltage::Voltage_Hscc_Vin:
        case T3Voltage::Voltage_Hscc_Vin_2:
            telemetry_value = read_i2c_voltage_sensor(sensorId);
            break;

        default: telemetry_value = TelemetryInvalid; break;
    }

    return telemetry_value;
}

// The strong function is defined in product code. For pg558_hpm, in main.cpp
__attribute__((weak)) bool is_busbar_available()
{
    return true;  // default: busbar is available
}

bool is_temp_sensor_available(uint8_t sensorId)
{
    if (sensorId == Type3TemperatureSensors::BusBar_Temp && !is_busbar_available()) {
        return false;
    }
    // Check if sensorId exists in mcuTemperatureSensors array
    for (const auto& validSensorId : mcuTemperatureSensors) {
        if (validSensorId == sensorId) {
            return true;  // Found the sensor
        }
    }
    return false;  // Sensor not found.
}

bool is_power_sensor_available(uint8_t sensorId)
{
    if constexpr (mcuPowerSensorsSize == 0) {
        return false;  // No power sensors in this configuration
    }

    for (const auto& validSensorId : mcuPowerSensors) {
        if (validSensorId == sensorId) {
            return true;  // Found the sensor
        }
    }
    return false;  // Sensor not found.
}

bool is_voltage_sensor_available(uint8_t sensorId)
{
    if constexpr (mcuVoltageSensorsSize == 0) {
        return false;  // No voltage sensors in this configuration
    }

    for (const auto& validSensorId : mcuVoltageSensors) {
        if (validSensorId == sensorId) {
            return true;  // Found the sensor
        }
    }
    return false;  // Sensor not found.
}

int32_t getTemperatureTelemetry(const uint8_t sensorId)
{
    auto    temperature = getThermalTelemetry(sensorId);
    int32_t telemetry{0};
    set_temperature_NvS24_8(temperature, telemetry);
    return telemetry;
}

Ccode createTemperatureReadingResponse(const uint8_t tagId, NsmPktResp& ntx)
{
    if (false == is_temp_sensor_available(tagId)) {
        return Ccode::ErrorInvalidData;
    }
    int32_t temperature = getTemperatureTelemetry(tagId);
    std::memcpy(&ntx.data[0], &temperature, sizeof(int32_t));
    ntx.data_size = sizeof(int32_t);
    return Ccode::Success;
}

/**
 * @brief Gets Power Draw telemetry and sets properly the response data in @a ntx
 * @param tagId  - The tag/sendor id to get Power Draw
 * @param ntx    - Response packet
 * @return The size of the response
 */
Ccode createPowerDrawResponse(const uint8_t tagId, NsmPktResp& ntx)
{
    if (false == is_power_sensor_available(tagId)) {
        return Ccode::ErrorInvalidData;
    }
    const uint32_t power_value = getPowerTelemetry(tagId);
    ntx.data_size              = sizeof(uint32_t);
    std::memcpy(&ntx.data[0], &power_value, sizeof(uint32_t));
    return Ccode::Success;
}

Ccode createVoltageResponse(const uint8_t tagId, NsmPktResp& ntx)
{
    if (false == is_voltage_sensor_available(tagId)) {
        return Ccode::ErrorInvalidData;
    }
    const uint32_t voltage_value = getVoltageTelemetry(tagId);
    ntx.data_size                = sizeof(uint32_t);
    std::memcpy(&ntx.data[0], &voltage_value, sizeof(uint32_t));
    return Ccode::Success;
}

template<typename Value32Bits>
bool appendTelemetryRecord(const uint8_t         tagId,
                           const Value32Bits&    telemetry,
                           TelemetryRecordArray& array,
                           const bool            valid = true)
{
    return array.addRecordNvU32(tagId, telemetry, valid);
}

Ccode createPowerDrawAggregateResponse(NsmPktBulkResp& ntx_bulk, uint16_t& size_response)
{
    // aggregateArray fills ntx_bulk->data[0] ... ntx_bulk->data[N]
    TelemetryRecordArray aggregateArray(&ntx_bulk.data[0],
                                        TelemetryRecordArray::DefaultMctpDataSize);
    if (false == aggregateArray.addTimestampRecord(NsmPlatEnvTempAggregate)) {
        return Ccode::ErrorInvalidLength;
    }
    uint32_t powerValue = 0;
    if constexpr (nv::mctp::mcuPowerSensorsSize > 0) {
        // loop all sensors
        for (auto const tagId : nv::mctp::mcuPowerSensors) {
            powerValue       = getPowerTelemetry(tagId);
            const bool valid = (powerValue != TelemetryInvalid) ? true : false;
            if (false == appendTelemetryRecord(tagId, powerValue, aggregateArray, valid)) {
                return Ccode::ErrorInvalidLength;
            }
        }
    }
    size_response            = aggregateArray.arraySize();
    ntx_bulk.telemetry_count = aggregateArray.elements();
    return Ccode::Success;
}

Ccode createVoltageAggregateResponse(NsmPktBulkResp& ntx_bulk, uint16_t& size_response)
{
    // aggregateArray fills ntx_bulk->data[0] ... ntx_bulk->data[N]
    TelemetryRecordArray aggregateArray(&ntx_bulk.data[0],
                                        TelemetryRecordArray::DefaultMctpDataSize);
    if (false == aggregateArray.addTimestampRecord(NsmPlatEnvTempAggregate)) {
        return Ccode::ErrorInvalidLength;
    }

    if constexpr (mcuVoltageSensorsSize == 0) {
        size_response            = aggregateArray.arraySize();
        ntx_bulk.telemetry_count = aggregateArray.elements();
        return Ccode::Success;
    }

    uint32_t voltageValue = 0;
    // loop all voltage sensors
    for (auto const tagId : mcuVoltageSensors) {
        voltageValue     = getVoltageTelemetry(tagId);
        const bool valid = (voltageValue != TelemetryInvalid) ? true : false;
        if (false == appendTelemetryRecord(tagId, voltageValue, aggregateArray, valid)) {
            return Ccode::ErrorInvalidLength;
        }
    }
    size_response            = aggregateArray.arraySize();
    ntx_bulk.telemetry_count = aggregateArray.elements();
    return Ccode::Success;
}

Ccode createTemperatureAggregateResponse(NsmPktBulkResp& ntx_bulk, uint16_t& size_response)
{
    constexpr auto TemperatureInvalid = static_cast<int32_t>(TelemetryInvalid);
    // aggregateArray fills ntx_bulk->data[0] ... ntx_bulk->data[N]
    TelemetryRecordArray aggregateArray(&ntx_bulk.data[0],
                                        TelemetryRecordArray::DefaultMctpDataSize);
    if (false == aggregateArray.addTimestampRecord(NsmPlatEnvTempAggregate)) {
        return Ccode::ErrorInvalidLength;
    }
    int32_t temperature = 0;
    // loop all sensors
    for (const auto tagId : mcuTemperatureSensors) {
        if (true == is_temp_sensor_available(tagId)) {
            temperature      = getTemperatureTelemetry(tagId);
            const bool valid = (temperature != TemperatureInvalid) ? true : false;
            if (false == appendTelemetryRecord(tagId, temperature, aggregateArray, valid)) {
                return Ccode::ErrorInvalidLength;
            }
        }
    }
    size_response            = aggregateArray.arraySize();
    ntx_bulk.telemetry_count = aggregateArray.elements();
    return Ccode::Success;
}

}  // namespace nsm_type3

// Nsm class methods

/**
 * @brief Used in Nsm::process()
 * @param rx
 * @param [out]tx
 * @return true if it was possible to handle the Type 3 command
 */
bool Nsm::process_platform_enviromentals(const Packet& rx, Packet& tx)
{
    using cmd = nv::mctp::NsmPlatEnvCmdCode;
    auto& nrx = NsmPktReq::from(rx);
    auto& ntx = NsmPktResp::from(tx);

    ntx.nv_msg_type = nrx.nv_msg_type;
    ntx.set_plat_env_code(nrx.get_plat_env_code());

    // Helper to handle unsupported commands
    [[maybe_unused]] auto unsupported_command = [&]() {
        fill_error_packet(Ccode::ErrorUnsupportedCmd, rx, tx);
        return false;
    };

    switch (nrx.get_plat_env_code()) {
        case cmd::GetTemperatureReading:
            if constexpr (is_cmd_set(NsmMsgType::PlatformEnviromentals,
                                     cmd::GetTemperatureReading)) {
                on_plat_env_getTemperatureReading(rx, tx);
                break;
            }
            return unsupported_command();

        case cmd::GetCurrentPowerDraw:
            if constexpr (is_cmd_set(NsmMsgType::PlatformEnviromentals,
                                     cmd::GetCurrentPowerDraw)) {
                on_plat_env_getPowerDraw(rx, tx);
                break;
            }
            return unsupported_command();

        case cmd::GetInventoryInformation:
            if constexpr (is_cmd_set(NsmMsgType::PlatformEnviromentals,
                                     cmd::GetInventoryInformation)) {
                on_plat_env_getInventoryInformation(rx, tx);
                break;
            }
            return unsupported_command();

        case cmd::SetThermalParameter:
            if constexpr (is_cmd_set(NsmMsgType::PlatformEnviromentals,
                                     cmd::SetThermalParameter)) {
                on_plat_env_setThermalParameter(rx, tx);
                break;
            }
            return unsupported_command();

        case cmd::GetThermalParameter:
            if constexpr (is_cmd_set(NsmMsgType::PlatformEnviromentals,
                                     cmd::GetThermalParameter)) {
                on_plat_env_getThermalParameter(rx, tx);
                break;
            }
            return unsupported_command();

        case cmd::GetVoltage:
            if constexpr (is_cmd_set(NsmMsgType::PlatformEnviromentals, cmd::GetVoltage)) {
                on_plat_env_getVoltage(rx, tx);
                break;
            }
            return unsupported_command();

        case cmd::GetLeakDetectionInfo:
            if constexpr (is_cmd_set(NsmMsgType::PlatformEnviromentals,
                                     cmd::GetLeakDetectionInfo)) {
                on_plat_env_getLeakDetectionInfo(rx, tx);
                break;
            }
            return unsupported_command();

        case cmd::SetLeakDetectionThresholds:
            if constexpr (is_cmd_set(NsmMsgType::PlatformEnviromentals,
                                     cmd::SetLeakDetectionThresholds)) {
                on_plat_env_setLeakDetectionThresholds(rx, tx);
                break;
            }
            return unsupported_command();

        default: return unsupported_command();
    }
    return true;
}

void Nsm::on_plat_env_getTemperatureReading(const Packet& rx, Packet& tx)
{
    constexpr uint8_t RequestSize = 1;
    if (!is_input_length_valid(rx, RequestSize)) {
        fill_error_packet(Ccode::ErrorInvalidLength, rx, tx);
        return;
    }
    fill_packet_header(rx, tx);
    fill_nsm_msg_header(rx, tx);
    auto& nrx           = NsmPktReq::from(rx);
    auto& ntx           = NsmPktResp::from(tx);
    ntx.data_size       = 0;
    ntx.completion_code = Ccode::Success;
    auto tagId          = nrx.data[0];

    if (tagId < NsmPlatEnvTempAggregate) {
        auto rcCode = nsm_type3::createTemperatureReadingResponse(tagId, ntx);
        if (rcCode != Ccode::Success) {
            fill_error_packet(rcCode, rx, tx);
            return;
        }
        tx.priv.packet_length = sizeof(Header) + HeaderResponseSize + ntx.data_size;
    }
    else {
        auto&    ntx_bulk      = NsmPktBulkResp::from(tx);
        uint16_t size_response = 0;
        auto rcCode = nsm_type3::createTemperatureAggregateResponse(ntx_bulk, size_response);
        if (rcCode != Ccode::Success) {
            fill_error_packet(rcCode, rx, tx);
            return;
        }
        tx.priv.packet_length = sizeof(Header) + AggregateHeaderResponseSize;
        if ((tx.priv.packet_length + size_response) < std::numeric_limits<uint16_t>::max()) {
            tx.priv.packet_length += size_response;
        }
    }
}

void Nsm::on_plat_env_getPowerDraw(const Packet& rx, Packet& tx)
{
    constexpr uint8_t RequestSize = 1;
    if (!is_input_length_valid(rx, RequestSize)) {
        fill_error_packet(Ccode::ErrorInvalidLength, rx, tx);
        return;
    }
    fill_packet_header(rx, tx);
    fill_nsm_msg_header(rx, tx);

    auto& nrx           = NsmPktReq::from(rx);
    auto& ntx           = NsmPktResp::from(tx);
    ntx.data_size       = 0;
    ntx.completion_code = Ccode::Success;
    auto tagId          = nrx.data[0];
    if (tagId < NsmPlatEnvTempAggregate) {
        auto rcCode = nsm_type3::createPowerDrawResponse(tagId, ntx);
        if (rcCode != Ccode::Success) {
            fill_error_packet(rcCode, rx, tx);
            return;
        }
        tx.priv.packet_length = sizeof(Header) + HeaderResponseSize + ntx.data_size;
    }
    else {
        auto&    ntx_bulk      = NsmPktBulkResp::from(tx);
        uint16_t size_response = 0;
        auto     rcCode = nsm_type3::createPowerDrawAggregateResponse(ntx_bulk, size_response);
        if (rcCode != Ccode::Success) {
            fill_error_packet(rcCode, rx, tx);
            return;
        }
        tx.priv.packet_length = sizeof(Header) + AggregateHeaderResponseSize;
        if ((tx.priv.packet_length + size_response) < std::numeric_limits<uint16_t>::max()) {
            tx.priv.packet_length += size_response;
        }
    }
}

void Nsm::on_plat_env_getVoltage(const Packet& rx, Packet& tx)
{
    constexpr uint8_t RequestSize = 1;
    if (!is_input_length_valid(rx, RequestSize)) {
        fill_error_packet(Ccode::ErrorInvalidLength, rx, tx);
        return;
    }

    if constexpr (mcuVoltageSensorsSize == 0) {
        fill_error_packet(Ccode::ErrorUnsupportedCmd, rx, tx);
        return;
    }

    fill_packet_header(rx, tx);
    fill_nsm_msg_header(rx, tx);

    auto& nrx           = NsmPktReq::from(rx);
    auto& ntx           = NsmPktResp::from(tx);
    ntx.data_size       = 0;
    ntx.completion_code = Ccode::Success;
    auto tagId          = nrx.data[0];
    if (tagId < NsmPlatEnvTempAggregate) {
        auto rcCode = nsm_type3::createVoltageResponse(tagId, ntx);
        if (rcCode != Ccode::Success) {
            fill_error_packet(rcCode, rx, tx);
            return;
        }
        tx.priv.packet_length = sizeof(Header) + HeaderResponseSize + ntx.data_size;
    }
    else {
        auto&    ntx_bulk      = NsmPktBulkResp::from(tx);
        uint16_t size_response = 0;
        auto     rcCode = nsm_type3::createVoltageAggregateResponse(ntx_bulk, size_response);
        if (rcCode != Ccode::Success) {
            fill_error_packet(rcCode, rx, tx);
            return;
        }
        tx.priv.packet_length = sizeof(Header) + AggregateHeaderResponseSize;
        if ((tx.priv.packet_length + size_response) < std::numeric_limits<uint16_t>::max()) {
            tx.priv.packet_length += size_response;
        }
    }
}

void Nsm::on_plat_env_getInventoryInformation(const Packet& rx, Packet& tx)
{
    constexpr uint8_t RequestSize = sizeof(GetInventoryInformationReq);
    if (!is_input_length_valid(rx, RequestSize)) {
        fill_error_packet(Ccode::ErrorInvalidLength, rx, tx);
        return;
    }

    fill_packet_header(rx, tx);
    fill_nsm_msg_header(rx, tx);
    auto& nrx = NsmPktReq::from(rx);
    auto& ntx = NsmPktResp::from(tx);

    GetInventoryInformationReq request{};
    memcpy(&request, &nrx.data, sizeof(GetInventoryInformationReq));

    switch (static_cast<NsmPlatEnvPropertyId>(request.property_id)) {
        case NsmPlatEnvPropertyId::SkuId: {
            const uint32_t sku = MCU_AP_SKU;
            memcpy(&ntx.data, &sku, sizeof(uint32_t));
            ntx.data_size = sizeof(uint32_t);
            break;
        }
        case NsmPlatEnvPropertyId::Uuid: {
            const auto& uuid = _ctl.router().ec.uuid;
            memcpy(&ntx.data, uuid.data(), uuid.size());
            ntx.data_size = uuid.size();
            break;
        }
        default: fill_error_packet(Ccode::ErrorInvalidData, rx, tx); return;
    }

    tx.priv.packet_length = sizeof(Header) + HeaderResponseSize + ntx.data_size;
    ntx.completion_code   = Ccode::Success;
}

void Nsm::on_plat_env_setThermalParameter(const Packet& rx, Packet& tx)
{
    constexpr uint8_t RequestSize = sizeof(NsmT3SetThermalParameterReq);
    if (!is_input_length_valid(rx, RequestSize)) {
        fill_error_packet(Ccode::ErrorInvalidLength, rx, tx);
        return;
    }

    fill_packet_header(rx, tx);
    fill_nsm_msg_header(rx, tx);

    auto& ntx = NsmPktResp::from(tx);

    auto&                       nrx = NsmPktReq::from(rx);
    NsmT3SetThermalParameterReq request{};
    memcpy(&request, &nrx.data, sizeof(NsmT3SetThermalParameterReq));

    switch (request.sensorId) {
        case Type3TemperatureSensors::BusBar_Temp: {
            auto ccode = nsm_type3::set_busbar_temperature_threshold(request);
            if (ccode != Ccode::Success) {
                fill_error_packet(ccode, rx, tx);
                return;
            }
            break;
        }
        case Type3TemperatureSensors::SMA_External:
        case Type3TemperatureSensors::CPU1_Die:
        case Type3TemperatureSensors::CPU1_SoC:
        case Type3TemperatureSensors::CPU2_Die:
        case Type3TemperatureSensors::CPU2_SoC:
        case Type3TemperatureSensors::PCB_Temp_1:
        case Type3TemperatureSensors::PCB_Temp_2:
        case Type3TemperatureSensors::PCB_Temp_3:
        case Type3TemperatureSensors::PCB_Temp_4:
        case Type3TemperatureSensors::GPU1_Die_A:
        case Type3TemperatureSensors::GPU1_Die_B:
        case Type3TemperatureSensors::GPU2_Die_A:
        case Type3TemperatureSensors::GPU2_Die_B:
        case Type3TemperatureSensors::NvLink_Temp:
        case Type3TemperatureSensors::CPLD_Temp   : {
            // Find sensor index using the extracted function
            const uint8_t sensorIndex = findTemperatureSensorIndex(request.sensorId);
            if (sensorIndex == Type3TemperatureSensors::TempSensor_End) {
                fill_error_packet(Ccode::ErrorInvalidData, rx, tx);
                return;
            }

            // Get sensor configuration from array using the index
            const auto&         sensorConfig = nv::mctp::I2cTempSensorList.at(sensorIndex);
            const nv::i2c::Port i2cPort      = sensorConfig.port;
            const uint8_t       i2cAddr      = sensorConfig.identified_addr;
            const uint8_t       id           = sensorConfig.sensor_model;

            if (id == nv::i2c::SensorModel::Sensor_Tmp461) {
                nv::i2c::Tmp461 tmp461(i2cPort, i2cAddr);
                tmp461.set_remote_high_alert_thresholds(static_cast<int8_t>(request.threshold));
            }
            else if (id == nv::i2c::SensorModel::Sensor_Emc1812) {
                nv::i2c::Emc1812 emc1812(i2cPort, i2cAddr);
                emc1812.set_remote_high_alert_threshold(static_cast<int8_t>(request.threshold));
            }
            else if (id == nv::i2c::SensorModel::Sensor_Tmp1075) {
                nv::i2c::Tmp1075 tmp1075(i2cPort, i2cAddr);
                tmp1075.set_high_limit(static_cast<int8_t>(request.threshold));
            }
            else if (id == nv::i2c::SensorModel::Sensor_Nct75) {
                nv::i2c::Nct75 nct75(i2cPort, i2cAddr);
                nct75.set_tos_limit(static_cast<int8_t>(request.threshold));
            }
            else {
                fill_error_packet(Ccode::ErrorGeneral, rx, tx);
                return;
            }
            break;
        }
        case Type3TemperatureSensors::HSCC_Temp:
        case Type3TemperatureSensors::HSCC_Temp_2: {
            const int8_t device_index = get_power_sensor_mgr().find_hscc_index_by_temp_sensor(
                static_cast<Type3TemperatureSensors>(request.sensorId));
            if (device_index >= 0) {
                auto status = get_power_sensor_mgr().hscc_write_ot_warn_limit(
                    device_index, request.threshold);
                if (status != nv::i2c::I2cStatus::Ok) {
                    fill_error_packet(Ccode::ErrorGeneral, rx, tx);
                    return;
                }
            }
            else {
                fill_error_packet(Ccode::ErrorGeneral, rx, tx);
                return;
            }
            break;
        }
        case Type3TemperatureSensors::HSC_Temp:
        case Type3TemperatureSensors::HSC_Temp_2:
        case Type3TemperatureSensors::HSC_Temp_3:
        case Type3TemperatureSensors::HSC_Temp_4:
        case Type3TemperatureSensors::HSC_Sby_Temp: {
            const int8_t device_index = get_power_sensor_mgr().find_hsc_index_by_temp_sensor(
                static_cast<Type3TemperatureSensors>(request.sensorId));
            if (device_index >= 0) {
                auto status = get_power_sensor_mgr().hsc_write_ot_warn_limit(device_index,
                                                                             request.threshold);
                if (status != nv::i2c::I2cStatus::Ok) {
                    fill_error_packet(Ccode::ErrorGeneral, rx, tx);
                    return;
                }
            }
            else {
                fill_error_packet(Ccode::ErrorGeneral, rx, tx);
                return;
            }
            break;
        }
        default: fill_error_packet(Ccode::ErrorInvalidData, rx, tx); return;
    }

    ntx.data_size         = 0;
    ntx.completion_code   = Ccode::Success;
    tx.priv.packet_length = sizeof(Header) + HeaderResponseSize + ntx.data_size;
}

void Nsm::on_plat_env_getThermalParameter(const Packet& rx, Packet& tx)
{
    constexpr uint8_t RequestSize = sizeof(NsmT3GetThermalParameterReq);
    if (!is_input_length_valid(rx, RequestSize)) {
        fill_error_packet(Ccode::ErrorInvalidLength, rx, tx);
        return;
    }

    fill_packet_header(rx, tx);
    fill_nsm_msg_header(rx, tx);

    auto& ntx = NsmPktResp::from(tx);

    auto&                       nrx = NsmPktReq::from(rx);
    NsmT3GetThermalParameterReq request{};
    memcpy(&request, &nrx.data, sizeof(NsmT3GetThermalParameterReq));

    struct NsmT3GetThermalParameterRes response
    {};
    switch (request.sensorId) {
        case Type3TemperatureSensors::BusBar_Temp: {
            uint32_t threshold = 0;
            auto     status    = nsm_type3::read_busbar_temperature_threshold(threshold);
            if (status != Ccode::Success) {
                fill_error_packet(status, rx, tx);
                return;
            }
            response.threshold = threshold;
            break;
        }
        case Type3TemperatureSensors::SMA_External:
        case Type3TemperatureSensors::CPU1_Die:
        case Type3TemperatureSensors::CPU1_SoC:
        case Type3TemperatureSensors::CPU2_Die:
        case Type3TemperatureSensors::CPU2_SoC:
        case Type3TemperatureSensors::PCB_Temp_1:
        case Type3TemperatureSensors::PCB_Temp_2:
        case Type3TemperatureSensors::PCB_Temp_3:
        case Type3TemperatureSensors::PCB_Temp_4:
        case Type3TemperatureSensors::GPU1_Die_A:
        case Type3TemperatureSensors::GPU1_Die_B:
        case Type3TemperatureSensors::GPU2_Die_A:
        case Type3TemperatureSensors::GPU2_Die_B:
        case Type3TemperatureSensors::NvLink_Temp : {
            const uint8_t sensorIndex = findTemperatureSensorIndex(request.sensorId);
            if (sensorIndex == Type3TemperatureSensors::TempSensor_End) {
                fill_error_packet(Ccode::ErrorInvalidData, rx, tx);
                return;
            }

            const auto&         sensorConfig = nv::mctp::I2cTempSensorList.at(sensorIndex);
            const nv::i2c::Port i2cPort      = sensorConfig.port;
            const uint8_t       i2cAddr      = sensorConfig.identified_addr;
            const uint8_t       id           = sensorConfig.sensor_model;
            int8_t              threshold    = 0;

            if (id == nv::i2c::SensorModel::Sensor_Tmp461) {
                nv::i2c::Tmp461 tmp461(i2cPort, i2cAddr);
                tmp461.get_remote_high_alert_thresholds(threshold);
            }
            else if (id == nv::i2c::SensorModel::Sensor_Emc1812) {
                nv::i2c::Emc1812 emc1812(i2cPort, i2cAddr);
                emc1812.get_remote_high_alert_threshold(threshold);
            }
            else if (id == nv::i2c::SensorModel::Sensor_Tmp1075) {
                nv::i2c::Tmp1075 tmp1075(i2cPort, i2cAddr);
                tmp1075.get_high_limit(threshold);
            }
            else if (id == nv::i2c::SensorModel::Sensor_Nct75) {
                nv::i2c::Nct75 nct75(i2cPort, i2cAddr);
                nct75.get_tos_limit(threshold);
            }
            else {
                fill_error_packet(Ccode::ErrorGeneral, rx, tx);
                return;
            }
            response.threshold = static_cast<uint32_t>(static_cast<uint8_t>(threshold));
            break;
        }
        case Type3TemperatureSensors::HSCC_Temp:
        case Type3TemperatureSensors::HSCC_Temp_2: {
            uint8_t      threshold    = 0;
            const int8_t device_index = get_power_sensor_mgr().find_hscc_index_by_temp_sensor(
                static_cast<Type3TemperatureSensors>(request.sensorId));
            if (device_index >= 0) {
                auto status = get_power_sensor_mgr().hscc_read_ot_warn_limit(device_index,
                                                                             threshold);
                if (status != nv::i2c::I2cStatus::Ok) {
                    fill_error_packet(Ccode::ErrorGeneral, rx, tx);
                    return;
                }
            }
            else {
                fill_error_packet(Ccode::ErrorGeneral, rx, tx);
                return;
            }
            response.threshold = static_cast<uint32_t>(static_cast<uint8_t>(threshold));
            break;
        }
        case Type3TemperatureSensors::HSC_Temp:
        case Type3TemperatureSensors::HSC_Temp_2:
        case Type3TemperatureSensors::HSC_Temp_3:
        case Type3TemperatureSensors::HSC_Temp_4:
        case Type3TemperatureSensors::HSC_Sby_Temp: {
            uint8_t      threshold    = 0;
            const int8_t device_index = get_power_sensor_mgr().find_hsc_index_by_temp_sensor(
                static_cast<Type3TemperatureSensors>(request.sensorId));
            if (device_index >= 0) {
                auto status = get_power_sensor_mgr().hsc_read_ot_warn_limit(device_index,
                                                                            threshold);
                if (status != nv::i2c::I2cStatus::Ok) {
                    fill_error_packet(Ccode::ErrorGeneral, rx, tx);
                    return;
                }
            }
            else {
                fill_error_packet(Ccode::ErrorGeneral, rx, tx);
                return;
            }
            response.threshold = static_cast<uint32_t>(static_cast<uint8_t>(threshold));
            break;
        }
        default: fill_error_packet(Ccode::ErrorInvalidData, rx, tx); return;
    }

    memcpy(&ntx.data, &response, sizeof(NsmT3GetThermalParameterRes));

    ntx.data_size         = sizeof(NsmT3GetThermalParameterRes);
    ntx.completion_code   = Ccode::Success;
    tx.priv.packet_length = sizeof(Header) + HeaderResponseSize + ntx.data_size;
}

void Nsm::on_plat_env_setLeakDetectionThresholds(const Packet& rx, Packet& tx)
{
    fill_packet_header(rx, tx);
    fill_nsm_msg_header(rx, tx);
    auto& nrx = NsmPktReq::from(rx);

    // Check if leak detection is enabled at runtime
    if (!nv::leak_detect::LeakDetect::inst().is_enabled()) {
        fill_error_packet_v2(Ccode::ErrorUnsupportedCmd, Rcode::Null, rx, tx);
        return;
    }

    if (nrx.ocp_version != 1) {
        fill_error_packet_v2(Ccode::ErrorInvalidOcpVersion, Rcode::Null, rx, tx);
        return;
    }

    auto num_sensors    = nrx.data[0];
    auto num_thresholds = nrx.data[1];

    const uint8_t RequestSize = sizeof(LeakDetectionHeader)
                              + sizeof(SetLeakSensorThresholdsRequest) * num_sensors;

    if (!is_input_length_valid(rx, RequestSize) || nrx.data_size != RequestSize) {
        fill_error_packet_v2(Ccode::ErrorInvalidLength, Rcode::Null, rx, tx);
        return;
    }

    if (num_sensors == 0
        || num_sensors > nv::ipc::voltage_monitor_config::LeakDetectSensorNum) {
        fill_error_packet_v2(Ccode::ErrorInvalidData, Rcode::InvalidSensorNumber, rx, tx);
        return;
    }

    if (num_thresholds != NsmPlatEnvThresholdSize) {
        fill_error_packet_v2(Ccode::ErrorInvalidData, Rcode::InvalidThresholdNumber, rx, tx);
        return;
    }

    NsmPlatEnvLeakDetectionRequest leak_detection_request{};

    memcpy(&leak_detection_request, &nrx.data[0], RequestSize);

    // Validate all sensor IDs first to avoid partial update.
    std::array<uint8_t, nv::ipc::voltage_monitor_config::LeakDetectSensorNum> sensor_indices{};
    for (auto i = 0U; i < static_cast<unsigned>(num_sensors); i++) {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
        const auto& sensor = leak_detection_request.sensors[i];
        uint8_t     sensorIdx{};

        if (nv::leak_detect::LeakDetect::inst().find_sensor_index(sensor.sensor_id, sensorIdx)
            != nv::volt_mon::Status::Ok) {
            fill_error_packet_v2(Ccode::ErrorInvalidData, Rcode::InvalidSensorId, rx, tx);
            return;
        }
        sensor_indices.at(static_cast<std::size_t>(i)) = sensorIdx;
    }

    // Apply thresholds only after all sensor IDs are validated.
    for (auto i = 0U; i < static_cast<unsigned>(num_sensors); i++) {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
        const auto& sensor    = leak_detection_request.sensors[i];
        const auto  sensorIdx = sensor_indices.at(static_cast<std::size_t>(i));

        leak_detect::ThresholdLeakDet config{};
        config.minLeak   = sensor.thresholds[0];
        config.maxLeak   = sensor.thresholds[1];
        config.maxNormal = sensor.thresholds[2];

        Ccode ccode  = Ccode::Success;
        Rcode rcode  = Rcode::Null;
        auto  status = nv::leak_detect::LeakDetect::inst().set_thresholds(sensorIdx, config);
        switch (status) {
            case nv::volt_mon::Status::Ok: break;
            case nv::volt_mon::Status::InvalidSensorId:
                ccode = Ccode::ErrorInvalidData;
                rcode = Rcode::InvalidSensorId;
                break;
            case nv::volt_mon::Status::InvalidThreshold:
                ccode = Ccode::ErrorInvalidData;
                rcode = Rcode::InvalidThreshold;
                break;
            default:
                ccode = Ccode::ErrorGeneral;
                rcode = Rcode::Null;
                break;
        }
        if (ccode != Ccode::Success) {
            fill_error_packet_v2(ccode, rcode, rx, tx);
            return;
        }
    }

    /** @note after setting thresholds, update sensor state/vrgpio state by querying the sensor
     * info */
    std::array<nv::volt_mon::LeakDetectSensor,
               nv::ipc::voltage_monitor_config::LeakDetectSensorNum>
        sensor_info{};

    nv::leak_detect::LeakDetect::inst().get_sensor_info(sensor_info);

    auto& ntx             = NsmPktResp::from(tx);
    ntx.data_size         = 0;
    ntx.completion_code   = Ccode::Success;
    tx.priv.packet_length = sizeof(Header) + HeaderResponseSize + ntx.data_size;
}

void Nsm::on_plat_env_getLeakDetectionInfo(const Packet& rx, Packet& tx)
{
    fill_packet_header(rx, tx);
    fill_nsm_msg_header(rx, tx);
    auto& nrx = NsmPktReq::from(rx);

    // Check if leak detection is enabled at runtime
    if (!nv::leak_detect::LeakDetect::inst().is_enabled()) {
        fill_error_packet(Ccode::ErrorUnsupportedCmd, rx, tx);
        return;
    }

    if (nrx.ocp_version != 1) {
        fill_error_packet(Ccode::ErrorInvalidData, rx, tx);
        return;
    }

    auto& ntx = NsmPktResp::from(tx);

    NsmPlatEnvLeakDetectionResponse leak_detection_response{};

    leak_detection_response.header
        .n_sensors = nv::ipc::voltage_monitor_config::LeakDetectSensorNum;
    leak_detection_response.header
        .n_sensors = nv::ipc::voltage_monitor_config::LeakDetectSensorNum;
    leak_detection_response.header.n_thresholds = NsmPlatEnvThresholdSize;

    std::array<nv::volt_mon::LeakDetectSensor,
               nv::ipc::voltage_monitor_config::LeakDetectSensorNum>
        sensor_info{};
    if (nv::leak_detect::LeakDetect::inst().get_sensor_info(sensor_info)
        != nv::volt_mon::Status::Ok) {
        fill_error_packet(Ccode::ErrorGeneral, rx, tx);
        return;
    }

    for (auto i = 0U; i < nv::ipc::voltage_monitor_config::LeakDetectSensorNum; i++) {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
        auto&       response_sensor = leak_detection_response.sensors[i];
        const auto& info            = sensor_info.at(i);

        response_sensor.sensor_id   = info.id;
        response_sensor.leak        = static_cast<uint8_t>(info.state);
        response_sensor.adc_reading = volt_mon::to_vol_value(info.reading);

        // For VR, all leak detection sensors shall have three thresholds. Check against the
        // sensor info to detect the change in the future if any.
        // if (sensor_info[i].thNum != NsmPlatEnvThresholdSize) {
        //     fill_error_packet(Ccode::ErrorGeneral, rx, tx);
        //     return;
        // }
        // if (sensor_info[i].thNum != NsmPlatEnvThresholdSize) {
        //     fill_error_packet(Ccode::ErrorGeneral, rx, tx);
        //     return;
        // }

        response_sensor.thresholds[0] = volt_mon::to_vol_value(info.minLeak);
        response_sensor.thresholds[1] = volt_mon::to_vol_value(info.maxLeak);
        response_sensor.thresholds[2] = volt_mon::to_vol_value(info.maxNormal);
    }

    ntx.data_size = sizeof(LeakDetectionHeader)
                  + sizeof(SetLeakSensorThresholdsResponse)
                        * nv::ipc::voltage_monitor_config::LeakDetectSensorNum;
    ntx.completion_code   = Ccode::Success;
    tx.priv.packet_length = sizeof(Header) + HeaderResponseSize + ntx.data_size;

    memcpy(&ntx.data[0], &leak_detection_response, ntx.data_size);
}
}  // namespace nv::mctp
