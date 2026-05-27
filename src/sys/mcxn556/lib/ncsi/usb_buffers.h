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
    volatile uint8_t  tx_busy;
};

#if USB_CONFIG_COMPOSITE
struct usb_hid_bufs
{
    USB_DMA_NONINIT_DATA_ALIGN(USB_DATA_ALIGN_SIZE)
    uint8_t rx_buffer[USB_HID_OUT_BUFFER_LENGTH];
    USB_DMA_NONINIT_DATA_ALIGN(USB_DATA_ALIGN_SIZE)
    uint8_t           tx_buffer[USB_HID_IN_BUFFER_LENGTH];
    volatile uint32_t rx_length;
    volatile uint8_t  tx_busy;
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
static_assert(nv::lstp::LstpNumChannels <= 32,
              "Core1 LSTP supports up to 32 channels, extend tx_pending_mask");

#define USB_SET_LSTP_TX_PENDING(bufs, channel_id) (bufs.tx_pending_mask |= (1U << channel_id))
#define USB_CLEAR_LSTP_TX_PENDING(bufs, channel_id)                                            \
    (bufs.tx_pending_mask &= ~(1U << channel_id))
#define USB_IS_LSTP_TX_PENDING(bufs, channel_id) (bufs.tx_pending_mask & (1U << channel_id))

#define USB_CLEAR_LSTP_TX_BUSY(bufs) (bufs.tx_busy_ch_id = nv::lstp::LstpNumChannels)
#define USB_IS_LSTP_TX_BUSY(bufs)    (bufs.tx_busy_ch_id != nv::lstp::LstpNumChannels)

struct usb_lstp_tx_bufs
{
    USB_DMA_NONINIT_DATA_ALIGN(USB_DATA_ALIGN_SIZE)
    uint8_t           buffer[USB_LSTP_IN_BUFFER_LENGTH];
    volatile uint32_t length;
};

struct usb_lstp_bufs
{
    USB_DMA_NONINIT_DATA_ALIGN(USB_DATA_ALIGN_SIZE)
    uint8_t           rx_buffer[USB_LSTP_OUT_BUFFER_LENGTH];
    volatile uint32_t rx_length;
    volatile uint32_t tx_pending_mask;
    usb_lstp_tx_bufs  tx_buffers[nv::lstp::LstpNumChannels];
    volatile uint8_t  tx_busy_ch_id;
    uint8_t           tx_arb_cursor;
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
