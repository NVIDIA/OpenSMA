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
#include "nv/uart/driver.h"

#include "fsl_debug_console.h"

#include "nv/nv.h"

using namespace nv::uart;

Driver::Driver(Port port) : sys::uart::Driver()
{
    switch (port) {
        case Port::Zero  : _base = LPUART0; break;
        case Port::One   : _base = LPUART1; break;
        case Port::Two   : _base = LPUART2; break;
        case Port::Three : _base = LPUART3; break;
        case Port::Four  : _base = LPUART4; break;
        case Port::Five  : _base = LPUART5; break;
        case Port::Six   : _base = LPUART6; break;
        case Port::Seven : _base = LPUART7; break;
        case Port::flexio: flexio_en = true; break;
        default          : _base = nullptr; break;
    }
    // nv::info("UART %d create\n", port);
}

Status Driver::tx(std::span<uint8_t> data)
{
    if (!flexio_en) {
        if (_base == nullptr) {
            return Status::NotInit;
        }

        auto status = LPUART_WriteBlocking(_base, data.data(), data.size());
        if (status != kStatus_Success) {
            return Status::TxFail;
        }
    }
    else {
        auto status = FLEXIO_UART_WriteBlocking(&_flexio_uart, data.data(), data.size());
        if (status != kStatus_Success) {
            return Status::TxFail;
        }
    }

    return Status::Ok;
}
