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
#include <algorithm>
#include <cstdint>

#include "nv/mctp/interface.h"

#include NV_IPC_CONFIG_H

namespace nv::mctp {

/**
 * @brief The Constants class provides some constants
 */
class Constants
{
public:
    constexpr static uint32_t PldmRxBufSize = 256;

    constexpr static uint32_t SpdmRxBufSize = nv::ipc::SpdmRequestQueueSize;
    constexpr static uint32_t MctpTxBufSize = std::max(PldmRxBufSize, SpdmRxBufSize);

    constexpr static uint8_t  BufferSize      = PktBufDataLen + sizeof(mctp::PrivateHeader);
    constexpr static uint32_t MultiPktBufSize = 32 + 4096;

    constexpr static uint8_t  HeaderSize = 5;  // type:1 + header:4
    constexpr static uint32_t UsbBufSize = 512;

    // Nsm Constants

    // This HeaderSizeResponse represents a packet format with the following fields (12
    // bytes):
    // - 6 bytes: NVIDIA OEM binding
    // - 1 bytes: command code
    // - 1 bytes: completion code
    // - 2 bytes: reserved
    // - 2 bytes: data size
    constexpr static uint8_t NsmHeaderResponseSize = 12;

    // - 6 bytes: NVIDIA OEM binding
    // - 1 bytes: command code
    // - 1 bytes: completion code
    // - 2 bytes: telemetry count
    constexpr static uint8_t NsmAggregateHeaderResponseSize = 10;

    // - 6 bytes: NVIDIA OEM binding
    // - 1 bytes: command code
    // - 1 bytes: data size
    constexpr static uint8_t NsmHeaderRequestSize = 8;

    // - 6 bytes: NVIDIA OEM binding
    // - 1 bytes: rsvd1, ackr, event version
    // - 1 bytes: event id
    // - 1 bytes: event class
    // - 2 bytes: event state
    // - 1 bytes: data size
    constexpr static uint8_t NsmHeaderEventMsgSize = 12;
};

}  // namespace nv::mctp
