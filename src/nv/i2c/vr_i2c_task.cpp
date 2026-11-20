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

#include "nv/i2c/vr_i2c_task.h"

#include <algorithm>
#include <bit>
#include <chrono>
#include <utility>
#include <variant>

#include "nv/bootloader.h"
#include "nv/common/preproc.h"
#include "nv/emulation/at24c01.h"
#include "nv/emulation/i2c-rtc.h"
#include "nv/emulation/pca9544a.h"
#include "nv/emulation/pca9555.h"
#include "nv/i2c/task.h"
#include "nv/i2c/vr_i2c_bus_slot.h"
#include "nv/lstp/lstp_router.h"
#include "nv/logger/log.h"
#include "nv/nv.h"
#include "nv/usb/task.h"
#include "nv/watchdog/runtime.h"
#include "sys/i2c/i2c_slave.h"
#include "sys/ipc/task.h"
#include "sys/rtc/rtc.h"

#include NV_IPC_CONFIG_H

namespace nv::i2c {

__attribute__((weak)) bool is_emulation_device_enabled([[maybe_unused]] VrI2cDeviceId device_id)
{
    return true;
}

namespace {

using VrI2cEmulator = std::
    variant<emulation::At24c01, emulation::Pca9555, emulation::Pca9544a, emulation::I2cRtc>;

/** @brief Emulator instances and address map entries built from config slots. */
template<size_t SlotCount>
struct VrI2cSlotDevices
{
    std::array<VrI2cEmulator, SlotCount>    instances{};
    std::array<VrI2cDeviceEntry, SlotCount> entries;

    explicit VrI2cSlotDevices(const std::array<VrI2cBusSlot, SlotCount>& slots)
    : entries(make_entries(instances, slots))
    {}

private:
    /** @brief Emplace @p device_id into @p emulator; initializes RTC HAL once for I2C RTC.
     */
    static std::optional<std::reference_wrapper<emulation::EmulationDevice>>
    make_emulator(VrI2cEmulator& emulator, VrI2cDeviceId device_id)
    {
        static bool rtc_initialized = false;

        if (!is_emulation_device_enabled(device_id)) {
            return std::nullopt;
        }

        switch (device_id) {
            case VrI2cDeviceId::At24c01:
                emulator.emplace<emulation::At24c01>();
                return std::get<emulation::At24c01>(emulator);
            case VrI2cDeviceId::Pca9555:
                emulator.emplace<emulation::Pca9555>();
                return std::get<emulation::Pca9555>(emulator);
            case VrI2cDeviceId::Pca9544a:
                emulator.emplace<emulation::Pca9544a>();
                return std::get<emulation::Pca9544a>(emulator);
            case VrI2cDeviceId::I2cRtc:
                if (!rtc_initialized) {
                    sys::rtc::Driver::init();
                    rtc_initialized = true;
                }
                emulator.emplace<emulation::I2cRtc>();
                return std::get<emulation::I2cRtc>(emulator);
        }
        NV_ASSERT(false);
        emulator.emplace<emulation::At24c01>();
        return std::get<emulation::At24c01>(emulator);
    }

    template<size_t... I>
    static std::array<VrI2cDeviceEntry, SlotCount>
    make_entries_impl(std::array<VrI2cEmulator, SlotCount>&      instances,
                      const std::array<VrI2cBusSlot, SlotCount>& slots,
                      [[maybe_unused]] std::index_sequence<I...> indices)
    {
        return {{VrI2cDeviceEntry{make_emulator(instances.at(I), slots.at(I).device_id),
                                  slots.at(I).address}...}};
    }

    static std::array<VrI2cDeviceEntry, SlotCount>
    make_entries(std::array<VrI2cEmulator, SlotCount>&      instances,
                 const std::array<VrI2cBusSlot, SlotCount>& slots)
    {
        return make_entries_impl(instances, slots, std::make_index_sequence<SlotCount>{});
    }
};

/** @brief Select VrI2c0 or VrI2c1 slot table from project config. */
template<ipchandler::Id BusId>
struct VrI2cBusConfig
{
    static_assert(BusId == ipchandler::Id::VrI2c0 || BusId == ipchandler::Id::VrI2c1);

    static constexpr size_t DeviceNum = [] {
        if constexpr (BusId == ipchandler::Id::VrI2c0) {
            return ipc::VrI2c0DeviceNum;
        }
        else {
            return ipc::VrI2c1DeviceNum;
        }
    }();

    static constexpr nv::i2c::Port SlavePort = [] {
        if constexpr (BusId == ipchandler::Id::VrI2c0) {
            return ipc::VrI2c0SlavePort;
        }
        else {
            return ipc::VrI2c1SlavePort;
        }
    }();

    static constexpr const std::array<VrI2cBusSlot, DeviceNum>& slots()
    {
        if constexpr (BusId == ipchandler::Id::VrI2c0) {
            return ipc::VrI2c0DeviceSlots;
        }
        else {
            return ipc::VrI2c1DeviceSlots;
        }
    }
};

/** Emulators + task in one NV_TASK_DATA object so MPU private region covers both. */
template<size_t SlotCount>
struct VrI2cTaskData
{
    VrI2cSlotDevices<SlotCount> devices;
    VrI2cTask                   task;

    VrI2cTaskData(VrI2cTask::Config                          cfg,
                  const std::array<VrI2cBusSlot, SlotCount>& slots,
                  const nv::i2c::Port                        port)
    : devices(slots)
    , task(cfg, devices.entries, port)
    {}
};

}  // namespace

VrI2cTask::VrI2cTask(Config                            config,
                     std::span<const VrI2cDeviceEntry> devices,
                     nv::i2c::Port                     port) noexcept
: nv::ipc::Task(config.task_id, config.task_name)
, _queue(nv::ipc::Queue::make(config.queue_id))
, _ipchandler_id(config.ipchandler_id)
, _boot_event(config.boot_event)
{
    for (const auto& entry : devices) {
        if (entry.device.has_value() && entry.address < AddressSpace) {
            _device_map.at(entry.address) = entry.device;
        }
    }

    if (port != nv::i2c::Port::End) {
        std::array<uint8_t, sys::i2c::NumI2cTargetAddresses> target_addresses{};
        size_t                                               target_count = 0U;
        for (const auto& entry : devices) {
            if (entry.device.has_value()) {
                target_addresses.at(target_count++) = entry.address;
            }
        }

        if (target_count > 0U) {
            _driver.bind(port, target_addresses, this);
            _driver.start();
        }
    }
}

void VrI2cTask::i2c_write(uint8_t address, std::span<const uint8_t> i2c_buffer)
{
    if (address >= AddressSpace || i2c_buffer.empty()) {
        return;
    }

    const auto& device_ref = _device_map.at(address);
    if (device_ref.has_value()) {
        device_ref->get().i2c_write(i2c_buffer);
    }
}

size_t VrI2cTask::i2c_read(uint8_t                                          address,
                           std::span<uint8_t, sys::i2c::I2cSlaveBufferSize> i2c_buffer,
                           bool                                             new_transaction)
{
    if (address >= AddressSpace) {
        return 0U;
    }

    const auto& device_ref = _device_map.at(address);
    if (!device_ref.has_value()) {
        return 0U;
    }

    size_t i2c_size = 0U;
    device_ref->get().i2c_read(i2c_buffer, i2c_size, new_transaction);
    return i2c_size;
}

bool VrI2cTask::has_device(uint8_t address) const
{
    return address < AddressSpace && _device_map.at(address).has_value();
}

void VrI2cTask::i2c_callback(uint8_t&           address,
                             bool               is_read,
                             std::span<uint8_t> i2c_buffer,
                             size_t&            i2c_size,
                             void*              this_task_instance,
                             bool               new_transaction)
{
    // NOLINTNEXTLINE: no way around this without function pointers
    auto* task = static_cast<VrI2cTask*>(this_task_instance);

    if (is_read) {
        i2c_size = task->i2c_read(
            address, i2c_buffer.first<sys::i2c::I2cSlaveBufferSize>(), new_transaction);
    }
    else {
        task->i2c_write(address, i2c_buffer.first(i2c_size));
    }
}

template<ipchandler::Id BusId>
void VrI2cTask::make_task_by_bus(VrI2cTask::Config config)
{
    using BusConfig = VrI2cBusConfig<BusId>;

    static_assert(validate_vr_i2c_bus_slots(BusConfig::slots()),
                  "Invalid VrI2c device slots in config.h");

    // leave ~264 bytes of margin above the worst-case observed stack usage
    constexpr auto StackSize = std::max(640, int(configMINIMAL_STACK_SIZE));

    NV_TASK_DATA static VrI2cTaskData<BusConfig::DeviceNum> data(
        config, BusConfig::slots(), BusConfig::SlavePort);
    NV_STACK static sys::ipc::TaskStack<StackSize> stack;

    // NOLINTNEXTLINE(*-reinterpret-cast)
    const std::span<uint8_t> Priv(reinterpret_cast<uint8_t*>(&data), sizeof(data));
    data.task.setup(stack.span(), Priv, nv::ipc::Task::Priority::Norm, VrI2cTask::entrypoint);
}

void VrI2cTask::make(VrI2cTask::Config config)
{
    switch (config.ipchandler_id) {
        case ipchandler::Id::VrI2c0: make_task_by_bus<ipchandler::Id::VrI2c0>(config); break;
        case ipchandler::Id::VrI2c1: make_task_by_bus<ipchandler::Id::VrI2c1>(config); break;
        default                    : NV_ASSERT(false); break;
    }
}

void VrI2cTask::entrypoint(void* params)
{
    NV_ASSERT(params != nullptr);
    auto& task = *static_cast<VrI2cTask*>(params);
    task.start();
    task.suspend();
}

[[noreturn]] void VrI2cTask::start()
{
    nv::bootloader::Driver::set_task_booted(_boot_event);

    nv::i2c::Task::Request request{};
    auto item = nv::ipc::Queue::Item(std::bit_cast<uint8_t*>(&request), sizeof(request));

    while (true) {
        auto status = _queue.recv(item);
        if (status != nv::ipc::Queue::Status::Ok) {
            nv::logger::error(nv::logger::Event::I2CQueueFail,
                              {static_cast<uint8_t>(_queue.id()),
                               static_cast<uint8_t>(request.type),
                               static_cast<uint8_t>(nv::i2c::I2cQueueError::GetReqFail)});
            continue;
        }
        switch (request.type) {
            case nv::i2c::Task::RequestType::I2cRequest:
                handle_queued_i2c_request(request.data);
                break;
            case nv::i2c::Task::RequestType::WdtEvent: handle_wdt_event(); break;
            default:
                nv::logger::error(nv::logger::Event::I2CQueueFail,
                                  {static_cast<uint8_t>(_queue.id()),
                                   static_cast<uint8_t>(request.type),
                                   static_cast<uint8_t>(nv::i2c::I2cQueueError::GetReqFail)});
                break;
        }
    }
}

void VrI2cTask::handle_wdt_event()
{
    const auto index = nv::i2c::i2c_queue_to_wdt(_queue.id());
    if (index == nv::watchdog::TaskMonitorIndex::End) {
        return;
    }
    nv::watchdog::Runtime::mark_task_alive(index);
}

void VrI2cTask::wdt_notify(nv::watchdog::TaskMonitorIndex task_id)
{
    using namespace nv::ipc;
    using namespace std::chrono_literals;

    const auto queue_id = nv::i2c::wdt_to_i2c_queue(task_id);
    if (queue_id == QueueId::End) {
        return;
    }

    const nv::i2c::Task::Request request{.type = nv::i2c::Task::RequestType::WdtEvent};
    auto item         = Queue::Item(std::bit_cast<uint8_t*>(&request), sizeof(request));
    auto queue_status = Queue::make(queue_id).send(item, 0s);
    if (queue_status != Queue::Status::Ok) {
        nv::logger::error(nv::logger::Event::I2CQueueFail,
                          {static_cast<uint8_t>(queue_id),
                           static_cast<uint8_t>(request.type),
                           static_cast<uint8_t>(nv::i2c::I2cQueueError::PutWdtNotifyFail)});
    }
}

/*****************************************************
 * Process I2C requests from queue
 *****************************************************/

void VrI2cTask::handle_queued_i2c_request(std::span<uint8_t> buffer)
{
    NV_ASSERT(buffer.size() >= sizeof(nv::i2c::I2cRequest));
    const auto& request = *std::bit_cast<const nv::i2c::I2cRequest*>(
        static_cast<const void*>(buffer.data()));

    const size_t write_length = std::min(static_cast<size_t>(request.write_length),
                                         request.write_buffer.size());
    size_t       read_length  = std::min(static_cast<size_t>(request.read_length),
                                  _read_buffer.size());

    // SmbusBlockRead asks for MaxI2cPayloadSize; host only accepts <= 33(+PEC).
    if ((request.flags & nv::i2c::I2cFlags::RecvLen) != nv::i2c::I2cFlags::NoFlag) {
        const size_t pec = ((request.flags & nv::i2c::I2cFlags::SmbusPec)
                            != nv::i2c::I2cFlags::NoFlag)
                             ? 1U
                             : 0U;
        read_length      = std::min(
            read_length, static_cast<size_t>(nv::i2c::Task::MaxSMBusBlockReadLength) + pec);
    }

    std::span<const uint8_t> write_data;
    if (write_length > 0U) {
        write_data = std::span<const uint8_t>(request.write_buffer.data(), write_length);
    }

    process_i2c_request(request.src_id,
                        request.address,
                        static_cast<uint16_t>(write_length),
                        static_cast<uint16_t>(read_length),
                        request.flags,
                        write_data);
}

void VrI2cTask::process_i2c_request(uint8_t                  src_id,
                                    uint8_t                  address,
                                    uint16_t                 write_length,
                                    uint16_t                 read_length,
                                    nv::i2c::I2cFlags        flags,
                                    std::span<const uint8_t> write_data)
{
    const std::optional<DeviceRef> device = (address < AddressSpace) ? _device_map.at(address)
                                                                     : std::nullopt;

    const std::span<uint8_t> reply_data(_read_buffer.data(), _read_buffer.size());

    if (is_i2c_address_probe(write_length, read_length, flags)) {
        const auto result = device.has_value() ? nv::i2c::I2cStatus::Ok
                                               : nv::i2c::I2cStatus::Nak;
        reply_i2c(src_id, _ipchandler_id, 0, reply_data, result);
        return;
    }

    nv::i2c::I2cStatus result = nv::i2c::I2cStatus::Nak;
    if (device.has_value()) {
        const std::span<uint8_t> read_data(_read_buffer.data(), read_length);
        read_length = run_emulated_i2c_transfer(device->get(), write_data, read_data);
        result      = nv::i2c::I2cStatus::Ok;
    }
    else {
        // No device at this address: NAK with an empty payload
        read_length = 0;
    }

    reply_i2c(src_id, _ipchandler_id, read_length, reply_data, result);
}

/*****************************************************
 * I2C transfer helpers
 *****************************************************/

bool VrI2cTask::is_i2c_address_probe(size_t            write_length,
                                     size_t            read_length,
                                     nv::i2c::I2cFlags flags)
{
    if (write_length == 0U && read_length == 0U) {
        return true;
    }
    return ((flags & nv::i2c::I2cFlags::QuickRead) != nv::i2c::I2cFlags::NoFlag)
        || ((flags & nv::i2c::I2cFlags::QuickWrite) != nv::i2c::I2cFlags::NoFlag);
}

uint16_t VrI2cTask::run_emulated_i2c_transfer(emulation::EmulationDevice& device,
                                              std::span<const uint8_t>    write_data,
                                              std::span<uint8_t>          read_data)
{
    if (!write_data.empty()) {
        const auto len = std::min(write_data.size(), sys::i2c::I2cSlaveBufferSize);
        device.i2c_write_virtual(write_data.first(len));
    }

    size_t bytes_read = 0U;
    while (bytes_read < read_data.size()) {
        size_t     read_size       = 0U;
        const bool new_transaction = (bytes_read == 0U);

        device.i2c_read_virtual(read_data.subspan(bytes_read), read_size, new_transaction);
        bytes_read += read_size;

        if (read_size == 0U) {
            break;
        }
    }
    return static_cast<uint16_t>(std::min(bytes_read, read_data.size()));
}

void VrI2cTask::reply_i2c(uint8_t            src_id,
                          ipchandler::Id     bus_id,
                          uint16_t           read_len,
                          std::span<uint8_t> read_data,
                          nv::i2c::I2cStatus i2c_status)
{
    switch (src_id) {
        case static_cast<uint8_t>(ipchandler::Id::Usb):
            nv::usb::Task::to_usb(bus_id, read_len, read_data, i2c_status);
            break;
        case static_cast<uint8_t>(ipchandler::Id::Lstp):
            if constexpr (nv::lstp::EnableI2c) {
                nv::lstp::LstpRouter::send_i2c(bus_id, read_len, read_data, i2c_status);
            }
            break;
        default:
            nv::logger::error(nv::logger::Event::I2CForwardFail,
                              {static_cast<uint8_t>(bus_id), src_id});
            break;
    }
}

}  // namespace nv::i2c
