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
#include "usb_device_config.h"

#ifdef __cplusplus
extern "C" {
#endif
#include <usb.h>               // usb_status_t, usb_device_handle
#include <usb_device.h>        // usb_device_callback_t (required by usb_device_class.h)
#include <usb_device_class.h>  // class_handle_t
#ifdef __cplusplus
}
#endif

namespace sys::usb {

typedef struct UsbCompositeStruct
{
    usb_device_handle       device_handle;
    class_handle_t          mctp_handle;
    std::array<uint8_t*, 2> mctp_buffer;
    class_handle_t          hid_handle;
    std::array<uint8_t*, 3> hid_buffer;
    class_handle_t          spi_handle;
    std::array<uint8_t*, 2> spi_buffer;
    uint32_t*               spi_rx_len;
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
    static uint8_t init(void* mctp_buffer0,
                        void* mctp_buffer1,
                        void* hid_buffer0,
                        void* hid_buffer1,
                        void* hid_buffer2,
                        void* spi_buffer0 = nullptr,
                        void* spi_buffer1 = nullptr,
                        void* spi_rx_len  = 0);
    uint8_t        write_mctp(uint8_t* data, uint32_t length);
    uint8_t        write_hid(uint8_t* data, uint32_t length);
    uint8_t        write_spi(uint8_t* data, uint32_t length);
    usb_status_t   enable_mctp_rx();
    usb_status_t   enable_hid_rx();
    usb_status_t   enable_spi_rx();
    void           recover_spi_endpoint();
    bool           check_vbus();
    static bool    is_device_connected();

    static usb_status_t
    usb_devicemctpcallback(class_handle_t handle, uint32_t event, void* param);
    static usb_status_t
    usb_devicehidcallback(class_handle_t handle, uint32_t event, void* param);
    static usb_status_t
    usb_devicecallback(usb_device_handle handle, uint32_t event, void* param);
    static usb_status_t
    usb_deviceSpicallback(class_handle_t handle, uint32_t event, void* param);

    static void vcom_rearm_rx(void* handle, uint8_t* buffer);

    // Get VCOM handle
    static void* get_vcom_handle();

    // Check if VCOM is ready
    static bool is_vcom_ready();

    // Send data via USB VCOM, returns true if busy
    static bool vcom_send(void* handle, uint8_t* data, uint32_t length);

#if defined(USB_CONFIG_UART_BRIDGE)
    static usb_status_t
    usb_devicevcomcallback(class_handle_t handle, uint32_t event, void* param);

    static usb_status_t usb_vcomsetconfigure(class_handle_t handle, uint8_t configure);
    // Re-arm USB VCOM RX with dynamic packet size

    // VCOM RX callback: returns 0 on success
    using VcomRxCallback = uint8_t (*)(uint8_t* data, uint32_t length);

    // Register VCOM RX callback
    static void set_vcom_rx_callback(VcomRxCallback callback);

    // VCOM close callback (DTR low)
    using VcomCloseCallback = void (*)();

    // Register VCOM close callback
    static void set_vcom_close_callback(VcomCloseCallback callback);

#endif

private:
    void usb_deviceapplicationinit(void* buffer0, void* buffer1);
};

}  // namespace sys::usb
