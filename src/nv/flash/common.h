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
#include <cstddef>
#include <cstdint>

#include "nv/common/utils.h"
#include "nv/flash/datastore.h"
#include "sys/flash/flash_config.h"
namespace nv::flash {
constexpr uint32_t SectorSize = sys::flash::config::SectorSize;
constexpr uint32_t PhraseSize = sys::flash::config::PhraseSize;  // smallest size flash could
                                                                 // write in one operation,
using Address                   = sys::flash::config::Address;
constexpr Address  RemapSize    = sys::flash::config::RemapSize;
constexpr Address  MaxAddress   = sys::flash::config::MaxAddress;
constexpr Address  FmcFwAddress = sys::flash::config::FmcFwAddress;
constexpr Address  FmcFwSize    = sys::flash::config::FmcFwSize;
constexpr uint32_t BufferSize   = 256;
using Buffer                    = std::array<uint8_t, BufferSize>;

using ProgressPercent = uint8_t;

enum class Status : uint32_t
{
    Ok = 0,
    Error,
    Busy,
    Timeout,
    InvalidParam,
    BackgroundCopyIdle,
    BackgroundCopyDone,
    BackgroundCopyFailed,
    BackgroundCopyInprogress,
    CumulativeWrite,
    StatusLogMask = (nv::common::bit(Status::Busy) | nv::common::bit(Status::InvalidParam)
                     | nv::common::bit(Status::Timeout)
                     | nv::common::bit(Status::CumulativeWrite)),
};

enum class RequestType : uint32_t
{
    Read,
    Write,
    Erase,
    BgStatusQuery,
    DataStoreSet,
    DataStoreGet,
    CfpaRead,
    CfpaWriteSecVersion,
    CfpaWriteKeyRevoke,
    CfpaCustomerRead,
    CfpaCustomerWrite,
    EfuseRead,
    EfuseProgram,
    CmpaRead,
    CrcRead,
    EfuseLifeCycleRead,
};

enum class FlashEvent : uint32_t
{
    Begin          = 0,
    FlashOperation = Begin,
    BackgroundCopyStart,
    BackgroundCopyQuery,
    BackgroundCopyNext,
    DataStore,
    Timeout,
    WdtEvent,
    End,
};

enum EventBits : uint32_t
{
    None                     = 0,
    FlashOperationEvent      = nv::common::bit(FlashEvent::FlashOperation),
    BackgroundCopyStartEvent = nv::common::bit(FlashEvent::BackgroundCopyStart),
    BackgroundCopyQueryEvent = nv::common::bit(FlashEvent::BackgroundCopyQuery),
    BackgroundCopyNextEvent  = nv::common::bit(FlashEvent::BackgroundCopyNext),
    DataStoreEvent           = nv::common::bit(FlashEvent::DataStore),
    TimeoutEvent             = nv::common::bit(FlashEvent::Timeout),
    WdtEvent                 = nv::common::bit(FlashEvent::WdtEvent)
};

struct [[gnu::packed]] Request
{
    RequestType type;
    Address     address;
    Address     offset;
    uint32_t    sec_key_version;
    uint32_t    length;
    Buffer      buffer;
    Key         key;
};

struct [[gnu::packed]] Response
{
    RequestType type;
    Address     address;
    uint32_t    length;
    Buffer      buffer;
    Status      status;
};

}  // namespace nv::flash