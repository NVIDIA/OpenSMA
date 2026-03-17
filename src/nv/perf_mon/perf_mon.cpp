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

#include "nv/perf_mon/perf_mon.h"

#include <cstring>

#include "nv/common/utils.h"
#include "nv/ipc/supervisor.h"
#include "nv/logger/log.h"
#include "nv/mctp/interface.h"

using namespace nv;

extern "C" {
void task_switch_hook(void)
{
    auto& driver = perf_mon::Driver::inst();
    if (driver.mode == perf_mon::Mode::Disable) {
        return;
    }
    auto tick = ctimer::Driver::read_ticks();
    // coverity[check_return] - expected to return a valid value
    auto id     = ipc::Supervisor::inst().current_task_id();
    auto tmp_id = static_cast<uint32_t>(id);

    auto index = driver.cur_index;
    // coverity[cert_int30_c_violation] Suspress warning for performance concern
    driver.buffer.at(index).cpu.at(tmp_id).execution_time += (tick - driver.last_tick);
    driver.last_tick                                       = tick;
}
}

namespace nv::perf_mon {
NV_SHARED_BSS Driver driver;  // NOLINT(*-non-const-global-variables)

Driver& Driver::inst()
{
    return driver;
}

void Driver::init()
{
    using namespace std::chrono_literals;
    auto timer = ipc::Timer::make(ipc::TimerId::PerfMonitor, 1s, Driver::on_timer, true);
    timer.start();

    driver.mode = Mode::Auto;
}

void Driver::on_timer([[maybe_unused]] ipc::Timer& id)
{
    auto& driver = Driver::inst();
    if (driver.mode == Mode::Disable) {
        return;
    }

    auto  index = driver.cur_index;
    auto& mode  = driver.mode;

    if (mode == Mode::Auto) {
        index++;
        index            %= BufferSize;
        driver.cur_index  = index;

        // clear before start measure
        driver.buffer.at(index) = {};
    }
}

void Driver::log_pkt_meas(uint32_t type, uint32_t bytes, bool is_rx, bool is_drop)
{
    auto& driver = Driver::inst();
    if (driver.mode == Mode::Disable) {
        return;
    }

    auto  index = driver.cur_index;
    auto& iface = driver.buffer.at(index).interface.at(type);

    if (is_rx) {
        iface.receive_packets = nv::common::add(iface.receive_packets,
                                                static_cast<uint16_t>(1));
        iface.receive_bytes   = nv::common::add(iface.receive_bytes, bytes);
    }
    else {
        iface.transmit_packets = nv::common::add(iface.transmit_packets,
                                                 static_cast<uint16_t>(1));
        iface.transmit_bytes   = nv::common::add(iface.transmit_bytes,
                                               static_cast<uint32_t>(bytes));
    }

    if (is_drop) {
        iface.packet_dropped = nv::common::add(iface.packet_dropped, static_cast<uint16_t>(1));
    }

    iface.type = type;
}

void Driver::log_pkt_latency(LatencyEvent event, [[maybe_unused]] uint32_t interface)
{
    const uint32_t timestamp = nv::ctimer::Driver::read_ticks();
    auto&          driver    = Driver::inst();

    // Record timestamp
    driver.latency_measurement.timestamps.at(static_cast<uint8_t>(event)) = timestamp;

    // UsbCallDriverTx is the last one called log_pkt_latency
    if (event == LatencyEvent::UsbCallDriverTx) {
        // Record latency from USB ISR to task "type" driver write done
        driver.latency_measurement.timestamps.at(
            static_cast<uint8_t>(LatencyEvent::UsbRxIsrToI3cTx)) = sys::ctimer::Driver::
            get_counter_difference(driver.latency_measurement.timestamps.at(
                                       static_cast<uint8_t>(LatencyEvent::UsbRecvRxIsr)),
                                   driver.latency_measurement.timestamps.at(
                                       static_cast<uint8_t>(LatencyEvent::I3cTaskDriverTx)));
        driver.latency_measurement.timestamps.at(
            static_cast<uint8_t>(LatencyEvent::I3cRxIsrToUsbTx)) = sys::ctimer::Driver::
            get_counter_difference(driver.latency_measurement.timestamps.at(
                                       static_cast<uint8_t>(LatencyEvent::I3cTaskRecvRxIsr)),
                                   driver.latency_measurement.timestamps.at(
                                       static_cast<uint8_t>(LatencyEvent::UsbCallDriverTx)));
        if (EnableI3cDebugLog) {
            // For performance tuning if needed (print out all timestamp)
            if (interface == static_cast<uint32_t>(mctp::Client::DsI3c0)
                || interface == static_cast<uint32_t>(mctp::Client::DsI3c1)) {
                for (uint8_t i = 0; i < static_cast<uint8_t>(LatencyEvent::End); ++i) {
                    const uint32_t timestamp = driver.latency_measurement.timestamps.at(
                        static_cast<uint8_t>(i));
                    nv::logger::info(
                        nv::logger::Event::PerfI3c,
                        nv::logger::data_from_two_u32(static_cast<uint32_t>(i),
                                                      static_cast<uint32_t>(timestamp)));
                }
            }
        }
    }
}

void Driver::get_pkt_latency(uint32_t& usb_rx_to_i3c_tx_latency,
                             uint32_t& i3c_rx_to_usb_tx_latency)
{
    auto& driver = Driver::inst();

    // Return UsbRxIsrToI3cTx : from USB ISR to I3C task ISR
    usb_rx_to_i3c_tx_latency = driver.latency_measurement.timestamps.at(
        static_cast<uint8_t>(LatencyEvent::UsbRxIsrToI3cTx));

    // Return I3cRxIsrToUsbTx : from I3C task ISR to USB Tx
    i3c_rx_to_usb_tx_latency = driver.latency_measurement.timestamps.at(
        static_cast<uint8_t>(LatencyEvent::I3cRxIsrToUsbTx));
}

void Driver::dump()
{
    if (EnablePerfDump) {
        auto& driver = Driver::inst();
        nv::logger::info(nv::logger::Event::PerfTimestamp,
                         nv::logger::data_from_u32(static_cast<uint32_t>(xTaskGetTickCount())));
        for (uint32_t i = 0; i < (static_cast<uint32_t>(nv::ipc::TaskId::End) + 2); i++) {
            nv::logger::info(
                nv::logger::Event::PerfCpuMeasurement,
                nv::logger::data_from_two_u32(
                    static_cast<uint32_t>(i),
                    static_cast<uint32_t>(driver.buffer.at(0).cpu.at(i).execution_time)));
        }

        const size_t                   dump_num = 11;
        std::array<uint16_t, dump_num> dump_interface{};

        // NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers)
        dump_interface[0]  = 0;
        dump_interface[1]  = static_cast<uint16_t>(mctp::Client::DsI2c0);
        dump_interface[2]  = static_cast<uint16_t>(mctp::Client::DsI2c1);
        dump_interface[3]  = static_cast<uint16_t>(mctp::Client::DsI2c2);
        dump_interface[4]  = static_cast<uint16_t>(mctp::Client::DsI2c3);
        dump_interface[5]  = static_cast<uint16_t>(mctp::Client::DsI3c0);
        dump_interface[6]  = static_cast<uint16_t>(mctp::Client::DsI3c1);
        dump_interface[7]  = static_cast<uint16_t>(mctp::Client::DsI2c4);
        dump_interface[8]  = static_cast<uint16_t>(mctp::Client::DsI2c5);
        dump_interface[9]  = static_cast<uint16_t>(mctp::Client::DsI2c6);
        dump_interface[10] = static_cast<uint16_t>(mctp::Client::DsI2c7);
        // NOLINTEND(cppcoreguidelines-avoid-magic-numbers)

        for (auto client : dump_interface) {
            nv::logger::info(nv::logger::Event::PerfInterfaceType,
                             nv::logger::data_from_u32(static_cast<uint32_t>(client)));
            nv::logger::info(nv::logger::Event::PerfInterfaceSend,
                             nv::logger::data_from_two_u32(
                                 static_cast<uint32_t>(
                                     driver.buffer.at(0).interface.at(client).transmit_packets),
                                 static_cast<uint32_t>(
                                     driver.buffer.at(0).interface.at(client).transmit_bytes)));
            nv::logger::info(nv::logger::Event::PerfInterfaceRecv,
                             nv::logger::data_from_two_u32(
                                 static_cast<uint32_t>(
                                     driver.buffer.at(0).interface.at(client).receive_packets),
                                 static_cast<uint32_t>(
                                     driver.buffer.at(0).interface.at(client).receive_bytes)));
        }
    }
}

void Driver::mode_change(Mode new_mode)
{
    auto& driver = Driver::inst();
    driver.mode  = new_mode;
}

bool Driver::reset_perf_mon()
{
    auto& driver = Driver::inst();
    if (driver.mode != Mode::Disable) {
        return false;
    }

    memset(driver.buffer.data(), 0, sizeof(driver.buffer));
    driver.cur_index = 0;
    return true;
}

uint32_t Driver::get_index()
{
    auto& driver = Driver::inst();
    return driver.cur_index;
}

Mode Driver::get_current_mode()
{
    auto& driver = Driver::inst();
    return driver.mode;
}

/**
 * @brief Average CPU execution time (sum of all task execution time in us) per 1 second
 *        Record 20 entries of the most recent 20 seconds.
 * @note  from latest to oldest
 *
 * @param utilization
 * @return none
 */
void Driver::get_cpu_utilization(CpuUtilization& utilization)
{
    const auto& size   = Driver::get_buffer_size();
    const auto& index  = Driver::get_index() - 1;  // start with current_index-1
    const auto& buffer = Driver::inst().buffer;

    // coverity[cert_int30_c_violation] underflow of --isec is handled in the loop
    for (uint32_t isec = index, iutil = 0; iutil < CpuUtilizationEntryNum; --isec, ++iutil) {
        // handle underflow
        isec = (isec >= size) ? (size - 1) : isec;

        // accumulate execution time
        for (auto itask = static_cast<uint32_t>(ipc::TaskId::Begin);
             itask < static_cast<uint32_t>(ipc::TaskId::End);
             ++itask) {
            // coverity[cert_int30_c_violation] overflow takes ~4300 seconds
            utilization.at(iutil) += buffer.at(isec).cpu.at(itask).execution_time;
        }
    }
}

/**
 * @brief Average task execution time per 1 second in us
 *        Record 20 entries of the most recent 20 seconds
 *
 * @param itask
 * @param taskExeTime
 * @return none
 */
void Driver::get_task_execution_time(ipc::TaskId itask, TaskExecutionTime& taskExeTime)
{
    const auto& size   = Driver::get_buffer_size();
    const auto& index  = Driver::get_index() - 1;  // start with current_index-1
    const auto& buffer = Driver::inst().buffer;

    // coverity[cert_int30_c_violation] underflow of --isec is handled in the loop
    for (uint32_t isec = index, iexe = 0; iexe < TaskExecutionTimeEntryNum; --isec, ++iexe) {
        // handle underflow
        isec = (isec >= size) ? (size - 1) : isec;

        // fetch execution time
        taskExeTime.at(
            iexe) = buffer.at(isec).cpu.at(static_cast<uint32_t>(itask)).execution_time;
    }
}

/**
 * @brief Average task execution time per 1 second in us
 *        Record 20 entries of the most recent 20 seconds
 *
 * @param allTaskExeTime
 * @return none
 */
void Driver::get_all_task_execution_time(AllTaskExecutionTime& allTaskExeTime)
{
    const auto& size   = Driver::get_buffer_size();
    const auto& index  = Driver::get_index() - 1;  // start with current_index-1
    const auto& buffer = Driver::inst().buffer;

    for (auto itask = static_cast<uint32_t>(ipc::TaskId::Begin);
         itask < static_cast<uint32_t>(ipc::TaskId::End);
         ++itask) {
        for (uint32_t isec = index, iexe = 0; iexe < TaskExecutionTimeEntryNum;
             // coverity[cert_int30_c_violation] underflow of --isec is handled in the loop
             --isec,
                      ++iexe) {
            // handle underflow
            isec = (isec >= size) ? (size - 1) : isec;

            // fetch execution time
            allTaskExeTime.at(itask).at(iexe) = buffer.at(isec).cpu.at(itask).execution_time;
        }
    }
}

}  // namespace nv::perf_mon
