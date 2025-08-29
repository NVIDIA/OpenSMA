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
#include <cstdint>

#include "usb_config_wrapper.h"

namespace sys::usb {

typedef struct UsbCompositeStruct
{
    usb_device_handle       device_handle;
    class_handle_t          mctp_handle;
    std::array<uint8_t*, 2> mctp_buffer;
    class_handle_t          hid_handle;
    std::array<uint8_t*, 2> hid_buffer;
    uint8_t                 buffer_index;
    uint8_t                 speed;
    uint8_t                 attach;
    uint8_t                 idle_rate;
    uint8_t                 current_configuration;
    uint8_t current_interface_alternate_setting[SYS_USB_COMPOSITE_INTERFACE_COUNT];
#if ((defined(USB_DEVICE_CONFIG_REMOTE_WAKEUP)) && (USB_DEVICE_CONFIG_REMOTE_WAKEUP > 0U))
    volatile uint8_t remote_wakeup;
#endif
} usb_composite_struct_t;

class Driver
{
public:
    static uint8_t
            init(void* mctp_buffer0, void* mctp_buffer1, void* hid_buffer0, void* hid_buffer1);
    uint8_t write_mctp(uint8_t* data, uint32_t length);
    uint8_t write_hid(uint8_t* data, uint32_t length);
    usb_status_t enable_mctp_rx();
    usb_status_t enable_hid_rx();
    bool         check_vbus();

    static usb_status_t
    usb_devicemctpcallback(class_handle_t handle, uint32_t event, void* param);
    static usb_status_t
    usb_devicehidcallback(class_handle_t handle, uint32_t event, void* param);
    static usb_status_t
    usb_devicecallback(usb_device_handle handle, uint32_t event, void* param);

private:
    void usb_deviceapplicationinit(void* buffer0, void* buffer1);
};

}  // namespace sys::usb
