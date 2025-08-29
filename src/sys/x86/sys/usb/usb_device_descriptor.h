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

#ifndef __USB_DEVICE_DESCRIPTOR_H__
#define __USB_DEVICE_DESCRIPTOR_H__

#ifdef __cplusplus
extern "C" {
#endif

/* MCTP descriptor */
#define USB_DEVICE_SPECIFIC_BCD_VERSION (0x0200U)
#define USB_DEVICE_DEMO_BCD_VERSION     (0x0101U)
#define USB_DEVICE_MAX_POWER            (0x32U)

#define USB_DESCRIPTOR_LENGTH_CONFIGURATION_ALL (sizeof(g_UsbDeviceConfigurationDescriptor))
#define USB_DESCRIPTOR_LENGTH_MCTP_REPORT       (sizeof(g_UsbDeviceMctpReportDescriptor))
#define USB_DESCRIPTOR_LENGTH_STRING0           (sizeof(g_UsbDeviceString0))
#define USB_DESCRIPTOR_LENGTH_STRING1           (sizeof(g_UsbDeviceString1))
#define USB_DESCRIPTOR_LENGTH_STRING2           (sizeof(g_UsbDeviceString2))

#define USB_DEVICE_CONFIGURATION_COUNT (1U)
#define USB_DEVICE_STRING_COUNT        (4U)
#define USB_DEVICE_LANGUAGE_COUNT      (1U)

#define USB_MCTP_CONFIGURE_INDEX (1U)
#define USB_MCTP_INTERFACE_COUNT (1U)

#define USB_MCTP_IN_BUFFER_LENGTH  (512U)
#define USB_MCTP_OUT_BUFFER_LENGTH (512U)
#define USB_MCTP_ENDPOINT_COUNT    (2U)
#define USB_MCTP_INTERFACE_INDEX   (0U)
#define USB_MCTP_ENDPOINT_IN       (1U)
#define USB_MCTP_ENDPOINT_OUT      (2U)

#define USB_MCTP_INTERFACE_ALTERNATE_COUNT (1U)
#define USB_MCTP_INTERFACE_ALTERNATE_0     (0U)

#define USB_DEVICE_VID (0x0955U)
#define USB_DEVICE_PID (0xCF10U)

/* MCTP class */
#define USB_MCTP_GENERIC_CLASS    (0x14U)
#define USB_MCTP_GENERIC_SUBCLASS (0x00U)
#define USB_MCTP_GENERIC_PROTOCOL (0x01U)

#define HS_MCTP_CLASS_IN_PACKET_SIZE  (512U)
#define HS_MCTP_CLASS_OUT_PACKET_SIZE (512U)
#define FS_MCTP_CLASS_IN_PACKET_SIZE  (512U)
#define FS_MCTP_CLASS_OUT_PACKET_SIZE (512U)

#define HS_MCTP_CLASS_IN_INTERVAL  (0x01U)
#define HS_MCTP_CLASS_OUT_INTERVAL (0x01U)
#define FS_MCTP_CLASS_IN_INTERVAL  (0x01U)
#define FS_MCTP_CLASS_OUT_INTERVAL (0x01U)

#ifdef __cplusplus
}
#endif

#endif /* __USB_DEVICE_DESCRIPTOR_H__ */

// NOLINTEND