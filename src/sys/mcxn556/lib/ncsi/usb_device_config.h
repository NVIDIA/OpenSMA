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

#ifndef NV_ECM_USB_DEVICE_CONFIG_H
#define NV_ECM_USB_DEVICE_CONFIG_H

/*******************************************************************************
 * USB Device Controller Configuration
 ******************************************************************************/

// Controller type selection (only one should be enabled)
#define USB_DEVICE_CONFIG_KHCI                         0
#define USB_DEVICE_CONFIG_KHCI_DMA_ALIGN_BUFFER_LENGTH 64
#define USB_DEVICE_CONFIG_EHCI                         1
#define USB_DEVICE_CONFIG_EHCI_MAX_DTD                 16
#define USB_DEVICE_CONFIG_LPCIP3511FS                  0
#define USB_DEVICE_CONFIG_LPCIP3511HS                  0

#define USB_DEVICE_CONFIG_NUM                                                                  \
    (USB_DEVICE_CONFIG_KHCI + USB_DEVICE_CONFIG_EHCI + USB_DEVICE_CONFIG_LPCIP3511FS           \
     + USB_DEVICE_CONFIG_LPCIP3511HS)

/*******************************************************************************
 * USB Device Class Configuration
 * Composite device: MCTP + optional HID, CDC-ECM, LSTP, CDC-ACM
 * SDK class drivers: HID, CDC-ECM, CDC-ACM
 * Raw endpoints (no class driver): MCTP, LSTP
 ******************************************************************************/

// CDC-ECM for NC-SI - auto-derived from USB_CONFIG_ECM
// Use #ifndef so command-line -D flags take precedence
#ifndef USB_DEVICE_CONFIG_CDC_ECM
#if defined(USB_CONFIG_ECM) && (USB_CONFIG_ECM > 0U)
#define USB_DEVICE_CONFIG_CDC_ECM 1
#else
#define USB_DEVICE_CONFIG_CDC_ECM 0
#endif
#endif

// Enable HID class drivers (always on for ecm_bm)
#ifndef USB_DEVICE_CONFIG_HID
#define USB_DEVICE_CONFIG_HID 1
#endif

// CDC-ACM for USB UART Bridge - auto-derived from USB_CONFIG_UART_BRIDGE
// Use #ifndef so command-line -D flags take precedence
#ifndef USB_DEVICE_CONFIG_CDC_ACM
#if defined(USB_CONFIG_UART_BRIDGE) && (USB_CONFIG_UART_BRIDGE > 0U)
#define USB_DEVICE_CONFIG_CDC_ACM 1
#else
#define USB_DEVICE_CONFIG_CDC_ACM 0
#endif
#endif

// Disable other classes
#ifndef USB_DEVICE_CONFIG_AUDIO
#define USB_DEVICE_CONFIG_AUDIO 0
#endif
#ifndef USB_DEVICE_CONFIG_CDC_RNDIS
#define USB_DEVICE_CONFIG_CDC_RNDIS 0
#endif
#ifndef USB_DEVICE_CONFIG_PRINTER
#define USB_DEVICE_CONFIG_PRINTER 0
#endif
#ifndef USB_DEVICE_CONFIG_MSC
#define USB_DEVICE_CONFIG_MSC 0
#endif
#ifndef USB_DEVICE_CONFIG_CCID
#define USB_DEVICE_CONFIG_CCID 0
#endif
#ifndef USB_DEVICE_CONFIG_VIDEO
#define USB_DEVICE_CONFIG_VIDEO 0
#endif
#ifndef USB_DEVICE_CONFIG_PHDC
#define USB_DEVICE_CONFIG_PHDC 0
#endif
#ifndef USB_DEVICE_CONFIG_DFU
#define USB_DEVICE_CONFIG_DFU 0
#endif
#ifndef USB_DEVICE_CONFIG_MTP
#define USB_DEVICE_CONFIG_MTP 0
#endif

/*******************************************************************************
 * USB Device Common Configuration
 ******************************************************************************/

#define USB_DEVICE_CONFIG_SELF_POWER 1
#ifndef USB_DEVICE_CONFIG_USE_TASK
#define USB_DEVICE_CONFIG_USE_TASK 0
#endif
#define USB_DEVICE_CONFIG_MAX_MESSAGES  8
#define USB_DEVICE_CONFIG_DETACH_ENABLE 1
#define USB_DEVICE_CONFIG_ROOT2_TEST    1

/*******************************************************************************
 * USB Data Alignment
 ******************************************************************************/

#ifndef USB_DATA_ALIGN_SIZE
#define USB_DATA_ALIGN_SIZE 4U
#endif

#ifndef USB_DMA_INIT_DATA_ALIGN
#define USB_DMA_INIT_DATA_ALIGN(n) __attribute__((aligned(n)))
#endif

#ifndef USB_DMA_NONINIT_DATA_ALIGN
#define USB_DMA_NONINIT_DATA_ALIGN(n) __attribute__((aligned(n)))
#endif

#endif  // NV_ECM_USB_DEVICE_CONFIG_H
