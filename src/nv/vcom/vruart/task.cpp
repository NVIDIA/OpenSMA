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

#include NV_IPC_CONFIG_H

namespace nv::vruart {

void BridgeTask::make()
{
    constexpr auto                 StackSize = std::max(1024, int(configMINIMAL_STACK_SIZE));
    NV_TASK_DATA static BridgeTask task;
    NV_STACK static sys::ipc::TaskStack<StackSize> stack;

    // NOLINTNEXTLINE(*-reinterpret-cast)
    const std::span<uint8_t> Priv(reinterpret_cast<uint8_t*>(&task), sizeof(BridgeTask));
    task.setup(stack.span(), Priv, Priority::Usb, BridgeTask::entrypoint);
}

void BridgeTask::entrypoint(void* params)
{
    NV_ASSERT(params != nullptr);
    auto& task = *static_cast<BridgeTask*>(params);
    task.main();
}

BridgeTask::BridgeTask() : ipc::Task(ipc::TaskId::Ubridge, "UartBridge"), bridge(Bridge::inst())
{}

[[noreturn]] void BridgeTask::main()
{
    bridge.init(nv::ipc::UartOverUsbUartInstance,
                nv::ipc::pintx,
                nv::ipc::pinrx,
                nv::ipc::UartOverUsbBaudrate,
                nv::ipc::UartOverUsbEdmaInstance,
                nv::ipc::UartOverUsbEdmaTxChn,
                nv::ipc::UartOverUsbEdmaRxChn);
    nv::bootloader::Driver::set_task_booted(nv::ipc::BootedEventBits::Ubridge);
    bridge.main();
}

}  // namespace nv::vruart
