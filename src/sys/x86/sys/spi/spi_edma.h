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
#include <span>
#include "nv/gpio/driver.h"

#include "nv/ipc/event.h"
#include "nv/common/literals.h"

#define TRANSFER_SIZE 512U /* Transfer dataSize */
#include "nv/spi/port.h"
namespace sys::spi {

using nv::operator""_bit;

enum Event : nv::ipc::Event::Bits
{
    XferDone  = 1_bit,
    XferError = 2_bit,
};

enum class Status
{
    Ok,
    EventTimeout,
    EventClearFail,
    EventXferFail,
};

struct MasterTransferContext
{
    volatile bool xferDone;
    void*         driver;  // Pointer to EdmaDriver instance
};

class EdmaDriver
{
public:
    void             bind(nv::spi::Flexcomm flexcomm, nv::ipc::EventId event_id);
    void             init();
    sys::spi::Status sendRecv(uint32_t                 send_len,
                              const std::span<uint8_t> sbuf,
                              uint32_t                 recv_len,
                              std::span<uint8_t>       rbuf);
};
}  // namespace sys::spi