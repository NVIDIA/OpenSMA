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

#include <chrono>
#include <cstdint>
#include <span>
#include <string_view>

#include "FreeRTOSConfig.h"

#include "nv/common/literals.h"
#include "nv/common/utils.h"
#include "sys/ipc/task.h"
#include NV_IPC_CONFIG_H

namespace nv::ipc {

/**
 * Generic task interface.
 *
 * Any system dependent data is inside of sys::ipc::Task in sys/{target}/sys/ipc/task.h.
 * The code is implemtned as nv::ipc::Task in sys/{target}/sys/ipc/task.cpp
 *
 * @todo Design not completed.
 */
class Task : public sys::ipc::Task
{
public:
    Task() = delete;

    using EntryPoint = void (*)(void*);

    // TODO: need to decide on this.
    enum class Priority
    {
        Begin  = 0,
        Idle   = Begin,
        Norm   = 1,
        Pldm   = 2,
        Spdm   = 2,
        Logger = 2,
        Mctp   = 3,
        I2c    = 4,
        Lstp   = 4,
        Usb    = 5,
        Ipc    = 6,
        Timer  = configTIMER_TASK_PRIORITY,
        Max    = configMAX_PRIORITIES,
        End,
        Invalid = 0xFF,
    };

    /// Generic Task memory region permission.
    enum class Permission : uint32_t
    {
        None            = 0,
        Read            = 0_bit,
        Write           = 1_bit,
        PrivilegedRead  = 2_bit,
        PrivilegedWrite = 3_bit,
        Executable      = 4_bit,
        Cacheable       = 5_bit,
    };

    /// Use system-specific memory region information from sys::ipc::MpuRegion
    using MpuRegion = sys::ipc::MpuRegion;

    /// Status
    enum class Status
    {
        Dead      = 0,
        Running   = 0_bit,
        Suspended = 1_bit,
    };

    bool setup(const std::span<uint8_t>& stack_region,
               const std::span<uint8_t>& private_region,
               Priority                  priority,
               EntryPoint                ep);

    void suspend() const;
    void resume() const;
    void delay(std::chrono::microseconds ms) const;
    void setup_shared_region(MemoryRegion_t& mr) const;
    void setup_peripheral_region(MemoryRegion_t& mr) const;
    void setup_rom_api_region(MemoryRegion_t& mr) const;
    void setup_private_region(MemoryRegion_t& mr, const std::span<uint8_t>& priv) const;

    Status status() const;

    auto&                         name() const { return _name; }
    auto                          id() const { return _id; }
    std::pair<uint8_t*, uint8_t*> stack_region() const
    {
        return std::make_pair(
            reinterpret_cast<uint8_t*>(_stack_region.data()),
            reinterpret_cast<uint8_t*>(_stack_region.data() + _stack_region.size()));
    }

    using PairIdAndPriority = std::pair<TaskId, Priority>;
    using TaskIdAndPriority = std::array<PairIdAndPriority,
                                         static_cast<uint8_t>(TaskId::KernelEnd)>;
    static void get_task_id_and_priority(TaskIdAndPriority& idAndPriority);

    template<typename... Params>
    bool checking_parameter_is_from_self_stack(const Params&... params)
    {
        auto [stack_start, stack_end] = stack_region();
        auto check_address_in_stack   = []<typename T>(const T&    param,
                                                     const void* stack_start,
                                                     const void* stack_end) {
            // If param have data and size attribute
            if constexpr (requires {
                              param.data();
                              param.size();
                          }) {
                return std::bit_cast<void*>(param.data()) >= std::bit_cast<void*>(stack_start)
                    && std::bit_cast<void*>(param.data()) <= std::bit_cast<void*>(stack_end)
                    && std::bit_cast<void*>(param.data() + param.size())
                           >= std::bit_cast<void*>(stack_start)
                    && std::bit_cast<void*>(param.data() + param.size())
                           <= std::bit_cast<void*>(stack_end);
            }
            else {
                return std::bit_cast<void*>(&param) >= std::bit_cast<void*>(stack_start)
                    && std::bit_cast<void*>(&param) <= std::bit_cast<void*>(stack_end)
                    && std::bit_cast<void*>(&param + 1) >= std::bit_cast<void*>(stack_start)
                    && std::bit_cast<void*>(&param + 1) <= std::bit_cast<void*>(stack_end);
            }
        };
        return ((check_address_in_stack(params, stack_start, stack_end)) && ...);
    };

protected:
    Task(TaskId id, std::string_view name) noexcept;
    void setup_region(std::span<uint8_t>& region, Permission perm);

private:
    TaskId             _id;
    std::string_view   _name;
    std::span<uint8_t> _stack_region{};
};

class TaskPublic : public sys::ipc::TaskPublic
{
public:
    auto& id() { return _id; }
    Task* task{};

private:
    TaskId _id{};
};

}  // namespace nv::ipc
