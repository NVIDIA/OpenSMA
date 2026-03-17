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

#include "usb_device_descriptor.h"

#include "nv/common/preproc.h"

// NOLINTBEGIN
using namespace nv;
using namespace ipc;

#if defined(USB_CONFIG_UART_BRIDGE)
/* cdc virtual com information */
/* Define endpoint for communication class */
NV_SHARED_DATA usb_device_endpoint_struct_t
    g_cdcVcomCicEndpoints[USB_CDC_VCOM_CIC_ENDPOINT_COUNT] = {
        {
         USB_CDC_VCOM_CIC_INTERRUPT_IN_ENDPOINT | (USB_IN << 7U),
         USB_ENDPOINT_INTERRUPT, FS_CDC_VCOM_INTERRUPT_IN_PACKET_SIZE,
         FS_CDC_VCOM_INTERRUPT_IN_INTERVAL, },
};

/* Define endpoint for data class */
NV_SHARED_DATA usb_device_endpoint_struct_t
    g_cdcVcomDicEndpoints[USB_CDC_VCOM_DIC_ENDPOINT_COUNT] = {
        {
         USB_CDC_VCOM_DIC_BULK_IN_ENDPOINT | (USB_IN << 7U),
         USB_ENDPOINT_BULK,  FS_CDC_VCOM_BULK_IN_PACKET_SIZE,
         0U, },
        {
         USB_CDC_VCOM_DIC_BULK_OUT_ENDPOINT | (USB_OUT << 7U),
         USB_ENDPOINT_BULK, FS_CDC_VCOM_BULK_OUT_PACKET_SIZE,
         0U, },
};

/* Define interface for communication class */
NV_SHARED_DATA usb_device_interface_struct_t g_cdcVcomCicInterface[] = {
    {USB_CDC_VCOM_CIC_INTERFACE_ALTERNATE_0,
     {
         USB_CDC_VCOM_CIC_ENDPOINT_COUNT,
         g_cdcVcomCicEndpoints,
     }, NULL}
};

/* Define interface for data class */
NV_SHARED_DATA usb_device_interface_struct_t g_cdcVcomDicInterface[] = {
    {USB_CDC_VCOM_DIC_INTERFACE_ALTERNATE_0,
     {
         USB_CDC_VCOM_DIC_ENDPOINT_COUNT,
         g_cdcVcomDicEndpoints,
     }, NULL}
};

/* Define interfaces for virtual com */
NV_SHARED_DATA usb_device_interfaces_struct_t
    g_cdcVcomInterfaces[USB_CDC_VCOM_INTERFACE_COUNT] = {
        {USB_CDC_VCOM_CIC_CLASS,
         USB_CDC_VCOM_CIC_SUBCLASS, USB_CDC_VCOM_CIC_PROTOCOL,
         USB_CDC_VCOM_CIC_INTERFACE_INDEX, g_cdcVcomCicInterface,
         sizeof(g_cdcVcomCicInterface) / sizeof(usb_device_interface_struct_t)},
        {USB_CDC_VCOM_DIC_CLASS,
         USB_CDC_VCOM_DIC_SUBCLASS, USB_CDC_VCOM_DIC_PROTOCOL,
         USB_CDC_VCOM_DIC_INTERFACE_INDEX, g_cdcVcomDicInterface,
         sizeof(g_cdcVcomDicInterface) / sizeof(usb_device_interface_struct_t)},
};

/* Define configurations for virtual com */
NV_SHARED_DATA usb_device_interface_list_t
    g_UsbDeviceCdcVcomInterfaceList[USB_DEVICE_CONFIGURATION_COUNT] = {
        {
         USB_CDC_VCOM_INTERFACE_COUNT, g_cdcVcomInterfaces,
         },
};

/* Define class information for virtual com */
NV_SHARED_DATA usb_device_class_struct_t g_UsbDeviceCdcVcomConfig = {
    g_UsbDeviceCdcVcomInterfaceList,
    kUSB_DeviceClassTypeCdc,
    USB_DEVICE_CONFIGURATION_COUNT,
};

/* VCOM descriptor length */
#define USB_DESCRIPTOR_LENGTH_VCOM                                                             \
    (USB_IAD_DESC_SIZE + USB_DESCRIPTOR_LENGTH_INTERFACE                                       \
     + USB_DESCRIPTOR_LENGTH_CDC_HEADER_FUNC + USB_DESCRIPTOR_LENGTH_CDC_CALL_MANAG            \
     + USB_DESCRIPTOR_LENGTH_CDC_ABSTRACT + USB_DESCRIPTOR_LENGTH_CDC_UNION_FUNC               \
     + USB_DESCRIPTOR_LENGTH_ENDPOINT + USB_DESCRIPTOR_LENGTH_INTERFACE                        \
     + USB_DESCRIPTOR_LENGTH_ENDPOINT + USB_DESCRIPTOR_LENGTH_ENDPOINT)
#else
#define USB_DESCRIPTOR_LENGTH_VCOM (0U)
#endif  // defined(USB_CONFIG_UART_BRIDGE)

/* MCTP class endpoint information */
NV_SHARED_DATA usb_device_endpoint_struct_t
    g_UsbDeviceMctpEndpoints[USB_MCTP_ENDPOINT_COUNT] = {
        {
         USB_MCTP_ENDPOINT_IN | (USB_IN << USB_DESCRIPTOR_ENDPOINT_ADDRESS_DIRECTION_SHIFT),
         USB_ENDPOINT_BULK,  FS_MCTP_CLASS_IN_PACKET_SIZE,
         FS_MCTP_CLASS_IN_INTERVAL, },
        {
         USB_MCTP_ENDPOINT_OUT
         | (USB_OUT << USB_DESCRIPTOR_ENDPOINT_ADDRESS_DIRECTION_SHIFT),
         USB_ENDPOINT_BULK, FS_MCTP_CLASS_OUT_PACKET_SIZE,
         FS_MCTP_CLASS_OUT_INTERVAL, }
};

/* MCTP class interface information */
NV_SHARED_DATA usb_device_interface_struct_t
    g_UsbDeviceMctpInterface[USB_MCTP_INTERFACE_COUNT] = {
        {
         USB_MCTP_INTERFACE_ALTERNATE_0, {
                USB_MCTP_ENDPOINT_COUNT,
                g_UsbDeviceMctpEndpoints,
            }, NULL,
         }
};

NV_SHARED_DATA usb_device_interfaces_struct_t
    g_UsbDeviceMctpInterfaces[USB_MCTP_INTERFACE_COUNT] = {
        {
         USB_MCTP_GENERIC_CLASS, USB_MCTP_GENERIC_SUBCLASS,
         USB_MCTP_GENERIC_PROTOCOL, USB_MCTP_INTERFACE_INDEX, /* The interface number of the MCTP */
            g_UsbDeviceMctpInterface, /* Interfaces handle */
            sizeof(g_UsbDeviceMctpInterface) / sizeof(usb_device_interface_struct_t),
         },
};

NV_SHARED_DATA usb_device_interface_list_t
    g_UsbDeviceMctpInterfaceList[USB_DEVICE_CONFIGURATION_COUNT] = {
        {
         USB_MCTP_INTERFACE_COUNT,  /* The interface count of the MCTP */
            g_UsbDeviceMctpInterfaces, /* The interfaces handle */
        },
};

NV_SHARED_DATA usb_device_class_struct_t g_UsbDeviceMctpGenericConfig = {
    g_UsbDeviceMctpInterfaceList,                       /* The interface list of the MCTP */
    (_usb_usb_device_class_type)USB_MCTP_GENERIC_CLASS, /* The MCTP class type */
    USB_DEVICE_CONFIGURATION_COUNT,                     /* The configuration count */
};

#if defined(USB_CONFIG_MCTP)
USB_DMA_INIT_DATA_ALIGN(USB_DATA_ALIGN_SIZE)
NV_SHARED_DATA uint8_t g_UsbDeviceDescriptor[] = {

    USB_DESCRIPTOR_LENGTH_DEVICE,  // bLength: Size of this descriptor in bytes
    USB_DESCRIPTOR_TYPE_DEVICE,    // bDescriptorType: DEVICE
    USB_SHORT_GET_LOW(USB_DEVICE_SPECIFIC_BCD_VERSION),
    USB_SHORT_GET_HIGH(USB_DEVICE_SPECIFIC_BCD_VERSION), /* bcdUSB: USB specification
                                                            version USB Specification
                                                            Release Number in Binary-Coded
                                                            Decimal (i.e., 2.10 is 210H).
                                                          */

#if !defined(USB_CONFIG_UART_BRIDGE)
    USB_MCTP_GENERIC_CLASS,     // bDeviceClass: Device class (0 for each interface
                                // defines class)
    USB_MCTP_GENERIC_SUBCLASS,  // bDeviceSubClass: Device subclass
    USB_MCTP_GENERIC_PROTOCOL,  // bDeviceProtocol: Device protocol
    0x40,                       // bMaxPacketSize0: Max packet size for endpoint 0 (64 bytes)
#else
    USB_COMPOSTIE_CLASS,          // bDeviceClass: Class code
    USB_COMPOSTIE_SUBCLASS,       // bDeviceSubClass: Subclass code
    USB_COMPOSTIE_PROTOCOL,       // bDeviceProtocol: Protocol code
    USB_CONTROL_MAX_PACKET_SIZE,  // bMaxPacketSize0: Maximum packet size for
                                  // endpoint zero
                                  //                  (only 8, 16, 32, or 64 are
                                  //                  valid)
#endif

    USB_SHORT_GET_LOW(UsbDeviceVid),
    USB_SHORT_GET_HIGH(UsbDeviceVid), /* Vendor ID
                                         (assigned by the
                                         USB-IF) */
    USB_SHORT_GET_LOW(UsbDevicePid),
    USB_SHORT_GET_HIGH(UsbDevicePid), /* Product ID
                                         (assigned by the
                                         manufacturer) */
    USB_SHORT_GET_LOW(USB_DEVICE_DEMO_BCD_VERSION),
    USB_SHORT_GET_HIGH(USB_DEVICE_DEMO_BCD_VERSION),  // bcdDevice: Device
                                                      // release number (2.00)
    0x00U,  // iManufacturer: Index of string descriptor describing manufacturer
    0x00U,  // iProduct: Index of string descriptor describing product
    0x00U,  // iSerialNumber: Index of string descriptor describing the device's
            // serial number

    USB_DEVICE_CONFIGURATION_COUNT,  // bNumConfigurations: Number of possible
                                     // configurations
};

USB_DMA_INIT_DATA_ALIGN(USB_DATA_ALIGN_SIZE)
NV_SHARED_DATA uint8_t g_UsbDeviceConfigurationDescriptor[] = {

    /* Size of this descriptor in bytes */
    USB_DESCRIPTOR_LENGTH_CONFIGURE,
    /* CONFIGURATION Descriptor Type */
    USB_DESCRIPTOR_TYPE_CONFIGURE,
    /* Total length of data returned for this configuration. */
    USB_SHORT_GET_LOW(USB_DESCRIPTOR_LENGTH_CONFIGURE + USB_DESCRIPTOR_LENGTH_INTERFACE
                      + USB_DESCRIPTOR_LENGTH_ENDPOINT + USB_DESCRIPTOR_LENGTH_ENDPOINT
                      + USB_DESCRIPTOR_LENGTH_VCOM),
    USB_SHORT_GET_HIGH(USB_DESCRIPTOR_LENGTH_CONFIGURE + USB_DESCRIPTOR_LENGTH_INTERFACE
                       + USB_DESCRIPTOR_LENGTH_ENDPOINT + USB_DESCRIPTOR_LENGTH_ENDPOINT
                       + USB_DESCRIPTOR_LENGTH_VCOM),
    SYS_USB_COMPOSITE_INTERFACE_COUNT,  // bNumInterfaces: Number of interfaces
                                        // supported by this configuration
    SYS_USB_COMPOSITE_CONFIGURE_INDEX,  // bConfigurationValue: Value to use in
                                        // SetConfiguration request
    /* Index of string descriptor describing this configuration */
    0x00,
    /* Configuration characteristics D7: Reserved (set to one) D6: Self-powered
       D5: Remote Wakeup D4...0: Reserved (reset to zero) */
    (USB_DESCRIPTOR_CONFIGURE_ATTRIBUTE_D7_MASK)
        | (USB_DEVICE_CONFIG_SELF_POWER
           << USB_DESCRIPTOR_CONFIGURE_ATTRIBUTE_SELF_POWERED_SHIFT)
        | (USB_DEVICE_CONFIG_REMOTE_WAKEUP
           << USB_DESCRIPTOR_CONFIGURE_ATTRIBUTE_REMOTE_WAKEUP_SHIFT),
    /* Maximum power consumption of the USB device from the bus in this specific
       configuration when the device is fully operational. Expressed in 2 mA
       units (i.e., 50 = 100 mA). */
    USB_DEVICE_MAX_POWER,

    /* Data Interface Descriptor */
    USB_DESCRIPTOR_LENGTH_INTERFACE, /* Size of this descriptor in bytes */
    USB_DESCRIPTOR_TYPE_INTERFACE,   /* INTERFACE Descriptor Type */
    USB_MCTP_INTERFACE_INDEX,        /* Number of this interface. */
    USB_MCTP_INTERFACE_ALTERNATE_0,  /* Value used to select this alternate
                                        setting for  the interface identified in
                                        the prior field
                                      */
    USB_MCTP_ENDPOINT_COUNT,         /* Number of endpoints used by this interface
                                        (excluding   endpoint zero). */
    USB_MCTP_GENERIC_CLASS,          /* Class code (assigned by the USB-IF). */
    USB_MCTP_GENERIC_SUBCLASS,       /* Subclass code (assigned by the USB-IF). */
    USB_MCTP_GENERIC_PROTOCOL,       /* Protocol code (assigned by the USB). */
    0x00,                            /* Interface Description String Index*/

    /* Bulk IN Endpoint descriptor */
    USB_DESCRIPTOR_LENGTH_ENDPOINT,
    USB_DESCRIPTOR_TYPE_ENDPOINT,
    USB_MCTP_ENDPOINT_IN | (USB_IN << USB_DESCRIPTOR_ENDPOINT_ADDRESS_DIRECTION_SHIFT),
    USB_ENDPOINT_BULK,
    USB_SHORT_GET_LOW(HS_MCTP_CLASS_IN_PACKET_SIZE),
    USB_SHORT_GET_HIGH(HS_MCTP_CLASS_IN_PACKET_SIZE),
    0x01,  // bInterval (1 frame)

    /* Bulk OUT Endpoint descriptor */
    USB_DESCRIPTOR_LENGTH_ENDPOINT,
    USB_DESCRIPTOR_TYPE_ENDPOINT,
    USB_MCTP_ENDPOINT_OUT | (USB_OUT << USB_DESCRIPTOR_ENDPOINT_ADDRESS_DIRECTION_SHIFT),
    USB_ENDPOINT_BULK,
    USB_SHORT_GET_LOW(HS_MCTP_CLASS_OUT_PACKET_SIZE),
    USB_SHORT_GET_HIGH(HS_MCTP_CLASS_OUT_PACKET_SIZE),
    0x01,  // bInterval (1 frame)

#if defined(USB_CONFIG_UART_BRIDGE)
    /* Interface Association Descriptor */
    /* Size of this descriptor in bytes */
    USB_IAD_DESC_SIZE,
    /* INTERFACE_ASSOCIATION Descriptor Type  */
    USB_DESCRIPTOR_TYPE_INTERFACE_ASSOCIATION,
    /* The first interface number associated with this function */
    0x01,
    /* The number of contiguous interfaces associated with this function */
    0x02,
    /* The function belongs to the Communication Device/Interface Class  */
    USB_CDC_VCOM_CIC_CLASS,
    USB_CDC_VCOM_CIC_SUBCLASS,
    /* The function uses the No class specific protocol required Protocol  */
    0x00,
    /* The Function string descriptor index */
    0x00,

    /* Interface Descriptor */
    USB_DESCRIPTOR_LENGTH_INTERFACE,
    USB_DESCRIPTOR_TYPE_INTERFACE,
    USB_CDC_VCOM_CIC_INTERFACE_INDEX,
    USB_CDC_VCOM_CIC_INTERFACE_ALTERNATE_0,
    USB_CDC_VCOM_CIC_ENDPOINT_COUNT,
    USB_CDC_VCOM_CIC_CLASS,
    USB_CDC_VCOM_CIC_SUBCLASS,
    USB_CDC_VCOM_CIC_PROTOCOL,
    0x00,

    /* CDC Class-Specific descriptor */
    USB_DESCRIPTOR_LENGTH_CDC_HEADER_FUNC, /* Size of this descriptor in bytes */
    USB_DESCRIPTOR_TYPE_CDC_CS_INTERFACE,  /* CS_INTERFACE Descriptor Type */
    USB_CDC_HEADER_FUNC_DESC,
    0x10,
    0x01, /* USB Class Definitions for Communications the Communication specification
             version 1.10 */

    USB_DESCRIPTOR_LENGTH_CDC_CALL_MANAG, /* Size of this descriptor in bytes */
    USB_DESCRIPTOR_TYPE_CDC_CS_INTERFACE, /* CS_INTERFACE Descriptor Type */
    USB_CDC_CALL_MANAGEMENT_FUNC_DESC,
    0x01, /*Bit 0: Whether device handle call management itself 1, Bit 1: Whether device can
             send/receive call management information over a Data Class Interface 0 */
    0x01, /* Indicates multiplexed commands are handled via data interface */

    USB_DESCRIPTOR_LENGTH_CDC_ABSTRACT,   /* Size of this descriptor in bytes */
    USB_DESCRIPTOR_TYPE_CDC_CS_INTERFACE, /* CS_INTERFACE Descriptor Type */
    USB_CDC_ABSTRACT_CONTROL_FUNC_DESC,
    0x06, /* Bit 0: Whether device supports the request combination of Set_Comm_Feature,
             Clear_Comm_Feature, and Get_Comm_Feature 0, Bit 1: Whether device supports the
             request combination of Set_Line_Coding, Set_Control_Line_State, Get_Line_Coding,
             and the notification Serial_State 1, Bit ...  */

    USB_DESCRIPTOR_LENGTH_CDC_UNION_FUNC, /* Size of this descriptor in bytes */
    USB_DESCRIPTOR_TYPE_CDC_CS_INTERFACE, /* CS_INTERFACE Descriptor Type */
    USB_CDC_UNION_FUNC_DESC,
    0x01, /* bControlInterface: The interface number of the Communications or Data Class
             interface designated as the controlling interface */
    0x02, /* bSubordinateInterface: Interface number of subordinate interface in the Union  */

    /*Notification Endpoint descriptor */
    USB_DESCRIPTOR_LENGTH_ENDPOINT,
    USB_DESCRIPTOR_TYPE_ENDPOINT,
    USB_CDC_VCOM_CIC_INTERRUPT_IN_ENDPOINT | (USB_IN << 7U),
    USB_ENDPOINT_INTERRUPT,
    USB_SHORT_GET_LOW(FS_CDC_VCOM_INTERRUPT_IN_PACKET_SIZE),
    USB_SHORT_GET_HIGH(FS_CDC_VCOM_INTERRUPT_IN_PACKET_SIZE),
    FS_CDC_VCOM_INTERRUPT_IN_INTERVAL,

    /* Data Interface Descriptor */
    USB_DESCRIPTOR_LENGTH_INTERFACE,
    USB_DESCRIPTOR_TYPE_INTERFACE,
    USB_CDC_VCOM_DIC_INTERFACE_INDEX,
    USB_CDC_VCOM_DIC_INTERFACE_ALTERNATE_0,
    USB_CDC_VCOM_DIC_ENDPOINT_COUNT,
    USB_CDC_VCOM_DIC_CLASS,
    USB_CDC_VCOM_DIC_SUBCLASS,
    USB_CDC_VCOM_DIC_PROTOCOL,
    0x00, /* Interface Description String Index*/

    /*Bulk IN Endpoint descriptor */
    USB_DESCRIPTOR_LENGTH_ENDPOINT,
    USB_DESCRIPTOR_TYPE_ENDPOINT,
    USB_CDC_VCOM_DIC_BULK_IN_ENDPOINT | (USB_IN << 7U),
    USB_ENDPOINT_BULK,
    USB_SHORT_GET_LOW(FS_CDC_VCOM_BULK_IN_PACKET_SIZE),
    USB_SHORT_GET_HIGH(FS_CDC_VCOM_BULK_IN_PACKET_SIZE),
    0x00, /* The polling interval value is every 0 Frames */

    /*Bulk OUT Endpoint descriptor */
    USB_DESCRIPTOR_LENGTH_ENDPOINT,
    USB_DESCRIPTOR_TYPE_ENDPOINT,
    USB_CDC_VCOM_DIC_BULK_OUT_ENDPOINT | (USB_OUT << 7U),
    USB_ENDPOINT_BULK,
    USB_SHORT_GET_LOW(FS_CDC_VCOM_BULK_OUT_PACKET_SIZE),
    USB_SHORT_GET_HIGH(FS_CDC_VCOM_BULK_OUT_PACKET_SIZE),
    0x00, /* The polling interval value is every 0 Frames */
#endif
};
#elif defined(USB_CONFIG_COMPOSITE)
// clang-format off
USB_DMA_INIT_DATA_ALIGN(USB_DATA_ALIGN_SIZE)
NV_SHARED_DATA uint8_t g_UsbDeviceHidGenericReportDescriptor[] = {
    0x06, 0x00, 0xFF,     // Usage Page (Vendor Defined 0xFF00)
    0x09, 0x01,           // Usage (Vendor Usage 1)
    0xA1, 0x01,           // Collection (Application)

    // Report ID 0x01: Reset Device
    0x85, 0x01,          // Report ID (1)
    0x95, 0x01,          // Report Count (1 byte)
    0x75, 0x08,          // Report Size (8 bits)
    0x26, 0xFF, 0x00,    // Logical Maximum (255)
    0x15, 0x00,          // Logical Minimum (0)
    0x09, 0x01,          // Usage (Vendor Usage 1)
    0xB1, 0x02,          // Feature (Data, Variable, Absolute)

    // Report ID 0x02: GetSetGPIO
    0x85, 0x02,          // Report ID (2)
    0x95, 0x04,          // Report Count (4 bytes)
    0x75, 0x08,          // Report Size (8 bits)
    0x26, 0xFF, 0x00,    // Logical Maximum (255)
    0x15, 0x00,          // Logical Minimum (0)
    0x09, 0x01,          // Usage (Vendor Usage 1)
    0xB1, 0x02,          // Feature (Data, Variable, Absolute)

    // Report ID 0x03: GetGPIO
    0x85, 0x03,          // Report ID (3)
    0x95, 0x01,          // Report Count (1 byte)
    0x75, 0x08,          // Report Size (8 bits)
    0x26, 0xFF, 0x00,    // Logical Maximum (255)
    0x15, 0x00,          // Logical Minimum (0)
    0x09, 0x01,          // Usage (Vendor Usage 1)
    0xB1, 0x02,          // Feature (Data, Variable, Absolute)

    // Report ID 0x04: SetGPIO
    0x85, 0x04,          // Report ID (4)
    0x95, 0x02,          // Report Count (2 bytes)
    0x75, 0x08,          // Report Size (8 bits)
    0x26, 0xFF, 0x00,    // Logical Maximum (255)
    0x15, 0x00,          // Logical Minimum (0)
    0x09, 0x01,          // Usage (Vendor Usage 1)
    0xB1, 0x02,          // Feature (Data, Variable, Absolute)

    // Report ID 0x05: GetVersionInfo
    0x85, 0x05,          // Report ID (5)
    0x95, 0x02,          // Report Count (2 bytes)
    0x75, 0x08,          // Report Size (8 bits)
    0x26, 0xFF, 0x00,    // Logical Maximum (255)
    0x15, 0x00,          // Logical Minimum (0)
    0x09, 0x01,          // Usage (Vendor Usage 1)
    0xB1, 0x02,          // Feature (Data, Variable, Absolute)

    // Report ID 0x06: GetSetSMBusConfig
    0x85, 0x06,          // Report ID (6)
    0x95, 0x0D,          // Report Count (13 bytes)
    0x75, 0x08,          // Report Size (8 bits)
    0x26, 0xFF, 0x00,    // Logical Maximum (255)
    0x15, 0x00,          // Logical Minimum (0)
    0x09, 0x01,          // Usage (Vendor Usage 1)
    0xB1, 0x02,          // Feature (Data, Variable, Absolute)

    // Output Reports (Host to Device)
    // Report ID 0x10: WriteRequest
    0x85, 0x10,          // Report ID (16)
    0x95, 0x3F,          // Report Count (63 bytes)
    0x75, 0x08,          // Report Size (8 bits)
    0x26, 0xFF, 0x00,    // Logical Maximum (255)
    0x15, 0x00,          // Logical Minimum (0)
    0x09, 0x01,          // Usage (Vendor Usage 1)
    0x91, 0x02,          // Output (Data, Variable, Absolute)

    // Report ID 0x11: WriteReadRequest
    0x85, 0x11,          // Report ID (17)
    0x95, 0x3F,          // Report Count (63 bytes)
    0x75, 0x08,          // Report Size (8 bits)
    0x26, 0xFF, 0x00,    // Logical Maximum (255)
    0x15, 0x00,          // Logical Minimum (0)
    0x09, 0x01,          // Usage (Vendor Usage 1)
    0x91, 0x02,          // Output (Data, Variable, Absolute)

    // Report ID 0x12: ReadRequest
    0x85, 0x12,          // Report ID (18)
    0x95, 0x3F,          // Report Count (63 bytes)
    0x75, 0x08,          // Report Size (8 bits)
    0x26, 0xFF, 0x00,    // Logical Maximum (255)
    0x15, 0x00,          // Logical Minimum (0)
    0x09, 0x01,          // Usage (Vendor Usage 1)
    0x91, 0x02,          // Output (Data, Variable, Absolute)

    // Input Reports (Device to Host)
    // Report ID 0x13: ReadResponse
    0x85, 0x13,          // Report ID (19)
    0x95, 0x3F,          // Report Count (63 bytes)
    0x75, 0x08,          // Report Size (8 bits)
    0x26, 0xFF, 0x00,    // Logical Maximum (255)
    0x15, 0x00,          // Logical Minimum (0)
    0x09, 0x01,          // Usage (Vendor Usage 1)
    0x81, 0x02,          // Input (Data, Variable, Absolute)

    // Report ID 0x14: WriteResponse
    0x85, 0x14,          // Report ID (20)
    0x95, 0x3F,          // Report Count (63 bytes)
    0x75, 0x08,          // Report Size (8 bits)
    0x26, 0xFF, 0x00,    // Logical Maximum (255)
    0x15, 0x00,          // Logical Minimum (0)
    0x09, 0x01,          // Usage (Vendor Usage 1)
    0x91, 0x02,          // Output (Data, Variable, Absolute)

    // Report ID 0x15: TransferStatusRequest
    0x85, 0x15,          // Report ID (21)
    0x95, 0x3F,          // Report Count (63 bytes)
    0x75, 0x08,          // Report Size (8 bits)
    0x26, 0xFF, 0x00,    // Logical Maximum (255)
    0x15, 0x00,          // Logical Minimum (0)
    0x09, 0x01,          // Usage (Vendor Usage 1)
    0x91, 0x02,          // Output (Data, Variable, Absolute)

    // Report ID 0x16: TransferStatusResponse
    0x85, 0x16,          // Report ID (22)
    0x95, 0x3F,          // Report Count (63 bytes)
    0x75, 0x08,          // Report Size (8 bits)
    0x26, 0xFF, 0x00,    // Logical Maximum (255)
    0x15, 0x00,          // Logical Minimum (0)
    0x09, 0x01,          // Usage (Vendor Usage 1)
    0x81, 0x02,          // Input (Data, Variable, Absolute)

    // Report ID 0x17: CancelTransfer
    0x85, 0x17,          // Report ID (23)
    0x95, 0x3F,          // Report Count (63 bytes)
    0x75, 0x08,          // Report Size (8 bits)
    0x26, 0xFF, 0x00,    // Logical Maximum (255)
    0x15, 0x00,          // Logical Minimum (0)
    0x09, 0x01,          // Usage (Vendor Usage 1)
    0x91, 0x02,          // Output (Data, Variable, Absolute)

    // Status and Configuration Reports
    // Report ID 0x20: Lock
    0x85, 0x20,          // Report ID (32)
    0x95, 0x01,          // Report Count (1 byte)
    0x09, 0x01,          // Usage (Vendor Usage 1)
    0xB1, 0x02,          // Feature (Data, Variable, Absolute)

    // Report ID 0x21: USB Configuration
    0x85, 0x21,          // Report ID (33)
    0x95, 0x09,          // Report Count (9 bytes)
    0x09, 0x01,          // Usage (Vendor Usage 1)
    0xB1, 0x02,          // Feature (Data, Variable, Absolute)

    // Report ID 0x22: Manufacturing String 1
    0x85, 0x22,          // Report ID (34)
    0x95, 0x3E,          // Report Count (62 bytes)
    0x09, 0x01,          // Usage (Vendor Usage 1)
    0xB1, 0x02,          // Feature (Data, Variable, Absolute)

    // Report ID 0x23: Manufacturing String 2
    0x85, 0x23,          // Report ID (35)
    0x95, 0x3E,          // Report Count (62 bytes)
    0x09, 0x01,          // Usage (Vendor Usage 1)
    0xB1, 0x02,          // Feature (Data, Variable, Absolute)

    // Report ID 0x24: Manufacturing String 3
    0x85, 0x24,          // Report ID (36)
    0x95, 0x3E,          // Report Count (62 bytes)
    0x09, 0x01,          // Usage (Vendor Usage 1)
    0xB1, 0x02,          // Feature (Data, Variable, Absolute)

    0xC0                  // End Collection
};
// clang-format on

/* HID class endpoint information */
NV_SHARED_DATA usb_device_endpoint_struct_t
    g_UsbDeviceHidEndpoints[USB_HID_GENERIC_ENDPOINT_COUNT] = {
        /* HID class interrupt IN pipe */
        {
         USB_HID_GENERIC_ENDPOINT_IN
         | (USB_IN << USB_DESCRIPTOR_ENDPOINT_ADDRESS_DIRECTION_SHIFT),
         USB_ENDPOINT_INTERRUPT,  FS_HID_GENERIC_INTERRUPT_IN_PACKET_SIZE,
         FS_HID_GENERIC_INTERRUPT_IN_INTERVAL, },
        /* HID class interrupt OUT pipe */
        {
         USB_HID_GENERIC_ENDPOINT_OUT
         | (USB_OUT << USB_DESCRIPTOR_ENDPOINT_ADDRESS_DIRECTION_SHIFT),
         USB_ENDPOINT_INTERRUPT, FS_HID_GENERIC_INTERRUPT_OUT_PACKET_SIZE,
         FS_HID_GENERIC_INTERRUPT_OUT_INTERVAL, },
};

/* HID class interface information */
NV_SHARED_DATA usb_device_interface_struct_t g_UsbDeviceHidInterface[] = {
    {
     USB_MCTP_INTERFACE_ALTERNATE_0,  // The alternate setting of the interface
        {
            USB_HID_GENERIC_ENDPOINT_COUNT,  // Endpoint count
            g_UsbDeviceHidEndpoints,         // Endpoints handle
        }, NULL,
     }
};

/* HID class interfaces */
NV_SHARED_DATA usb_device_interfaces_struct_t
    g_UsbDeviceHidInterfaces[USB_MCTP_INTERFACE_COUNT] = {
        {
         USB_HID_GENERIC_CLASS, USB_HID_GENERIC_SUBCLASS,
         USB_HID_GENERIC_PROTOCOL, USB_HID_GENERIC_INTERFACE_INDEX,  // The interface number of the HID
            g_UsbDeviceHidInterface,          // Interfaces handle
            sizeof(g_UsbDeviceHidInterface) / sizeof(usb_device_interface_struct_t),
         },
};

/* HID class interface list */
NV_SHARED_DATA usb_device_interface_list_t
    g_UsbDeviceHidInterfaceList[USB_DEVICE_CONFIGURATION_COUNT] = {
        {
         USB_HID_GENERIC_INTERFACE_COUNT,  // The interface count of the HID
            g_UsbDeviceHidInterfaces,         // The interfaces handle
        },
};

/* HID class information */
NV_SHARED_DATA usb_device_class_struct_t g_UsbDeviceHidGenericConfig = {
    g_UsbDeviceHidInterfaceList,    /* Interface list */
    kUSB_DeviceClassTypeHid,        /* The HID class type */
    USB_DEVICE_CONFIGURATION_COUNT, /* Number of configurations */
};

#ifdef USB_DEVICE_CONFIG_VENDOR_SPECIFIC
/* NV_SMA_SPI class endpoint information */
NV_SHARED_DATA usb_device_endpoint_struct_t
    g_UsbDeviceSpiEndpoints[USB_NV_SMA_SPI_ENDPOINT_COUNT] = {
        /* NV_SMA_SPI class interrupt IN pipe */
        {
         USB_NV_SMA_SPI_ENDPOINT_IN
         | (USB_IN << USB_DESCRIPTOR_ENDPOINT_ADDRESS_DIRECTION_SHIFT),
         USB_ENDPOINT_BULK,  FS_NV_SMA_SPI_CLASS_IN_PACKET_SIZE,
         FS_NV_SMA_SPI_CLASS_IN_INTERVAL, },
        /* NV_SMA_SPI class interrupt OUT pipe */
        {
         USB_NV_SMA_SPI_ENDPOINT_OUT
         | (USB_OUT << USB_DESCRIPTOR_ENDPOINT_ADDRESS_DIRECTION_SHIFT),
         USB_ENDPOINT_BULK, FS_NV_SMA_SPI_CLASS_OUT_PACKET_SIZE,
         FS_NV_SMA_SPI_CLASS_OUT_INTERVAL, },
};

/* NV_SMA_SPI class interface information */
NV_SHARED_DATA usb_device_interface_struct_t g_UsbDeviceSpiInterface[] = {
    {
     USB_MCTP_INTERFACE_ALTERNATE_0,  // The alternate setting of the interface
        {
            USB_NV_SMA_SPI_ENDPOINT_COUNT,  // Endpoint count
            g_UsbDeviceSpiEndpoints,        // Endpoints handle
        }, NULL,
     }
};

/* NV_SMA_SPI class interfaces */
NV_SHARED_DATA usb_device_interfaces_struct_t
    g_UsbDeviceSpiInterfaces[USB_NV_SMA_SPI_INTERFACE_COUNT] = {
        {
         USB_NV_SMA_SPI_CLASS, USB_NV_SMA_SPI_SUBCLASS,
         USB_NV_SMA_SPI_PROTOCOL, USB_NV_SMA_SPI_INTERFACE_INDEX,  // The interface number of the NV_SMA_SPI
            g_UsbDeviceSpiInterface,         // Interfaces handle
            sizeof(g_UsbDeviceSpiInterface) / sizeof(usb_device_interface_struct_t),
         },
};

/* NV_SMA_SPI class interface list */
NV_SHARED_DATA usb_device_interface_list_t
    g_UsbDeviceSpiInterfaceList[USB_DEVICE_CONFIGURATION_COUNT] = {
        {
         USB_NV_SMA_SPI_ENDPOINT_COUNT,  // The interface count of the NV_SMA
            g_UsbDeviceSpiInterfaces,       // The interfaces handle
        },
};

/* NV_SMA_SPI class information */
NV_SHARED_DATA usb_device_class_struct_t g_UsbDeviceSpiConfig = {
    g_UsbDeviceSpiInterfaceList,                      /* Interface list */
    (_usb_usb_device_class_type)USB_NV_SMA_SPI_CLASS, /* The Vendor specific class type */
    USB_DEVICE_CONFIGURATION_COUNT,                   /* Number of configurations */
};
#endif

/* COMPOSTIE Device Descriptor */
USB_DMA_INIT_DATA_ALIGN(USB_DATA_ALIGN_SIZE)
NV_SHARED_DATA uint8_t g_UsbDeviceDescriptor[] = {

    USB_DESCRIPTOR_LENGTH_DEVICE,  // bLength: Size of this descriptor in bytes
    USB_DESCRIPTOR_TYPE_DEVICE,    // bDescriptorType: DEVICE

    // bcdUSB: USB Specification Release Number in Binary-Coded Decimal
    // (e.g., 2.10 is 210H)
    USB_SHORT_GET_LOW(USB_DEVICE_SPECIFIC_BCD_VERSION),
    USB_SHORT_GET_HIGH(USB_DEVICE_SPECIFIC_BCD_VERSION),

    USB_COMPOSTIE_CLASS,          // bDeviceClass: Class code
    USB_COMPOSTIE_SUBCLASS,       // bDeviceSubClass: Subclass code
    USB_COMPOSTIE_PROTOCOL,       // bDeviceProtocol: Protocol code
    USB_CONTROL_MAX_PACKET_SIZE,  // bMaxPacketSize0: Maximum packet size for
                                  // endpoint zero
                                  //                  (only 8, 16, 32, or 64 are
                                  //                  valid)

    // idVendor: Vendor ID
    USB_SHORT_GET_LOW(UsbDeviceVid),
    USB_SHORT_GET_HIGH(UsbDeviceVid),

    // idProduct: Product ID
    USB_SHORT_GET_LOW(UsbDevicePid),
    USB_SHORT_GET_HIGH(UsbDevicePid),

    // bcdDevice: Device release number in binary-coded decimal
    USB_SHORT_GET_LOW(USB_DEVICE_DEMO_BCD_VERSION),
    USB_SHORT_GET_HIGH(USB_DEVICE_DEMO_BCD_VERSION),

    0x00U,  // iManufacturer: Index of string descriptor describing manufacturer
    0x00U,  // iProduct: Index of string descriptor describing product
    0x00U,  // iSerialNumber: Index of string descriptor describing the device's
            // serial number

    USB_DEVICE_CONFIGURATION_COUNT,  // bNumConfigurations: Number of possible
                                     // configurations
};

USB_DMA_INIT_DATA_ALIGN(USB_DATA_ALIGN_SIZE)
NV_SHARED_DATA uint8_t g_UsbDeviceConfigurationDescriptor[] = {
    USB_DESCRIPTOR_LENGTH_INTERFACE,  // bLength: Size of this descriptor in
                                      // bytes
    USB_DESCRIPTOR_TYPE_CONFIGURE,    // bDescriptorType: CONFIGURATION

    // wTotalLength: Total length of data returned for this configuration
    USB_SHORT_GET_LOW(USB_DESCRIPTOR_LENGTH_CONFIGURE
                      + USB_DESCRIPTOR_LENGTH_INTERFACE     // MCTP Interface
                      + 2 * USB_DESCRIPTOR_LENGTH_ENDPOINT  // MCTP IN/OUT endpoints
                      + USB_DESCRIPTOR_LENGTH_INTERFACE     // HID Interface
                      + USB_DESCRIPTOR_LENGTH_HID           // HID Descriptor
                      + 2 * USB_DESCRIPTOR_LENGTH_ENDPOINT  // HID IN/OUT endpoints
#ifdef USB_DEVICE_CONFIG_VENDOR_SPECIFIC
                      + USB_DESCRIPTOR_LENGTH_INTERFACE     // NV_SMA_SPI Interface
                      + 2 * USB_DESCRIPTOR_LENGTH_ENDPOINT  // NV_SMA_SPI IN/OUT endpoints
#endif
                      + USB_DESCRIPTOR_LENGTH_VCOM),
    USB_SHORT_GET_HIGH(USB_DESCRIPTOR_LENGTH_CONFIGURE
                       + USB_DESCRIPTOR_LENGTH_INTERFACE     // MCTP Interface
                       + 2 * USB_DESCRIPTOR_LENGTH_ENDPOINT  // MCTP IN/OUT endpoints
                       + USB_DESCRIPTOR_LENGTH_INTERFACE     // HID Interface
                       + USB_DESCRIPTOR_LENGTH_HID           // HID Descriptor
                       + 2 * USB_DESCRIPTOR_LENGTH_ENDPOINT  // HID IN/OUT endpoints
#ifdef USB_DEVICE_CONFIG_VENDOR_SPECIFIC
                       + USB_DESCRIPTOR_LENGTH_INTERFACE     // NV_SMA_SPI Interface
                       + 2 * USB_DESCRIPTOR_LENGTH_ENDPOINT  // NV_SMA_SPI IN/OUT endpoints
#endif
                       + USB_DESCRIPTOR_LENGTH_VCOM),

    SYS_USB_COMPOSITE_INTERFACE_COUNT,  // bNumInterfaces: Number of interfaces
                                        // supported by this configuration
    SYS_USB_COMPOSITE_CONFIGURE_INDEX,  // bConfigurationValue: Value to use in
                                        // SetConfiguration request
    0x00,  // iConfiguration: Index of string descriptor for this configuration
    (USB_DESCRIPTOR_CONFIGURE_ATTRIBUTE_D7_MASK)
        | (USB_DEVICE_CONFIG_SELF_POWER
           << USB_DESCRIPTOR_CONFIGURE_ATTRIBUTE_SELF_POWERED_SHIFT)
        | (USB_DEVICE_CONFIG_REMOTE_WAKEUP
           << USB_DESCRIPTOR_CONFIGURE_ATTRIBUTE_REMOTE_WAKEUP_SHIFT),
    USB_DEVICE_MAX_POWER,  // bMaxPower: Maximum power consumption in 2 mA units

    /* Data Interface Descriptor (MCTP) */
    USB_DESCRIPTOR_LENGTH_INTERFACE,  // Size of this descriptor in bytes
    USB_DESCRIPTOR_TYPE_INTERFACE,    // INTERFACE Descriptor Type
    USB_MCTP_INTERFACE_INDEX,         // Interface Index 1 for MCTP (Second interface)
    USB_MCTP_INTERFACE_ALTERNATE_0,   // Alternate setting
    USB_MCTP_ENDPOINT_COUNT,          // Number of endpoints
    USB_MCTP_GENERIC_CLASS,           // Class code
    USB_MCTP_GENERIC_SUBCLASS,        // Subclass code
    USB_MCTP_GENERIC_PROTOCOL,        // Protocol code
    0x00,                             // Interface Description String Index

    /* Bulk IN Endpoint descriptor (MCTP) */
    USB_DESCRIPTOR_LENGTH_ENDPOINT,  // Size of this descriptor in bytes
    USB_DESCRIPTOR_TYPE_ENDPOINT,    // ENDPOINT Descriptor Type
    USB_MCTP_ENDPOINT_IN | (USB_IN << USB_DESCRIPTOR_ENDPOINT_ADDRESS_DIRECTION_SHIFT),
    USB_ENDPOINT_BULK,  // Endpoint Attributes
    USB_SHORT_GET_LOW(HS_MCTP_CLASS_IN_PACKET_SIZE),
    USB_SHORT_GET_HIGH(HS_MCTP_CLASS_IN_PACKET_SIZE),
    0x01,  // bInterval (1 frame)

    /* Bulk OUT Endpoint descriptor (MCTP) */
    USB_DESCRIPTOR_LENGTH_ENDPOINT,  // Size of this descriptor in bytes
    USB_DESCRIPTOR_TYPE_ENDPOINT,    // ENDPOINT Descriptor Type
    USB_MCTP_ENDPOINT_OUT | (USB_OUT << USB_DESCRIPTOR_ENDPOINT_ADDRESS_DIRECTION_SHIFT),
    USB_ENDPOINT_BULK,  // Endpoint Attributes
    USB_SHORT_GET_LOW(HS_MCTP_CLASS_OUT_PACKET_SIZE),
    USB_SHORT_GET_HIGH(HS_MCTP_CLASS_OUT_PACKET_SIZE),
    0x01,  // bInterval (1 frame)

    /* HID Interface Descriptor */
    USB_DESCRIPTOR_LENGTH_INTERFACE,        // bLength: Size of this descriptor in
                                            // bytes
    USB_DESCRIPTOR_TYPE_INTERFACE,          // bDescriptorType: INTERFACE
    USB_HID_GENERIC_INTERFACE_INDEX,        // bInterfaceNumber: Index of this
                                            // interface
    USB_HID_GENERIC_INTERFACE_ALTERNATE_0,  // bAlternateSetting: Alternate
                                            // setting
    USB_HID_GENERIC_ENDPOINT_COUNT,         // bNumEndpoints: Number of endpoints in
                                            // this interface
    USB_HID_GENERIC_CLASS,                  // bInterfaceClass: HID Class
    USB_HID_GENERIC_SUBCLASS,               // bInterfaceSubClass: HID Subclass
    USB_HID_GENERIC_PROTOCOL,               // bInterfaceProtocol: HID Protocol
    0x00,  // iInterface: Index of string descriptor for this interface

    /* HID Descriptor */
    USB_DESCRIPTOR_LENGTH_INTERFACE,  // bLength: Size of this descriptor in
                                      // bytes
    0x21,                             // bDescriptorType: HID
    0x01,
    0x01,  // bcdHID: HID version 1.01
    0x00,  // bCountryCode: Country code
    0x01,  // bNumDescriptors: Number of HID class descriptors to follow
    0x22,  // bDescriptorType: Report descriptor
    USB_SHORT_GET_LOW(USB_DESCRIPTOR_HID_REPORT),
    USB_SHORT_GET_HIGH(USB_DESCRIPTOR_HID_REPORT),  // wDescriptorLength: Total length of
                                                    // Report descriptor

    /* HID Endpoint Descriptor (IN) */
    USB_DESCRIPTOR_LENGTH_ENDPOINT,  // bLength
    USB_DESCRIPTOR_TYPE_ENDPOINT,    // bDescriptorType (Endpoint)
    USB_HID_GENERIC_ENDPOINT_IN
        | (USB_IN << USB_DESCRIPTOR_ENDPOINT_ADDRESS_DIRECTION_SHIFT),  // bEndpointAddress
                                                                        // (IN endpoint 1)
    USB_ENDPOINT_INTERRUPT,  // bmAttributes (Interrupt)
    USB_SHORT_GET_LOW(HS_HID_GENERIC_INTERRUPT_IN_PACKET_SIZE),
    USB_SHORT_GET_HIGH(HS_HID_GENERIC_INTERRUPT_IN_PACKET_SIZE),
    0x01,  // bInterval (1 frame)

    /* HID Endpoint Descriptor (OUT) */
    USB_DESCRIPTOR_LENGTH_ENDPOINT,  // bLength
    USB_DESCRIPTOR_TYPE_ENDPOINT,    // bDescriptorType (Endpoint)
    USB_HID_GENERIC_ENDPOINT_OUT
        | (USB_OUT << USB_DESCRIPTOR_ENDPOINT_ADDRESS_DIRECTION_SHIFT),  // bEndpointAddress
                                                                         // (OUT endpoint
                                                                         // 1)
    USB_ENDPOINT_INTERRUPT,  // bmAttributes (Interrupt)
    USB_SHORT_GET_LOW(HS_HID_GENERIC_INTERRUPT_OUT_PACKET_SIZE),
    USB_SHORT_GET_HIGH(HS_HID_GENERIC_INTERRUPT_OUT_PACKET_SIZE),
    0x01,  // bInterval (1 frame)
#ifdef USB_DEVICE_CONFIG_VENDOR_SPECIFIC
    /* NV_SMA_SPI Interface Descriptor */
    USB_DESCRIPTOR_LENGTH_INTERFACE,       // bLength: Size of this descriptor in
                                           // bytes
    USB_DESCRIPTOR_TYPE_INTERFACE,         // bDescriptorType: INTERFACE
    USB_NV_SMA_SPI_INTERFACE_INDEX,        // bInterfaceNumber: Index of this
                                           // interface
    USB_NV_SMA_SPI_INTERFACE_ALTERNATE_0,  // bAlternateSetting: Alternate
                                           // setting
    USB_NV_SMA_SPI_ENDPOINT_COUNT,         // bNumEndpoints: Number of endpoints in
                                           // this interface
    USB_NV_SMA_SPI_CLASS,                  // bInterfaceClass: HID Class
    USB_NV_SMA_SPI_SUBCLASS,               // bInterfaceSubClass: HID Subclass
    USB_NV_SMA_SPI_PROTOCOL,               // bInterfaceProtocol: HID Protocol
    0x00,  // iInterface: Index of string descriptor for this interface

    /* NV_SMA_SPI Endpoint Descriptor (IN) */
    USB_DESCRIPTOR_LENGTH_ENDPOINT,  // bLength
    USB_DESCRIPTOR_TYPE_ENDPOINT,    // bDescriptorType (Endpoint)
    USB_NV_SMA_SPI_ENDPOINT_IN
        | (USB_IN << USB_DESCRIPTOR_ENDPOINT_ADDRESS_DIRECTION_SHIFT),  // bEndpointAddress
                                                                        // (IN endpoint 1)
    USB_ENDPOINT_BULK,                                                  // Endpoint Attributes
    USB_SHORT_GET_LOW(HS_NV_SMA_SPI_CLASS_IN_PACKET_SIZE),
    USB_SHORT_GET_HIGH(HS_NV_SMA_SPI_CLASS_IN_PACKET_SIZE),
    0x01,  // bInterval (1 frame)

    /* NV_SMA_SPI Endpoint Descriptor (OUT) */
    USB_DESCRIPTOR_LENGTH_ENDPOINT,  // bLength
    USB_DESCRIPTOR_TYPE_ENDPOINT,    // bDescriptorType (Endpoint)
    USB_NV_SMA_SPI_ENDPOINT_OUT
        | (USB_OUT << USB_DESCRIPTOR_ENDPOINT_ADDRESS_DIRECTION_SHIFT),  // bEndpointAddress
                                                                         // (OUT endpoint
                                                                         // 1)
    USB_ENDPOINT_BULK,                                                   // Endpoint Attributes
    USB_SHORT_GET_LOW(HS_NV_SMA_SPI_CLASS_OUT_PACKET_SIZE),
    USB_SHORT_GET_HIGH(HS_NV_SMA_SPI_CLASS_OUT_PACKET_SIZE),
    0x01,  // bInterval (1 frame)
#endif
#if defined(USB_CONFIG_UART_BRIDGE)
    /* Interface Association Descriptor */
    /* Size of this descriptor in bytes */
    USB_IAD_DESC_SIZE,
    /* INTERFACE_ASSOCIATION Descriptor Type  */
    USB_DESCRIPTOR_TYPE_INTERFACE_ASSOCIATION,
    /* The first interface number associated with this function */
    0x02,
    /* The number of contiguous interfaces associated with this function */
    0x02,
    /* The function belongs to the Communication Device/Interface Class  */
    USB_CDC_VCOM_CIC_CLASS,
    USB_CDC_VCOM_CIC_SUBCLASS,
    /* The function uses the No class specific protocol required Protocol  */
    0x00,
    /* The Function string descriptor index */
    0x00,

    /* Interface Descriptor */
    USB_DESCRIPTOR_LENGTH_INTERFACE,
    USB_DESCRIPTOR_TYPE_INTERFACE,
    USB_CDC_VCOM_CIC_INTERFACE_INDEX,
    USB_CDC_VCOM_CIC_INTERFACE_ALTERNATE_0,
    USB_CDC_VCOM_CIC_ENDPOINT_COUNT,
    USB_CDC_VCOM_CIC_CLASS,
    USB_CDC_VCOM_CIC_SUBCLASS,
    USB_CDC_VCOM_CIC_PROTOCOL,
    0x00,

    /* CDC Class-Specific descriptor */
    USB_DESCRIPTOR_LENGTH_CDC_HEADER_FUNC, /* Size of this descriptor in bytes */
    USB_DESCRIPTOR_TYPE_CDC_CS_INTERFACE,  /* CS_INTERFACE Descriptor Type */
    USB_CDC_HEADER_FUNC_DESC,
    0x10,
    0x01, /* USB Class Definitions for Communications the Communication specification
             version 1.10 */

    USB_DESCRIPTOR_LENGTH_CDC_CALL_MANAG, /* Size of this descriptor in bytes */
    USB_DESCRIPTOR_TYPE_CDC_CS_INTERFACE, /* CS_INTERFACE Descriptor Type */
    USB_CDC_CALL_MANAGEMENT_FUNC_DESC,
    0x01, /*Bit 0: Whether device handle call management itself 1, Bit 1: Whether device can
             send/receive call management information over a Data Class Interface 0 */
    0x01, /* Indicates multiplexed commands are handled via data interface */

    USB_DESCRIPTOR_LENGTH_CDC_ABSTRACT,   /* Size of this descriptor in bytes */
    USB_DESCRIPTOR_TYPE_CDC_CS_INTERFACE, /* CS_INTERFACE Descriptor Type */
    USB_CDC_ABSTRACT_CONTROL_FUNC_DESC,
    0x06, /* Bit 0: Whether device supports the request combination of Set_Comm_Feature,
             Clear_Comm_Feature, and Get_Comm_Feature 0, Bit 1: Whether device supports the
             request combination of Set_Line_Coding, Set_Control_Line_State, Get_Line_Coding,
             and the notification Serial_State 1, Bit ...  */

    USB_DESCRIPTOR_LENGTH_CDC_UNION_FUNC, /* Size of this descriptor in bytes */
    USB_DESCRIPTOR_TYPE_CDC_CS_INTERFACE, /* CS_INTERFACE Descriptor Type */
    USB_CDC_UNION_FUNC_DESC,
    0x02, /* bControlInterface: The interface number of the Communications or Data Class
             interface designated as the controlling interface */
    0x03, /* bSubordinateInterface: Interface number of subordinate interface in the Union  */

    /*Notification Endpoint descriptor */
    USB_DESCRIPTOR_LENGTH_ENDPOINT,
    USB_DESCRIPTOR_TYPE_ENDPOINT,
    USB_CDC_VCOM_CIC_INTERRUPT_IN_ENDPOINT | (USB_IN << 7U),
    USB_ENDPOINT_INTERRUPT,
    USB_SHORT_GET_LOW(FS_CDC_VCOM_INTERRUPT_IN_PACKET_SIZE),
    USB_SHORT_GET_HIGH(FS_CDC_VCOM_INTERRUPT_IN_PACKET_SIZE),
    FS_CDC_VCOM_INTERRUPT_IN_INTERVAL,

    /* Data Interface Descriptor */
    USB_DESCRIPTOR_LENGTH_INTERFACE,
    USB_DESCRIPTOR_TYPE_INTERFACE,
    USB_CDC_VCOM_DIC_INTERFACE_INDEX,
    USB_CDC_VCOM_DIC_INTERFACE_ALTERNATE_0,
    USB_CDC_VCOM_DIC_ENDPOINT_COUNT,
    USB_CDC_VCOM_DIC_CLASS,
    USB_CDC_VCOM_DIC_SUBCLASS,
    USB_CDC_VCOM_DIC_PROTOCOL,
    0x00, /* Interface Description String Index*/

    /*Bulk IN Endpoint descriptor */
    USB_DESCRIPTOR_LENGTH_ENDPOINT,
    USB_DESCRIPTOR_TYPE_ENDPOINT,
    USB_CDC_VCOM_DIC_BULK_IN_ENDPOINT | (USB_IN << 7U),
    USB_ENDPOINT_BULK,
    USB_SHORT_GET_LOW(FS_CDC_VCOM_BULK_IN_PACKET_SIZE),
    USB_SHORT_GET_HIGH(FS_CDC_VCOM_BULK_IN_PACKET_SIZE),
    0x00, /* The polling interval value is every 0 Frames */

    /*Bulk OUT Endpoint descriptor */
    USB_DESCRIPTOR_LENGTH_ENDPOINT,
    USB_DESCRIPTOR_TYPE_ENDPOINT,
    USB_CDC_VCOM_DIC_BULK_OUT_ENDPOINT | (USB_OUT << 7U),
    USB_ENDPOINT_BULK,
    USB_SHORT_GET_LOW(FS_CDC_VCOM_BULK_OUT_PACKET_SIZE),
    USB_SHORT_GET_HIGH(FS_CDC_VCOM_BULK_OUT_PACKET_SIZE),
    0x00, /* The polling interval value is every 0 Frames */
#endif
};

usb_status_t USB_DeviceGetHidDescriptor(usb_device_handle                       handle,
                                        usb_device_get_hid_descriptor_struct_t* hidDescriptor)
{
    if (USB_HID_GENERIC_INTERFACE_INDEX == hidDescriptor->interfaceNumber) {
        hidDescriptor
            ->buffer = &g_UsbDeviceConfigurationDescriptor[USB_DESCRIPTOR_LENGTH_CONFIGURE
                                                           + USB_DESCRIPTOR_LENGTH_INTERFACE];
        hidDescriptor->length = USB_DESCRIPTOR_LENGTH_HID;
    }
    else {
        return kStatus_USB_InvalidRequest;
    }
    return kStatus_USB_Success;
}

usb_status_t USB_DeviceGetHidReportDescriptor(
    usb_device_handle                              handle,
    usb_device_get_hid_report_descriptor_struct_t* hidReportDescriptor)
{
    if (USB_HID_GENERIC_INTERFACE_INDEX == hidReportDescriptor->interfaceNumber) {
        hidReportDescriptor->buffer = g_UsbDeviceHidGenericReportDescriptor;
        hidReportDescriptor->length = USB_DESCRIPTOR_HID_REPORT;
    }
    else {
        return kStatus_USB_InvalidRequest;
    }
    return kStatus_USB_Success;
}

usb_status_t USB_DeviceGetHidPhysicalDescriptor(
    usb_device_handle                                handle,
    usb_device_get_hid_physical_descriptor_struct_t* hidPhysicalDescriptor)
{
    return kStatus_USB_InvalidRequest;
}
#endif

USB_DMA_INIT_DATA_ALIGN(USB_DATA_ALIGN_SIZE)
NV_SHARED_DATA uint8_t g_UsbDeviceString0[] = {
    2U + 2U,
    USB_DESCRIPTOR_TYPE_STRING,
    0x09U,
    0x04U,
};

/* Microsoft OS must be 12 03 4D 00 53 00 46 00 54 00 31 00 30 00 30 00 90 00 */
USB_DMA_INIT_DATA_ALIGN(USB_DATA_ALIGN_SIZE)
NV_SHARED_DATA uint8_t g_UsbDeviceOSString[] = {
    (8 * 2 + 2),
    USB_DESCRIPTOR_TYPE_STRING,
    'M',  // Signature:
    0x00U,
    'S',
    0x00U,
    'F',
    0x00U,
    'T',
    0x00U,
    '1',
    0x00U,
    '0',
    0x00U,
    '0',
    0x00U,
    0x90U,  // Vendor Code
    0x00U,
};

NV_SHARED_DATA uint32_t g_UsbDeviceStringDescriptorLength[USB_DEVICE_STRING_COUNT] = {
    sizeof(g_UsbDeviceString0),
};

NV_SHARED_DATA uint8_t* g_UsbDeviceStringDescriptorArray[USB_DEVICE_STRING_COUNT] = {
    g_UsbDeviceString0,
};

NV_SHARED_DATA usb_language_t g_UsbDeviceLanguage[USB_DEVICE_LANGUAGE_COUNT] = {
    {
     g_UsbDeviceStringDescriptorArray, g_UsbDeviceStringDescriptorLength,
     (uint16_t)0x0409U,
     }
};

NV_SHARED_DATA usb_language_list_t g_UsbDeviceLanguageList = {
    g_UsbDeviceString0,
    sizeof(g_UsbDeviceString0),
    g_UsbDeviceLanguage,
    USB_DEVICE_LANGUAGE_COUNT,
};

usb_status_t
USB_DeviceGetDeviceDescriptor(usb_device_handle                          handle,
                              usb_device_get_device_descriptor_struct_t* deviceDescriptor)
{
    deviceDescriptor->buffer = g_UsbDeviceDescriptor;
    deviceDescriptor->length = USB_DESCRIPTOR_LENGTH_DEVICE;
    return kStatus_USB_Success;
}

usb_status_t USB_DeviceGetConfigurationDescriptor(
    usb_device_handle                                 handle,
    usb_device_get_configuration_descriptor_struct_t* configurationDescriptor)
{
    if (USB_MCTP_CONFIGURE_INDEX > configurationDescriptor->configuration) {
        configurationDescriptor->buffer = g_UsbDeviceConfigurationDescriptor;
        configurationDescriptor->length = USB_DESCRIPTOR_LENGTH_CONFIGURATION_ALL;
        return kStatus_USB_Success;
    }
    return kStatus_USB_InvalidRequest;
}

usb_status_t
USB_DeviceGetStringDescriptor(usb_device_handle                          handle,
                              usb_device_get_string_descriptor_struct_t* stringDescriptor)
{
    if (stringDescriptor->stringIndex == 0U) {
        stringDescriptor->buffer = (uint8_t*)g_UsbDeviceLanguageList.languageString;
        stringDescriptor->length = g_UsbDeviceLanguageList.stringLength;
    }
    else {
        uint8_t languageId    = 0U;
        uint8_t languageIndex = USB_DEVICE_STRING_COUNT;

        for (; languageId < USB_DEVICE_LANGUAGE_COUNT; languageId++) {
            if (stringDescriptor->languageId
                == g_UsbDeviceLanguageList.languageList[languageId].languageId) {
                if (stringDescriptor->stringIndex < USB_DEVICE_STRING_COUNT) {
                    languageIndex = stringDescriptor->stringIndex;
                }
                break;
            }
        }
        if (0xEE == stringDescriptor->stringIndex) {
            stringDescriptor->buffer = (uint8_t*)g_UsbDeviceOSString;
            stringDescriptor->length = sizeof(g_UsbDeviceOSString);
            return kStatus_USB_Success;
        }
        if (USB_DEVICE_STRING_COUNT == languageIndex) {
            return kStatus_USB_InvalidRequest;
        }
        stringDescriptor->buffer = (uint8_t*)g_UsbDeviceLanguageList.languageList[languageId]
                                       .string[languageIndex];
        stringDescriptor->length = g_UsbDeviceLanguageList.languageList[languageId]
                                       .length[languageIndex];
    }
    return kStatus_USB_Success;
}

usb_status_t USB_DeviceSetSpeed(usb_device_handle handle, uint8_t speed)
{
    usb_descriptor_union_t* descriptorHead;
    usb_descriptor_union_t* descriptorTail;

    descriptorHead = (usb_descriptor_union_t*)&g_UsbDeviceConfigurationDescriptor[0];
    descriptorTail = (usb_descriptor_union_t*)(&g_UsbDeviceConfigurationDescriptor
                                                   [USB_DESCRIPTOR_LENGTH_CONFIGURATION_ALL
                                                    - 1U]);

    while (descriptorHead < descriptorTail) {
        if (descriptorHead->common.bDescriptorType == USB_DESCRIPTOR_TYPE_ENDPOINT) {
            if (USB_SPEED_HIGH == speed) {
                // MCTP Endpoints
                if (((descriptorHead->endpoint.bEndpointAddress
                      & USB_DESCRIPTOR_ENDPOINT_ADDRESS_DIRECTION_MASK)
                     == USB_DESCRIPTOR_ENDPOINT_ADDRESS_DIRECTION_IN)
                    && (USB_MCTP_ENDPOINT_IN
                        == (descriptorHead->endpoint.bEndpointAddress
                            & USB_ENDPOINT_NUMBER_MASK))) {
                    descriptorHead->endpoint.bInterval = HS_MCTP_CLASS_IN_INTERVAL;
                    USB_SHORT_TO_LITTLE_ENDIAN_ADDRESS(HS_MCTP_CLASS_IN_PACKET_SIZE,
                                                       descriptorHead->endpoint.wMaxPacketSize);
                }
                else if (((descriptorHead->endpoint.bEndpointAddress
                           & USB_DESCRIPTOR_ENDPOINT_ADDRESS_DIRECTION_MASK)
                          == USB_DESCRIPTOR_ENDPOINT_ADDRESS_DIRECTION_OUT)
                         && (USB_MCTP_ENDPOINT_OUT
                             == (descriptorHead->endpoint.bEndpointAddress
                                 & USB_ENDPOINT_NUMBER_MASK))) {
                    descriptorHead->endpoint.bInterval = HS_MCTP_CLASS_OUT_INTERVAL;
                    USB_SHORT_TO_LITTLE_ENDIAN_ADDRESS(HS_MCTP_CLASS_OUT_PACKET_SIZE,
                                                       descriptorHead->endpoint.wMaxPacketSize);
                }
#if defined(USB_CONFIG_UART_BRIDGE)
                // CDC VCOM Endpoints
                else if ((USB_CDC_VCOM_CIC_INTERRUPT_IN_ENDPOINT
                          == (descriptorHead->endpoint.bEndpointAddress
                              & USB_ENDPOINT_NUMBER_MASK))
                         && ((descriptorHead->endpoint.bEndpointAddress
                              & USB_DESCRIPTOR_ENDPOINT_ADDRESS_DIRECTION_MASK)
                             == USB_DESCRIPTOR_ENDPOINT_ADDRESS_DIRECTION_IN)) {
                    descriptorHead->endpoint.bInterval = HS_CDC_VCOM_INTERRUPT_IN_INTERVAL;
                    USB_SHORT_TO_LITTLE_ENDIAN_ADDRESS(HS_CDC_VCOM_INTERRUPT_IN_PACKET_SIZE,
                                                       descriptorHead->endpoint.wMaxPacketSize);
                }
                else if ((USB_CDC_VCOM_DIC_BULK_IN_ENDPOINT
                          == (descriptorHead->endpoint.bEndpointAddress
                              & USB_ENDPOINT_NUMBER_MASK))
                         && ((descriptorHead->endpoint.bEndpointAddress
                              & USB_DESCRIPTOR_ENDPOINT_ADDRESS_DIRECTION_MASK)
                             == USB_DESCRIPTOR_ENDPOINT_ADDRESS_DIRECTION_IN)) {
                    USB_SHORT_TO_LITTLE_ENDIAN_ADDRESS(HS_CDC_VCOM_BULK_IN_PACKET_SIZE,
                                                       descriptorHead->endpoint.wMaxPacketSize);
                }
                else if ((USB_CDC_VCOM_DIC_BULK_OUT_ENDPOINT
                          == (descriptorHead->endpoint.bEndpointAddress
                              & USB_ENDPOINT_NUMBER_MASK))
                         && ((descriptorHead->endpoint.bEndpointAddress
                              & USB_DESCRIPTOR_ENDPOINT_ADDRESS_DIRECTION_MASK)
                             == USB_DESCRIPTOR_ENDPOINT_ADDRESS_DIRECTION_OUT)) {
                    USB_SHORT_TO_LITTLE_ENDIAN_ADDRESS(HS_CDC_VCOM_BULK_OUT_PACKET_SIZE,
                                                       descriptorHead->endpoint.wMaxPacketSize);
                }
#endif
#if defined(USB_CONFIG_COMPOSITE)
                // HID Endpoints
                else if (((descriptorHead->endpoint.bEndpointAddress
                           & USB_DESCRIPTOR_ENDPOINT_ADDRESS_DIRECTION_MASK)
                          == USB_DESCRIPTOR_ENDPOINT_ADDRESS_DIRECTION_IN)
                         && (USB_HID_GENERIC_ENDPOINT_IN
                             == (descriptorHead->endpoint.bEndpointAddress
                                 & USB_ENDPOINT_NUMBER_MASK))) {
                    descriptorHead->endpoint.bInterval = HS_HID_GENERIC_INTERRUPT_IN_INTERVAL;
                    USB_SHORT_TO_LITTLE_ENDIAN_ADDRESS(HS_HID_GENERIC_INTERRUPT_IN_PACKET_SIZE,
                                                       descriptorHead->endpoint.wMaxPacketSize);
                }
                else if (((descriptorHead->endpoint.bEndpointAddress
                           & USB_DESCRIPTOR_ENDPOINT_ADDRESS_DIRECTION_MASK)
                          == USB_DESCRIPTOR_ENDPOINT_ADDRESS_DIRECTION_OUT)
                         && (USB_HID_GENERIC_ENDPOINT_OUT
                             == (descriptorHead->endpoint.bEndpointAddress
                                 & USB_ENDPOINT_NUMBER_MASK))) {
                    descriptorHead->endpoint.bInterval = HS_HID_GENERIC_INTERRUPT_OUT_INTERVAL;
                    USB_SHORT_TO_LITTLE_ENDIAN_ADDRESS(HS_HID_GENERIC_INTERRUPT_OUT_PACKET_SIZE,
                                                       descriptorHead->endpoint.wMaxPacketSize);
                }
#ifdef USB_DEVICE_CONFIG_VENDOR_SPECIFIC
                // NV_SMA_SPI Endpoints
                else if (((descriptorHead->endpoint.bEndpointAddress
                           & USB_DESCRIPTOR_ENDPOINT_ADDRESS_DIRECTION_MASK)
                          == USB_DESCRIPTOR_ENDPOINT_ADDRESS_DIRECTION_IN)
                         && (USB_NV_SMA_SPI_ENDPOINT_IN
                             == (descriptorHead->endpoint.bEndpointAddress
                                 & USB_ENDPOINT_NUMBER_MASK))) {
                    descriptorHead->endpoint.bInterval = HS_NV_SMA_SPI_CLASS_IN_INTERVAL;
                    USB_SHORT_TO_LITTLE_ENDIAN_ADDRESS(HS_NV_SMA_SPI_CLASS_IN_PACKET_SIZE,
                                                       descriptorHead->endpoint.wMaxPacketSize);
                }
                else if (((descriptorHead->endpoint.bEndpointAddress
                           & USB_DESCRIPTOR_ENDPOINT_ADDRESS_DIRECTION_MASK)
                          == USB_DESCRIPTOR_ENDPOINT_ADDRESS_DIRECTION_OUT)
                         && (USB_NV_SMA_SPI_ENDPOINT_OUT
                             == (descriptorHead->endpoint.bEndpointAddress
                                 & USB_ENDPOINT_NUMBER_MASK))) {
                    descriptorHead->endpoint.bInterval = HS_NV_SMA_SPI_CLASS_OUT_INTERVAL;
                    USB_SHORT_TO_LITTLE_ENDIAN_ADDRESS(HS_NV_SMA_SPI_CLASS_OUT_PACKET_SIZE,
                                                       descriptorHead->endpoint.wMaxPacketSize);
                }
#endif
#endif
            }
            else {
                // MCTP Endpoints
                if (((descriptorHead->endpoint.bEndpointAddress
                      & USB_DESCRIPTOR_ENDPOINT_ADDRESS_DIRECTION_MASK)
                     == USB_DESCRIPTOR_ENDPOINT_ADDRESS_DIRECTION_IN)
                    && (USB_MCTP_ENDPOINT_IN
                        == (descriptorHead->endpoint.bEndpointAddress
                            & USB_ENDPOINT_NUMBER_MASK))) {
                    descriptorHead->endpoint.bInterval = FS_MCTP_CLASS_IN_INTERVAL;
                    USB_SHORT_TO_LITTLE_ENDIAN_ADDRESS(FS_MCTP_CLASS_IN_PACKET_SIZE,
                                                       descriptorHead->endpoint.wMaxPacketSize);
                }
                else if (((descriptorHead->endpoint.bEndpointAddress
                           & USB_DESCRIPTOR_ENDPOINT_ADDRESS_DIRECTION_MASK)
                          == USB_DESCRIPTOR_ENDPOINT_ADDRESS_DIRECTION_OUT)
                         && (USB_MCTP_ENDPOINT_OUT
                             == (descriptorHead->endpoint.bEndpointAddress
                                 & USB_ENDPOINT_NUMBER_MASK))) {
                    descriptorHead->endpoint.bInterval = FS_MCTP_CLASS_OUT_INTERVAL;
                    USB_SHORT_TO_LITTLE_ENDIAN_ADDRESS(FS_MCTP_CLASS_OUT_PACKET_SIZE,
                                                       descriptorHead->endpoint.wMaxPacketSize);
                }
#if defined(USB_CONFIG_UART_BRIDGE)
                // CDC VCOM Endpoints
                else if ((USB_CDC_VCOM_CIC_INTERRUPT_IN_ENDPOINT
                          == (descriptorHead->endpoint.bEndpointAddress
                              & USB_ENDPOINT_NUMBER_MASK))
                         && ((descriptorHead->endpoint.bEndpointAddress
                              & USB_DESCRIPTOR_ENDPOINT_ADDRESS_DIRECTION_MASK)
                             == USB_DESCRIPTOR_ENDPOINT_ADDRESS_DIRECTION_IN)) {
                    descriptorHead->endpoint.bInterval = FS_CDC_VCOM_INTERRUPT_IN_INTERVAL;
                    USB_SHORT_TO_LITTLE_ENDIAN_ADDRESS(FS_CDC_VCOM_INTERRUPT_IN_PACKET_SIZE,
                                                       descriptorHead->endpoint.wMaxPacketSize);
                }
                else if ((USB_CDC_VCOM_DIC_BULK_IN_ENDPOINT
                          == (descriptorHead->endpoint.bEndpointAddress
                              & USB_ENDPOINT_NUMBER_MASK))
                         && ((descriptorHead->endpoint.bEndpointAddress
                              & USB_DESCRIPTOR_ENDPOINT_ADDRESS_DIRECTION_MASK)
                             == USB_DESCRIPTOR_ENDPOINT_ADDRESS_DIRECTION_IN)) {
                    USB_SHORT_TO_LITTLE_ENDIAN_ADDRESS(FS_CDC_VCOM_BULK_IN_PACKET_SIZE,
                                                       descriptorHead->endpoint.wMaxPacketSize);
                }
                else if ((USB_CDC_VCOM_DIC_BULK_OUT_ENDPOINT
                          == (descriptorHead->endpoint.bEndpointAddress
                              & USB_ENDPOINT_NUMBER_MASK))
                         && ((descriptorHead->endpoint.bEndpointAddress
                              & USB_DESCRIPTOR_ENDPOINT_ADDRESS_DIRECTION_MASK)
                             == USB_DESCRIPTOR_ENDPOINT_ADDRESS_DIRECTION_OUT)) {
                    USB_SHORT_TO_LITTLE_ENDIAN_ADDRESS(FS_CDC_VCOM_BULK_OUT_PACKET_SIZE,
                                                       descriptorHead->endpoint.wMaxPacketSize);
                }
#endif
#if defined(USB_CONFIG_COMPOSITE)
                // HID Endpoints
                else if (((descriptorHead->endpoint.bEndpointAddress
                           & USB_DESCRIPTOR_ENDPOINT_ADDRESS_DIRECTION_MASK)
                          == USB_DESCRIPTOR_ENDPOINT_ADDRESS_DIRECTION_IN)
                         && (USB_HID_GENERIC_ENDPOINT_IN
                             == (descriptorHead->endpoint.bEndpointAddress
                                 & USB_ENDPOINT_NUMBER_MASK))) {
                    descriptorHead->endpoint.bInterval = FS_HID_GENERIC_INTERRUPT_IN_INTERVAL;
                    USB_SHORT_TO_LITTLE_ENDIAN_ADDRESS(FS_HID_GENERIC_INTERRUPT_IN_PACKET_SIZE,
                                                       descriptorHead->endpoint.wMaxPacketSize);
                }
                else if (((descriptorHead->endpoint.bEndpointAddress
                           & USB_DESCRIPTOR_ENDPOINT_ADDRESS_DIRECTION_MASK)
                          == USB_DESCRIPTOR_ENDPOINT_ADDRESS_DIRECTION_OUT)
                         && (USB_HID_GENERIC_ENDPOINT_OUT
                             == (descriptorHead->endpoint.bEndpointAddress
                                 & USB_ENDPOINT_NUMBER_MASK))) {
                    descriptorHead->endpoint.bInterval = FS_HID_GENERIC_INTERRUPT_OUT_INTERVAL;
                    USB_SHORT_TO_LITTLE_ENDIAN_ADDRESS(FS_HID_GENERIC_INTERRUPT_OUT_PACKET_SIZE,
                                                       descriptorHead->endpoint.wMaxPacketSize);
                }
#ifdef USB_DEVICE_CONFIG_VENDOR_SPECIFIC
                // NV_SMA_SPI Endpoints
                else if (((descriptorHead->endpoint.bEndpointAddress
                           & USB_DESCRIPTOR_ENDPOINT_ADDRESS_DIRECTION_MASK)
                          == USB_DESCRIPTOR_ENDPOINT_ADDRESS_DIRECTION_IN)
                         && (USB_NV_SMA_SPI_ENDPOINT_IN
                             == (descriptorHead->endpoint.bEndpointAddress
                                 & USB_ENDPOINT_NUMBER_MASK))) {
                    descriptorHead->endpoint.bInterval = FS_NV_SMA_SPI_CLASS_IN_INTERVAL;
                    USB_SHORT_TO_LITTLE_ENDIAN_ADDRESS(FS_NV_SMA_SPI_CLASS_IN_PACKET_SIZE,
                                                       descriptorHead->endpoint.wMaxPacketSize);
                }
                else if (((descriptorHead->endpoint.bEndpointAddress
                           & USB_DESCRIPTOR_ENDPOINT_ADDRESS_DIRECTION_MASK)
                          == USB_DESCRIPTOR_ENDPOINT_ADDRESS_DIRECTION_OUT)
                         && (USB_NV_SMA_SPI_ENDPOINT_OUT
                             == (descriptorHead->endpoint.bEndpointAddress
                                 & USB_ENDPOINT_NUMBER_MASK))) {
                    descriptorHead->endpoint.bInterval = FS_NV_SMA_SPI_CLASS_OUT_INTERVAL;
                    USB_SHORT_TO_LITTLE_ENDIAN_ADDRESS(FS_NV_SMA_SPI_CLASS_OUT_PACKET_SIZE,
                                                       descriptorHead->endpoint.wMaxPacketSize);
                }
#endif
#endif
            }
        }
        descriptorHead = (usb_descriptor_union_t*)((uint8_t*)descriptorHead
                                                   + descriptorHead->common.bLength);
    }

    for (uint8_t i = 0U; i < USB_MCTP_ENDPOINT_COUNT; i++) {
        // Due to the same configuration of HS and FS MCTP endpoints, use the same configuration
        if (g_UsbDeviceMctpEndpoints[i].endpointAddress
            & USB_DESCRIPTOR_ENDPOINT_ADDRESS_DIRECTION_MASK) {
            g_UsbDeviceMctpEndpoints[i].maxPacketSize = HS_MCTP_CLASS_IN_PACKET_SIZE;
            g_UsbDeviceMctpEndpoints[i].interval      = HS_MCTP_CLASS_IN_INTERVAL;
        }
        else {
            g_UsbDeviceMctpEndpoints[i].maxPacketSize = HS_MCTP_CLASS_OUT_PACKET_SIZE;
            g_UsbDeviceMctpEndpoints[i].interval      = HS_MCTP_CLASS_OUT_INTERVAL;
        }
    }

#if defined(USB_CONFIG_UART_BRIDGE)
    for (int i = 0; i < USB_CDC_VCOM_CIC_ENDPOINT_COUNT; i++) {
        if (USB_SPEED_HIGH == speed) {
            g_cdcVcomCicEndpoints[i].maxPacketSize = HS_CDC_VCOM_INTERRUPT_IN_PACKET_SIZE;
            g_cdcVcomCicEndpoints[i].interval      = HS_CDC_VCOM_INTERRUPT_IN_INTERVAL;
        }
        else {
            g_cdcVcomCicEndpoints[i].maxPacketSize = FS_CDC_VCOM_INTERRUPT_IN_PACKET_SIZE;
            g_cdcVcomCicEndpoints[i].interval      = FS_CDC_VCOM_INTERRUPT_IN_INTERVAL;
        }
    }

    for (int i = 0; i < USB_CDC_VCOM_DIC_ENDPOINT_COUNT; i++) {
        if (USB_SPEED_HIGH == speed) {
            if (g_cdcVcomDicEndpoints[i].endpointAddress
                & USB_DESCRIPTOR_ENDPOINT_ADDRESS_DIRECTION_MASK) {
                g_cdcVcomDicEndpoints[i].maxPacketSize = HS_CDC_VCOM_BULK_IN_PACKET_SIZE;
            }
            else {
                g_cdcVcomDicEndpoints[i].maxPacketSize = HS_CDC_VCOM_BULK_OUT_PACKET_SIZE;
            }
        }
        else {
            if (g_cdcVcomDicEndpoints[i].endpointAddress
                & USB_DESCRIPTOR_ENDPOINT_ADDRESS_DIRECTION_MASK) {
                g_cdcVcomDicEndpoints[i].maxPacketSize = FS_CDC_VCOM_BULK_IN_PACKET_SIZE;
            }
            else {
                g_cdcVcomDicEndpoints[i].maxPacketSize = FS_CDC_VCOM_BULK_OUT_PACKET_SIZE;
            }
        }
    }
#endif

#if defined(USB_CONFIG_COMPOSITE)
    for (uint8_t i = 0U; i < USB_HID_GENERIC_ENDPOINT_COUNT; i++) {
        if (USB_SPEED_HIGH == speed) {
            if (g_UsbDeviceHidEndpoints[i].endpointAddress
                & USB_DESCRIPTOR_ENDPOINT_ADDRESS_DIRECTION_MASK) {
                g_UsbDeviceHidEndpoints[i]
                    .maxPacketSize                  = HS_HID_GENERIC_INTERRUPT_IN_PACKET_SIZE;
                g_UsbDeviceHidEndpoints[i].interval = HS_HID_GENERIC_INTERRUPT_IN_INTERVAL;
            }
            else {
                g_UsbDeviceHidEndpoints[i]
                    .maxPacketSize                  = HS_HID_GENERIC_INTERRUPT_OUT_PACKET_SIZE;
                g_UsbDeviceHidEndpoints[i].interval = HS_HID_GENERIC_INTERRUPT_OUT_INTERVAL;
            }
        }
        else {
            if (g_UsbDeviceHidEndpoints[i].endpointAddress
                & USB_DESCRIPTOR_ENDPOINT_ADDRESS_DIRECTION_MASK) {
                g_UsbDeviceHidEndpoints[i]
                    .maxPacketSize                  = FS_HID_GENERIC_INTERRUPT_IN_PACKET_SIZE;
                g_UsbDeviceHidEndpoints[i].interval = FS_HID_GENERIC_INTERRUPT_IN_INTERVAL;
            }
            else {
                g_UsbDeviceHidEndpoints[i]
                    .maxPacketSize                  = FS_HID_GENERIC_INTERRUPT_OUT_PACKET_SIZE;
                g_UsbDeviceHidEndpoints[i].interval = FS_HID_GENERIC_INTERRUPT_OUT_INTERVAL;
            }
        }
    }
#ifdef USB_DEVICE_CONFIG_VENDOR_SPECIFIC
    for (uint8_t i = 0U; i < USB_NV_SMA_SPI_ENDPOINT_COUNT; i++) {
        if (USB_SPEED_HIGH == speed) {
            if (g_UsbDeviceSpiEndpoints[i].endpointAddress
                & USB_DESCRIPTOR_ENDPOINT_ADDRESS_DIRECTION_MASK) {
                g_UsbDeviceSpiEndpoints[i].maxPacketSize = HS_NV_SMA_SPI_CLASS_IN_PACKET_SIZE;
                g_UsbDeviceSpiEndpoints[i].interval      = HS_NV_SMA_SPI_CLASS_IN_INTERVAL;
            }
            else {
                g_UsbDeviceSpiEndpoints[i].maxPacketSize = HS_NV_SMA_SPI_CLASS_OUT_PACKET_SIZE;
                g_UsbDeviceSpiEndpoints[i].interval      = HS_NV_SMA_SPI_CLASS_OUT_INTERVAL;
            }
        }
        else {
            if (g_UsbDeviceSpiEndpoints[i].endpointAddress
                & USB_DESCRIPTOR_ENDPOINT_ADDRESS_DIRECTION_MASK) {
                g_UsbDeviceSpiEndpoints[i].maxPacketSize = FS_NV_SMA_SPI_CLASS_IN_PACKET_SIZE;
                g_UsbDeviceSpiEndpoints[i].interval      = FS_NV_SMA_SPI_CLASS_IN_INTERVAL;
            }
            else {
                g_UsbDeviceSpiEndpoints[i].maxPacketSize = FS_NV_SMA_SPI_CLASS_OUT_PACKET_SIZE;
                g_UsbDeviceSpiEndpoints[i].interval      = FS_NV_SMA_SPI_CLASS_OUT_INTERVAL;
            }
        }
    }
#endif
#endif

    return kStatus_USB_Success;
}

// NOLINTEND