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
#ifndef SYS_USB_USB_WRAPPER_H
#define SYS_USB_USB_WRAPPER_H

#include NV_IPC_CONFIG_H

#include "usb_device_descriptor.h"

#if ((defined(USB_CONFIG_MCTP)) && (USB_CONFIG_MCTP > 0U))
#define SYS_USB_COMPOSITE_CLASS_COUNT     (1U)
#define SYS_USB_COMPOSITE_INTERFACE_COUNT (1U)
#define SYS_USB_COMPOSITE_CONFIGURE_INDEX (1U)
#elif ((defined(USB_CONFIG_COMPOSITE)) && (USB_CONFIG_COMPOSITE > 0U))
#if ((defined(USB_CONFIG_LSTP)) && (USB_CONFIG_LSTP > 0U))
#define USB_DEVICE_CONFIG_VENDOR_SPECIFIC (1U)
#define SYS_USB_COMPOSITE_CLASS_COUNT     (3U)
#define SYS_USB_COMPOSITE_INTERFACE_COUNT (3U)
#else
#define SYS_USB_COMPOSITE_CLASS_COUNT     (2U)
#define SYS_USB_COMPOSITE_INTERFACE_COUNT (2U)
#endif
#define SYS_USB_COMPOSITE_CONFIGURE_INDEX (1U)
#else
#error USB class config error!
#endif

#endif  // SYS_USB_USB_WRAPPER_H