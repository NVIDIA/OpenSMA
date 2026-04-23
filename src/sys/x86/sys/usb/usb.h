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
    uint8_t init(void* mctp_buffer0,
                 void* mctp_buffer1,
                 void* hid_buffer0,
                 void* hid_buffer1,
                 void* hid_buffer2,
                 void* spi_buffer0 = nullptr,
                 void* spi_buffer1 = nullptr,
                 void* spi_rx_len  = 0);
    uint8_t init_svc(void* mctp_buffer0,
                     void* mctp_buffer1,
                     void* hid_buffer0,
                     void* hid_buffer1,
                     void* hid_buffer2);
    uint8_t write_mctp(uint8_t* data, uint32_t length);
    uint8_t write_hid(uint8_t* data, uint32_t length);
    bool    enable_mctp_rx();
    bool    enable_hid_rx();
    bool    check_vbus();

    uint8_t     write_spi(uint8_t* data, uint32_t length);
    bool        enable_spi_rx();
    void        recover_spi_endpoint();
    static bool is_device_connected();

    // VCOM (UART bridge) stubs
    static void  vcom_rearm_rx(void* handle, uint8_t* buffer);
    static bool  vcom_send(void* handle, uint8_t* data, uint32_t length);
    static void* get_vcom_handle();
    static bool  is_vcom_ready();

    // VCOM callback registration stubs
    using VcomRxCallback    = uint8_t (*)(uint8_t*, uint32_t);
    using VcomCloseCallback = void (*)();
    static void set_vcom_rx_callback(VcomRxCallback /*callback*/) {}
    static void set_vcom_close_callback(VcomCloseCallback /*callback*/) {}

private:
};

}  // namespace sys::usb
