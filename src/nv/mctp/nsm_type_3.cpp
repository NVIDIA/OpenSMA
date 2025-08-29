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

#include "nv/i2c/sensor.h"
#include "nv/logger/log.h"
#include "nv/mctp/nsm.h"
#include "sys/sensor/sensor.h"

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
        for (auto sensorId : mcuTemperatureSensors) {
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
        for (auto sensorId : mcuPowerSensors) {
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

TelemetryValue get_cx8_sensor_temperature(TelemetryItem sensorId)
{
    TelemetryValue temperature = TelemetryInvalid;
    if (nv::ipc::ModuleTempSensorSize > 0) {
        for (const auto& cx8_sensor : nv::ipc::ModuleTempSensorList) {
            auto telemetry = std::get<2>(cx8_sensor);
            if (telemetry == sensorId) {
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

TelemetryValue getTelemetry(const TelemetryItem sensorId)
{
    nv::telemetry::Cache& telemetry = nv::telemetry::Cache::inst();
    (void)telemetry.get_table();
    // gets corresponding sensor id in Cache
    auto telemetry_value = telemetry.get_cache(sensorId);
    // if cache failed handle some sensor temperatures
    if (telemetry_value == TelemetryInvalid || telemetry_value == TelemetryInvalidData) {
        switch (sensorId) {
            case nv::telemetry::TelemId::InternalTemp:
                telemetry_value = get_internal_sensor_temperature();
                break;

            default: telemetry_value = TelemetryInvalid; break;
        }
    }
    return telemetry_value;
}

Ccode getTemperatureTelemetry(const uint8_t tagId, int32_t& telemetry)
{
    auto sensorId = getValidCachedTemperatureSensor(tagId);
    if (sensorId == InvalidSensorId) {
        return Ccode::ErrorInvalidData;
    }
    auto temperature = getTelemetry(sensorId);
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
        default                          : fill_error_packet(Ccode::ErrorUnsupportedCmd, rx, tx); return false;
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

}  // namespace nv::mctp
