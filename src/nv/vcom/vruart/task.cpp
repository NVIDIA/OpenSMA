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

#include "nv/vcom/vruart/task.h"
#include "nv/bootloader.h"
#include "nv/common/preproc.h"

#include <atomic>
#include <limits>
#include <utility>

#include NV_IPC_CONFIG_H

namespace nv::vruart {

constexpr size_t BridgeCount = nv::ipc::UartOverUsbProtocol == Protocol::Lstp
                                 ? nv::lstp::LstpChannelTypeCount(
                                       nv::lstp::LstpChannels, nv::lstp::LstpChannelType::Uart)
                                 : nv::ipc::UartBridgeConfigs.size();
static_assert(BridgeCount > 0, "at least one UART bridge is required");
static_assert(BridgeCount == nv::ipc::UartBridgeConfigs.size(),
              "UartBridgeConfigs must contain one entry per UART bridge");

namespace {

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
NV_SHARED_BSS std::atomic<uint32_t> booted_tasks{0};

// Bridges may share one booted bit when the project is out of free bits. The bit must only be
// set once every bridge using it has finished init, so one stuck bridge still fails boot.
bool boot_bit_complete(size_t index)
{
    static_assert(BridgeCount <= std::numeric_limits<uint32_t>::digits);
    const auto bit   = nv::ipc::UartBridgeConfigs.at(index).booted_event;
    uint32_t   peers = 0;
    for (size_t i = 0; i < BridgeCount; i++) {
        if (nv::ipc::UartBridgeConfigs.at(i).booted_event == bit) {
            peers |= uint32_t{1} << i;
        }
    }
    const uint32_t self   = uint32_t{1} << index;
    const uint32_t booted = booted_tasks.fetch_or(self) | self;
    return (booted & peers) == peers;
}

}  // namespace

template<size_t Index>
void Task::make_instance()
{
    // leave ~304 bytes of margin above the worst-case observed stack usage
    constexpr auto           StackSize = std::max(320, int(configMINIMAL_STACK_SIZE));
    NV_TASK_DATA static Task task{static_cast<uint8_t>(Index)};
    NV_STACK static sys::ipc::TaskStack<StackSize> stack;

    // NOLINTNEXTLINE(*-reinterpret-cast)
    const std::span<uint8_t> Priv(reinterpret_cast<uint8_t*>(&task), sizeof(Task));
    task.setup(stack.span(), Priv, Priority::Usb, Task::entrypoint);
}

template<size_t... Indices>
void Task::make_instances(std::index_sequence<Indices...> /*indices*/)
{
    (make_instance<Indices>(), ...);
}

void Task::make()
{
    make_instances(std::make_index_sequence<BridgeCount>{});
}

void Task::entrypoint(void* params)
{
    NV_ASSERT(params != nullptr);
    auto& task = *static_cast<Task*>(params);
    task.main();
}

Task::Task(uint8_t index_) noexcept
: ipc::Task(nv::ipc::UartBridgeConfigs.at(index_).task_id,
            nv::ipc::UartBridgeConfigs.at(index_).task_name)
, index(index_)
, bridge(Bridge::inst(index_))
{}

[[noreturn]] void Task::main()
{
    const auto& config   = nv::ipc::UartBridgeConfigs.at(index);
    auto        baudrate = config.baudrate;
    if constexpr (nv::ipc::UartOverUsbProtocol == Protocol::Lstp) {
        const auto channel = BridgeCount == 1
                               ? nv::lstp::GetFirstChannelId(nv::lstp::LstpChannels,
                                                             nv::lstp::LstpChannelType::Uart)
                               : nv::lstp::GetChannelIdByInstance(
                                     nv::lstp::LstpChannels,
                                     nv::lstp::LstpChannelType::Uart,
                                     static_cast<uint8_t>(config.uart_instance));
        baudrate           = std::get<nv::lstp::LstpUartChannelConfig>(
                       nv::lstp::LstpChannels.at(channel).config)
                       .speed;
    }
    bridge.init(config.uart_instance,
                config.tx,
                config.rx,
                baudrate,
                config.edma_instance,
                config.edma_tx_channel,
                config.edma_rx_channel);
    if (boot_bit_complete(index)) {
        nv::bootloader::Driver::set_task_booted(config.booted_event);
    }
    bridge.main();
}

}  // namespace nv::vruart
