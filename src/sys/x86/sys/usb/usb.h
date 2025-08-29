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
#include <cstdint>

namespace sys::usb {

constexpr std::size_t Hidbuffersize = 64;

class Driver
{
public:
    uint8_t init(void* mctp_buffer0, void* mctp_buffer1, void* hid_buffer0, void* hid_buffer1);
    uint8_t
    init_svc(void* mctp_buffer0, void* mctp_buffer1, void* hid_buffer0, void* hid_buffer1);
    uint8_t write_mctp(uint8_t* data, uint32_t length);
    uint8_t write_hid(uint8_t* data, uint32_t length);
    bool    enable_mctp_rx();
    bool    enable_hid_rx();
    bool    check_vbus();

private:
};

}  // namespace sys::usb
