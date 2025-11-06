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
#include <limits>

#include "nv/common/expected.h"
#include "nv/ipc/object.h"
#include "sys/ipc/event.h"
#include NV_IPC_CONFIG_H

namespace nv::ipc {

/**
 * An abstracted Event interface.
 *
 * The non os-specific implementation can be found in nv/ipc/event.cpp,
 * while the os-specific implementation can be found in sys/{os}/sys/ipc/event.{h,cpp}
 */
class Event
: public Object
, sys::ipc::Event
{
public:
    enum class Status
    {
        Ok,                       ///< Success
        InsufficientPermissions,  ///< Calling task had no permission for event operation.
        Timeout,                  ///< A timeout occured.
        InvalidParam,             ///< One of the calling parameters was deemed invalid.
        Unknown,                  ///< An unknown error has occured.
        InvalidOperation,         ///< The operation is not allowed on the current core.
        CommunicationFailed,      ///< Communication failed.
    };

    using IdType = EventId;
    using Bits   = uint32_t;
    using Return = common::Expected<Bits, Status>;
    using Usecs  = std::chrono::microseconds;
    static Event& make(EventId id);

    /// Return the unique EventId associated with this Event.
    EventId id() const { return _id; }

    /// Get the currently set bits or error.
    Return bits() const;

    /// Set the provided bits.
    Status set(Bits bits, CoreId dest_core = CoreId::Invalid);

    /// Clear the provided bits.
    Status clear(Bits bits = std::numeric_limits<Bits>::max());

    /**
     * Wait on event.
     *
     * @param[in] bits         Which bits to wait on.
     * @param[in] clear        Clear bits on read.
     * @param[in] wait_for_all Wait for all bits to be set.
     * @param[in] timeout      Maximum time to wait.
     * @return                  Expected bits or error status.
     */
    Return
    wait(Bits bits, bool clear = true, bool wait_for_all = false, Usecs timeout = Usecs::max());

    /// For static array initialization in supervisor only.
    Event() noexcept : sys::ipc::Event() {}

private:
    explicit Event(EventId id);
    EventId _id = EventId::End;
};

}  // namespace nv::ipc
