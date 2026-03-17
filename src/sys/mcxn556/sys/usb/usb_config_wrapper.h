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

/**
 * @file usb_config_wrapper.h
 * @brief USB configuration calculation (class, interface, endpoint counts)
 *
 * User defines in config.h:
 *   USB_CONFIG_COMPOSITE   - Add HID      (+1C, +1I, +2E)
 *   USB_CONFIG_LSTP        - Add LSTP     (+1C, +1I, +2E)
 *   USB_CONFIG_UART_BRIDGE - Add CDC ACM  (+1C, +2I, +3E)
 *
 * MCTP is always included as base (+1C, +1I, +2E)
 */

#ifndef SYS_USB_USB_WRAPPER_H
#define SYS_USB_USB_WRAPPER_H

#include NV_IPC_CONFIG_H

/*
 * MCTP (base, always present)
 */
#define SYS_USB_MCTP_CLASS     (1U)
#define SYS_USB_MCTP_INTERFACE (1U)
#define SYS_USB_MCTP_ENDPOINTS (2U)

/*
 * HID (when USB_CONFIG_COMPOSITE defined)
 */
#ifdef USB_CONFIG_COMPOSITE
#define SYS_USB_HID_CLASS     (1U)
#define SYS_USB_HID_INTERFACE (1U)
#define SYS_USB_HID_ENDPOINTS (2U)
#else
#define SYS_USB_HID_CLASS     (0U)
#define SYS_USB_HID_INTERFACE (0U)
#define SYS_USB_HID_ENDPOINTS (0U)
#endif

/*
 * LSTP (when USB_CONFIG_LSTP defined)
 */
#ifdef USB_CONFIG_LSTP
#define USB_DEVICE_CONFIG_VENDOR_SPECIFIC (1U)
#define SYS_USB_LSTP_CLASS                (1U)
#define SYS_USB_LSTP_INTERFACE            (1U)
#define SYS_USB_LSTP_ENDPOINTS            (2U)
#else
#define SYS_USB_LSTP_CLASS     (0U)
#define SYS_USB_LSTP_INTERFACE (0U)
#define SYS_USB_LSTP_ENDPOINTS (0U)
#endif

/*
 * VCOM (when USB_CONFIG_UART_BRIDGE defined)
 */
#ifdef USB_CONFIG_UART_BRIDGE
#define SYS_USB_VCOM_CLASS     (1U)
#define SYS_USB_VCOM_INTERFACE (2U)
#define SYS_USB_VCOM_ENDPOINTS (3U)
#else
#define SYS_USB_VCOM_CLASS     (0U)
#define SYS_USB_VCOM_INTERFACE (0U)
#define SYS_USB_VCOM_ENDPOINTS (0U)
#endif

/*
 * Totals = MCTP + HID + SPI + VCOM
 */
#define SYS_USB_COMPOSITE_CLASS_COUNT                                                          \
    (SYS_USB_MCTP_CLASS + SYS_USB_HID_CLASS + SYS_USB_LSTP_CLASS + SYS_USB_VCOM_CLASS)
#define SYS_USB_COMPOSITE_INTERFACE_COUNT                                                      \
    (SYS_USB_MCTP_INTERFACE + SYS_USB_HID_INTERFACE + SYS_USB_LSTP_INTERFACE                   \
     + SYS_USB_VCOM_INTERFACE)
#define SYS_USB_ENDPOINTS_SUM                                                                  \
    (SYS_USB_MCTP_ENDPOINTS + SYS_USB_HID_ENDPOINTS + SYS_USB_LSTP_ENDPOINTS                   \
     + SYS_USB_VCOM_ENDPOINTS)
#define SYS_USB_COMPOSITE_CONFIGURE_INDEX (1U)

/* USB_DEVICE_CONFIG_ENDPOINTS was hardcoded to 4 before uart-bridge.
 * Keep minimum 4 for non-bridge configs to stay compatible with main. */
#if SYS_USB_ENDPOINTS_SUM < 4
#define USB_DEVICE_CONFIG_ENDPOINTS (4U)
#else
#define USB_DEVICE_CONFIG_ENDPOINTS SYS_USB_ENDPOINTS_SUM
#endif

#endif  // SYS_USB_USB_WRAPPER_H