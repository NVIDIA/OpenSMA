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

#include "nv/ctimer/ctimer.h"
#include "nv/ipc/task.h"
#include "nv/ipc/timer.h"
#include NV_IPC_CONFIG_H

extern char __text_size;
extern char __data_size;
extern char __max_fw_size;
extern char __max_ram_size;

namespace nv::perf_mon {

constexpr uint32_t CpuUtilizationEntryNum = 20;
using CpuUtilization                      = std::array<uint32_t, CpuUtilizationEntryNum>;

constexpr uint32_t TaskExecutionTimeEntryNum = 20;
constexpr uint32_t TaskNum                   = static_cast<uint32_t>(ipc::TaskId::KernelEnd);
using TaskExecutionTime                      = std::array<uint32_t, TaskExecutionTimeEntryNum>;
using AllTaskExecutionTime                   = std::array<TaskExecutionTime, TaskNum>;

enum class OobBus
{
    Begin = 0,
    Port0 = Begin,
    Port1,
    Port2,
    Port3,
    Port4,
    Port5,
    Port6,
    Port7,
    Port8,
    Port9,
    I3cPort0,
    I3cPort1,
    End
};

/** Used to make it clear which type is being used on a such port number */
enum class OobBusType : uint8_t
{
    I2c = 0,  // it will be the default in case not set
    I3c = 1,
    Spi = 2
};

constexpr uint8_t error_type_num = 16;
using OobBusErrorCount           = std::array<uint32_t, error_type_num>;
constexpr uint32_t OobBusTypeNum = static_cast<uint32_t>(OobBus::End);

#if 0

constexpr uint32_t OobBusTypeNum       = static_cast<uint32_t>(OobBus::End);
using OobBusErrorCount                 = std::array<uint32_t, error_type_num>;
using OobBusError                      = std::array<OobBusErrorCount, OobBusTypeNum>;
using OobBusErrorLatched               = std::array<uint8_t, OobBusTypeNum>;

constexpr uint32_t OobBusErrorEntryNum = 20;
using OobBusErrorBuf                   = std::array<OobBusError, OobBusErrorEntryNum>;
using OobBusErrorRecent                = std::array<OobBusErrorCount, OobBusTypeNum>;
#endif

enum class Mode
{
    Begin = 0,
    Auto  = Begin,
    Accumulate,
    Disable,
    End
};

class Driver
{
    constexpr static uint32_t BufferSize   = 30;
    constexpr static uint32_t TaskNum      = static_cast<uint32_t>(ipc::TaskId::KernelEnd);
    constexpr static uint32_t InterfaceNum = 20;
    constexpr static uint32_t ErrorNum     = 3;
    // Enable I3C Perf Debug Log
    constexpr static bool EnableI3cDebugLog = false;
    constexpr static bool EnablePerfDump    = false;
    struct InterfaceMeasurement
    {
        uint32_t type;  // USB/I2C/I3C/MCU
        uint32_t receive_bytes;
        uint32_t transmit_bytes;
        uint16_t receive_packets;
        uint16_t transmit_packets;
        uint16_t packet_dropped;
        uint16_t retransmittion;
        uint32_t idle_time;        // unit: us
        uint16_t total_latency;    // average latency, us
        uint16_t error[ErrorNum];  // interface self define error
    };

    struct CpuMeasurement
    {
        uint8_t  task_id;
        uint32_t execution_time;  // unit: us
    };

public:
    using InterfaceBuffer = std::array<InterfaceMeasurement, InterfaceNum>;
    using CpuBuffer       = std::array<CpuMeasurement, TaskNum>;

    struct Measurement
    {
        InterfaceBuffer interface;
        CpuBuffer       cpu;
    };

    using MeasurementBuffer = std::array<Measurement, BufferSize>;

    // Refer to
    // https://confluence.nvidia.com/display/GFWBC/MCU+SMA+Performance+Measurement
    // Packet latency
    enum class LatencyEvent : uint8_t
    {
        Begin        = 0,
        UsbRecvRxIsr = Begin,
        UsbHandleRxIsr,
        UsbSendTxPktToTask,
        UsbEnableRx,  // stay idle
        I3cTaskRecvTxPkt,
        I3cTaskHandleTx,
        I3cTaskHandleTxDone,
        I3cTaskDriverTx,
        I3cTaskDriverTxDone,
        I3cTaskRecvRxIsr,
        I3cTaskReadRxPkt,
        I3cTaskSendToUsb,
        UsbHandleRxPkt,
        UsbCallDriverTx,
        UsbRxIsrToI3cTx,
        I3cRxIsrToUsbTx,
        End
    };
    struct LatencyMeasurement
    {
        std::array<uint32_t, static_cast<size_t>(LatencyEvent::End)> timestamps;
    };

    /// Access singleton.
    static Driver& inst();
    static void    dump();
    static void    on_timer([[maybe_unused]] ipc::Timer& id);
    static void    init();
    static void    log_pkt_meas(uint32_t type, uint32_t bytes, bool is_rx, bool is_drop);
    // argument interface is for logging only (for performace tuning)
    static void log_pkt_latency(LatencyEvent event, uint32_t interface = 0);
    static void get_pkt_latency(uint32_t& usb_rx_to_i3c_tx_latency,
                                uint32_t& i3c_rx_to_usb_tx_latency);

    // Only enable reset when disable mode
    static bool reset_perf_mon();

    static void mode_change(Mode new_mode);

    static Mode get_current_mode();

    static uint32_t get_index();

    static uint32_t get_buffer_size() { return BufferSize; }
    static uint32_t get_total_size() { return sizeof(buffer); }
    static uint32_t get_measurement_size() { return sizeof(Measurement); }

    static void get_cpu_utilization(CpuUtilization& utilization);
    static void get_task_execution_time(ipc::TaskId itask, TaskExecutionTime& taskExeTime);
    static void get_all_task_execution_time(AllTaskExecutionTime& allTaskExeTime);

    // 2 fw slots, each slot has 512KB, but only 384KB is for fw
    static uint32_t get_fw_size_per_slot()
    {
        // coverity[cert_int31_c_violation] safe to cast
        return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(&__max_fw_size));
    }

    // fw size used is calculated by linker script
    static uint32_t get_fw_size_used()
    {
        // coverity[cert_int31_c_violation] safe to cast
        return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(&__text_size));
    }

    // ram total size is 256KB
    static uint32_t get_ram_size_total()
    {
        // coverity[cert_int31_c_violation] safe to cast
        return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(&__max_ram_size));
    }

    // ram size used is calculated by linker script
    static uint32_t get_ram_size_used()
    {
        // coverity[cert_int31_c_violation] safe to cast
        return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(&__data_size));
    }

    MeasurementBuffer  buffer{};
    LatencyMeasurement latency_measurement{};
    uint32_t           cur_index{};
    ctimer::NV_Ticks   last_tick{};

    Mode mode{};

    // OOB Bus error for NSM
    static void set_transaction_error(OobBus bus, uint8_t error_type);

    static uint32_t get_transaction_error(OobBus bus, uint8_t error_type);

    static uint8_t get_latached_error(OobBus bus);

    static void reset_transaction_error(OobBus bus);

    std::array<OobBusErrorCount, OobBusTypeNum> oob_bus_error_buf{};
    std::array<uint8_t, OobBusTypeNum>          oob_bus_error_latched{};
    std::array<bool, OobBusTypeNum>             oob_bus_valid{};
    std::array<OobBusType, OobBusTypeNum>       oob_bus_type{};

    static void reset_transaction_error_all();

    static void oob_bus_on_timer();

    static bool is_oob_bus_valid(OobBus bus);

    static void set_oob_bus_valid(OobBus bus);

    static OobBus flexcomm_port_to_oobBus(uint8_t port_number);

    static void set_oob_bus_type(OobBus bus, OobBusType type);

    static OobBusType get_oob_bus_type(OobBus bus);

    constexpr static uint32_t InvalidErrorCount = 0xFFFFFFFF;

    constexpr static uint8_t InvalidErrorType = 0xFF;

protected:
};

}  // namespace nv::perf_mon
