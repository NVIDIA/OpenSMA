/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES.
 * All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 *
 * C2C StreamBuffer Control Structure
 *
 * Shared between Core0 (FreeRTOS) and Core1 (bare-metal) for C2C communication.
 * This struct describes the memory layout of a FreeRTOS StreamBuffer's internal
 * control fields (StreamBuffer_t / StaticStreamBuffer_t) so that Core1 can
 * directly read/write the shared ring buffer without depending on FreeRTOS headers.
 *
 * IMPORTANT: On Core0, static_asserts in supervisor.cpp verify that this layout
 * matches FreeRTOS's StaticStreamBuffer_t. If FreeRTOS changes its internal
 * layout, the build will fail with a clear error message.
 */

#ifndef NV_IPC_C2C_STREAM_BUFFER_H
#define NV_IPC_C2C_STREAM_BUFFER_H

#include <cstddef>
#include <cstdint>

namespace nv::ipc {

// Memory layout of FreeRTOS StreamBuffer_t internal fields.
//
// Maps to StaticStreamBuffer_t dummy fields:
//   uxDummy1[0] -> tail             (read index)
//   uxDummy1[1] -> head             (write index)
//   uxDummy1[2] -> length           (ring buffer capacity)
//   uxDummy1[3] -> trigger_level
//   pvDummy2[0] -> task_waiting_rx  (unused by Core1)
//   pvDummy2[1] -> task_waiting_tx  (unused by Core1)
//   pvDummy2[2] -> buffer_ptr       (pointer to ring buffer data)
//   ucDummy3    -> flags            (unused by Core1)
struct C2CStreamBufferCtrl
{
    volatile size_t tail;             // offset 0:  read index
    volatile size_t head;             // offset 4:  write index
    size_t          length;           // offset 8:  total ring buffer length
    size_t          trigger_level;    // offset 12: trigger level bytes
    void*           task_waiting_rx;  // offset 16: FreeRTOS task handle (unused by Core1)
    void*           task_waiting_tx;  // offset 20: FreeRTOS task handle (unused by Core1)
    uint8_t*        buffer_ptr;       // offset 24: pointer to ring buffer data
    uint8_t         flags;            // offset 28: stream buffer flags
};

}  // namespace nv::ipc

#endif  // NV_IPC_C2C_STREAM_BUFFER_H
