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
#ifndef SYS_USB_USB_CONFIG_WRAPPER_H
#define SYS_USB_USB_CONFIG_WRAPPER_H

#include NV_IPC_CONFIG_H

/*
 * Core1 Bare-metal USB mode
 * When NCSI_ENABLE is defined, USB hardware is handled by Core1
 * Core0 uses usb_proxy task to forward MCTP/HID to Core1 via IPC
 */
#if NCSI_ENABLE
// USB handled by Core1 bare-metal - provide minimal stubs for Core0
// All USB classes/interfaces are disabled on Core0 when Core1 handles USB
#define SYS_USB_COMPOSITE_CLASS_COUNT     (0U)
#define SYS_USB_COMPOSITE_INTERFACE_COUNT (0U)
#define SYS_USB_COMPOSITE_CONFIGURE_INDEX (0U)
#ifndef USB_DEVICE_CONFIG_ENDPOINTS
#define USB_DEVICE_CONFIG_ENDPOINTS (0U)
#endif
#define SYS_USB_DISABLED (1U)
#else
#define SYS_USB_DISABLED       (0U)

/*
 * MCTP (base, always present)
 */
#define SYS_USB_MCTP_CLASS     (1U)
#define SYS_USB_MCTP_INTERFACE (1U)
#define SYS_USB_MCTP_ENDPOINTS (2U)

/*
 * HID (when USB_CONFIG_COMPOSITE defined)
 */
#if ((defined(USB_CONFIG_COMPOSITE)) && (USB_CONFIG_COMPOSITE > 0U))
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
#if ((defined(USB_CONFIG_LSTP)) && (USB_CONFIG_LSTP > 0U))
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
 * CDC-ECM (when USB_CONFIG_ECM defined)
 * CDC-ECM requires 2 interfaces (Comm + Data) and 3 endpoints (Notify + Bulk IN/OUT)
 */
#if ((defined(USB_CONFIG_ECM)) && (USB_CONFIG_ECM > 0U))
#define SYS_USB_ECM_CLASS     (1U)
#define SYS_USB_ECM_INTERFACE (2U)
#define SYS_USB_ECM_ENDPOINTS (3U)
#else
#define SYS_USB_ECM_CLASS     (0U)
#define SYS_USB_ECM_INTERFACE (0U)
#define SYS_USB_ECM_ENDPOINTS (0U)
#endif

/*
 * Totals = MCTP + HID + LSTP + ECM
 */
#define SYS_USB_COMPOSITE_CLASS_COUNT                                                          \
    (SYS_USB_MCTP_CLASS + SYS_USB_HID_CLASS + SYS_USB_LSTP_CLASS + SYS_USB_ECM_CLASS)
#define SYS_USB_COMPOSITE_INTERFACE_COUNT                                                      \
    (SYS_USB_MCTP_INTERFACE + SYS_USB_HID_INTERFACE + SYS_USB_LSTP_INTERFACE                   \
     + SYS_USB_ECM_INTERFACE)
/* USB_DEVICE_CONFIG_ENDPOINTS must be > highest EP number used by EHCI driver.
 * MCTP alone uses EP1 IN + EP2 OUT, so the floor is 3 (EP0, EP1, EP2). */
#ifndef USB_DEVICE_CONFIG_ENDPOINTS
#define SYS_USB_ENDPOINTS_SUM                                                                  \
    (SYS_USB_MCTP_ENDPOINTS + SYS_USB_HID_ENDPOINTS + SYS_USB_LSTP_ENDPOINTS                   \
     + SYS_USB_ECM_ENDPOINTS)
#if SYS_USB_ENDPOINTS_SUM < 3
#define USB_DEVICE_CONFIG_ENDPOINTS (3U)
#else
#define USB_DEVICE_CONFIG_ENDPOINTS SYS_USB_ENDPOINTS_SUM
#endif
#endif
#define SYS_USB_COMPOSITE_CONFIGURE_INDEX (1U)

#endif  // !NCSI_ENABLE

#endif  // SYS_USB_USB_CONFIG_WRAPPER_H
