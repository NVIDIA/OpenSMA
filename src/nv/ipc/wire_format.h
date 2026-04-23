/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES.
 * All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 *
 * IPC Wire Format Definitions
 *
 * Shared between Core0 (FreeRTOS) and Core1 (bare-metal) for C2C communication.
 * These structures define the exact binary layout used in the shared StreamBuffer.
 */

#ifndef NV_IPC_BM_WIRE_FORMAT_H
#define NV_IPC_BM_WIRE_FORMAT_H

#include <cstdint>

namespace nv::ipc_bm {

/*******************************************************************************
 * IPC Request Wire Format
 *
 * These structures match Core0's std::variant<QueueRequest, EventRequest> layout.
 * GCC libstdc++ std::variant stores _M_index FIRST, then the union storage.
 *
 * Total size: 16 bytes (aligned to 4)
 ******************************************************************************/

// Wire format for QueueRequest (variant index = 0)
struct alignas(4) QueueRequestWire
{
    uint8_t  variant_index;  // offset 0: must be 0 for QueueRequest
    uint8_t  padding0[3];    // offset 1-3: alignment padding
    uint16_t length;         // offset 4: data length
    uint8_t  is_front;       // offset 6: bool as uint8_t
    uint8_t  padding1;       // offset 7: padding
    uint32_t queue_id;       // offset 8: QueueId as uint32_t
    uint32_t padding2;       // offset 12: padding to 16 bytes
};

// Wire format for EventRequest (variant index = 1)
struct alignas(4) EventRequestWire
{
    uint8_t  variant_index;  // offset 0: must be 1 for EventRequest
    uint8_t  padding0[3];    // offset 1-3: alignment padding
    uint8_t  is_set;         // offset 4: bool as uint8_t
    uint8_t  padding1[3];    // offset 5-7: padding
    uint32_t bits;           // offset 8: event bits
    uint32_t event_id;       // offset 12: EventId as uint32_t
};

// Compile-time verification of structure sizes
static_assert(sizeof(QueueRequestWire) == 16, "QueueRequestWire must be 16 bytes");
static_assert(sizeof(EventRequestWire) == 16, "EventRequestWire must be 16 bytes");

}  // namespace nv::ipc_bm

#endif  // NV_IPC_BM_WIRE_FORMAT_H
