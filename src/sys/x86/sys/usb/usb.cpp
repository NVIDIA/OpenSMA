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
#include "usb.h"

#include "nv/usb/task.h"

// NOLINTBEGIN

namespace sys::usb {

uint8_t Driver::init_svc(void* mctp_buffer0,
                         void* mctp_buffer1,
                         void* hid_buffer0,
                         void* hid_buffer1,
                         void* hid_buffer2)
{
    return 0;
}

uint8_t Driver::init(void* mctp_buffer0,
                     void* mctp_buffer1,
                     void* hid_buffer0,
                     void* hid_buffer1,
                     void* hid_buffer2,
                     void* spi_buffer0,
                     void* spi_buffer1,
                     void* spi_rx_len)
{
    return 0;
}

uint8_t Driver::write_mctp(uint8_t* data, uint32_t length)
{
    return 0;
}

uint8_t Driver::write_hid(uint8_t* data, uint32_t length)
{
    return 0;
}

bool Driver::enable_mctp_rx()
{
    return 0;
}

bool Driver::enable_hid_rx()
{
    return 0;
}

bool Driver::check_vbus()
{
    return 0;
}

uint8_t Driver::write_spi(uint8_t* data, uint32_t length)
{
    return 0;
}

bool Driver::enable_spi_rx()
{
    return 0;
}

void Driver::recover_spi_endpoint() {}

}  // namespace sys::usb

// NOLINTEND
