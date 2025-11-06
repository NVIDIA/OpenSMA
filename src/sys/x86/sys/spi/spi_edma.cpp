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
#include "sys/spi/spi_edma.h"

#include "nv/spi/task.h"
#include "nv/nv.h"

using namespace nv;
using namespace sys::spi;

void EdmaDriver::bind([[maybe_unused]] nv::spi::Flexcomm flexcomm,
                      [[maybe_unused]] nv::ipc::EventId  event_id)
{
    return;
}

void EdmaDriver::init()
{
    return;
}

sys::spi::Status EdmaDriver::sendRecv([[maybe_unused]] uint32_t                 send_len,
                                      [[maybe_unused]] const std::span<uint8_t> sbuf,
                                      [[maybe_unused]] uint32_t                 recv_len,
                                      [[maybe_unused]] std::span<uint8_t>       rbuf)
{
    return sys::spi::Status::Ok;
}
