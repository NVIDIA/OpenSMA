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
#include <assert.h>

#include "fsl_lpuart.h"
#include "fsl_flexio_uart.h"

__attribute__((weak)) status_t FLEXIO_UART_WriteBlocking(FLEXIO_UART_Type* base,
                                                         const uint8_t*    txData,
                                                         size_t            txSize)
{
    return -1;
}
__attribute__((weak)) FLEXIO_UART_Type FLEXIO0_device;
namespace sys::uart {

class Driver
{
protected:
    bool             flexio_en    = false;
    LPUART_Type*     _base        = nullptr;
    FLEXIO_UART_Type _flexio_uart = FLEXIO0_device;
};

}  // namespace sys::uart
