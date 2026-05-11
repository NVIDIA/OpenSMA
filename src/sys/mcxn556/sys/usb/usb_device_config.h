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

// NOLINTBEGIN
#ifndef _USB_DEVICE_CONFIG_H_
#define _USB_DEVICE_CONFIG_H_

// All USB configuration (endpoints, classes, interfaces) calculated here
#include "usb_config_wrapper.h"

/* USB PHY configuration */
#define BOARD_USB_PHY_D_CAL     (0x04U)
#define BOARD_USB_PHY_TXCAL45DP (0x07U)
#define BOARD_USB_PHY_TXCAL45DM (0x07U)
#define BOARD_XTAL0_CLK_HZ      24000000U /*!< Board xtal0 frequency in Hz */

#if ((defined(USB_CONFIG_COMPOSITE)) && (USB_CONFIG_COMPOSITE > 0U))
#ifndef USB_DEVICE_CONFIG_HID
#define USB_DEVICE_CONFIG_HID (1U)
#endif
#if ((defined(USB_CONFIG_LSTP)) && (USB_CONFIG_LSTP > 0U))
#define USB_DEVICE_CONFIG_VENDOR_SPECIFIC (1U)
#endif
#endif
#if defined(USB_CONFIG_UART_BRIDGE) && (USB_CONFIG_UART_BRIDGE > 0U)
#ifndef USB_DEVICE_CONFIG_CDC_ACM
#define USB_DEVICE_CONFIG_CDC_ACM (1U)
#endif
#endif
#define USB_DEVICE_CONFIG_MCTP          (1U)
#define USB_DEVICE_CONFIG_KHCI          (0U)  // full speed usb driver (12Mbps)
#define USB_DEVICE_CONFIG_EHCI          (1U)  // high speed usb driver (480Mbps)
#define USB_DEVICE_CONFIG_LPCIP3511FS   (0U)
#define USB_DEVICE_CONFIG_LPCIP3511HS   (0U)
#define USB_DEVICE_CONFIG_REMOTE_WAKEUP (1U)

/*! @brief Device instance count, the sum of KHCI and EHCI instance counts*/
#define USB_DEVICE_CONFIG_NUM                                                                  \
    (USB_DEVICE_CONFIG_KHCI + USB_DEVICE_CONFIG_EHCI + USB_DEVICE_CONFIG_LPCIP3511FS           \
     + USB_DEVICE_CONFIG_LPCIP3511HS)

/*! @brief Whether device is self power. 1U supported, 0U not supported */
#define USB_DEVICE_CONFIG_SELF_POWER (1U)

/* USB_DEVICE_CONFIG_ENDPOINTS is defined in usb_config_wrapper.h */

#if ((defined(USB_DEVICE_CONFIG_EHCI)) && (USB_DEVICE_CONFIG_EHCI > 0U))
/*! @brief How many the DTD are supported. */
#define USB_DEVICE_CONFIG_EHCI_MAX_DTD (16U)
#endif

#if defined(USB_DEVICE_CONFIG_EHCI) && (USB_DEVICE_CONFIG_EHCI > 0U)
#ifndef CONTROLLER_ID
#define CONTROLLER_ID kUSB_ControllerEhci0
#endif
#endif
#if defined(USB_DEVICE_CONFIG_KHCI) && (USB_DEVICE_CONFIG_KHCI > 0U)
#ifndef CONTROLLER_ID
#define CONTROLLER_ID kUSB_ControllerKhci0
#endif
#endif
#if defined(USB_DEVICE_CONFIG_LPCIP3511FS) && (USB_DEVICE_CONFIG_LPCIP3511FS > 0U)
#ifndef CONTROLLER_ID
#define CONTROLLER_ID kUSB_ControllerLpcIp3511Fs0
#endif
#endif
#if defined(USB_DEVICE_CONFIG_LPCIP3511HS) && (USB_DEVICE_CONFIG_LPCIP3511HS > 0U)
#ifndef CONTROLLER_ID
#define CONTROLLER_ID kUSB_ControllerLpcIp3511Hs0
#endif
#endif

#endif /* _USB_DEVICE_CONFIG_H_ */
       // NOLINTEND
