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

#include <cstdint>
#include <cstring>

#include "config.h"

#include "nv/mctp/enums.h"
#include "nv/i2c/sensor.h"
#include "nv/i2c/tmp461.h"
#include "nv/i2c/emc1812.h"
#include "nv/i2c/tmp1075.h"
#include "nv/logger/log.h"
#include "nv/mctp/nsm.h"
#include "sys/sensor/sensor.h"
#include "nv/telemetry/utils.h"
#ifdef ENABLE_ADC_LEAK_DETECTION
#include "nv/leak_det/leak_detect.h"
#endif
using namespace nv;
using namespace mctp;

namespace nv::mctp {

/** A map indexed by Type3TemperatureSensors which has corresponding
 *  tag in nv::telemetry::TelemId::TelemId */
static constexpr std::array<nv::telemetry::TelemId, 8> cacheTemperatureSensorsMap{
    nv::telemetry::TelemId::Gpu1Temp,       // TempGpu1
    nv::telemetry::TelemId::Gpu2Temp,       // TempGpu2
    nv::telemetry::TelemId::ModuleTemp1,    // TempTMP451_1
    nv::telemetry::TelemId::ModuleTemp2,    // TempTMP451_2
    nv::telemetry::TelemId::MaxModuleTemp,  // TempMaxModule
    nv::telemetry::TelemId::InternalTemp,   // TempSMAInternal
    nv::telemetry::TelemId::CX8_1_Temp,     // TempCX8_1
    nv::telemetry::TelemId::CX8_2_Temp      // TempCX8_2
};

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
            if (nv::mctp::I2cTempSensorList.at(i).sensor_id == sensorId) {
                // coverity[DEADCODE:dead_error_line]
                if (I2cTempSensorList.at(i).port != nv::i2c::Port::End
                    && I2cTempSensorList.at(i).identified_addr != 0) {
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

using TelemetryValue                       = nv::telemetry::Cache::Value;
using TelemetryItem                        = nv::telemetry::TelemId;
auto constexpr InvalidSensorId             = nv::telemetry::TelemId::MaxItem;
static auto constexpr TelemetryInvalid     = nv::telemetry::Cache::InvalidItem;
static constexpr auto TelemetryInvalidData = static_cast<TelemetryValue>(
    nv::telemetry::Cache::InvalidData);
static auto constexpr ConversionFactor_NvS24_8 = 256;

/**
 * @brief getValidCachedTemperatureSensor
 *      - The tagId needs to match 2 tables:
 *       #- cacheTemperatureSensorsMap where the TagId is the index
 *       #- mcuTemperatureSensors List of valid sensors for the MCU
 *
 * @return The corresponding sensor Id if the tagId is valid (match 2 tables)
 */
TelemetryItem getValidCachedTemperatureSensor(const uint8_t tagId)
{
    if (tagId < cacheTemperatureSensorsMap.size()) {
        for (auto sensorId : nv::mctp::mcuTemperatureSensors) {
            if (sensorId == tagId) {
                return cacheTemperatureSensorsMap.at(tagId);
            }
        }
    }
    return InvalidSensorId;
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
    if (tagId < cachePowerSensorsMap.size()) {
        for (auto sensorId : nv::mctp::mcuPowerSensors) {
            if (sensorId == tagId) {
                return cachePowerSensorsMap.at(tagId);
            }
        }
    }
    return InvalidSensorId;
}

TelemetryValue get_internal_sensor_temperature()
{
    TelemetryValue temperature    = TelemetryInvalid;
    float          floatTemp      = 0.0;
    auto           gotTemperature = sys::sensor::Driver::get_current_temperature(floatTemp);
    if (true == gotTemperature) {
        temperature = static_cast<TelemetryValue>(floatTemp);
    }
    return temperature;
}

TelemetryValue get_cx8_sensor_temperature(uint8_t sensorId)
{
    TelemetryValue      temperature = TelemetryInvalid;
    const TelemetryItem telemetryId = nv::telemetry::getTelemIdFromType3(sensorId);
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
            nv::info("TI sensor error: %d\n", static_cast<int>(status));
            return TelemetryInvalid;
        }
    }

    else if (id == nv::i2c::SensorModel::Sensor_Emc1812) {
        nv::info("Microchip sensor, addr: %d\n", static_cast<int>(i2cAddr));
        nv::i2c::Emc1812 emc1812(i2cPort, i2cAddr);
        int8_t           temp   = 0;
        auto             status = emc1812.get_local_high_temp(temp);
        if (status == nv::i2c::I2cStatus::Ok) {
            return static_cast<TelemetryValue>(temp);
        }
        else {
            // TODO: Add to flash log
            nv::info("Microchip sensor error: %d\n", static_cast<int>(status));
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

    return TelemetryInvalid;
}

TelemetryValue getTelemetry(const uint8_t sensorId)
{
    TelemetryValue telemetry_value = TelemetryInvalid;

    // Check if the temperature sensor is available from cache
    if (sensorId < VR_TempSensor_Start) {
        auto cacheId = nv::telemetry::getTelemIdFromType3(sensorId);
        if (cacheId == nv::telemetry::TelemId::MaxItem) {
            return TelemetryInvalid;
        }
        nv::info("Read Temperature from Cache: cacheId: %d\n", cacheId);
        nv::telemetry::Cache& telemetry = nv::telemetry::Cache::inst();
        (void)telemetry.get_table();
        // gets corresponding sensor id in Cache
        telemetry_value = telemetry.get_cache(cacheId);
        if (telemetry_value != TelemetryInvalid && telemetry_value != TelemetryInvalidData) {
            switch (sensorId) {
                case nv::telemetry::TelemId::InternalTemp:
                    telemetry_value = get_internal_sensor_temperature();
                    break;
                case nv::telemetry::TelemId::CX8_1_Temp:
                case nv::telemetry::TelemId::CX8_2_Temp:
                    telemetry_value = get_cx8_sensor_temperature(sensorId);
                    break;
                default: telemetry_value = TelemetryInvalid; break;
            }
        }
        return telemetry_value;
    }
    // Directly read temperature from sensor
    else {
        nv::info("Read Temperature from Sensor: sensorId: %d\n", sensorId);
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
            case Type3TemperatureSensors::GPU1_Die_A:
            case Type3TemperatureSensors::GPU1_Die_B:
            case Type3TemperatureSensors::GPU2_Die_A:
            case Type3TemperatureSensors::GPU2_Die_B:
            case Type3TemperatureSensors::HSCC_Temp:
            case Type3TemperatureSensors::HSC_Temp:
                telemetry_value = read_i2c_temp_sensor(sensorId);
                break;

            default: telemetry_value = TelemetryInvalid; break;
        }
    }

    return telemetry_value;
}

bool is_temp_sensor_available(uint8_t sensorId)
{
    // Check if sensorId exists in mcuTemperatureSensors array
    for (const auto& validSensorId : mcuTemperatureSensors) {
        if (validSensorId == sensorId) {
            return true;  // Found the sensor
        }
    }
    return false;  // Sensor not found.
}

Ccode getTemperatureTelemetry(const uint8_t tagId, int32_t& telemetry)
{
    if (false == is_temp_sensor_available(tagId)) {
        return Ccode::ErrorInvalidData;
    }
    nv::info("getTemperatureTelemetry(): tagId: %d\n", tagId);

    auto temperature = getTelemetry(tagId);
    if (temperature == TelemetryInvalid || temperature == TelemetryInvalidData) {
        auto invalid = TelemetryInvalid;
        std::memcpy(&telemetry, &invalid, sizeof(int32_t));
    }
    else {
        telemetry = static_cast<int32_t>(temperature) * ConversionFactor_NvS24_8;
    }
    return Ccode::Success;
}

Ccode createTemperatureReadingResponse(const uint8_t tagId, NsmPktResp& ntx)
{
    int32_t temperature = 0;
    auto    rcCode      = getTemperatureTelemetry(tagId, temperature);
    if (rcCode != Ccode::Success) {
        return rcCode;
    }
    std::memcpy(&ntx.data[0], &temperature, sizeof(int32_t));
    ntx.data_size = sizeof(int32_t);
    return Ccode::Success;
}

Ccode getPowerDrawTelemetry(const uint8_t tagId, uint32_t& telemetry)
{
    auto sensorId = getValidCachedPowerDrawSensor(tagId);
    if (sensorId == InvalidSensorId) {
        return Ccode::ErrorInvalidData;
    }
    telemetry = getTelemetry(sensorId);
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
    uint32_t power_value = 0;
    auto     rcCode      = getPowerDrawTelemetry(tagId, power_value);
    if (rcCode != Ccode::Success) {
        return rcCode;
    }
    ntx.data_size = sizeof(uint32_t);
    std::memcpy(&ntx.data[0], &power_value, sizeof(uint32_t));
    return Ccode::Success;
}

template<typename Value32Bits>
bool appendTelemetryRecord(const uint8_t         tagId,
                           const Value32Bits&    telemetry,
                           TelemetryRecordArray& array)
{
    return array.addRecordNvU32(tagId, telemetry);
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
    Ccode    rcCode     = Ccode::Success;
    // loop all sensors
    for (auto const tagId : mcuPowerSensors) {
        if ((rcCode = getPowerDrawTelemetry(tagId, powerValue)) != Ccode::Success) {
            return rcCode;
        }
        if (false == appendTelemetryRecord(tagId, powerValue, aggregateArray)) {
            return Ccode::ErrorInvalidLength;
        }
    }
    size_response            = aggregateArray.arraySize();
    ntx_bulk.telemetry_count = aggregateArray.elements();
    return Ccode::Success;
}

Ccode createTemperatureAggregateResponse(NsmPktBulkResp& ntx_bulk, uint16_t& size_response)
{
    // aggregateArray fills ntx_bulk->data[0] ... ntx_bulk->data[N]
    TelemetryRecordArray aggregateArray(&ntx_bulk.data[0],
                                        TelemetryRecordArray::DefaultMctpDataSize);
    if (false == aggregateArray.addTimestampRecord(NsmPlatEnvTempAggregate)) {
        return Ccode::ErrorInvalidLength;
    }
    int32_t temperature = 0;
    Ccode   rcCode      = Ccode::Success;
    // loop all sensors
    for (const auto tagId : mcuTemperatureSensors) {
        if ((rcCode = getTemperatureTelemetry(tagId, temperature)) != Ccode::Success) {
            return rcCode;
        }
        if (false == appendTelemetryRecord(tagId, temperature, aggregateArray)) {
            return Ccode::ErrorInvalidLength;
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

    switch (nrx.get_plat_env_code()) {
        case cmd::GetTemperatureReading  : on_plat_env_getTemperatureReading(rx, tx); break;
        case cmd::GetCurrentPowerDraw    : on_plat_env_getPowerDraw(rx, tx); break;
        case cmd::GetInventoryInformation: on_plat_env_getInventoryInformation(rx, tx); break;
        case cmd::SetThermalParameter    : on_plat_env_setThermalParameter(rx, tx); break;
        case cmd::GetThermalParameter    : on_plat_env_getThermalParameter(rx, tx); break;
#ifdef ENABLE_ADC_LEAK_DETECTION
        case cmd::GetLeakDetectionInfo: on_plat_env_getLeakDetectionInfo(rx, tx); break;
        case cmd::SetLeakDetectionThresholds:
            on_plat_env_setLeakDetectionThresholds(rx, tx);
            break;
#endif
        default: fill_error_packet(Ccode::ErrorUnsupportedCmd, rx, tx); return false;
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
        default:
            nv::error("Nsm::on_plat_env_getInventoryInformation(): Invalid property ID %d\n",
                      static_cast<int>(request.property_id));
            fill_error_packet(Ccode::ErrorInvalidData, rx, tx);
            return;
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
        emc1812.set_local_high_alert_threshold(static_cast<int8_t>(request.threshold));
    }
    else if (id == nv::i2c::SensorModel::Sensor_Tmp1075) {
        nv::i2c::Tmp1075 tmp1075(i2cPort, i2cAddr);
        tmp1075.set_high_limit(static_cast<int8_t>(request.threshold));
    }
    else {
        fill_error_packet(Ccode::ErrorGeneral, rx, tx);
        return;
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

    const uint8_t sensorIndex = findTemperatureSensorIndex(request.sensorId);
    if (sensorIndex == Type3TemperatureSensors::TempSensor_End) {
        fill_error_packet(Ccode::ErrorInvalidData, rx, tx);
        return;
    }

    const auto&         sensorConfig = nv::mctp::I2cTempSensorList.at(sensorIndex);
    const nv::i2c::Port i2cPort      = sensorConfig.port;
    const uint8_t       i2cAddr      = sensorConfig.identified_addr;
    const uint8_t       id           = sensorConfig.sensor_model;

    struct NsmT3GetThermalParameterRes response
    {};

    int8_t threshold = 0;

    if (id == nv::i2c::SensorModel::Sensor_Tmp461) {
        nv::i2c::Tmp461 tmp461(i2cPort, i2cAddr);
        tmp461.get_remote_high_alert_thresholds(threshold);
    }
    else if (id == nv::i2c::SensorModel::Sensor_Emc1812) {
        nv::i2c::Emc1812 emc1812(i2cPort, i2cAddr);
        emc1812.get_local_high_alert_threshold(threshold);
    }
    else if (id == nv::i2c::SensorModel::Sensor_Tmp1075) {
        nv::i2c::Tmp1075 tmp1075(i2cPort, i2cAddr);
        tmp1075.get_high_limit(threshold);
    }
    else {
        fill_error_packet(Ccode::ErrorGeneral, rx, tx);
        return;
    }

    response.threshold = static_cast<uint32_t>(static_cast<uint8_t>(threshold));

    memcpy(&ntx.data, &response, sizeof(NsmT3GetThermalParameterRes));

    ntx.data_size         = sizeof(NsmT3GetThermalParameterRes);
    ntx.completion_code   = Ccode::Success;
    tx.priv.packet_length = sizeof(Header) + HeaderResponseSize + ntx.data_size;
}

void Nsm::on_plat_env_setLeakDetectionThresholds(const Packet& rx, Packet& tx)
{
#ifndef ENABLE_ADC_LEAK_DETECTION
    fill_error_packet(Ccode::ErrorUnsupportedCmd, rx, tx);
    return;
#else

    fill_packet_header(rx, tx);
    fill_nsm_msg_header(rx, tx);
    auto& nrx = NsmPktReq::from(rx);

    if (nrx.ocp_version != 1) {
        fill_error_packet(Ccode::ErrorInvalidData, rx, tx);
        return;
    }

    auto num_sensors    = nrx.data[0];
    auto num_thresholds = nrx.data[1];

    const uint8_t RequestSize = sizeof(LeakDetectionHeader)
                              + sizeof(SetLeakSensorThresholdsRequest) * num_sensors;

    if (!is_input_length_valid(rx, RequestSize) || nrx.data_size != RequestSize) {
        fill_error_packet(Ccode::ErrorInvalidLength, rx, tx);
        return;
    }

    if (num_sensors == 0 || num_sensors > nv::ipc::leak_detect_config::LeakDetectSensorNum) {
        fill_error_packet(Ccode::ErrorInvalidData, rx, tx);
        return;
    }

    if (num_thresholds != NsmPlatEnvThresholdSize) {
        fill_error_packet(Ccode::ErrorInvalidData, rx, tx);
        return;
    }

    NsmPlatEnvLeakDetectionRequest leak_detection_request{};

    memcpy(&leak_detection_request, &nrx.data[0], RequestSize);

    for (auto i = 0; i < num_sensors; i++) {
        // Convert sensor index to sensor ID, then back to index for API call
        // This demonstrates the use of find_sensor_index method
        uint8_t     sensorIdx = 0;
        const auto& sensor    = leak_detection_request.sensors[i];

        if (nv::leak_detect::LeakDetect::inst().find_sensor_index(sensor.sensor_id, sensorIdx)
            != leak_detect::Status::Ok) {
            fill_error_packet(Ccode::ErrorInvalidData, rx, tx);
            return;
        }
        leak_detect::ThresholdConfig config;
        config.minLeak   = sensor.thresholds[0];
        config.maxLeak   = sensor.thresholds[1];
        config.maxNormal = sensor.thresholds[2];

        if (nv::leak_detect::LeakDetect::inst().set_thresholds(sensorIdx, config)
            != leak_detect::Status::Ok) {
            fill_error_packet(Ccode::ErrorGeneral, rx, tx);
            return;
        }
    }

    auto& ntx             = NsmPktResp::from(tx);
    ntx.data_size         = 0;
    ntx.completion_code   = Ccode::Success;
    tx.priv.packet_length = sizeof(Header) + HeaderResponseSize + ntx.data_size;
#endif
}

void Nsm::on_plat_env_getLeakDetectionInfo(const Packet& rx, Packet& tx)
{
#ifndef ENABLE_ADC_LEAK_DETECTION
    fill_error_packet(Ccode::ErrorUnsupportedCmd, rx, tx);
    return;
#else

    fill_packet_header(rx, tx);
    fill_nsm_msg_header(rx, tx);
    auto& nrx = NsmPktReq::from(rx);
    if (nrx.ocp_version != 1) {
        fill_error_packet(Ccode::ErrorInvalidData, rx, tx);
        return;
    }

    auto& ntx = NsmPktResp::from(tx);

    NsmPlatEnvLeakDetectionResponse leak_detection_response{};

    leak_detection_response.header.n_sensors = nv::ipc::leak_detect_config::LeakDetectSensorNum;
    leak_detection_response.header.n_thresholds = NsmPlatEnvThresholdSize;

    std::array<nv::ipc::leak_detect_config::LeakDetectSensor,
               nv::ipc::leak_detect_config::LeakDetectSensorNum>
        sensor_info;
    if (nv::leak_detect::LeakDetect::inst().get_sensor_info(sensor_info)
        != leak_detect::Status::Ok) {
        fill_error_packet(Ccode::ErrorGeneral, rx, tx);
        return;
    }

    for (auto i = 0U; i < nv::ipc::leak_detect_config::LeakDetectSensorNum; i++) {
        leak_detection_response.sensors[i].sensor_id = sensor_info[i].id;
        leak_detection_response.sensors[i].leak = static_cast<uint8_t>(sensor_info[i].state);
        leak_detection_response.sensors[i].adc_reading = leak_detect::to_vol_value(
            sensor_info[i].reading);

        // For VR, all leak detection sensors shall have three thresholds. Check against the
        // sensor info to detect the change in the future if any.
        if (sensor_info[i].thNum != NsmPlatEnvThresholdSize) {
            fill_error_packet(Ccode::ErrorGeneral, rx, tx);
            return;
        }

        leak_detection_response.sensors[i].thresholds[0] = leak_detect::to_vol_value(
            sensor_info[i].minLeak);
        leak_detection_response.sensors[i].thresholds[1] = leak_detect::to_vol_value(
            sensor_info[i].maxLeak);
        leak_detection_response.sensors[i].thresholds[2] = leak_detect::to_vol_value(
            sensor_info[i].maxNormal);
    }

    ntx.data_size = sizeof(LeakDetectionHeader)
                  + sizeof(SetLeakSensorThresholdsResponse)
                        * nv::ipc::leak_detect_config::LeakDetectSensorNum;
    ntx.completion_code   = Ccode::Success;
    tx.priv.packet_length = sizeof(Header) + HeaderResponseSize + ntx.data_size;

    memcpy(&ntx.data[0], &leak_detection_response, ntx.data_size);
#endif
}
}  // namespace nv::mctp
