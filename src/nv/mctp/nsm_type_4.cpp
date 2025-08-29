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

#include "corepdk/platforms/mcxn236/pldm-fd/src/pldm_wrap.h"

#include "nv/common/system.h"
#include "nv/flash/flash.h"
#include "nv/gpio/common.h"
#include "nv/gpio/driver.h"
#include "nv/logger/log.h"
#include "nv/mctp/nsm.h"
#include "nv/bootloader.h"
#include "nv/watchdog/notify_interface.h"
namespace nv::mctp {

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

Ccode appendRecord_firmware_version(TelemetryRecordArray& devDiagTelemetryArray)
{
    std::array<char, NvMctpVersionLength> fw_version{};
    uint16_t                              major = 0;
    uint8_t                               minor = 0;
    uint16_t                              patch = 0;
    uint16_t                              build = 0;

    pldm::pldm_get_active_version(major, minor, patch, build);
    Nsm::generate_fw_version(fw_version, major, minor, patch, build);

    if (!devDiagTelemetryArray.addRecorVariableArray(
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
    int32_t temperature = 0;
    Ccode   rcCode      = Ccode::Success;
    rcCode              = nsm_type3::getTemperatureTelemetry(sensor_tagid, temperature);
    if (rcCode != Ccode::Success) {
        return rcCode;
    }
    if (false == devDiagTelemetryArray.addRecordNvU32(telemetry_tagid, temperature)) {
        return Ccode::ErrorInvalidLength;
    }
    return Ccode::Success;
}

Ccode appendRecord_power_raw_telemetry(uint8_t               telemetry_tagid,
                                       uint8_t               sensor_tagid,
                                       TelemetryRecordArray& devDiagTelemetryArray)
{
    uint32_t power  = 0;
    Ccode    rcCode = Ccode::Success;
    rcCode          = nsm_type3::getPowerDrawTelemetry(sensor_tagid, power);
    if (rcCode != Ccode::Success) {
        return rcCode;
    }
    if (false == devDiagTelemetryArray.addRecordNvU32(telemetry_tagid, power)) {
        return Ccode::ErrorInvalidLength;
    }
    return Ccode::Success;
}

Ccode appendRecord_error_counter_telemetry(uint8_t               telemetry_tagid,
                                           nv::perf_mon::OobBus  oobBus,
                                           TelemetryRecordArray& devDiagTelemetryArray)

{
    T4ErrorCounterResponse response{};
    response.latached_error = nv::perf_mon::Driver::get_latached_error(oobBus);
    // get counters for all error types
    for (uint8_t error_type = 0; error_type < nv::perf_mon::error_type_num; ++error_type) {
        response.error_count.at(error_type) = nv::perf_mon::Driver::get_transaction_error(
            oobBus, error_type);
    }
    if (false
        == devDiagTelemetryArray.addRecorVariableArray(
            telemetry_tagid, sizeof(response), response)) {
        return Ccode::ErrorInvalidLength;
    }

    return Ccode::Success;
}

Ccode appendRecord_all_error_counter_telemetries(TelemetryRecordArray& devDiagTelemetryArray)
{
    Ccode   rcCode               = Ccode::Success;
    uint8_t telemetry_tag_id     = 0;
    uint8_t next_boundary_tag_id = 0;

    for (auto oobBusId = static_cast<uint8_t>(nv::perf_mon::OobBus::Begin);
         oobBusId < static_cast<uint8_t>(perf_mon::OobBus::End);
         ++oobBusId) {
        auto oobBus = static_cast<nv::perf_mon::OobBus>(oobBusId);
        if (false == nv::perf_mon::Driver::is_oob_bus_valid(oobBus)) {
            continue;
        }
        /** Associate oobBus with T4 Get Device Diagnostics enum Tag
         *
            All the Error Counters main groups from the enum Type4McuDiagnosticEntries
            must be present in this switch case
        */
        switch (oobBus) {
            case perf_mon::OobBus::Spi0:
                telemetry_tag_id     = DIAG_SPI0_DOWNSTREAM_BUS_ERROR;
                next_boundary_tag_id = DIAG_ERROR_COUNTER_BOUNDARY;
                break;
            case perf_mon::OobBus::DsI2c0:
                telemetry_tag_id     = DIAG_I2C0_DOWNSTREAM_BUS_ERROR;
                next_boundary_tag_id = DIAG_SPI0_DOWNSTREAM_BUS_ERROR;
                break;
            case perf_mon::OobBus::UsI2c:
                telemetry_tag_id     = DIAG_I2C_UPSTREAM_ERROR;
                next_boundary_tag_id = DIAG_I2C0_DOWNSTREAM_BUS_ERROR;
                break;
            case perf_mon::OobBus::DsI3c0:
                telemetry_tag_id     = DIAG_I3C0_BUS_ERROR;
                next_boundary_tag_id = DIAG_I2C_UPSTREAM_ERROR;
                break;
            default: telemetry_tag_id++; break;
        }
        // In case nv::perf_mon::OobBus has been enlarged and T4 tags did not follow it
        if (telemetry_tag_id >= next_boundary_tag_id) {
            nv::error("The Error Counter tag id '%d' is beyond its boundary '%d'\n",
                      telemetry_tag_id,
                      next_boundary_tag_id);
            return Ccode::ErrorGeneral;
        }
        if ((rcCode = appendRecord_error_counter_telemetry(
                 telemetry_tag_id, oobBus, devDiagTelemetryArray))
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
        == devDiagTelemetryArray.addRecorVariableArray(
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
        == devDiagTelemetryArray.addRecorVariableArray(
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
        == devDiagTelemetryArray.addRecorVariableArray(
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
            == devDiagTelemetryArray.addRecorVariableArray(
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
        == devDiagTelemetryArray.addRecorVariableArray(
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
    if (rcCode != nv::flash::Status::Ok) {
        return Ccode::ErrorGeneral;
    }
    if (false == devDiagTelemetryArray.addRecordNvU32(DIAG_BOOT_TIME, boot_time)) {
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
        == devDiagTelemetryArray.addRecorVariableArray(
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
            case CacheTemperatureTelemetry:
                rcCode = appendRecord_temperature_telemetry(
                    telemetry_tagid, sensor_tagid, devDiagTelemetryArray);
                break;
            case CachePowerTelemetry:
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
    using cmd       = nv::mctp::NsmDevDiagCmdCode;
    auto& nrx       = NsmPktReq::from(rx);
    auto& ntx       = NsmPktResp::from(tx);
    ntx.nv_msg_type = nrx.nv_msg_type;
    ntx.set_dev_diag_code(nrx.get_dev_diag_code());
    switch (nrx.get_dev_diag_code()) {
        case cmd::GetDeviceResetStatistics: on_dev_diag_get_reset_statistics(rx, tx); break;
        case cmd::GetDeviceDiagnostics    : on_dev_diag_get_diagnostics(rx, tx); break;
        default                           : fill_error_packet(Ccode::ErrorUnsupportedCmd, rx, tx); return false;
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

    auto success = reset_cause_aggregate.addRecorVariableArray(
        DevDiagGetResetStatisticsResetCauseId,
        reset_reason_and_wdt_info.size(),
        reset_reason_and_wdt_info);

    if (!success) {
        fill_error_packet(Ccode::ErrorInvalidLength, rx, tx);
        return;
    }

    ntx.completion_code   = Ccode::Success;
    ntx.telemetry_count   = 1;
    tx.priv.packet_length = sizeof(Header) + AggregateHeaderResponseSize
                          + reset_cause_aggregate.arraySize();
}

}  // namespace nv::mctp
