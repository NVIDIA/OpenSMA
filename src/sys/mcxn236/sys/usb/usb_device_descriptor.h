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

#include NV_IPC_CONFIG_H

#include "usb_device.h"
#include "usb_device_class.h"
#include "usb_device_config.h"

/* MCTP descriptor */
#define USB_DEVICE_SPECIFIC_BCD_VERSION (0x0200U)
#define USB_DEVICE_DEMO_BCD_VERSION     (0x0200U)
#define USB_DEVICE_MAX_POWER            (0x32U)

#define USB_DESCRIPTOR_LENGTH_CONFIGURATION_ALL (sizeof(g_UsbDeviceConfigurationDescriptor))
#define USB_DESCRIPTOR_LENGTH_MCTP_REPORT       (sizeof(g_UsbDeviceMctpReportDescriptor))

#define USB_DEVICE_CONFIGURATION_COUNT (1U)
#define USB_DEVICE_STRING_COUNT        (1U)
#define USB_DEVICE_LANGUAGE_COUNT      (1U)

#define USB_MCTP_IN_BUFFER_LENGTH  (512U)
#define USB_MCTP_OUT_BUFFER_LENGTH (512U)
#define USB_MCTP_CONFIGURE_INDEX   (1U)
#define USB_MCTP_INTERFACE_COUNT   (1U)
#define USB_MCTP_INTERFACE_INDEX   (0U)
#define USB_MCTP_ENDPOINT_COUNT    (2U)
#define USB_MCTP_ENDPOINT_IN       (1U)
#define USB_MCTP_ENDPOINT_OUT      (2U)

#define USB_MCTP_INTERFACE_ALTERNATE_COUNT (1U)
#define USB_MCTP_INTERFACE_ALTERNATE_0     (0U)

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

/* COMPOSTIE class */
#define USB_COMPOSTIE_CLASS    (0xEFU)  // (Miscellaneous Device Class)
#define USB_COMPOSTIE_SUBCLASS (0x02U)  // (Common Class)
#define USB_COMPOSTIE_PROTOCOL (0x01U)  // IAD (Interface Association Descriptor) Protocol

/* HID class */
#define USB_DEVICE_CLASS    (0x00U)
#define USB_DEVICE_SUBCLASS (0x00U)
#define USB_DEVICE_PROTOCOL (0x00U)

#define USB_DESCRIPTOR_HID_REPORT (sizeof(g_UsbDeviceHidGenericReportDescriptor))
#define USB_DESCRIPTOR_LENGTH_HID (9U)

#define USB_HID_GENERIC_IN_BUFFER_LENGTH  (64U)
#define USB_HID_GENERIC_OUT_BUFFER_LENGTH (64U)
#define USB_HID_GENERIC_INTERFACE_COUNT   (1U)
#define USB_HID_GENERIC_INTERFACE_INDEX   (1U)
#define USB_HID_GENERIC_ENDPOINT_COUNT    (2U)
#define USB_HID_GENERIC_ENDPOINT_IN       (3U)
#define USB_HID_GENERIC_ENDPOINT_OUT      (3U)

#define USB_HID_GENERIC_INTERFACE_ALTERNATE_COUNT (1U)
#define USB_HID_GENERIC_INTERFACE_ALTERNATE_0     (0U)

#define USB_HID_GENERIC_CLASS    (0x03U)
#define USB_HID_GENERIC_SUBCLASS (0x00U)
#define USB_HID_GENERIC_PROTOCOL (0x00U)

#define HS_HID_GENERIC_INTERRUPT_OUT_PACKET_SIZE (64U)
#define FS_HID_GENERIC_INTERRUPT_OUT_PACKET_SIZE (64U)
#define HS_HID_GENERIC_INTERRUPT_OUT_INTERVAL    (0x04U) /* 2^(4-1) = 1ms */
#define FS_HID_GENERIC_INTERRUPT_OUT_INTERVAL    (0x01U)

#define HS_HID_GENERIC_INTERRUPT_IN_PACKET_SIZE (64U)
#define FS_HID_GENERIC_INTERRUPT_IN_PACKET_SIZE (64U)
#define HS_HID_GENERIC_INTERRUPT_IN_INTERVAL    (0x04U) /* 2^(4-1) = 1ms */
#define FS_HID_GENERIC_INTERRUPT_IN_INTERVAL    (0x01U)

#ifdef __cplusplus
extern "C" {
#endif

extern usb_status_t USB_DeviceSetSpeed(usb_device_handle handle, uint8_t speed);

usb_status_t
USB_DeviceGetDeviceDescriptor(usb_device_handle                          handle,
                              usb_device_get_device_descriptor_struct_t* deviceDescriptor);

usb_status_t USB_DeviceGetConfigurationDescriptor(
    usb_device_handle                                 handle,
    usb_device_get_configuration_descriptor_struct_t* configurationDescriptor);

usb_status_t
USB_DeviceGetStringDescriptor(usb_device_handle                          handle,
                              usb_device_get_string_descriptor_struct_t* stringDescriptor);

usb_status_t USB_DeviceGetHidDescriptor(usb_device_handle                       handle,
                                        usb_device_get_hid_descriptor_struct_t* hidDescriptor);

usb_status_t USB_DeviceGetHidReportDescriptor(
    usb_device_handle                              handle,
    usb_device_get_hid_report_descriptor_struct_t* hidReportDescriptor);

usb_status_t USB_DeviceGetHidPhysicalDescriptor(
    usb_device_handle                                handle,
    usb_device_get_hid_physical_descriptor_struct_t* hidPhysicalDescriptor);

#ifdef __cplusplus
}
#endif

#endif /* __USB_DEVICE_DESCRIPTOR_H__ */

// NOLINTEND