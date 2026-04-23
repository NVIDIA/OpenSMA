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

/*
 * USB buffer declarations for Core1 bare-metal.
 * Buffers and state are grouped per endpoint into structs.
 * Defined in usb_callbacks.cpp with DMA-aligned attributes.
 */
#ifndef SYS_NCSI_USB_BUFFERS_H
#define SYS_NCSI_USB_BUFFERS_H

#include "ecm_usb_descriptor.h"
#include "usb_device_config.h"

#ifdef __cplusplus
extern "C" {
#endif

struct usb_mctp_bufs
{
    USB_DMA_NONINIT_DATA_ALIGN(USB_DATA_ALIGN_SIZE)
    uint8_t rx_buffer[USB_MCTP_OUT_BUFFER_LENGTH];
    USB_DMA_NONINIT_DATA_ALIGN(USB_DATA_ALIGN_SIZE)
    uint8_t           tx_buffer[USB_MCTP_IN_BUFFER_LENGTH];
    volatile uint32_t rx_length;
};

#if USB_CONFIG_COMPOSITE
struct usb_hid_bufs
{
    USB_DMA_NONINIT_DATA_ALIGN(USB_DATA_ALIGN_SIZE)
    uint8_t rx_buffer[USB_HID_OUT_BUFFER_LENGTH];
    USB_DMA_NONINIT_DATA_ALIGN(USB_DATA_ALIGN_SIZE)
    uint8_t           tx_buffer[USB_HID_IN_BUFFER_LENGTH];
    volatile uint32_t rx_length;
};
#endif

#if USB_DEVICE_CONFIG_CDC_ACM
struct usb_acm_bufs
{
    USB_DMA_NONINIT_DATA_ALIGN(USB_DATA_ALIGN_SIZE)
    uint8_t rx_buffer[USB_ACM_OUT_BUFFER_LENGTH];
    USB_DMA_NONINIT_DATA_ALIGN(USB_DATA_ALIGN_SIZE)
    uint8_t           tx_buffer[USB_ACM_IN_BUFFER_LENGTH];
    volatile uint32_t rx_length;
    volatile uint8_t  rx_need_rearm;
    volatile uint8_t  tx_busy;
};
#endif

#if USB_CONFIG_LSTP
struct usb_lstp_bufs
{
    USB_DMA_NONINIT_DATA_ALIGN(USB_DATA_ALIGN_SIZE)
    uint8_t rx_buffer[USB_LSTP_OUT_BUFFER_LENGTH];
    USB_DMA_NONINIT_DATA_ALIGN(USB_DATA_ALIGN_SIZE)
    uint8_t           tx_buffer[USB_LSTP_IN_BUFFER_LENGTH];
    volatile uint32_t rx_length;
    volatile uint8_t  tx_pending;
    volatile uint32_t tx_length;
};
#endif

struct usb_mctp_bufs* get_usb_mctp_bufs(void);

#if USB_CONFIG_COMPOSITE
struct usb_hid_bufs* get_usb_hid_bufs(void);
#endif

#if USB_DEVICE_CONFIG_CDC_ACM
struct usb_acm_bufs* get_usb_acm_bufs(void);
#endif

#if USB_CONFIG_LSTP
struct usb_lstp_bufs* get_usb_lstp_bufs(void);
#endif

#ifdef __cplusplus
}
#endif

#endif /* SYS_NCSI_USB_BUFFERS_H */
