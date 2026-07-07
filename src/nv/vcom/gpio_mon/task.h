/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES.
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

#include "nv/vcom/gpio_mon/common.h"
#include "nv/ipc/task.h"

namespace nv::gpio_mon {

// Each scan window holds 1 FullIoDump (28B) plus up to N BankInterruptDumps (12B each).
// 1024 bytes accommodates 1 + 82 records, comfortably above what 100 ms of edge activity
// on five banks would generate before the buffer is shipped.
constexpr size_t BufferSize = 1024;

enum EventBits : uint32_t
{
    ScanTickBit = 1U << 0,  // 100 ms timer fired
    UsbRxBit    = 1U << 1,  // host sent an init-mask command
    UsbCloseBit = 1U << 2,  // host closed the VCOM port (DTR low)
};

class Task : public ipc::Task
{
public:
    Task();

    [[noreturn]] void main();

    static void  make();
    static void  entrypoint(void* params);
    static Task& inst();

    // Called from GPIO bank IRQ handlers (in ISR context).
    static void on_gpio_interrupt(uint8_t bank, uint32_t bank_io_val);

    // Called from the USB CDC callback; defers packet handling to this task.
    static uint8_t usb_rx_callback(uint8_t* data, uint32_t length);

    // Called when host closes the VCOM port (DTR low)
    static void usb_close_callback();

private:
    struct Buffer
    {
        // First FullIoDumpRecordSize bytes are reserved for the head snapshot
        std::array<uint8_t, BufferSize> data{};
        uint32_t                        offset = FullIoDumpRecordSize;
    };

    void apply_pending_mask();
    void disable_all_triggers();
    void drop_pending_records();
    void handle_usb_rx();
    void on_scan_tick();

    void write_full_io_dump(Buffer& buf);
    bool append_record_isr(uint32_t header, uint32_t bank_io_val);

    std::array<Buffer, 2>                buffers{};
    volatile uint8_t                     active_idx       = 0;
    volatile bool                        triggers_enabled = false;
    std::array<uint8_t, InitMaskCmdSize> pending_usb_cmd{};
    std::size_t                          pending_usb_len = 0;
    std::array<uint32_t, BankCount>      active_mask{};
    std::array<uint32_t, BankCount>      pending_mask{};
};

}  // namespace nv::gpio_mon
