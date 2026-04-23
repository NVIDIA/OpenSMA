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

#include <climits>
#include <cstdint>
#include <cstring>

#include "config.h"
#include "corepdk/platforms/mcxn236/pldm-fd/src/pldm_wrap.h"

#include "nv/bootloader.h"
#include "nv/common/system.h"
#include "nv/flash/flash.h"
#include "nv/gpio/common.h"
#include "nv/gpio/driver.h"
#include "nv/i2c/lattice_driver.h"
#include "nv/i2c/task.h"
#include "nv/ipc/supervisor.h"
#include "nv/logger/log.h"
#include "nv/mctp/control.h"
#include "nv/mctp/nsm.h"
#include "nv/mctp/task.h"
#include "nv/watchdog/notify_interface.h"

namespace nv::mctp {

constexpr auto TelemetryInvalid = nv::telemetry::Cache::InvalidItem;

// T4CpuUtilizationEntryNum cannot be greater than nv::perf_mon::CpuUtilizationEntryNum
constexpr auto T4CpuUtilizationEntryNum = nv::perf_mon::CpuUtilizationEntryNum;

// T4TaskExecutionTimeEntryNum cannot be greater than nv::perf_mon::TaskExecutionTimeEntryNum
constexpr auto T4TaskExecutionTimeEntryNum = 10;  // nv::perf_mon::TaskExecutionTimeEntryNum;

static_assert(
    T4CpuUtilizationEntryNum <= nv::perf_mon::CpuUtilizationEntryNum,
    "T4CpuUtilizationEntryNum cannot be greater than nv::perf_mon::CpuUtilizationEntryNum");
static_assert(T4TaskExecutionTimeEntryNum <= nv::perf_mon::TaskExecutionTimeEntryNum,
              "T4TaskExecutionTimeEntryNum cannot be greater than "
              "nv::perf_mon::TaskExecutionTimeEntryNum");

using OobBusT4Tags = std::tuple<nv::perf_mon::OobBus, uint8_t>;
constexpr std::array<OobBusT4Tags, nv::perf_mon::OobBusTypeNum> t4TagsForOobBusMap{

    OobBusT4Tags{ nv::perf_mon::OobBus::UsI2c,            DIAG_I2C_UPSTREAM_ERROR},
    OobBusT4Tags{nv::perf_mon::OobBus::DsI2c0, DIAG_I2C0_DOWNSTREAM_BUS_ERROR + 0},
    OobBusT4Tags{nv::perf_mon::OobBus::DsI2c1, DIAG_I2C0_DOWNSTREAM_BUS_ERROR + 1},
    OobBusT4Tags{nv::perf_mon::OobBus::DsI2c2, DIAG_I2C0_DOWNSTREAM_BUS_ERROR + 2},
    OobBusT4Tags{nv::perf_mon::OobBus::DsI2c3, DIAG_I2C0_DOWNSTREAM_BUS_ERROR + 3},
    OobBusT4Tags{nv::perf_mon::OobBus::DsI2c4, DIAG_I2C0_DOWNSTREAM_BUS_ERROR + 4},
    OobBusT4Tags{nv::perf_mon::OobBus::DsI2c5, DIAG_I2C0_DOWNSTREAM_BUS_ERROR + 5},
    OobBusT4Tags{nv::perf_mon::OobBus::DsI2c6, DIAG_I2C0_DOWNSTREAM_BUS_ERROR + 6},
    OobBusT4Tags{nv::perf_mon::OobBus::DsI2c7, DIAG_I2C0_DOWNSTREAM_BUS_ERROR + 7},

    OobBusT4Tags{nv::perf_mon::OobBus::DsI3c0,            DIAG_I3C0_BUS_ERROR + 0},
    OobBusT4Tags{nv::perf_mon::OobBus::DsI3c1,            DIAG_I3C0_BUS_ERROR + 1},

    OobBusT4Tags{  nv::perf_mon::OobBus::Spi0, DIAG_SPI0_DOWNSTREAM_BUS_ERROR + 0},
    OobBusT4Tags{  nv::perf_mon::OobBus::Spi1, DIAG_SPI0_DOWNSTREAM_BUS_ERROR + 1},
    OobBusT4Tags{  nv::perf_mon::OobBus::Spi2, DIAG_SPI0_DOWNSTREAM_BUS_ERROR + 2},
};

constexpr bool verifyT4TagsForOobBusMap()
{
    // in case  OobBusTypeNum increases the T4 tag will be zero
    const auto& last_item = t4TagsForOobBusMap.at(t4TagsForOobBusMap.size() - 1);
    const auto  last_tag  = std::get<1>(last_item);
    return last_tag != 0;
}

static_assert(verifyT4TagsForOobBusMap(),
              "nv::perf_mon::OobBusTypeNum does not match with 't4TagsForOobBusMap' elements,"
              " perhaps nv::perf_mon::OobBusTypeNum has been increased and then"
              " 't4TagsForOobBusMap' requires adjustment");

Ccode appendRecord_firmware_version(TelemetryRecordArray& devDiagTelemetryArray)
{
    std::array<char, NvMctpVersionLength> fw_version{};
    uint16_t                              major = 0;
    uint8_t                               minor = 0;
    uint16_t                              patch = 0;
    uint16_t                              build = 0;

    pldm::pldm_get_active_version(major, minor, patch, build);
    Nsm::generate_fw_version(fw_version, major, minor, patch, build);

    if (!devDiagTelemetryArray.addRecordVariableArray(
            DIAG_FIRMWARE_VERSION, sizeof(fw_version), fw_version)) {
        return Ccode::ErrorInvalidLength;
    }
    return Ccode::Success;
}

Ccode appendRecord_build_type(TelemetryRecordArray& devDiagTelemetryArray)
{
    uint8_t build_type = 0;
    (void)Nsm::fill_build_type(build_type);
    if (false == devDiagTelemetryArray.addRecordNvU8(DIAG_BUILD_INFORMATION, build_type)) {
        return Ccode::ErrorInvalidLength;
    }
    return Ccode::Success;
}

Ccode appendRecord_temperature_telemetry(uint8_t               telemetry_tagid,
                                         uint8_t               sensor_tagid,
                                         TelemetryRecordArray& devDiagTelemetryArray)
{
    const int32_t temperature = nsm_type3::getTemperatureTelemetry(sensor_tagid);
    const bool    valid       = (temperature != nsm_type3::TemperatureInvalid) ? true : false;
    if (false == devDiagTelemetryArray.addRecordNvU32(telemetry_tagid, temperature, valid)) {
        return Ccode::ErrorInvalidLength;
    }
    return Ccode::Success;
}

Ccode appendRecord_power_raw_telemetry(uint8_t               telemetry_tagid,
                                       uint8_t               sensor_tagid,
                                       TelemetryRecordArray& devDiagTelemetryArray)
{
    const uint32_t power = nsm_type3::getPowerTelemetry(sensor_tagid);
    const bool     valid = (power != TelemetryInvalid) ? true : false;
    if (false == devDiagTelemetryArray.addRecordNvU32(telemetry_tagid, power, valid)) {
        return Ccode::ErrorInvalidLength;
    }
    return Ccode::Success;
}

Ccode appendRecord_voltage_telemetry(uint8_t               telemetry_tagid,
                                     uint8_t               sensor_tagid,
                                     TelemetryRecordArray& devDiagTelemetryArray)
{
    const uint32_t voltage = nsm_type3::getVoltageTelemetry(sensor_tagid);
    const bool     valid   = (voltage != TelemetryInvalid) ? true : false;
    if (false == devDiagTelemetryArray.addRecordNvU32(telemetry_tagid, voltage, valid)) {
        return Ccode::ErrorInvalidLength;
    }
    return Ccode::Success;
}

Ccode appendRecord_error_counter_telemetry(uint8_t               telemetry_tagid,
                                           nv::perf_mon::OobBus  oobBus,
                                           TelemetryRecordArray& devDiagTelemetryArray,
                                           bool                  valid = true)

{
    T4ErrorCounterResponse response{};
    response.latached_error = nv::perf_mon::Driver::get_latached_error(oobBus);
    // get counters for all error types
    for (uint8_t error_type = 0; error_type < nv::perf_mon::error_type_num; ++error_type) {
        response.error_count.at(error_type) = nv::perf_mon::Driver::get_transaction_error(
            oobBus, error_type);
    }
    if (false
        == devDiagTelemetryArray.addRecordVariableArray(
            telemetry_tagid, sizeof(response), response, valid)) {
        return Ccode::ErrorInvalidLength;
    }

    return Ccode::Success;
}

uint8_t find_t4_tag_id(nv::perf_mon::OobBus oobBusIdReq)
{
    for (const auto& [oobBusId, t4_tag_id] : nv::mctp::t4TagsForOobBusMap) {
        if (oobBusIdReq == oobBusId) {
            return static_cast<uint8_t>(t4_tag_id);
        }
    }
    return static_cast<uint8_t>(perf_mon::OobBus::End);
}

Ccode appendRecord_all_error_counter_telemetries(TelemetryRecordArray& devDiagTelemetryArray)
{
    Ccode   rcCode           = Ccode::Success;
    uint8_t telemetry_tag_id = 0;

    for (auto oobBusId = static_cast<uint8_t>(nv::perf_mon::OobBus::Begin);
         oobBusId < static_cast<uint8_t>(perf_mon::OobBus::End);
         ++oobBusId) {
        auto oobBus = static_cast<nv::perf_mon::OobBus>(oobBusId);
        if (false == nv::perf_mon::Driver::is_oob_bus_valid(oobBus)) {
            continue;
        }
        telemetry_tag_id = find_t4_tag_id(oobBus);
        bool valid       = true;
        if (telemetry_tag_id == static_cast<uint8_t>(perf_mon::OobBus::End)) {
            nv::error("The Error Counter id '%d' is not defined in the T4 Tags table\n",
                      oobBusId);
            valid = false;
        }
        if ((rcCode = appendRecord_error_counter_telemetry(
                 telemetry_tag_id, oobBus, devDiagTelemetryArray, valid))
            != Ccode::Success) {
            return rcCode;
        }
    }

    return Ccode::Success;
}

Ccode appendRecord_gpio_telemetry(uint8_t               telemetry_tagid,
                                  TelemetryRecordArray& devDiagTelemetryArray)
{
    if (nv::ipc::GpioNum == 0) {
        const uint8_t empty = 0;
        if (true == devDiagTelemetryArray.addRecordNvU8(telemetry_tagid, empty)) {
            return Ccode::Success;
        }
        return Ccode::ErrorInvalidData;
    }

    // format output
    Type4GpioResp t4_resp{};
    t4_resp.gpioNum = nv::ipc::GpioNum;

    // Read GPIOs directly starting from offset
    for (uint16_t gpio_index = 0; gpio_index < nv::ipc::GpioNum; gpio_index++) {
        // Get GPIO configuration for this index
        const auto&              gpio_config = nv::ipc::GpioSetup.at(gpio_index);
        const nv::gpio::GpioPort port        = std::get<0>(gpio_config);
        const nv::gpio::GpioPin  pin         = std::get<1>(gpio_config);

        // Read GPIO value
        uint8_t pin_value = 0;
        if (nv::gpio::Driver::read(port, pin, pin_value) == nv::gpio::Status::Ok) {
            // Calculate position in response array
            const uint16_t byte_index = gpio_index / 8;
            const uint16_t bit_pos    = gpio_index % 8;

            // Set corresponding bit if GPIO is high
            if (pin_value) {
                t4_resp.gpio.at(byte_index) |= (1U << bit_pos);
            }
        }
    }

    if (false
        == devDiagTelemetryArray.addRecordVariableArray(
            telemetry_tagid, sizeof(Type4GpioResp), t4_resp)) {
        return Ccode::ErrorInvalidLength;
    }

    return Ccode::Success;
}

Ccode appendRecord_cpu_utilization(TelemetryRecordArray& devDiagTelemetryArray)
{
    nv::perf_mon::CpuUtilization utilization{};
    nv::perf_mon::Driver::get_cpu_utilization(utilization);
    /*
       perhaps not all the entries are required
       T4CpuUtilizationEntryNum <=  nv::perf_mon::CpuUtilizationEntryNum
    */
    const uint16_t element_size    = sizeof(utilization) / nv::perf_mon::CpuUtilizationEntryNum;
    const uint16_t t4_entries_size = element_size * T4CpuUtilizationEntryNum;
    if (false
        == devDiagTelemetryArray.addRecordVariableArray(
            DIAG_CPU_UTILIZATION, t4_entries_size, utilization)) {
        return Ccode::ErrorInvalidLength;
    }
    return Ccode::Success;
}

Ccode appendRecord_task_priority(TelemetryRecordArray& devDiagTelemetryArray)
{
    nv::ipc::Task::TaskIdAndPriority task_priority{};
    nv::ipc::Task::get_task_id_and_priority(task_priority);
    T4TaskIdAndPriorityResponse response{};
    uint8_t                     task     = 0;
    uint8_t                     priority = 0;
    // TaskId::Timer and TaskId::Idle will NOT be copied into response
    size_t     size_to_copy = 0;
    const auto task_timer   = static_cast<uint8_t>(nv::ipc::TaskId::Timer);
    for (size_t counter = 0; counter < task_priority.size(); ++counter) {
        const auto& item = task_priority.at(counter);
        task             = static_cast<uint8_t>(std::get<0>(item));
        priority         = static_cast<uint8_t>(std::get<1>(item));
        if (task >= task_timer) {
            // stop at TaskId::Timer as there is no information,
            //  neither for TaskId::Idle
            break;
        }
        size_to_copy         += sizeof(task) + sizeof(priority);
        response.at(counter)  = std::make_pair(task, priority);
    }
    if (false
        == devDiagTelemetryArray.addRecordVariableArray(
            DIAG_TASK_PRIORITY, size_to_copy, response)) {
        return Ccode::ErrorInvalidLength;
    }
    return Ccode::Success;
}

/**
 * @brief Get Task Execution time values and inserts into Aggregate response array
 *        For each Task there will be a Aggregate element data
 *        It starts with tagId DIAG_TASK0_EXECUTION_TIME and then increment the TagId
 * @param devDiagTelemetryArray - The Aggregate response array
 * @return none
 */
Ccode appendRecord_task_execution_time(TelemetryRecordArray& devDiagTelemetryArray)
{
    TaskExecutionTimeResp           resp{};
    nv::perf_mon::TaskExecutionTime execution_time;
    /*
       perhaps not all entries are required
       T4TaskExecutionTimeEntryNum <= nv::perf_mon::TaskExecutionTimeEntryNum
    */
    const uint16_t element_size = sizeof(execution_time)
                                / nv::perf_mon::TaskExecutionTimeEntryNum;
    const uint16_t t4_resp_size = sizeof(resp.task_id)
                                + (element_size * T4TaskExecutionTimeEntryNum);

    uint8_t task_counter = 0;  // will be used to set the current tag
    // ipc::TaskId::Timer and ipc::TaskId::Idle will be the latest 2 TASK_X_EXECUTION_TIME
    for (auto itask = static_cast<uint32_t>(ipc::TaskId::Begin);
         itask < static_cast<uint32_t>(ipc::TaskId::KernelEnd);
         ++itask, ++task_counter) {
        execution_time.fill(0);
        nv::perf_mon::Driver::get_task_execution_time(static_cast<ipc::TaskId>(itask),
                                                      execution_time);
        resp.task_id        = static_cast<uint8_t>(itask);
        resp.execution_time = execution_time;
        if (false
            == devDiagTelemetryArray.addRecordVariableArray(
                DIAG_TASK0_EXECUTION_TIME + task_counter, t4_resp_size, resp)) {
            return Ccode::ErrorInvalidLength;
        }
    }
    return Ccode::Success;
}

Ccode appendRecord_flash_usage(TelemetryRecordArray& devDiagTelemetryArray)
{
    T4FlashUsageResp flash_resp{};
    flash_resp.total_slots = nv::perf_mon::Driver::get_fw_size_per_slot();
    flash_resp.usage       = nv::perf_mon::Driver::get_fw_size_used();

    if (false
        == devDiagTelemetryArray.addRecordVariableArray(
            DIAG_FLASH_USAGE_AND_SIZE, sizeof(T4FlashUsageResp), flash_resp)) {
        return Ccode::ErrorInvalidLength;
    }
    return Ccode::Success;
}

Ccode appendRecord_boot_time(TelemetryRecordArray& devDiagTelemetryArray)
{
    nv::flash::Data boot_time{};
    auto rcCode = nv::flash::Flash::get_data(nv::flash::Key::NpdsBootTimeFromFmcEndToMctpReady,
                                             boot_time);
    const bool valid = (rcCode == nv::flash::Status::Ok) ? true : false;
    if (false == devDiagTelemetryArray.addRecordNvU32(DIAG_BOOT_TIME, boot_time, valid)) {
        return Ccode::ErrorInvalidLength;
    }
    return Ccode::Success;
}

Ccode appendRecord_ram_usage(TelemetryRecordArray& devDiagTelemetryArray)
{
    T4RamSizeResp ram_resp{};
    ram_resp.usage = nv::perf_mon::Driver::get_ram_size_used();
    ram_resp.total = nv::perf_mon::Driver::get_ram_size_total();

    if (false
        == devDiagTelemetryArray.addRecordVariableArray(
            DIAG_RAM_USAGE_AND_SIZE, sizeof(T4RamSizeResp), ram_resp)) {
        return Ccode::ErrorInvalidLength;
    }
    return Ccode::Success;
}

Ccode appendRecord_task_sw_latency(TelemetryRecordArray& devDiagTelemetryArray)
{
    auto& systemObj       = nv::common::System::inst();
    auto  task_sw_latency = systemObj.task_switch_latency();
    if (false
        == devDiagTelemetryArray.addRecordNvU32(DIAG_TASK_SWITCH_LATENCY, task_sw_latency)) {
        return Ccode::ErrorInvalidLength;
    }
    return Ccode::Success;
}

/**
 * @brief Get all (all tagIds) T4 Device Diagnostics
 * @param devDiagTelemetryArray - The Aggregate output array
 * @return Ccode::Success or any other error
 */
Ccode getDeviceDiagnostics(TelemetryRecordArray& devDiagTelemetryArray)
{
    Ccode rcCode = Ccode::Success;
    if ((rcCode = appendRecord_firmware_version(devDiagTelemetryArray)) != Ccode::Success) {
        return rcCode;
    }

    if ((rcCode = appendRecord_build_type(devDiagTelemetryArray)) != Ccode::Success) {
        return rcCode;
    }

    if ((rcCode = appendRecord_flash_usage(devDiagTelemetryArray)) != Ccode::Success) {
        return rcCode;
    }

    if ((rcCode = appendRecord_ram_usage(devDiagTelemetryArray)) != Ccode::Success) {
        return rcCode;
    }

    if ((rcCode = appendRecord_boot_time(devDiagTelemetryArray)) != Ccode::Success) {
        return rcCode;
    }

    if ((rcCode = appendRecord_task_sw_latency(devDiagTelemetryArray)) != Ccode::Success) {
        return rcCode;
    }

    if ((rcCode = appendRecord_task_priority(devDiagTelemetryArray)) != Ccode::Success) {
        return rcCode;
    }

    if ((rcCode = appendRecord_cpu_utilization(devDiagTelemetryArray)) != Ccode::Success) {
        return rcCode;
    }

    if ((rcCode = appendRecord_task_execution_time(devDiagTelemetryArray)) != Ccode::Success) {
        return rcCode;
    }

    if ((rcCode = appendRecord_all_error_counter_telemetries(devDiagTelemetryArray))
        != Ccode::Success) {
        return rcCode;
    }

    for (const auto& [telemetry_tagid, telemetry_type, sensor_tagid] :
         mcuDiagnosticTelemetries) {
        switch (telemetry_type) {
            case VoltageTelemetry:
                rcCode = appendRecord_voltage_telemetry(
                    telemetry_tagid, sensor_tagid, devDiagTelemetryArray);
                break;
            case TemperatureTelemetry:
                rcCode = appendRecord_temperature_telemetry(
                    telemetry_tagid, sensor_tagid, devDiagTelemetryArray);
                break;
            case PowerTelemetry:
                rcCode = appendRecord_power_raw_telemetry(
                    telemetry_tagid, sensor_tagid, devDiagTelemetryArray);
                break;
            case GpioTelemetry:
                rcCode = appendRecord_gpio_telemetry(telemetry_tagid, devDiagTelemetryArray);
                break;
            case FirmwareInfoTelemetry: break;
            case PerformanceTelemetry : break;
            case ErrorCounterTelemetry: break;
            case TimestampTelemetry   : break;
        }
        if (rcCode != Ccode::Success) {
            return rcCode;
        }
    }

    // finally add Timestamp telemetry
    if (false == devDiagTelemetryArray.addTimestampRecord(DIAG_CurrentTimestamp)) {
        return Ccode::ErrorInvalidLength;
    }
    return Ccode::Success;
}

bool Nsm::process_diagnostics(const Packet& rx, Packet& tx)
{
    auto& nrx = NsmPktReq::from(rx);

    using cmd       = nv::mctp::NsmDevDiagCmdCode;
    auto& ntx       = NsmPktResp::from(tx);
    ntx.nv_msg_type = nrx.nv_msg_type;
    ntx.set_dev_diag_code(nrx.get_dev_diag_code());

    // Helper to handle unsupported commands
    [[maybe_unused]] auto unsupported_command = [&]() {
        fill_error_packet(Ccode::ErrorUnsupportedCmd, rx, tx);
        return false;
    };

    switch (nrx.get_dev_diag_code()) {
        case cmd::GetDeviceResetStatistics:
            if constexpr (is_cmd_set(NsmMsgType::Diagnostics, cmd::GetDeviceResetStatistics)) {
                on_dev_diag_get_reset_statistics(rx, tx);
                break;
            }
            return unsupported_command();

        case cmd::GetDeviceDiagnostics:
            if constexpr (is_cmd_set(NsmMsgType::Diagnostics, cmd::GetDeviceDiagnostics)) {
                on_dev_diag_get_diagnostics(rx, tx);
                break;
            }
            return unsupported_command();

        case cmd::BridgePortRecovery:
            if constexpr (is_cmd_set(NsmMsgType::Diagnostics, cmd::BridgePortRecovery)) {
                on_dev_diag_bridge_port_recovery(rx, tx);
                break;
            }
            return unsupported_command();

        case cmd::EnableDisableWriteProtection:
            if constexpr (is_cmd_set(NsmMsgType::Diagnostics,
                                     cmd::EnableDisableWriteProtection)) {
                on_dev_diag_enable_disable_write_protection(rx, tx);
                break;
            }
            return unsupported_command();

        case cmd::GetDeviceHscAlert:
            if constexpr (is_cmd_set(NsmMsgType::Diagnostics, cmd::GetDeviceHscAlert)) {
                on_dev_diag_get_device_hsc_alert(rx, tx);
                break;
            }
            return unsupported_command();

        case cmd::ClearHscFaults:
            if constexpr (is_cmd_set(NsmMsgType::Diagnostics, cmd::ClearHscFaults)) {
                on_dev_diag_clear_hsc_faults(rx, tx);
                break;
            }
            return unsupported_command();

        case cmd::GetCpldRegisterTable:
            if constexpr (is_cmd_set(NsmMsgType::Diagnostics, cmd::GetCpldRegisterTable)) {
                on_dev_diag_get_cpld_register_table(rx, tx);
                break;
            }
            return unsupported_command();

        default: return unsupported_command();
    }
    return true;
}

void Nsm::on_dev_diag_get_diagnostics(const Packet& rx, Packet& tx)
{
    constexpr uint8_t RequestSize = 1;
    if (!is_input_length_valid(rx, RequestSize)) {
        fill_error_packet(Ccode::ErrorInvalidLength, rx, tx);
        return;
    }
    fill_packet_header(rx, tx);
    fill_nsm_msg_header(rx, tx);
    // set tx variables
    auto& ntx             = NsmPktResp::from(tx);
    ntx.completion_code   = Ccode::Success;
    auto&         nrx     = NsmPktReq::from(rx);
    const uint8_t segment = nrx.data[0];

    // segment must be always be
    if (segment != DevDiagGetDiagnosticsFirstSegment) {
        fill_error_packet(Ccode::ErrorInvalidData, rx, tx);
        return;
    }

    // devDiagTelemetryArray will copy data into &ntx.data[1],
    // considering size = TelemetryRecordArray::MaxNsmBulkResponseSize including the Segment
    TelemetryRecordArray devDiagTelemetryArray(
        &ntx.data[1], TelemetryRecordArray::MaxNsmBulkResponseSize - 1);
    auto rcCode = getDeviceDiagnostics(devDiagTelemetryArray);

    if (rcCode != Ccode::Success) {
        fill_error_packet(rcCode, rx, tx);
        return;
    }
    const auto data_size   = devDiagTelemetryArray.arraySize() + 1;
    bool       errorLength = true;
    if (data_size <= std::numeric_limits<uint16_t>::max()) {
        ntx.data_size            = static_cast<uint16_t>(data_size);
        const auto packet_length = sizeof(Header) + HeaderResponseSize + data_size;
        if (packet_length <= std::numeric_limits<uint16_t>::max()) {
            tx.priv.packet_length = packet_length;
            errorLength           = false;
        }
    }
    if (true == errorLength) {
        fill_error_packet(Ccode::ErrorInvalidLength, rx, tx);
        return;
    }

    // set no more segments
    ntx.data[0] = DevDiagGetDiagnosticsNoMoreSegments;
}

void Nsm::on_dev_diag_get_reset_statistics(const Packet& rx, Packet& tx)
{
    fill_packet_header(rx, tx);
    fill_nsm_msg_header(rx, tx);
    // set tx variables
    auto& ntx           = NsmPktBulkResp::from(tx);
    ntx.completion_code = Ccode::Success;

    std::array<uint8_t, 32> reset_reason_and_wdt_info{};

    uint32_t offset = 0;

    nv::flash::Data _boot_reason{};
    if (nv::flash::Flash::get_data(nv::flash::Key::NpdsBootReasonOriginal, _boot_reason)
        != nv::flash::Status::Ok) {
        fill_error_packet(Ccode::ErrorGeneral, rx, tx);
        return;
    }

    TelemetryRecordArray reset_cause_aggregate(&ntx.data[0],
                                               TelemetryRecordArray::DefaultMctpDataSize);

    memcpy(reset_reason_and_wdt_info.data(), &_boot_reason, sizeof(_boot_reason));
    offset += sizeof(_boot_reason);

    nv::flash::Data app_fault_record_magic{};
    nv::flash::Data reset_event_bits{};
    nv::flash::Data reset_cfsr{};
    nv::flash::Data reset_hfsr{};
    if (nv::flash::Flash::get_data(nv::flash::Key::NpdsApplicationFaultMagic,
                                   app_fault_record_magic)
        != nv::flash::Status::Ok) {
        fill_error_packet(Ccode::ErrorGeneral, rx, tx);
        return;
    }

    if (app_fault_record_magic == sys::bootloader::Driver::ApplicationFaultMagic) {
        if (nv::flash::Flash::get_data(nv::flash::Key::NpdsCfsr, reset_cfsr)
            != nv::flash::Status::Ok) {
            fill_error_packet(Ccode::ErrorGeneral, rx, tx);
            return;
        }

        if (nv::flash::Flash::get_data(nv::flash::Key::NpdsHfsr, reset_hfsr)
            != nv::flash::Status::Ok) {
            fill_error_packet(Ccode::ErrorGeneral, rx, tx);
            return;
        }

        if (nv::flash::Flash::get_data(nv::flash::Key::NpdsWdtEventBits, reset_event_bits)
            != nv::flash::Status::Ok) {
            fill_error_packet(Ccode::ErrorGeneral, rx, tx);
            return;
        }
    }

    // Copy reset_cfsr, reset_hfsr, and reset_event_bits to the array
    memcpy(reset_reason_and_wdt_info.data() + offset, &reset_cfsr, sizeof(reset_cfsr));
    offset += sizeof(reset_cfsr);

    memcpy(reset_reason_and_wdt_info.data() + offset, &reset_hfsr, sizeof(reset_hfsr));
    offset += sizeof(reset_hfsr);

    uint32_t aggregate_task_event = 0;
    for (uint32_t i = 0; i < static_cast<uint32_t>(nv::watchdog::TaskMonitorIndex::End); i++) {
        if (reset_event_bits & nv::common::bit(i)) {
            switch (i) {
                case static_cast<uint32_t>(nv::watchdog::TaskMonitorIndex::Mctp):
                    aggregate_task_event |= static_cast<uint32_t>(AggregateTaskEvent::Mctp);
                    break;
                case static_cast<uint32_t>(nv::watchdog::TaskMonitorIndex::I2c0):
                case static_cast<uint32_t>(nv::watchdog::TaskMonitorIndex::I2c1):
                case static_cast<uint32_t>(nv::watchdog::TaskMonitorIndex::I2c2):
                case static_cast<uint32_t>(nv::watchdog::TaskMonitorIndex::I2c3):
                case static_cast<uint32_t>(nv::watchdog::TaskMonitorIndex::I2c4):
                    aggregate_task_event |= static_cast<uint32_t>(AggregateTaskEvent::I2C);
                    break;
                case static_cast<uint32_t>(nv::watchdog::TaskMonitorIndex::I3c0):
                case static_cast<uint32_t>(nv::watchdog::TaskMonitorIndex::I3c1):
                    aggregate_task_event |= static_cast<uint32_t>(AggregateTaskEvent::I3C);
                    break;
                case static_cast<uint32_t>(nv::watchdog::TaskMonitorIndex::Pldm):
                    aggregate_task_event |= static_cast<uint32_t>(AggregateTaskEvent::PLDM);
                    break;
                case static_cast<uint32_t>(nv::watchdog::TaskMonitorIndex::Usb):
                    aggregate_task_event |= static_cast<uint32_t>(AggregateTaskEvent::USB);
                    break;
                case static_cast<uint32_t>(nv::watchdog::TaskMonitorIndex::Flash):
                    aggregate_task_event |= static_cast<uint32_t>(AggregateTaskEvent::Flash);
                    break;
                case static_cast<uint32_t>(nv::watchdog::TaskMonitorIndex::Logger):
                    aggregate_task_event |= static_cast<uint32_t>(AggregateTaskEvent::Logger);
                    break;
                case static_cast<uint32_t>(nv::watchdog::TaskMonitorIndex::Spdm):
                    aggregate_task_event |= static_cast<uint32_t>(AggregateTaskEvent::SPDM);
                    break;
                // coverity[dead_error_begin] -- suspress warning for default case
                default: break;
            }
        }
    }

    memcpy(reset_reason_and_wdt_info.data() + offset,
           &aggregate_task_event,
           sizeof(aggregate_task_event));

    auto success = reset_cause_aggregate.addRecordVariableArray(
        DevDiagGetResetStatisticsResetCauseId,
        reset_reason_and_wdt_info.size(),
        reset_reason_and_wdt_info);

    if (!success) {
        fill_error_packet(Ccode::ErrorInvalidLength, rx, tx);
        return;
    }

    ntx.completion_code = Ccode::Success;
    ntx.telemetry_count = 1;
    // coverity[cert_int31_c_violation]
    tx.priv.packet_length = sizeof(Header) + AggregateHeaderResponseSize
                          + reset_cause_aggregate.arraySize();
}

void Nsm::on_dev_diag_bridge_port_recovery(const Packet& rx, Packet& tx)
{
    constexpr uint8_t RequestSize = sizeof(BridgePortRecoveryReq);
    if (!is_input_length_valid(rx, RequestSize)) {
        fill_error_packet(Ccode::ErrorInvalidLength, rx, tx);
        return;
    }

    fill_packet_header(rx, tx);
    fill_nsm_msg_header(rx, tx);

    auto& nrx = NsmPktReq::from(rx);
    auto& ntx = NsmPktResp::from(tx);

    // Parse request data
    BridgePortRecoveryReq req{};
    memcpy(&req, &nrx.data, sizeof(BridgePortRecoveryReq));

    if (req.recovery_level != static_cast<uint8_t>(RecoveryLevel::ProtocolReset)
        && req.recovery_level != static_cast<uint8_t>(RecoveryLevel::PortReset)) {
        // Only Protocol Reset (1), Port Reset (2), and Query Next Target (255) are supported
        fill_error_packet(Ccode::ErrorInvalidData, rx, tx);
        return;
    }

    // TODO: Add data validaty for reset_target

    // TODO: Implement actual bridge and port recovery logic here
    // This would typically involve:
    // 1. Processing the recovery level and reset target
    // 2. Performing the appropriate reset operation based on recovery level:
    //    - Protocol Reset (1): Reset protocol for target EID (reset_target contains EID)
    //    - Port Reset (2): Reset port number (bits [4:0] of reset_target contain port number)
    // 3. Tracking reset timing and next available targets
    // 4. Setting appropriate response values:
    //    - next_reset_target: Next available target or 0xFF if no more targets
    //    - time_since_last_reset: Time in msec since last reset (0=in progress,
    //    0xFFFF=saturated)

    // For now, we return a successful response with default values (reset target from req
    // payload) In a real implementation, you would:
    // - Execute the reset operation based on recovery level
    // - Track timing for each reset target independently
    // - Return the next available reset target
    // - Return actual time since last reset (or 0xFFFF if saturated)

    // Prepare response
    // Value will be set in conditional branches below
    // coverity[UNUSED_VALUE] - Variable initialized per C++ Core Guidelines
    nv::i2c::Task::Status recovery_status = nv::i2c::Task::Status::InvalidInterface;
    mctp::Client          client          = mctp::Client::End;
    nv::i2c::RecoveryCmd  recovery_cmd    = nv::i2c::RecoveryCmd::None;

    // Determine client and recovery command based on recovery level
    if (req.recovery_level == static_cast<uint8_t>(RecoveryLevel::ProtocolReset)) {
        // Protocol Reset: Find client by EID
        const uint8_t target_eid = req.reset_target;
        for (const auto& entry : _ctl.routing_map()) {
            if (entry.assigned_eid == target_eid) {
                client = static_cast<mctp::Client>(entry.client);
                break;
            }
        }
        recovery_cmd = nv::i2c::RecoveryCmd::QuickRecovery;
    }
    else if (req.recovery_level == static_cast<uint8_t>(RecoveryLevel::PortReset)) {
        // Port Reset: Find client by port number
        constexpr uint8_t NSM_PORT_NUMBER_MASK = 0x1F;  // bits [4:0]
        const uint8_t     port_number          = req.reset_target & NSM_PORT_NUMBER_MASK;
        for (const auto& entry : _ctl.routing_map()) {
            // Enum range is validated - casting is safe within mctp::Client bounds
            // coverity[cert_int31_c_violation] - Enum bounds checked explicitly
            if (static_cast<uint8_t>(entry.client) == port_number) {
                client = entry.client;
                break;
            }
        }
        recovery_cmd = nv::i2c::RecoveryCmd::BusRecovery;
    }
    else {
        // Invalid recovery level
        fill_error_packet(Ccode::ErrorInvalidData, rx, tx);
        return;
    }

    // Common validation: Check if client was found
    if (client == mctp::Client::End) {
        fill_error_packet(Ccode::ErrorInvalidData, rx, tx);
        return;
    }

    // Common execution: Execute recovery based on interface type
    if (client == mctp::Client::DsI2c0 || client == mctp::Client::DsI2c1
        || client == mctp::Client::DsI2c2 || client == mctp::Client::DsI2c3) {
        // I2C Recovery (both Protocol and Port Reset)
        recovery_status = nv::i2c::Task::send_recovery_request(recovery_cmd, client);
    }
    else {
        // Unsupported interface type
        recovery_status = nv::i2c::Task::Status::InvalidInterface;
    }

    // Check if recovery operation was successful
    if (recovery_status != nv::i2c::Task::Status::Ok) {
        fill_error_packet(Ccode::ErrorGeneral, rx, tx);
        return;
    }

    // Prepare response with hardcoded values as per NSM specification
    BridgePortRecoveryResp resp{};
    resp.next_reset_target = NoMoreResetTargets;  // No more targets (single device
                                                  // environment)
    resp.reserved              = ReservedField;   // Reserved field
    resp.time_since_last_reset = ResetTime1Ms;    // 1 msec (hardcoded for L1/L2 resets)

    // Copy response data to output buffer
    memcpy(&ntx.data, &resp, sizeof(BridgePortRecoveryResp));

    ntx.completion_code   = Ccode::Success;
    ntx.data_size         = sizeof(BridgePortRecoveryResp);
    tx.priv.packet_length = sizeof(Header) + HeaderResponseSize
                          + sizeof(BridgePortRecoveryResp);
}

__attribute__((weak)) NsmStatus platform_write_protection_gpio(uint8_t function, uint8_t mode)
{
    (void)function;
    (void)mode;
    return NsmStatus::NotSupported;
}

void Nsm::on_dev_diag_enable_disable_write_protection(const Packet& rx, Packet& tx)
{
    fill_packet_header(rx, tx);
    fill_nsm_msg_header(rx, tx);

    if (!is_input_length_valid(rx, sizeof(T4WriteProtectionRequest))) {
        fill_error_packet(Ccode::ErrorInvalidLength, rx, tx);
        return;
    }

    auto& nrx = NsmPktReq::from(rx);
    if (nrx.ocp_version != 1) {
        fill_error_packet(Ccode::ErrorInvalidData, rx, tx);
        return;
    }

    T4WriteProtectionRequest request{};

    // Check request data
    std::memcpy(&request.function, &nrx.data[0], sizeof(T4WriteProtectionRequest));

    // Validate mode
    if (request.mode != T4WriteProtectionMode::Clear
        && request.mode != T4WriteProtectionMode::Set) {
        fill_error_packet(Ccode::ErrorInvalidData, rx, tx);
        return;
    }

    auto wp_status = platform_write_protection_gpio(request.function, request.mode);
    if (wp_status == NsmStatus::InvalidData) {
        fill_error_packet(Ccode::ErrorInvalidData, rx, tx);
        return;
    }
    else if (wp_status != NsmStatus::OK) {
        fill_error_packet(Ccode::ErrorGeneral, rx, tx);
        return;
    }
    else if (wp_status == NsmStatus::NotSupported) {
        fill_error_packet(Ccode::ErrorUnsupportedCmd, rx, tx);
        return;
    }

    auto& ntx             = NsmPktResp::from(tx);
    ntx.data_size         = 0;
    ntx.completion_code   = Ccode::Success;
    tx.priv.packet_length = sizeof(Header) + HeaderResponseSize;
}

void Nsm::on_dev_diag_get_device_hsc_alert(const Packet& rx, Packet& tx)
{
    constexpr uint8_t RequestSize = 1;
    if (!is_input_length_valid(rx, RequestSize)) {
        fill_error_packet(Ccode::ErrorInvalidLength, rx, tx);
        return;
    }
    fill_packet_header(rx, tx);
    fill_nsm_msg_header(rx, tx);
    auto&         nrx    = NsmPktReq::from(rx);
    auto&         ntx    = NsmPktResp::from(tx);
    const uint8_t device = nrx.data[0];
    uint16_t      faults = 0;
    // coverity[assigned_value]
    nv::i2c::I2cStatus status = nv::i2c::I2cStatus::Error;
    switch (device) {
        case static_cast<uint8_t>(PowerSensorFaults::HSC):
        case static_cast<uint8_t>(PowerSensorFaults::HSC_2):
        case static_cast<uint8_t>(PowerSensorFaults::HSC_3):
        case static_cast<uint8_t>(PowerSensorFaults::HSC_4):
        case static_cast<uint8_t>(PowerSensorFaults::HSC_Sby): {
            const int8_t device_index = get_power_sensor_mgr().find_hsc_index_by_alert_sensor(
                static_cast<PowerSensorFaults>(device));
            if (device_index >= 0) {
                status = get_power_sensor_mgr().hsc_read_faults(device_index, faults);
                if (status != nv::i2c::I2cStatus::Ok) {
                    fill_error_packet(Ccode::ErrorI2CError, rx, tx);
                    return;
                }
            }
            else {
                fill_error_packet(Ccode::ErrorGeneral, rx, tx);
                return;
            }
            break;
        }
        case static_cast<uint8_t>(PowerSensorFaults::HSCC):
        case static_cast<uint8_t>(PowerSensorFaults::HSCC_2): {
            const int8_t device_index = get_power_sensor_mgr().find_hscc_index_by_alert_sensor(
                static_cast<PowerSensorFaults>(device));
            if (device_index >= 0) {
                status = get_power_sensor_mgr().hscc_read_faults(device_index, faults);
                if (status != nv::i2c::I2cStatus::Ok) {
                    fill_error_packet(Ccode::ErrorI2CError, rx, tx);
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

    memcpy(&ntx.data[0], &faults, sizeof(faults));
    ntx.data_size         = sizeof(faults);
    ntx.completion_code   = Ccode::Success;
    tx.priv.packet_length = sizeof(Header) + HeaderResponseSize + sizeof(faults);
}

void Nsm::on_dev_diag_clear_hsc_faults(const Packet& rx, Packet& tx)
{
    constexpr uint8_t RequestSize = 1;
    if (!is_input_length_valid(rx, RequestSize)) {
        fill_error_packet(Ccode::ErrorInvalidLength, rx, tx);
        return;
    }
    fill_packet_header(rx, tx);
    fill_nsm_msg_header(rx, tx);
    auto&         nrx    = NsmPktReq::from(rx);
    auto&         ntx    = NsmPktResp::from(tx);
    const uint8_t device = nrx.data[0];
    // coverity[assigned_value]
    nv::i2c::I2cStatus status = nv::i2c::I2cStatus::Error;
    switch (device) {
        case static_cast<uint8_t>(PowerSensorFaults::HSC):
        case static_cast<uint8_t>(PowerSensorFaults::HSC_2):
        case static_cast<uint8_t>(PowerSensorFaults::HSC_3):
        case static_cast<uint8_t>(PowerSensorFaults::HSC_4):
        case static_cast<uint8_t>(PowerSensorFaults::HSC_Sby): {
            const int8_t device_index = get_power_sensor_mgr().find_hsc_index_by_alert_sensor(
                static_cast<PowerSensorFaults>(device));
            if (device_index >= 0) {
                status = get_power_sensor_mgr().hsc_clear_faults(device_index);
                if (status != nv::i2c::I2cStatus::Ok) {
                    fill_error_packet(Ccode::ErrorI2CError, rx, tx);
                    return;
                }
            }
            else {
                fill_error_packet(Ccode::ErrorGeneral, rx, tx);
                return;
            }
            break;
        }
        case static_cast<uint8_t>(PowerSensorFaults::HSCC):
        case static_cast<uint8_t>(PowerSensorFaults::HSCC_2): {
            const int8_t device_index = get_power_sensor_mgr().find_hscc_index_by_alert_sensor(
                static_cast<PowerSensorFaults>(device));
            if (device_index >= 0) {
                status = get_power_sensor_mgr().hscc_clear_faults(device_index);
                if (status != nv::i2c::I2cStatus::Ok) {
                    fill_error_packet(Ccode::ErrorI2CError, rx, tx);
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
    tx.priv.packet_length = sizeof(Header) + HeaderResponseSize;
}

void Nsm::on_dev_diag_get_cpld_register_table(const Packet& rx, Packet& tx)
{
    constexpr uint8_t RequestSize = sizeof(CpldRegisterTableReq);
    if (!is_input_length_valid(rx, RequestSize)) {
        fill_error_packet(Ccode::ErrorInvalidLength, rx, tx);
        return;
    }

    if constexpr (!nv::i2c::LatticeCpld::is_enabled()) {
        fill_error_packet(Ccode::ErrorUnsupportedCmd, rx, tx);
        return;
    }

    fill_packet_header(rx, tx);
    fill_nsm_msg_header(rx, tx);

    auto& nrx = NsmPktReq::from(rx);
    auto& ntx = NsmPktResp::from(tx);

    // Parse request data
    CpldRegisterTableReq req{};
    memcpy(&req, static_cast<const void*>(nrx.data), sizeof(CpldRegisterTableReq));

    // Validate segment index, MCU only supports one segment 0
    if (req.segment_index != CpldRegisterTableFirstSegment) {
        // Client should not send 0xFF in request
        fill_error_packet(Ccode::ErrorInvalidData, rx, tx);
        return;
    }

    std::array<uint8_t, Cpld_User_Reg::CPLD_USER_REG_SIZE> cpld_reg_buf = {};

    auto& cpld   = nv::i2c::LatticeCpld::inst();
    auto  status = cpld.dump_cpld_registers(cpld_reg_buf);

    if (status != nv::i2c::I2cStatus::Ok) {
        fill_error_packet(Ccode::ErrorGeneral, rx, tx);
        return;
    }

    // Prepare response
    CpldRegisterTableResp resp{};
    resp.next_segment = CpldRegisterTableNoMoreSegments;  // Default: no more segments
    std::memcpy(&resp.segment_data[0], cpld_reg_buf.data(), sizeof(resp.segment_data));

    if (sizeof(CpldRegisterTableResp) > CpldRegTablePayloadMaxSize) {
        fill_error_packet(Ccode::ErrorInvalidLength, rx, tx);
        return;
    }

    memcpy(static_cast<void*>(ntx.data), &resp, sizeof(CpldRegisterTableResp));

    const uint16_t total_data_size = sizeof(CpldRegisterTableResp);

    ntx.completion_code = Ccode::Success;
    ntx.data_size       = total_data_size;

    const auto packet_length = sizeof(Header) + HeaderResponseSize + total_data_size;
    if (packet_length <= std::numeric_limits<uint16_t>::max()) {
        tx.priv.packet_length = packet_length;
    }
    else {
        fill_error_packet(Ccode::ErrorInvalidLength, rx, tx);
        return;
    }
}

}  // namespace nv::mctp
