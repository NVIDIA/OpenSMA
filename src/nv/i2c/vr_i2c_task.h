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

#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string_view>

#include NV_IPC_CONFIG_H

#include "nv/emulation/emulation_device.h"
#include "nv/i2c/common.h"
#include "nv/ipc/queue.h"
#include "nv/ipc/task.h"
#include "nv/ipchandler/enums.h"
#include "nv/watchdog/notify_interface.h"

namespace nv::i2c {

/** @brief Runtime pairing of an emulator instance with its 7-bit bus address. */
struct VrI2cDeviceEntry
{
    std::optional<std::reference_wrapper<nv::emulation::EmulationDevice>> device;
    uint8_t                                                               address = 0;
};

/** @brief Project hook to decide whether specific emulator should be enabled. */
bool is_emulation_device_enabled(VrI2cDeviceId device_id);

/**
 * @brief Virtual I2C task: serves emulated slaves on VrI2c0 / VrI2c1.
 *
 * Receives i2c::Task requests from the ipchandler queue and replies via LSTP or USB.
 */
class VrI2cTask : public nv::ipc::Task
{
public:
    struct Config
    {
        nv::ipc::TaskId          task_id;
        std::string_view         task_name;
        nv::ipc::QueueId         queue_id;
        nv::ipchandler::Id       ipchandler_id;
        nv::ipc::BootedEventBits boot_event;
    };

    VrI2cTask(Config                            config,
              std::span<const VrI2cDeviceEntry> devices,
              nv::i2c::Port                     port) noexcept;

    static void       make(Config config);
    static void       entrypoint(void* params);
    static void       wdt_notify(nv::watchdog::TaskMonitorIndex task_id);
    [[noreturn]] void start();

    void   i2c_write(uint8_t address, std::span<const uint8_t> i2c_buffer);
    size_t i2c_read(uint8_t                                          address,
                    std::span<uint8_t, sys::i2c::I2cSlaveBufferSize> i2c_buffer,
                    bool                                             new_transaction);

    bool has_device(uint8_t address) const;

    static void i2c_callback(uint8_t&           address,
                             bool               is_read,
                             std::span<uint8_t> i2c_buffer,
                             size_t&            i2c_size,
                             void*              this_task_instance,
                             bool               new_transaction);

private:
    void handle_queued_i2c_request(std::span<uint8_t> buffer);
    void handle_wdt_event();

    void process_i2c_request(uint8_t                  src_id,
                             uint8_t                  address,
                             uint16_t                 write_length,
                             uint16_t                 read_length,
                             nv::i2c::I2cFlags        flags,
                             std::span<const uint8_t> write_data);

    static bool
    is_i2c_address_probe(size_t write_length, size_t read_length, nv::i2c::I2cFlags flags);

    static uint16_t run_emulated_i2c_transfer(nv::emulation::EmulationDevice& device,
                                              std::span<const uint8_t>        write_data,
                                              std::span<uint8_t>              read_data);

    static void reply_i2c(uint8_t            src_id,
                          nv::ipchandler::Id bus_id,
                          uint16_t           read_len,
                          std::span<uint8_t> read_data,
                          nv::i2c::I2cStatus i2c_status);

    static constexpr size_t AddressSpace = 128U;

    template<nv::ipchandler::Id BusId>
    static void make_task_by_bus(Config config);

    using DeviceRef = std::reference_wrapper<nv::emulation::EmulationDevice>;

    nv::ipc::Queue&                                    _queue;
    nv::ipchandler::Id                                 _ipchandler_id;
    nv::ipc::BootedEventBits                           _boot_event;
    std::array<std::optional<DeviceRef>, AddressSpace> _device_map{};
    nv::i2c::I2cBuffer                                 _read_buffer{};
    sys::i2c::I2CSlaveDriver<VrI2cTask>                _driver;
};

}  // namespace nv::i2c
