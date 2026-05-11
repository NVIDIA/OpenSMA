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
#include "nv/spi/flashrom_task.h"

#include "nv/bootloader.h"
#include "nv/nv.h"
#include "nv/logger/log.h"

using namespace nv::spi;
using namespace std::chrono_literals;

void FlashromTask::make(Config config)
{
    if constexpr (nv::lstp::EnableSpi) {
        // Flashrom task requires more stack for USB communication
        constexpr auto StackSize = std::max(480, int(configMINIMAL_STACK_SIZE));

        NV_TASK_DATA static FlashromTask               task(config);
        NV_STACK static sys::ipc::TaskStack<StackSize> stack;

        // NOLINTNEXTLINE(*-reinterpret-cast)
        const std::span<uint8_t> priv(reinterpret_cast<uint8_t*>(&task), sizeof(FlashromTask));
        task.setup(stack.span(), priv, Priority::Norm, FlashromTask::entrypoint);
    }
}

void FlashromTask::entrypoint(void* params)
{
    NV_ASSERT(params != nullptr);
    auto& task = *static_cast<FlashromTask*>(params);
    task.start();
    task.suspend();
}

FlashromTask::FlashromTask(Config config) noexcept
: nv::ipc::Task(config.task_id, config.task_name)
, _boot_event(config.boot_event)
, _flashrom(*config.edma_driver)
{
    _flashrom.bind(config.flexcomm_id,
                   nv::ipc::EventId::Spi0EdmaDriverEvent,
                   config.cs_port_id,
                   config.cs_pin_id,
                   config.cs1_port_id,
                   config.cs1_pin_id);
    _flashrom.init();
}

[[noreturn]] void FlashromTask::start()
{
    using namespace ipc;

    bootloader::Driver::set_task_booted(_boot_event);
    std::array<uint8_t, nv::ipc::UsbLstpMsgSize> tx_msg{};
    auto tx_msg_item    = Queue::Item(std::bit_cast<uint8_t*>(&tx_msg), sizeof(tx_msg));
    auto flashrom_queue = nv::ipc::Queue::make(nv::ipc::QueueId::LstpToSpi);

    // Main task loop - runs continuously processing SPI flashrom messages
    // This task is designed to run for the lifetime of the system
    // coverity[no_escape : FALSE]
    while (true) {
        // Block on queue receive with timeout (5 seconds)
        auto status = flashrom_queue.recv(tx_msg_item, 5s);

        if (status == Queue::Status::Ok) {
            _flashrom.handle_tx(tx_msg);
        }
    }
}

bool FlashromTask::to_spi(std::span<uint8_t>& msg)
{
    auto status = nv::ipc::Queue::make(nv::ipc::QueueId::LstpToSpi).send(msg);
    if (status != nv::ipc::Queue::Status::Ok) {
        logger::error(logger::Event::SpiFlashromQueueFail, {static_cast<uint8_t>(status)});
        return false;
    }

    return true;
}
