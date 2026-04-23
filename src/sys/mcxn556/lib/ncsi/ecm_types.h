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

#include "nv/usb/usb_mctp_header.h"

// Forward declarations for USB CDC ECM types
extern "C" {
#include "usb_device.h"
#include "usb_device_class.h"
#include "usb_device_cdc_ecm.h"
}

namespace nv::ecm_bm {

// ECM configuration constants
constexpr uint16_t EthFrameMaxLength = 1514U;
constexpr uint8_t  MacAddressLength  = 6U;

// Re-export USB MCTP types from nv::usb for backward compatibility
using nv::usb::UsbDmtfId;
using nv::usb::UsbMctpHeader;

// ECM NIC event types (bit flags)
enum class EcmEvent : uint32_t
{
    NotifyNetworkChange = (1U << 1),
};

// Application event bits for USB callbacks
enum AppEvent : uint32_t
{
    kAPP_MctpRxReady = 3,  // MCTP packet received from USB
    kAPP_HidRxReady  = 4,  // HID report received from USB
    kAPP_AcmRxReady  = 5,  // ACM data received from USB (UART bridge)
    kAPP_LstpRxReady = 6,  // LSTP packet received from USB
};

// Application event helper macros
#define APP_EVENT_SET(event, bit)   ((event) |= (1U << (bit)))
#define APP_EVENT_CLEAR(event, bit) ((event) &= ~(1U << (bit)))
#define APP_EVENT_CHECK(event, bit) ((event) & (1U << (bit)))

// ECM NIC handle structure
struct EcmNicHandle
{
    usb_device_handle deviceHandle;
#if USB_DEVICE_CONFIG_CDC_ECM
    class_handle_t cdcEcmHandle;
#endif
    uint8_t          configuration;
    uint8_t          interfaceAltSetting[2];
    volatile uint8_t attachStatus;
    uint8_t          deviceSpeed;
    volatile uint8_t linkStatus;
    uint32_t         linkSpeed;
};

// Event helpers
inline void ecm_event_set(volatile uint32_t& events, EcmEvent ev)
{
    events |= static_cast<uint32_t>(ev);
}

inline void ecm_event_clear(volatile uint32_t& events)
{
    events = 0;
}

// ECM API functions (implemented in usb_callbacks.cpp)
void ecm_usb_init(EcmNicHandle* handle, volatile uint32_t* app_event);
void ecm_usb_run(EcmNicHandle* handle);
void ecm_transfer_usb_recv(EcmNicHandle* handle);
void ecm_transfer_eth_to_usb_send(EcmNicHandle* handle);
void ecm_send_network_notification(EcmNicHandle* handle, bool connected);
void ecm_send_speed_notification(EcmNicHandle* handle, uint32_t speed);

}  // namespace nv::ecm_bm
