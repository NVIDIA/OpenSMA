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
#include "usb_config_wrapper.h"

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

/* CDC VCOM class */
#define LINE_CODING_SIZE       (0x07)
#define LINE_CODING_DTERATE    (115200)
#define LINE_CODING_CHARFORMAT (0x00)
#define LINE_CODING_PARITYTYPE (0x00)
#define LINE_CODING_DATABITS   (0x08)

#define COMM_FEATURE_DATA_SIZE (0x02)
#define STATUS_ABSTRACT_STATE  (0x0000)
#define COUNTRY_SETTING        (0x0000)

#define NOTIF_PACKET_SIZE  (0x08)
#define UART_BITMAP_SIZE   (0x02)
#define NOTIF_REQUEST_TYPE (0xA1)

#define USB_CDC_VCOM_REPORT_DESCRIPTOR_LENGTH (33)
#define USB_IAD_DESC_SIZE                     (8)
#define USB_DESCRIPTOR_LENGTH_CDC_HEADER_FUNC (5)
#define USB_DESCRIPTOR_LENGTH_CDC_CALL_MANAG  (5)
#define USB_DESCRIPTOR_LENGTH_CDC_ABSTRACT    (4)
#define USB_DESCRIPTOR_LENGTH_CDC_UNION_FUNC  (5)

#define USB_CDC_VCOM_CIC_CLASS    (0x02)
#define USB_CDC_VCOM_CIC_SUBCLASS (0x02)
#define USB_CDC_VCOM_CIC_PROTOCOL (0x00)
#define USB_CDC_VCOM_DIC_CLASS    (0x0A)
#define USB_CDC_VCOM_DIC_SUBCLASS (0x00)
#define USB_CDC_VCOM_DIC_PROTOCOL (0x00)

#define USB_CDC_VCOM_INTERFACE_COUNT               (2)
#define USB_CDC_VCOM_CIC_INTERFACE_ALTERNATE_COUNT (1)
#define USB_CDC_VCOM_DIC_INTERFACE_ALTERNATE_COUNT (1)
#define USB_CDC_VCOM_CIC_INTERFACE_ALTERNATE_0     (0)
#define USB_CDC_VCOM_DIC_INTERFACE_ALTERNATE_0     (0)
#define USB_CDC_VCOM_CIC_ENDPOINT_COUNT            (1)
#define USB_CDC_VCOM_CIC_INTERRUPT_IN_ENDPOINT     (4)
#define USB_CDC_VCOM_DIC_ENDPOINT_COUNT            (2)

#if ((defined(USB_CONFIG_MCTP)) && (USB_CONFIG_MCTP > 0U))
#define USB_CDC_VCOM_CIC_INTERFACE_INDEX   (1)
#define USB_CDC_VCOM_DIC_INTERFACE_INDEX   (2)
#define USB_CDC_VCOM_DIC_BULK_IN_ENDPOINT  (3)
#define USB_CDC_VCOM_DIC_BULK_OUT_ENDPOINT (3)
#else  // USB_CONFIG_COMPOSITE
#define USB_CDC_VCOM_CIC_INTERFACE_INDEX   (2)
#define USB_CDC_VCOM_DIC_INTERFACE_INDEX   (3)
#define USB_CDC_VCOM_DIC_BULK_IN_ENDPOINT  (5)
#define USB_CDC_VCOM_DIC_BULK_OUT_ENDPOINT (5)
#endif

/* Communication Class SubClass Codes */
#define USB_CDC_DIRECT_LINE_CONTROL_MODEL         (0x01)
#define USB_CDC_ABSTRACT_CONTROL_MODEL            (0x02)
#define USB_CDC_TELEPHONE_CONTROL_MODEL           (0x03)
#define USB_CDC_MULTI_CHANNEL_CONTROL_MODEL       (0x04)
#define USB_CDC_CAPI_CONTROL_MOPDEL               (0x05)
#define USB_CDC_ETHERNET_NETWORKING_CONTROL_MODEL (0x06)
#define USB_CDC_ATM_NETWORKING_CONTROL_MODEL      (0x07)
#define USB_CDC_WIRELESS_HANDSET_CONTROL_MODEL    (0x08)
#define USB_CDC_DEVICE_MANAGEMENT                 (0x09)
#define USB_CDC_MOBILE_DIRECT_LINE_MODEL          (0x0A)
#define USB_CDC_OBEX                              (0x0B)
#define USB_CDC_ETHERNET_EMULATION_MODEL          (0x0C)

/* Communication Class Protocol Codes */
#define USB_CDC_NO_CLASS_SPECIFIC_PROTOCOL  (0x00) /*also for Data Class Protocol Code */
#define USB_CDC_AT_250_PROTOCOL             (0x01)
#define USB_CDC_AT_PCCA_101_PROTOCOL        (0x02)
#define USB_CDC_AT_PCCA_101_ANNEX_O         (0x03)
#define USB_CDC_AT_GSM_7_07                 (0x04)
#define USB_CDC_AT_3GPP_27_007              (0x05)
#define USB_CDC_AT_TIA_CDMA                 (0x06)
#define USB_CDC_ETHERNET_EMULATION_PROTOCOL (0x07)
#define USB_CDC_EXTERNAL_PROTOCOL           (0xFE)
#define USB_CDC_VENDOR_SPECIFIC             (0xFF) /*also for Data Class Protocol Code */

/* Data Class Protocol Codes */
#define USB_CDC_PYHSICAL_INTERFACE_PROTOCOL (0x30)
#define USB_CDC_HDLC_PROTOCOL               (0x31)
#define USB_CDC_TRANSPARENT_PROTOCOL        (0x32)
#define USB_CDC_MANAGEMENT_PROTOCOL         (0x50)
#define USB_CDC_DATA_LINK_Q931_PROTOCOL     (0x51)
#define USB_CDC_DATA_LINK_Q921_PROTOCOL     (0x52)
#define USB_CDC_DATA_COMPRESSION_V42BIS     (0x90)
#define USB_CDC_EURO_ISDN_PROTOCOL          (0x91)
#define USB_CDC_RATE_ADAPTION_ISDN_V24      (0x92)
#define USB_CDC_CAPI_COMMANDS               (0x93)
#define USB_CDC_HOST_BASED_DRIVER           (0xFD)
#define USB_CDC_UNIT_FUNCTIONAL             (0xFE)

/* Descriptor SubType in Communications Class Functional Descriptors */
#define USB_CDC_HEADER_FUNC_DESC              (0x00)
#define USB_CDC_CALL_MANAGEMENT_FUNC_DESC     (0x01)
#define USB_CDC_ABSTRACT_CONTROL_FUNC_DESC    (0x02)
#define USB_CDC_DIRECT_LINE_FUNC_DESC         (0x03)
#define USB_CDC_TELEPHONE_RINGER_FUNC_DESC    (0x04)
#define USB_CDC_TELEPHONE_REPORT_FUNC_DESC    (0x05)
#define USB_CDC_UNION_FUNC_DESC               (0x06)
#define USB_CDC_COUNTRY_SELECT_FUNC_DESC      (0x07)
#define USB_CDC_TELEPHONE_MODES_FUNC_DESC     (0x08)
#define USB_CDC_TERMINAL_FUNC_DESC            (0x09)
#define USB_CDC_NETWORK_CHANNEL_FUNC_DESC     (0x0A)
#define USB_CDC_PROTOCOL_UNIT_FUNC_DESC       (0x0B)
#define USB_CDC_EXTENSION_UNIT_FUNC_DESC      (0x0C)
#define USB_CDC_MULTI_CHANNEL_FUNC_DESC       (0x0D)
#define USB_CDC_CAPI_CONTROL_FUNC_DESC        (0x0E)
#define USB_CDC_ETHERNET_NETWORKING_FUNC_DESC (0x0F)
#define USB_CDC_ATM_NETWORKING_FUNC_DESC      (0x10)
#define USB_CDC_WIRELESS_CONTROL_FUNC_DESC    (0x11)
#define USB_CDC_MOBILE_DIRECT_LINE_FUNC_DESC  (0x12)
#define USB_CDC_MDLM_DETAIL_FUNC_DESC         (0x13)
#define USB_CDC_DEVICE_MANAGEMENT_FUNC_DESC   (0x14)
#define USB_CDC_OBEX_FUNC_DESC                (0x15)
#define USB_CDC_COMMAND_SET_FUNC_DESC         (0x16)
#define USB_CDC_COMMAND_SET_DETAIL_FUNC_DESC  (0x17)
#define USB_CDC_TELEPHONE_CONTROL_FUNC_DESC   (0x18)
#define USB_CDC_OBEX_SERVICE_ID_FUNC_DESC     (0x19)

/* Packet size. */
#define HS_CDC_VCOM_INTERRUPT_IN_PACKET_SIZE (16)
#define FS_CDC_VCOM_INTERRUPT_IN_PACKET_SIZE (16)
#define HS_CDC_VCOM_INTERRUPT_IN_INTERVAL    (0x07) /* 2^(7-1) = 8ms */
#define FS_CDC_VCOM_INTERRUPT_IN_INTERVAL    (0x08)

#define HS_CDC_VCOM_BULK_IN_PACKET_SIZE  (512)
#define FS_CDC_VCOM_BULK_IN_PACKET_SIZE  (64)
#define HS_CDC_VCOM_BULK_OUT_PACKET_SIZE (512)
#define FS_CDC_VCOM_BULK_OUT_PACKET_SIZE (64)

#define USB_DESCRIPTOR_TYPE_CDC_CS_INTERFACE (0x24)
#define USB_DESCRIPTOR_TYPE_CDC_CS_ENDPOINT  (0x25)

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
#define USB_HID_GENERIC_INTERFACE_INDEX   (USB_MCTP_INTERFACE_COUNT)
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

/* NV_SMA_SPI class */
#define USB_NV_SMA_SPI_INTERFACE_COUNT           (1U)
#define USB_NV_SMA_SPI_INTERFACE_INDEX           (USB_MCTP_INTERFACE_COUNT + SYS_USB_HID_CP2112)
#define USB_NV_SMA_SPI_ENDPOINT_COUNT            (2U)
#define USB_NV_SMA_SPI_ENDPOINT_IN               (2U)
#define USB_NV_SMA_SPI_ENDPOINT_OUT              (1U)
#define USB_NV_SMA_SPI_INTERFACE_ALTERNATE_COUNT (1U)
#define USB_NV_SMA_SPI_INTERFACE_ALTERNATE_0     (0U)

#define USB_NV_SMA_SPI_CLASS    (0xFFU)  // vendor specific class
#define USB_NV_SMA_SPI_SUBCLASS (0x3fU)
#define USB_NV_SMA_SPI_PROTOCOL (0x01U)

#define USB_NV_SMA_SPI_IN_BUFFER_LENGTH  (512U)
#define USB_NV_SMA_SPI_OUT_BUFFER_LENGTH (512U)

#define HS_NV_SMA_SPI_CLASS_IN_PACKET_SIZE  (512U)
#define HS_NV_SMA_SPI_CLASS_OUT_PACKET_SIZE (512U)
#define FS_NV_SMA_SPI_CLASS_IN_PACKET_SIZE  (512U)
#define FS_NV_SMA_SPI_CLASS_OUT_PACKET_SIZE (512U)

#define HS_NV_SMA_SPI_CLASS_IN_INTERVAL  (0x01U)
#define HS_NV_SMA_SPI_CLASS_OUT_INTERVAL (0x01U)
#define FS_NV_SMA_SPI_CLASS_IN_INTERVAL  (0x01U)
#define FS_NV_SMA_SPI_CLASS_OUT_INTERVAL (0x01U)

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

#if (SYS_USB_HID_CP2112 > 0U)
usb_status_t USB_DeviceGetHidDescriptor(usb_device_handle                       handle,
                                        usb_device_get_hid_descriptor_struct_t* hidDescriptor);

usb_status_t USB_DeviceGetHidReportDescriptor(
    usb_device_handle                              handle,
    usb_device_get_hid_report_descriptor_struct_t* hidReportDescriptor);

usb_status_t USB_DeviceGetHidPhysicalDescriptor(
    usb_device_handle                                handle,
    usb_device_get_hid_physical_descriptor_struct_t* hidPhysicalDescriptor);
#endif /* SYS_USB_HID_CP2112 */

#ifdef __cplusplus
}
#endif

#endif /* __USB_DEVICE_DESCRIPTOR_H__ */

// NOLINTEND