/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES.
 * All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 *
 * USB Composite Device Descriptor
 * Supports multiple configurations: MCTP + optional HID, CDC-ECM, LSTP, CDC-ACM
 * Uses UsbDeviceVid/UsbDevicePid from project config.h (same as Core0)
 */

#include "usb_device_config.h"
#include "usb.h"
#include "usb_device.h"
#include "usb_device_class.h"
#include "ecm_usb_descriptor.h"
#include "eth_adapter.h"
#include <cstring>

/**
 * HID Report, Endpoint and Interface (CP2112 compatible)
 * Only compiled when USB_CONFIG_COMPOSITE is enabled
 */
#if USB_CONFIG_COMPOSITE
/*******************************************************************************
 * HID Report Descriptor (CP2112 compatible)
 ******************************************************************************/
USB_DMA_INIT_DATA_ALIGN(USB_DATA_ALIGN_SIZE)
uint8_t g_UsbDeviceHidReportDescriptor[] = {
    0x06,
    0x00,
    0xFF,  // Usage Page (Vendor Defined 0xFF00)
    0x09,
    0x01,  // Usage (Vendor Usage 1)
    0xA1,
    0x01,  // Collection (Application)

    // Feature Reports (required for CP2112 driver initialization)
    // Report ID 0x01: Reset Device
    0x85,
    0x01,  // Report ID (1)
    0x95,
    0x01,  // Report Count (1 byte)
    0x75,
    0x08,  // Report Size (8 bits)
    0x26,
    0xFF,
    0x00,  // Logical Maximum (255)
    0x15,
    0x00,  // Logical Minimum (0)
    0x09,
    0x01,  // Usage (Vendor Usage 1)
    0xB1,
    0x02,  // Feature (Data, Variable, Absolute)

    // Report ID 0x02: GetSetGPIO
    0x85,
    0x02,  // Report ID (2)
    0x95,
    0x04,  // Report Count (4 bytes)
    0x75,
    0x08,  // Report Size (8 bits)
    0x26,
    0xFF,
    0x00,  // Logical Maximum (255)
    0x15,
    0x00,  // Logical Minimum (0)
    0x09,
    0x01,  // Usage (Vendor Usage 1)
    0xB1,
    0x02,  // Feature (Data, Variable, Absolute)

    // Report ID 0x03: GetGPIO
    0x85,
    0x03,  // Report ID (3)
    0x95,
    0x01,  // Report Count (1 byte)
    0x75,
    0x08,  // Report Size (8 bits)
    0x26,
    0xFF,
    0x00,  // Logical Maximum (255)
    0x15,
    0x00,  // Logical Minimum (0)
    0x09,
    0x01,  // Usage (Vendor Usage 1)
    0xB1,
    0x02,  // Feature (Data, Variable, Absolute)

    // Report ID 0x04: SetGPIO
    0x85,
    0x04,  // Report ID (4)
    0x95,
    0x02,  // Report Count (2 bytes)
    0x75,
    0x08,  // Report Size (8 bits)
    0x26,
    0xFF,
    0x00,  // Logical Maximum (255)
    0x15,
    0x00,  // Logical Minimum (0)
    0x09,
    0x01,  // Usage (Vendor Usage 1)
    0xB1,
    0x02,  // Feature (Data, Variable, Absolute)

    // Report ID 0x05: GetVersionInfo
    0x85,
    0x05,  // Report ID (5)
    0x95,
    0x02,  // Report Count (2 bytes)
    0x75,
    0x08,  // Report Size (8 bits)
    0x26,
    0xFF,
    0x00,  // Logical Maximum (255)
    0x15,
    0x00,  // Logical Minimum (0)
    0x09,
    0x01,  // Usage (Vendor Usage 1)
    0xB1,
    0x02,  // Feature (Data, Variable, Absolute)

    // Report ID 0x06: GetSetSMBusConfig
    0x85,
    0x06,  // Report ID (6)
    0x95,
    0x0D,  // Report Count (13 bytes)
    0x75,
    0x08,  // Report Size (8 bits)
    0x26,
    0xFF,
    0x00,  // Logical Maximum (255)
    0x15,
    0x00,  // Logical Minimum (0)
    0x09,
    0x01,  // Usage (Vendor Usage 1)
    0xB1,
    0x02,  // Feature (Data, Variable, Absolute)

    // Output Reports (Host to Device)
    // Report ID 0x10: WriteRequest (SMBus Write)
    0x85,
    0x10,  // Report ID (16)
    0x95,
    0x3F,  // Report Count (63 bytes)
    0x75,
    0x08,  // Report Size (8 bits)
    0x26,
    0xFF,
    0x00,  // Logical Maximum (255)
    0x15,
    0x00,  // Logical Minimum (0)
    0x09,
    0x01,  // Usage (Vendor Usage 1)
    0x91,
    0x02,  // Output (Data, Variable, Absolute)

    // Report ID 0x11: WriteReadRequest
    0x85,
    0x11,  // Report ID (17)
    0x95,
    0x3F,  // Report Count (63 bytes)
    0x75,
    0x08,  // Report Size (8 bits)
    0x26,
    0xFF,
    0x00,  // Logical Maximum (255)
    0x15,
    0x00,  // Logical Minimum (0)
    0x09,
    0x01,  // Usage (Vendor Usage 1)
    0x91,
    0x02,  // Output (Data, Variable, Absolute)

    // Report ID 0x12: ReadRequest
    0x85,
    0x12,  // Report ID (18)
    0x95,
    0x3F,  // Report Count (63 bytes)
    0x75,
    0x08,  // Report Size (8 bits)
    0x26,
    0xFF,
    0x00,  // Logical Maximum (255)
    0x15,
    0x00,  // Logical Minimum (0)
    0x09,
    0x01,  // Usage (Vendor Usage 1)
    0x91,
    0x02,  // Output (Data, Variable, Absolute)

    // Input Reports (Device to Host)
    // Report ID 0x13: ReadResponse
    0x85,
    0x13,  // Report ID (19)
    0x95,
    0x3F,  // Report Count (63 bytes)
    0x75,
    0x08,  // Report Size (8 bits)
    0x26,
    0xFF,
    0x00,  // Logical Maximum (255)
    0x15,
    0x00,  // Logical Minimum (0)
    0x09,
    0x01,  // Usage (Vendor Usage 1)
    0x81,
    0x02,  // Input (Data, Variable, Absolute)

    // Report ID 0x14: DataReadForceSend (host forces pending read data)
    0x85,
    0x14,  // Report ID (20)
    0x95,
    0x3F,  // Report Count (63 bytes)
    0x75,
    0x08,  // Report Size (8 bits)
    0x26,
    0xFF,
    0x00,  // Logical Maximum (255)
    0x15,
    0x00,  // Logical Minimum (0)
    0x09,
    0x01,  // Usage (Vendor Usage 1)
    0x91,
    0x02,  // Output (Data, Variable, Absolute)

    // Report ID 0x15: TransferStatusRequest
    0x85,
    0x15,  // Report ID (21)
    0x95,
    0x3F,  // Report Count (63 bytes)
    0x75,
    0x08,  // Report Size (8 bits)
    0x26,
    0xFF,
    0x00,  // Logical Maximum (255)
    0x15,
    0x00,  // Logical Minimum (0)
    0x09,
    0x01,  // Usage (Vendor Usage 1)
    0x91,
    0x02,  // Output (Data, Variable, Absolute)

    // Report ID 0x16: TransferStatusResponse
    0x85,
    0x16,  // Report ID (22)
    0x95,
    0x3F,  // Report Count (63 bytes)
    0x75,
    0x08,  // Report Size (8 bits)
    0x26,
    0xFF,
    0x00,  // Logical Maximum (255)
    0x15,
    0x00,  // Logical Minimum (0)
    0x09,
    0x01,  // Usage (Vendor Usage 1)
    0x81,
    0x02,  // Input (Data, Variable, Absolute)

    // Report ID 0x17: CancelTransfer
    0x85,
    0x17,  // Report ID (23)
    0x95,
    0x3F,  // Report Count (63 bytes)
    0x75,
    0x08,  // Report Size (8 bits)
    0x26,
    0xFF,
    0x00,  // Logical Maximum (255)
    0x15,
    0x00,  // Logical Minimum (0)
    0x09,
    0x01,  // Usage (Vendor Usage 1)
    0x91,
    0x02,  // Output (Data, Variable, Absolute)

    // Report ID 0x20: USB Lock
    0x85,
    0x20,  // Report ID (32)
    0x95,
    0x01,  // Report Count (1 byte)
    0x75,
    0x08,  // Report Size (8 bits)
    0x26,
    0xFF,
    0x00,  // Logical Maximum (255)
    0x15,
    0x00,  // Logical Minimum (0)
    0x09,
    0x01,  // Usage (Vendor Usage 1)
    0xB1,
    0x02,  // Feature (Data, Variable, Absolute)

    0xC0  // End Collection
};

uint32_t g_UsbDeviceHidReportDescriptorLength = sizeof(g_UsbDeviceHidReportDescriptor);

/*******************************************************************************
 * HID Endpoints and Interface
 ******************************************************************************/
static usb_device_endpoint_struct_t s_hid_endpoints[] = {
    {
     USB_HID_ENDPOINT_IN | (USB_IN << 7U),
     USB_ENDPOINT_INTERRUPT,  FS_HID_INTERRUPT_IN_PACKET_SIZE,
     FS_HID_INTERRUPT_IN_INTERVAL, },
    {
     USB_HID_ENDPOINT_OUT | (USB_OUT << 7U),
     USB_ENDPOINT_INTERRUPT, FS_HID_INTERRUPT_OUT_PACKET_SIZE,
     FS_HID_INTERRUPT_OUT_INTERVAL, },
};
#endif /* USB_CONFIG_COMPOSITE */

/*******************************************************************************
 * CDC-ECM Endpoints and Interface
 * Only compiled when USB_DEVICE_CONFIG_CDC_ECM is enabled
 ******************************************************************************/
#if USB_DEVICE_CONFIG_CDC_ECM

static usb_device_endpoint_struct_t s_ecm_comm_endpoints[] = {
    {
     USB_DEVICE_CDC_ECM_COMM_INTERRUPT_IN_EP_NUMBER | (USB_IN << 7U),
     USB_ENDPOINT_INTERRUPT, USB_DEVICE_CDC_ECM_COMM_INTERRUPT_IN_EP_MAXPKT_SIZE,
     USB_DEVICE_CDC_ECM_COMM_INTERRUPT_IN_EP_INTERVAL_FS, },
};

static usb_device_endpoint_struct_t s_ecm_data_endpoints[] = {
    {
     USB_DEVICE_CDC_ECM_DATA_BULK_IN_EP_NUMBER | (USB_IN << 7U),
     USB_ENDPOINT_BULK,  USB_DEVICE_CDC_ECM_DATA_BULK_IN_EP_MAXPKT_SIZE_FS,
     USB_DEVICE_CDC_ECM_DATA_BULK_IN_EP_INTERVAL, },
    {
     USB_DEVICE_CDC_ECM_DATA_BULK_OUT_EP_NUMBER | (USB_OUT << 7U),
     USB_ENDPOINT_BULK, USB_DEVICE_CDC_ECM_DATA_BULK_OUT_EP_MAXPKT_SIZE_FS,
     USB_DEVICE_CDC_ECM_DATA_BULK_OUT_EP_INTERVAL, },
};

#endif  // USB_DEVICE_CONFIG_CDC_ECM

#if USB_CONFIG_COMPOSITE
/*******************************************************************************
 * HID Class Structures (for SDK class driver support)
 ******************************************************************************/
static usb_device_interface_struct_t s_hid_interface[] = {
    {
     USB_HID_INTERFACE_ALTERNATE, {USB_HID_ENDPOINT_COUNT, s_hid_endpoints},
     NULL, },
};

static usb_device_interfaces_struct_t s_hid_interfaces[] = {
    {
     USB_HID_GENERIC_CLASS, USB_HID_GENERIC_SUBCLASS,
     USB_HID_GENERIC_PROTOCOL, USB_HID_INTERFACE_INDEX,
     s_hid_interface, 1U,
     },
};

static usb_device_interface_list_t s_hid_configurations[] = {
    {
     1U, s_hid_interfaces,
     },
};

usb_device_class_struct_t g_ecm_hid_class = {
    s_hid_configurations,
    kUSB_DeviceClassTypeHid,
    USB_DEVICE_CONFIGURATION_COUNT,
};
#endif /* USB_CONFIG_COMPOSITE */

/*******************************************************************************
 * CDC-ECM Class Structures
 * Only compiled when USB_DEVICE_CONFIG_CDC_ECM is enabled
 ******************************************************************************/
#if USB_DEVICE_CONFIG_CDC_ECM

static usb_device_interface_struct_t s_ecm_comm_interface[] = {
    {
     0U, {1U, s_ecm_comm_endpoints},
     NULL, },
};

static usb_device_interface_struct_t s_ecm_data_interface[] = {
    {
     USB_DEVICE_CDC_ECM_DATA_INTERFACE_ALTERNATE0, {USB_DEVICE_CDC_ECM_DATA_ENDPOINT_NUMBER, s_ecm_data_endpoints},
     NULL, },
};

static usb_device_interfaces_struct_t s_ecm_interfaces[] = {
    {
     USB_DEVICE_CDC_ECM_COMM_INTERFACE_CLASS_CODE, USB_DEVICE_CDC_ECM_COMM_INTERFACE_SUBCLASS_CODE,
     USB_DEVICE_CDC_ECM_COMM_INTERFACE_PROTOCOL_CODE, USB_DEVICE_CDC_ECM_COMM_INTERFACE_NUMBER,
     s_ecm_comm_interface, 1U,
     },
    {
     USB_DEVICE_CDC_ECM_DATA_INTERFACE_CLASS_CODE, USB_DEVICE_CDC_ECM_DATA_INTERFACE_SUBCLASS_CODE,
     USB_DEVICE_CDC_ECM_DATA_INTERFACE_PROTOCOL_CODE, USB_DEVICE_CDC_ECM_DATA_INTERFACE_NUMBER,
     s_ecm_data_interface, 1U,
     },
};

static usb_device_interface_list_t s_ecm_configurations[] = {
    {
     2U, s_ecm_interfaces,
     },
};

usb_device_class_struct_t g_ecm_cdc_class = {
    s_ecm_configurations,
    kUSB_DeviceClassTypeCdcEcm,
    USB_DEVICE_CONFIGURATION_COUNT,
};

#endif  // USB_DEVICE_CONFIG_CDC_ECM

/*******************************************************************************
 * CDC-ACM Endpoints and Interface (UART Bridge)
 * Only compiled when USB_DEVICE_CONFIG_CDC_ACM is enabled
 ******************************************************************************/
#if USB_DEVICE_CONFIG_CDC_ACM

static usb_device_endpoint_struct_t s_acm_comm_endpoints[] = {
    {
     USB_DEVICE_CDC_ACM_COMM_INTERRUPT_IN_EP_NUMBER | (USB_IN << 7U),
     USB_ENDPOINT_INTERRUPT, USB_DEVICE_CDC_ACM_COMM_INTERRUPT_IN_EP_MAXPKT_SIZE,
     USB_DEVICE_CDC_ACM_COMM_INTERRUPT_IN_EP_INTERVAL_FS, },
};

static usb_device_endpoint_struct_t s_acm_data_endpoints[] = {
    {
     USB_DEVICE_CDC_ACM_DATA_BULK_IN_EP_NUMBER | (USB_IN << 7U),
     USB_ENDPOINT_BULK,  USB_DEVICE_CDC_ACM_DATA_BULK_IN_EP_MAXPKT_SIZE_FS,
     USB_DEVICE_CDC_ACM_DATA_BULK_IN_EP_INTERVAL, },
    {
     USB_DEVICE_CDC_ACM_DATA_BULK_OUT_EP_NUMBER | (USB_OUT << 7U),
     USB_ENDPOINT_BULK, USB_DEVICE_CDC_ACM_DATA_BULK_OUT_EP_MAXPKT_SIZE_FS,
     USB_DEVICE_CDC_ACM_DATA_BULK_OUT_EP_INTERVAL, },
};

/*******************************************************************************
 * CDC-ACM Class Structures
 ******************************************************************************/
static usb_device_interface_struct_t s_acm_comm_interface[] = {
    {
     USB_DEVICE_CDC_ACM_COMM_INTERFACE_ALTERNATE, {USB_DEVICE_CDC_ACM_COMM_ENDPOINT_NUMBER, s_acm_comm_endpoints},
     NULL, },
};

static usb_device_interface_struct_t s_acm_data_interface[] = {
    {
     USB_DEVICE_CDC_ACM_DATA_INTERFACE_ALTERNATE, {USB_DEVICE_CDC_ACM_DATA_ENDPOINT_NUMBER, s_acm_data_endpoints},
     NULL, },
};

static usb_device_interfaces_struct_t s_acm_interfaces[] = {
    {
     USB_DEVICE_CDC_ACM_COMM_CLASS_CODE, USB_DEVICE_CDC_ACM_COMM_SUBCLASS_CODE,
     USB_DEVICE_CDC_ACM_COMM_PROTOCOL_CODE, USB_DEVICE_CDC_ACM_COMM_INTERFACE_NUMBER,
     s_acm_comm_interface, 1U,
     },
    {
     USB_DEVICE_CDC_ACM_DATA_CLASS_CODE, USB_DEVICE_CDC_ACM_DATA_SUBCLASS_CODE,
     USB_DEVICE_CDC_ACM_DATA_PROTOCOL_CODE, USB_DEVICE_CDC_ACM_DATA_INTERFACE_NUMBER,
     s_acm_data_interface, 1U,
     },
};

static usb_device_interface_list_t s_acm_configurations[] = {
    {
     USB_DEVICE_CDC_ACM_INTERFACE_COUNT, s_acm_interfaces,
     },
};

usb_device_class_struct_t g_acm_cdc_class = {
    s_acm_configurations,
    kUSB_DeviceClassTypeCdc,
    USB_DEVICE_CONFIGURATION_COUNT,
};

#endif /* USB_DEVICE_CONFIG_CDC_ACM */

/*******************************************************************************
 * Configuration Descriptor Total Length Calculation
 ******************************************************************************/

#define USB_MCTP_DESC_LENGTH                                                                   \
    (USB_DESCRIPTOR_LENGTH_INTERFACE + 2 * USB_DESCRIPTOR_LENGTH_ENDPOINT)

#if USB_CONFIG_COMPOSITE
#define USB_HID_DESC_LENGTH                                                                    \
    (USB_DESCRIPTOR_LENGTH_INTERFACE + USB_DESCRIPTOR_LENGTH_HID                               \
     + 2 * USB_DESCRIPTOR_LENGTH_ENDPOINT)
#else
#define USB_HID_DESC_LENGTH 0
#endif

/* Additional length for CDC-ECM (IAD + Comm IF + functional descs + Data IF + endpoints) */
#if USB_DEVICE_CONFIG_CDC_ECM
#define USB_CDC_ECM_DESC_LENGTH                                                                \
    (USB_DESCRIPTOR_LENGTH_IAD + USB_DESCRIPTOR_LENGTH_INTERFACE + USB_DEVICE_CDC_FUNC_LENGTH  \
     + USB_DEVICE_CDC_FUNC_UNION_LENGTH + USB_DEVICE_CDC_ECM_CLASS_DESCRIPTOR_LENGTH           \
     + USB_DESCRIPTOR_LENGTH_ENDPOINT  /* CDC-ECM Data Interface (alt 0) */                    \
     + USB_DESCRIPTOR_LENGTH_INTERFACE /* CDC-ECM Data Interface (alt 1) */                    \
     + USB_DESCRIPTOR_LENGTH_INTERFACE + 2 * USB_DESCRIPTOR_LENGTH_ENDPOINT)
#else
#define USB_CDC_ECM_DESC_LENGTH 0
#endif /* USB_DEVICE_CONFIG_CDC_ECM */

#if USB_CONFIG_LSTP
#define USB_LSTP_DESC_LENGTH                                                                   \
    (USB_DESCRIPTOR_LENGTH_INTERFACE + 2 * USB_DESCRIPTOR_LENGTH_ENDPOINT)
#else
#define USB_LSTP_DESC_LENGTH 0
#endif /* USB_CONFIG_LSTP */

/* CDC-ACM functional descriptor lengths */
#if USB_DEVICE_CONFIG_CDC_ACM
#define USB_DEVICE_CDC_ACM_FUNC_HEADER_LENGTH (0x05U) /* CDC Header */
#define USB_DEVICE_CDC_ACM_FUNC_CM_LENGTH     (0x05U) /* Call Management */
#define USB_DEVICE_CDC_ACM_FUNC_ACM_LENGTH    (0x04U) /* Abstract Control Management */
#define USB_DEVICE_CDC_ACM_FUNC_UNION_LENGTH  (0x05U) /* Union */

/* Additional length for CDC-ACM (IAD + Comm IF + functional descs + Data IF + endpoints) */
#define USB_CDC_ACM_DESC_LENGTH                                                                \
    (USB_DESCRIPTOR_LENGTH_IAD + USB_DESCRIPTOR_LENGTH_INTERFACE                               \
     + USB_DEVICE_CDC_ACM_FUNC_HEADER_LENGTH + USB_DEVICE_CDC_ACM_FUNC_CM_LENGTH               \
     + USB_DEVICE_CDC_ACM_FUNC_ACM_LENGTH + USB_DEVICE_CDC_ACM_FUNC_UNION_LENGTH               \
     + USB_DESCRIPTOR_LENGTH_ENDPOINT + USB_DESCRIPTOR_LENGTH_INTERFACE                        \
     + 2 * USB_DESCRIPTOR_LENGTH_ENDPOINT)
#else
#define USB_CDC_ACM_DESC_LENGTH 0
#endif /* USB_DEVICE_CONFIG_CDC_ACM */

#define USB_COMPOSITE_CONFIG_DESC_LENGTH                                                       \
    (USB_DESCRIPTOR_LENGTH_CONFIGURE + USB_MCTP_DESC_LENGTH + USB_HID_DESC_LENGTH              \
     + USB_CDC_ECM_DESC_LENGTH + USB_LSTP_DESC_LENGTH + USB_CDC_ACM_DESC_LENGTH)

/* Byte offset where ECM descriptors begin (for runtime splice) */
#define USB_DESC_OFFSET_CDC_ECM                                                                \
    (USB_DESCRIPTOR_LENGTH_CONFIGURE + USB_MCTP_DESC_LENGTH + USB_HID_DESC_LENGTH)

#if USB_DEVICE_CONFIG_CDC_ECM
#define USB_COMPOSITE_CONFIG_DESC_LENGTH_NO_ECM                                                \
    (USB_COMPOSITE_CONFIG_DESC_LENGTH - USB_CDC_ECM_DESC_LENGTH)
#define USB_COMPOSITE_INTERFACE_COUNT_NO_ECM                                                   \
    (USB_COMPOSITE_INTERFACE_COUNT - USB_CDC_ECM_INTERFACE_COUNT)
#endif

/*******************************************************************************
 * Device Descriptor - Uses UsbDeviceVid/UsbDevicePid from config.h
 ******************************************************************************/
USB_DMA_INIT_DATA_ALIGN(USB_DATA_ALIGN_SIZE)
static uint8_t s_device_descriptor[] = {
    USB_DESCRIPTOR_LENGTH_DEVICE,
    USB_DESCRIPTOR_TYPE_DEVICE,
    USB_SHORT_GET_LOW(USB_DEVICE_SPECIFIC_BCD_VERSION),
    USB_SHORT_GET_HIGH(USB_DEVICE_SPECIFIC_BCD_VERSION),
    USB_DEVICE_CLASS,     // 0xEF - Miscellaneous
    USB_DEVICE_SUBCLASS,  // 0x02 - Common Class
    USB_DEVICE_PROTOCOL,  // 0x01 - IAD Protocol
    USB_CONTROL_MAX_PACKET_SIZE,
    USB_SHORT_GET_LOW(nv::ipc::UsbDeviceVid),
    USB_SHORT_GET_HIGH(nv::ipc::UsbDeviceVid),
    USB_SHORT_GET_LOW(nv::ipc::UsbDevicePid),
    USB_SHORT_GET_HIGH(nv::ipc::UsbDevicePid),
    USB_SHORT_GET_LOW(USB_DEVICE_DEMO_BCD_VERSION),
    USB_SHORT_GET_HIGH(USB_DEVICE_DEMO_BCD_VERSION),
    USB_DEVICE_MANUFACTURER_STRING_INDEX,
    USB_DEVICE_PRODUCT_STRING_INDEX,
    USB_DEVICE_SERIAL_NUMBER_STRING_INDEX,
    USB_DEVICE_CONFIGURATION_COUNT,
};

/*******************************************************************************
 * Configuration Descriptor - Composite: MCTP + HID + CDC-ECM + LSTP + CDC-ACM
 * Stored in flash; active descriptor used by USB is in s_config_buffer (RAM).
 ******************************************************************************/
static const uint8_t s_config_descriptor[] = {
    // Configuration Descriptor
    USB_DESCRIPTOR_LENGTH_CONFIGURE,
    USB_DESCRIPTOR_TYPE_CONFIGURE,
    USB_SHORT_GET_LOW(USB_COMPOSITE_CONFIG_DESC_LENGTH),
    USB_SHORT_GET_HIGH(USB_COMPOSITE_CONFIG_DESC_LENGTH),
    USB_COMPOSITE_INTERFACE_COUNT,
    USB_COMPOSITE_CONFIGURE_INDEX,
    0x00U,
    (USB_DESCRIPTOR_CONFIGURE_ATTRIBUTE_D7_MASK)
        | (USB_DESCRIPTOR_CONFIGURE_ATTRIBUTE_SELF_POWERED_MASK),
    USB_DEVICE_MAX_POWER,

    // ========== Interface 0: MCTP ==========
    USB_DESCRIPTOR_LENGTH_INTERFACE,
    USB_DESCRIPTOR_TYPE_INTERFACE,
    USB_MCTP_INTERFACE_INDEX,  // 0
    USB_MCTP_INTERFACE_ALTERNATE_0,
    USB_MCTP_ENDPOINT_COUNT,  // 2
    USB_MCTP_GENERIC_CLASS,   // 0x14
    USB_MCTP_GENERIC_SUBCLASS,
    USB_MCTP_GENERIC_PROTOCOL,
    0x00U,

    // MCTP Bulk IN Endpoint (EP1 IN)
    USB_DESCRIPTOR_LENGTH_ENDPOINT,
    USB_DESCRIPTOR_TYPE_ENDPOINT,
    USB_MCTP_ENDPOINT_IN | (USB_IN << 7U),
    USB_ENDPOINT_BULK,
    USB_SHORT_GET_LOW(HS_MCTP_CLASS_IN_PACKET_SIZE),
    USB_SHORT_GET_HIGH(HS_MCTP_CLASS_IN_PACKET_SIZE),
    0x00U,

    // MCTP Bulk OUT Endpoint (EP1 OUT)
    USB_DESCRIPTOR_LENGTH_ENDPOINT,
    USB_DESCRIPTOR_TYPE_ENDPOINT,
    USB_MCTP_ENDPOINT_OUT | (USB_OUT << 7U),
    USB_ENDPOINT_BULK,
    USB_SHORT_GET_LOW(HS_MCTP_CLASS_OUT_PACKET_SIZE),
    USB_SHORT_GET_HIGH(HS_MCTP_CLASS_OUT_PACKET_SIZE),
    0x00U,

#if USB_CONFIG_COMPOSITE
    // ========== Interface 1: HID ==========
    USB_DESCRIPTOR_LENGTH_INTERFACE,
    USB_DESCRIPTOR_TYPE_INTERFACE,
    USB_HID_INTERFACE_INDEX,  // 1
    USB_HID_INTERFACE_ALTERNATE,
    USB_HID_ENDPOINT_COUNT,  // 2
    USB_HID_GENERIC_CLASS,   // 0x03
    USB_HID_GENERIC_SUBCLASS,
    USB_HID_GENERIC_PROTOCOL,
    0x00U,

    // HID Descriptor
    USB_DESCRIPTOR_LENGTH_HID,
    0x21U,  // HID descriptor type
    0x01U,
    0x01U,  // HID version 1.01
    0x00U,  // Country code
    0x01U,  // Number of HID descriptors
    0x22U,  // Report descriptor type
    USB_SHORT_GET_LOW(sizeof(g_UsbDeviceHidReportDescriptor)),
    USB_SHORT_GET_HIGH(sizeof(g_UsbDeviceHidReportDescriptor)),

    // HID Interrupt IN Endpoint (EP2)
    USB_DESCRIPTOR_LENGTH_ENDPOINT,
    USB_DESCRIPTOR_TYPE_ENDPOINT,
    USB_HID_ENDPOINT_IN | (USB_IN << 7U),
    USB_ENDPOINT_INTERRUPT,
    USB_SHORT_GET_LOW(HS_HID_INTERRUPT_IN_PACKET_SIZE),
    USB_SHORT_GET_HIGH(HS_HID_INTERRUPT_IN_PACKET_SIZE),
    HS_HID_INTERRUPT_IN_INTERVAL,

    // HID Interrupt OUT Endpoint (EP2)
    USB_DESCRIPTOR_LENGTH_ENDPOINT,
    USB_DESCRIPTOR_TYPE_ENDPOINT,
    USB_HID_ENDPOINT_OUT | (USB_OUT << 7U),
    USB_ENDPOINT_INTERRUPT,
    USB_SHORT_GET_LOW(HS_HID_INTERRUPT_OUT_PACKET_SIZE),
    USB_SHORT_GET_HIGH(HS_HID_INTERRUPT_OUT_PACKET_SIZE),
    HS_HID_INTERRUPT_OUT_INTERVAL,
#endif /* USB_CONFIG_COMPOSITE */

#if USB_DEVICE_CONFIG_CDC_ECM
    // ========== IAD for CDC-ECM (Interfaces 2-3) ==========
    USB_DESCRIPTOR_LENGTH_IAD,
    0x0BU,                                     // IAD descriptor type
    USB_DEVICE_CDC_ECM_COMM_INTERFACE_NUMBER,  // First interface (2)
    USB_DEVICE_CDC_ECM_INTERFACE_COUNT,        // Interface count (2)
    USB_DEVICE_CDC_CLASS_COMM_CODE,            // 0x02 - Communications
    USB_DEVICE_CDC_ECM_SUBCLASS_CODE,          // 0x06 - Ethernet Networking
    USB_DEVICE_CDC_ECM_PROTOCOL_CODE,          // 0x00
    0x00U,

    // ========== Interface 2: CDC-ECM Communication ==========
    USB_DESCRIPTOR_LENGTH_INTERFACE,
    USB_DESCRIPTOR_TYPE_INTERFACE,
    USB_DEVICE_CDC_ECM_COMM_INTERFACE_NUMBER,  // 2
    USB_DEVICE_CDC_ECM_COMM_INTERFACE_ALTERNATE,
    USB_DEVICE_CDC_ECM_COMM_ENDPOINT_NUMBER,  // 1
    USB_DEVICE_CDC_ECM_COMM_INTERFACE_CLASS_CODE,
    USB_DEVICE_CDC_ECM_COMM_INTERFACE_SUBCLASS_CODE,
    USB_DEVICE_CDC_ECM_COMM_INTERFACE_PROTOCOL_CODE,
    0x00U,

    // CDC Header Functional Descriptor
    USB_DEVICE_CDC_FUNC_LENGTH,
    USB_DEVICE_CDC_FUNC_TYPE_CS_INTERFACE,
    USB_DEVICE_CDC_FUNC_SUBTYPE_HEADER,
    0x10U,
    0x01U,  // CDC 1.10

    // CDC Union Functional Descriptor
    USB_DEVICE_CDC_FUNC_UNION_LENGTH,
    USB_DEVICE_CDC_FUNC_UNION_TYPE,
    USB_DEVICE_CDC_FUNC_UNION_SUBTYPE,
    USB_DEVICE_CDC_ECM_COMM_INTERFACE_NUMBER,  // Master interface (2)
    USB_DEVICE_CDC_ECM_DATA_INTERFACE_NUMBER,  // Slave interface (3)

    // CDC ECM Functional Descriptor
    USB_DEVICE_CDC_ECM_CLASS_DESCRIPTOR_LENGTH,
    USB_DEVICE_CDC_ECM_CLASS_DESCRIPTOR_TYPE,
    USB_DEVICE_CDC_ECM_CLASS_DESCRIPTOR_SUBTYPE,
    USB_DEVICE_CDC_ECM_CLASS_DESCRIPTOR_MAC_ADDRESS_STRING_INDEX,
    USB_LONG_GET_BYTE0(USB_DEVICE_CDC_ECM_CLASS_DESCRIPTOR_ETHERNET_STATISTICS),
    USB_LONG_GET_BYTE1(USB_DEVICE_CDC_ECM_CLASS_DESCRIPTOR_ETHERNET_STATISTICS),
    USB_LONG_GET_BYTE2(USB_DEVICE_CDC_ECM_CLASS_DESCRIPTOR_ETHERNET_STATISTICS),
    USB_LONG_GET_BYTE3(USB_DEVICE_CDC_ECM_CLASS_DESCRIPTOR_ETHERNET_STATISTICS),
    USB_SHORT_GET_LOW(USB_DEVICE_CDC_ECM_CLASS_DESCRIPTOR_MAX_SEGMENT_SIZE),
    USB_SHORT_GET_HIGH(USB_DEVICE_CDC_ECM_CLASS_DESCRIPTOR_MAX_SEGMENT_SIZE),
    USB_SHORT_GET_LOW(USB_DEVICE_CDC_ECM_CLASS_DESCRIPTOR_MULTICAST_FILTERS_NUMBER),
    USB_SHORT_GET_HIGH(USB_DEVICE_CDC_ECM_CLASS_DESCRIPTOR_MULTICAST_FILTERS_NUMBER),
    USB_DEVICE_CDC_ECM_CLASS_DESCRIPTOR_POWER_FILTERS_NUMBER,

    // Notification Endpoint (EP3)
    USB_DESCRIPTOR_LENGTH_ENDPOINT,
    USB_DESCRIPTOR_TYPE_ENDPOINT,
    USB_DEVICE_CDC_ECM_COMM_INTERRUPT_IN_EP_NUMBER | (USB_IN << 7U),
    USB_ENDPOINT_INTERRUPT,
    USB_SHORT_GET_LOW(USB_DEVICE_CDC_ECM_COMM_INTERRUPT_IN_EP_MAXPKT_SIZE),
    USB_SHORT_GET_HIGH(USB_DEVICE_CDC_ECM_COMM_INTERRUPT_IN_EP_MAXPKT_SIZE),
    USB_DEVICE_CDC_ECM_COMM_INTERRUPT_IN_EP_INTERVAL_FS,

    // ========== Interface 3: CDC-ECM Data (Alternate 0 - no endpoints) ==========
    USB_DESCRIPTOR_LENGTH_INTERFACE,
    USB_DESCRIPTOR_TYPE_INTERFACE,
    USB_DEVICE_CDC_ECM_DATA_INTERFACE_NUMBER,  // 3
    USB_DEVICE_CDC_ECM_DATA_INTERFACE_ALTERNATE0,
    0x00U,  // 0 endpoints
    USB_DEVICE_CDC_ECM_DATA_INTERFACE_CLASS_CODE,
    USB_DEVICE_CDC_ECM_DATA_INTERFACE_SUBCLASS_CODE,
    USB_DEVICE_CDC_ECM_DATA_INTERFACE_PROTOCOL_CODE,
    0x00U,

    // ========== Interface 3: CDC-ECM Data (Alternate 1 - with endpoints) ==========
    USB_DESCRIPTOR_LENGTH_INTERFACE,
    USB_DESCRIPTOR_TYPE_INTERFACE,
    USB_DEVICE_CDC_ECM_DATA_INTERFACE_NUMBER,  // 3
    USB_DEVICE_CDC_ECM_DATA_INTERFACE_ALTERNATE1,
    USB_DEVICE_CDC_ECM_DATA_ENDPOINT_NUMBER,  // 2
    USB_DEVICE_CDC_ECM_DATA_INTERFACE_CLASS_CODE,
    USB_DEVICE_CDC_ECM_DATA_INTERFACE_SUBCLASS_CODE,
    USB_DEVICE_CDC_ECM_DATA_INTERFACE_PROTOCOL_CODE,
    0x00U,

    // Data Bulk IN Endpoint (EP4)
    USB_DESCRIPTOR_LENGTH_ENDPOINT,
    USB_DESCRIPTOR_TYPE_ENDPOINT,
    USB_DEVICE_CDC_ECM_DATA_BULK_IN_EP_NUMBER | (USB_IN << 7U),
    USB_ENDPOINT_BULK,
    USB_SHORT_GET_LOW(USB_DEVICE_CDC_ECM_DATA_BULK_IN_EP_MAXPKT_SIZE_HS),
    USB_SHORT_GET_HIGH(USB_DEVICE_CDC_ECM_DATA_BULK_IN_EP_MAXPKT_SIZE_HS),
    USB_DEVICE_CDC_ECM_DATA_BULK_IN_EP_INTERVAL,

    // Data Bulk OUT Endpoint (EP5)
    USB_DESCRIPTOR_LENGTH_ENDPOINT,
    USB_DESCRIPTOR_TYPE_ENDPOINT,
    USB_DEVICE_CDC_ECM_DATA_BULK_OUT_EP_NUMBER | (USB_OUT << 7U),
    USB_ENDPOINT_BULK,
    USB_SHORT_GET_LOW(USB_DEVICE_CDC_ECM_DATA_BULK_OUT_EP_MAXPKT_SIZE_HS),
    USB_SHORT_GET_HIGH(USB_DEVICE_CDC_ECM_DATA_BULK_OUT_EP_MAXPKT_SIZE_HS),
    USB_DEVICE_CDC_ECM_DATA_BULK_OUT_EP_INTERVAL,
#endif  // USB_DEVICE_CONFIG_CDC_ECM

#if USB_CONFIG_LSTP
    // ========== LSTP Interface ==========
    USB_DESCRIPTOR_LENGTH_INTERFACE,
    USB_DESCRIPTOR_TYPE_INTERFACE,
    USB_LSTP_INTERFACE_INDEX,
    USB_LSTP_INTERFACE_ALTERNATE_0,
    USB_LSTP_ENDPOINT_COUNT,  // 2
    USB_LSTP_GENERIC_CLASS,   // 0xFF
    USB_LSTP_GENERIC_SUBCLASS,
    USB_LSTP_GENERIC_PROTOCOL,
    0x00U,

    // LSTP Bulk IN Endpoint
    USB_DESCRIPTOR_LENGTH_ENDPOINT,
    USB_DESCRIPTOR_TYPE_ENDPOINT,
    USB_LSTP_ENDPOINT_IN | (USB_IN << 7U),
    USB_ENDPOINT_BULK,
    USB_SHORT_GET_LOW(USB_LSTP_CLASS_IN_PACKET_SIZE),
    USB_SHORT_GET_HIGH(USB_LSTP_CLASS_IN_PACKET_SIZE),
    USB_LSTP_INTERVAL,

    // LSTP Bulk OUT Endpoint
    USB_DESCRIPTOR_LENGTH_ENDPOINT,
    USB_DESCRIPTOR_TYPE_ENDPOINT,
    USB_LSTP_ENDPOINT_OUT | (USB_OUT << 7U),
    USB_ENDPOINT_BULK,
    USB_SHORT_GET_LOW(USB_LSTP_CLASS_OUT_PACKET_SIZE),
    USB_SHORT_GET_HIGH(USB_LSTP_CLASS_OUT_PACKET_SIZE),
    USB_LSTP_INTERVAL,
#endif  // USB_CONFIG_LSTP

#if USB_DEVICE_CONFIG_CDC_ACM
    // ========== IAD for CDC-ACM (Interfaces 4-5) ==========
    USB_DESCRIPTOR_LENGTH_IAD,
    0x0BU,                                     // IAD descriptor type
    USB_DEVICE_CDC_ACM_COMM_INTERFACE_NUMBER,  // First interface (4)
    USB_DEVICE_CDC_ACM_INTERFACE_COUNT,        // Interface count (2)
    USB_DEVICE_CDC_ACM_COMM_CLASS_CODE,        // 0x02 - Communications
    USB_DEVICE_CDC_ACM_COMM_SUBCLASS_CODE,     // 0x02 - Abstract Control Model
    USB_DEVICE_CDC_ACM_COMM_PROTOCOL_CODE,     // 0x00
    0x00U,

    // ========== Interface 4: CDC-ACM Communication ==========
    USB_DESCRIPTOR_LENGTH_INTERFACE,
    USB_DESCRIPTOR_TYPE_INTERFACE,
    USB_DEVICE_CDC_ACM_COMM_INTERFACE_NUMBER,  // 4
    USB_DEVICE_CDC_ACM_COMM_INTERFACE_ALTERNATE,
    USB_DEVICE_CDC_ACM_COMM_ENDPOINT_NUMBER,  // 1
    USB_DEVICE_CDC_ACM_COMM_CLASS_CODE,
    USB_DEVICE_CDC_ACM_COMM_SUBCLASS_CODE,
    USB_DEVICE_CDC_ACM_COMM_PROTOCOL_CODE,
    0x00U,

    // CDC Header Functional Descriptor
    USB_DEVICE_CDC_ACM_FUNC_HEADER_LENGTH,
    USB_DEVICE_CDC_FUNC_TYPE_CS_INTERFACE,  // 0x24
    USB_DEVICE_CDC_FUNC_SUBTYPE_HEADER,     // 0x00
    0x10U,
    0x01U,  // CDC 1.10

    // CDC Call Management Functional Descriptor
    USB_DEVICE_CDC_ACM_FUNC_CM_LENGTH,
    USB_DEVICE_CDC_FUNC_TYPE_CS_INTERFACE,     // 0x24
    0x01U,                                     // Call Management subtype
    0x01U,                                     // bmCapabilities: device handles call management
    USB_DEVICE_CDC_ACM_DATA_INTERFACE_NUMBER,  // Data interface (5)

    // CDC Abstract Control Management Functional Descriptor
    USB_DEVICE_CDC_ACM_FUNC_ACM_LENGTH,
    USB_DEVICE_CDC_FUNC_TYPE_CS_INTERFACE,  // 0x24
    0x02U,                                  // ACM subtype
    0x02U,                                  // bmCapabilities: line coding and serial state

    // CDC Union Functional Descriptor
    USB_DEVICE_CDC_ACM_FUNC_UNION_LENGTH,
    USB_DEVICE_CDC_FUNC_TYPE_CS_INTERFACE,     // 0x24
    USB_DEVICE_CDC_FUNC_SUBTYPE_UNION_FUNC,    // 0x06
    USB_DEVICE_CDC_ACM_COMM_INTERFACE_NUMBER,  // Master interface (4)
    USB_DEVICE_CDC_ACM_DATA_INTERFACE_NUMBER,  // Slave interface (5)

    // ACM Notification Endpoint (EP6)
    USB_DESCRIPTOR_LENGTH_ENDPOINT,
    USB_DESCRIPTOR_TYPE_ENDPOINT,
    USB_DEVICE_CDC_ACM_COMM_INTERRUPT_IN_EP_NUMBER | (USB_IN << 7U),
    USB_ENDPOINT_INTERRUPT,
    USB_SHORT_GET_LOW(USB_DEVICE_CDC_ACM_COMM_INTERRUPT_IN_EP_MAXPKT_SIZE),
    USB_SHORT_GET_HIGH(USB_DEVICE_CDC_ACM_COMM_INTERRUPT_IN_EP_MAXPKT_SIZE),
    USB_DEVICE_CDC_ACM_COMM_INTERRUPT_IN_EP_INTERVAL_FS,

    // ========== Interface 5: CDC-ACM Data ==========
    USB_DESCRIPTOR_LENGTH_INTERFACE,
    USB_DESCRIPTOR_TYPE_INTERFACE,
    USB_DEVICE_CDC_ACM_DATA_INTERFACE_NUMBER,  // 5
    USB_DEVICE_CDC_ACM_DATA_INTERFACE_ALTERNATE,
    USB_DEVICE_CDC_ACM_DATA_ENDPOINT_NUMBER,  // 2
    USB_DEVICE_CDC_ACM_DATA_CLASS_CODE,
    USB_DEVICE_CDC_ACM_DATA_SUBCLASS_CODE,
    USB_DEVICE_CDC_ACM_DATA_PROTOCOL_CODE,
    0x00U,

    // ACM Data Bulk IN Endpoint (EP7)
    USB_DESCRIPTOR_LENGTH_ENDPOINT,
    USB_DESCRIPTOR_TYPE_ENDPOINT,
    USB_DEVICE_CDC_ACM_DATA_BULK_IN_EP_NUMBER | (USB_IN << 7U),
    USB_ENDPOINT_BULK,
    USB_SHORT_GET_LOW(USB_DEVICE_CDC_ACM_DATA_BULK_IN_EP_MAXPKT_SIZE_HS),
    USB_SHORT_GET_HIGH(USB_DEVICE_CDC_ACM_DATA_BULK_IN_EP_MAXPKT_SIZE_HS),
    USB_DEVICE_CDC_ACM_DATA_BULK_IN_EP_INTERVAL,

    // ACM Data Bulk OUT Endpoint (EP7)
    USB_DESCRIPTOR_LENGTH_ENDPOINT,
    USB_DESCRIPTOR_TYPE_ENDPOINT,
    USB_DEVICE_CDC_ACM_DATA_BULK_OUT_EP_NUMBER | (USB_OUT << 7U),
    USB_ENDPOINT_BULK,
    USB_SHORT_GET_LOW(USB_DEVICE_CDC_ACM_DATA_BULK_OUT_EP_MAXPKT_SIZE_HS),
    USB_SHORT_GET_HIGH(USB_DEVICE_CDC_ACM_DATA_BULK_OUT_EP_MAXPKT_SIZE_HS),
    USB_DEVICE_CDC_ACM_DATA_BULK_OUT_EP_INTERVAL,
#endif /* USB_DEVICE_CONFIG_CDC_ACM */
};

/* Single RAM buffer for config descriptor (DMA-able). */
USB_DMA_INIT_DATA_ALIGN(USB_DATA_ALIGN_SIZE)
static uint8_t s_config_buffer[USB_COMPOSITE_CONFIG_DESC_LENGTH];

/*******************************************************************************
 * String Descriptors
 ******************************************************************************/
USB_DMA_INIT_DATA_ALIGN(USB_DATA_ALIGN_SIZE)
static uint8_t s_lang_string[] = {
    4U,
    USB_DESCRIPTOR_TYPE_STRING,
    USB_SHORT_GET_LOW(USB_DEVICE_LANGID),
    USB_SHORT_GET_HIGH(USB_DEVICE_LANGID),
};

USB_DMA_INIT_DATA_ALIGN(USB_DATA_ALIGN_SIZE)
static uint8_t s_manufacturer_string[2U + sizeof(USB_DEVICE_MANUFACTURER_STRING) * 2] = {
    2U + sizeof(USB_DEVICE_MANUFACTURER_STRING) * 2 - 2,
    USB_DESCRIPTOR_TYPE_STRING,
};

USB_DMA_INIT_DATA_ALIGN(USB_DATA_ALIGN_SIZE)
static uint8_t s_product_string[2U + sizeof(USB_DEVICE_PRODUCT_STRING) * 2] = {
    2U + sizeof(USB_DEVICE_PRODUCT_STRING) * 2 - 2,
    USB_DESCRIPTOR_TYPE_STRING,
};

USB_DMA_INIT_DATA_ALIGN(USB_DATA_ALIGN_SIZE)
static uint8_t s_serial_string[2U + sizeof(USB_DEVICE_SERIAL_NUMBER_STRING) * 2] = {
    2U + sizeof(USB_DEVICE_SERIAL_NUMBER_STRING) * 2 - 2,
    USB_DESCRIPTOR_TYPE_STRING,
};

USB_DMA_INIT_DATA_ALIGN(USB_DATA_ALIGN_SIZE)
static uint8_t s_mac_string[2U + 12 * 2] = {
    2U + 12 * 2,
    USB_DESCRIPTOR_TYPE_STRING,
};

static uint8_t* s_string_descriptors[] = {
    s_lang_string,
    s_manufacturer_string,
    s_product_string,
    s_serial_string,
    s_mac_string,
};

static uint32_t s_string_lengths[] = {
    sizeof(s_lang_string),
    sizeof(s_manufacturer_string),
    sizeof(s_product_string),
    sizeof(s_serial_string),
    sizeof(s_mac_string),
};

/*******************************************************************************
 * Code
 ******************************************************************************/

static void ascii_to_unicode(uint8_t* buffer, const char* str)
{
    uint16_t* p = reinterpret_cast<uint16_t*>(buffer);
    while (*str) {
        *p++ = static_cast<uint16_t>(*str++);
    }
}

#if USB_DEVICE_CONFIG_CDC_ECM
static void mac_to_unicode(uint8_t* buffer, const uint8_t* mac)
{
    static const char hex[] = "0123456789ABCDEF";
    uint16_t*         p     = reinterpret_cast<uint16_t*>(buffer);

    for (int i = 0; i < 6; i++) {
        *p++ = hex[(mac[i] >> 4) & 0x0F];
        *p++ = hex[mac[i] & 0x0F];
    }
}
#endif  // USB_DEVICE_CONFIG_CDC_ECM

void ECM_USB_FillStringDescriptorBuffer(void)
{
    ascii_to_unicode(s_manufacturer_string + 2, USB_DEVICE_MANUFACTURER_STRING);
    ascii_to_unicode(s_product_string + 2, USB_DEVICE_PRODUCT_STRING);
    ascii_to_unicode(s_serial_string + 2, USB_DEVICE_SERIAL_NUMBER_STRING);

#if USB_DEVICE_CONFIG_CDC_ECM
    uint8_t mac[6] = ETH_ADAPTER_MAC_ADDRESS;
    ETH_ADAPTER_GetMacAddress(mac);
    mac_to_unicode(s_mac_string + 2, mac);
#endif  // USB_DEVICE_CONFIG_CDC_ECM
}

usb_status_t ECM_USB_DeviceSetSpeed(usb_device_handle handle, uint8_t speed)
{
    if (speed == USB_SPEED_HIGH) {
#if USB_CONFIG_COMPOSITE
        // High speed settings - HID
        s_hid_endpoints[0].maxPacketSize = HS_HID_INTERRUPT_IN_PACKET_SIZE;
        s_hid_endpoints[0].interval      = HS_HID_INTERRUPT_IN_INTERVAL;
        s_hid_endpoints[1].maxPacketSize = HS_HID_INTERRUPT_OUT_PACKET_SIZE;
        s_hid_endpoints[1].interval      = HS_HID_INTERRUPT_OUT_INTERVAL;
#endif

#if USB_DEVICE_CONFIG_CDC_ECM
        // High speed settings - CDC-ECM
        s_ecm_comm_endpoints[0].interval = USB_DEVICE_CDC_ECM_COMM_INTERRUPT_IN_EP_INTERVAL_HS;
        s_ecm_data_endpoints[0]
            .maxPacketSize = USB_DEVICE_CDC_ECM_DATA_BULK_IN_EP_MAXPKT_SIZE_HS;
        s_ecm_data_endpoints[1]
            .maxPacketSize = USB_DEVICE_CDC_ECM_DATA_BULK_OUT_EP_MAXPKT_SIZE_HS;
#endif

#if USB_DEVICE_CONFIG_CDC_ACM
        // High speed settings - CDC-ACM
        s_acm_comm_endpoints[0].interval = USB_DEVICE_CDC_ACM_COMM_INTERRUPT_IN_EP_INTERVAL_HS;
        s_acm_data_endpoints[0]
            .maxPacketSize = USB_DEVICE_CDC_ACM_DATA_BULK_IN_EP_MAXPKT_SIZE_HS;
        s_acm_data_endpoints[1]
            .maxPacketSize = USB_DEVICE_CDC_ACM_DATA_BULK_OUT_EP_MAXPKT_SIZE_HS;
#endif
    }
    else {
#if USB_CONFIG_COMPOSITE
        // Full speed settings - HID
        s_hid_endpoints[0].maxPacketSize = FS_HID_INTERRUPT_IN_PACKET_SIZE;
        s_hid_endpoints[0].interval      = FS_HID_INTERRUPT_IN_INTERVAL;
        s_hid_endpoints[1].maxPacketSize = FS_HID_INTERRUPT_OUT_PACKET_SIZE;
        s_hid_endpoints[1].interval      = FS_HID_INTERRUPT_OUT_INTERVAL;
#endif

#if USB_DEVICE_CONFIG_CDC_ECM
        // Full speed settings - CDC-ECM
        s_ecm_comm_endpoints[0].interval = USB_DEVICE_CDC_ECM_COMM_INTERRUPT_IN_EP_INTERVAL_FS;
        s_ecm_data_endpoints[0]
            .maxPacketSize = USB_DEVICE_CDC_ECM_DATA_BULK_IN_EP_MAXPKT_SIZE_FS;
        s_ecm_data_endpoints[1]
            .maxPacketSize = USB_DEVICE_CDC_ECM_DATA_BULK_OUT_EP_MAXPKT_SIZE_FS;
#endif

#if USB_DEVICE_CONFIG_CDC_ACM
        // Full speed settings - CDC-ACM
        s_acm_comm_endpoints[0].interval = USB_DEVICE_CDC_ACM_COMM_INTERRUPT_IN_EP_INTERVAL_FS;
        s_acm_data_endpoints[0]
            .maxPacketSize = USB_DEVICE_CDC_ACM_DATA_BULK_IN_EP_MAXPKT_SIZE_FS;
        s_acm_data_endpoints[1]
            .maxPacketSize = USB_DEVICE_CDC_ACM_DATA_BULK_OUT_EP_MAXPKT_SIZE_FS;
#endif
    }

    return kStatus_USB_Success;
}

usb_status_t
ECM_USB_DeviceGetDeviceDescriptor(usb_device_handle                          handle,
                                  usb_device_get_device_descriptor_struct_t* deviceDescriptor)
{
    deviceDescriptor->buffer = s_device_descriptor;
    deviceDescriptor->length = sizeof(s_device_descriptor);
    return kStatus_USB_Success;
}

/* When false, configuration descriptor and class list exclude CDC-ECM (no valid MAC). */
#if USB_DEVICE_CONFIG_CDC_ECM
static bool s_ecm_interface_visible = true;

static void prepare_config_buffer(void)
{
    if (s_ecm_interface_visible) {
        std::memcpy(s_config_buffer, s_config_descriptor, USB_COMPOSITE_CONFIG_DESC_LENGTH);
    }
    else {
        std::memcpy(s_config_buffer, s_config_descriptor, USB_DESC_OFFSET_CDC_ECM);
        std::memcpy(s_config_buffer + USB_DESC_OFFSET_CDC_ECM,
                    s_config_descriptor + USB_DESC_OFFSET_CDC_ECM + USB_CDC_ECM_DESC_LENGTH,
                    USB_COMPOSITE_CONFIG_DESC_LENGTH_NO_ECM - USB_DESC_OFFSET_CDC_ECM);
        s_config_buffer[2] = USB_SHORT_GET_LOW(USB_COMPOSITE_CONFIG_DESC_LENGTH_NO_ECM);
        s_config_buffer[3] = USB_SHORT_GET_HIGH(USB_COMPOSITE_CONFIG_DESC_LENGTH_NO_ECM);
        s_config_buffer[4] = USB_COMPOSITE_INTERFACE_COUNT_NO_ECM;
    }
}

void ECM_USB_SetEcmInterfaceVisible(bool visible)
{
    s_ecm_interface_visible = visible;
    prepare_config_buffer();
}

bool ECM_USB_IsEcmInterfaceVisible(void)
{
    return s_ecm_interface_visible;
}
#else
void ECM_USB_SetEcmInterfaceVisible(bool)
{
    std::memcpy(s_config_buffer, s_config_descriptor, USB_COMPOSITE_CONFIG_DESC_LENGTH);
}
#endif /* USB_DEVICE_CONFIG_CDC_ECM */

usb_status_t ECM_USB_DeviceGetConfigurationDescriptor(
    usb_device_handle                                 handle,
    usb_device_get_configuration_descriptor_struct_t* configurationDescriptor)
{
    (void)handle;
    if (configurationDescriptor->configuration >= USB_DEVICE_CONFIGURATION_COUNT) {
        return kStatus_USB_InvalidRequest;
    }
    configurationDescriptor->buffer = s_config_buffer;
#if USB_DEVICE_CONFIG_CDC_ECM
    configurationDescriptor->length = s_ecm_interface_visible
                                        ? USB_COMPOSITE_CONFIG_DESC_LENGTH
                                        : USB_COMPOSITE_CONFIG_DESC_LENGTH_NO_ECM;
#else
    configurationDescriptor->length = USB_COMPOSITE_CONFIG_DESC_LENGTH;
#endif
    return kStatus_USB_Success;
}

usb_status_t
ECM_USB_DeviceGetStringDescriptor(usb_device_handle                          handle,
                                  usb_device_get_string_descriptor_struct_t* stringDescriptor)
{
    if (stringDescriptor->stringIndex == 0U) {
        stringDescriptor->buffer = s_lang_string;
        stringDescriptor->length = sizeof(s_lang_string);
        return kStatus_USB_Success;
    }

    if (stringDescriptor->stringIndex < USB_DEVICE_STRING_COUNT) {
        stringDescriptor->buffer = s_string_descriptors[stringDescriptor->stringIndex];
        stringDescriptor->length = s_string_lengths[stringDescriptor->stringIndex];
        return kStatus_USB_Success;
    }

    return kStatus_USB_InvalidRequest;
}

#if USB_CONFIG_COMPOSITE
// HID descriptor offset in config descriptor
#define HID_DESC_OFFSET_IN_CONFIG                                                              \
    (USB_DESCRIPTOR_LENGTH_CONFIGURE + USB_DESCRIPTOR_LENGTH_INTERFACE                         \
     + 2 * USB_DESCRIPTOR_LENGTH_ENDPOINT + USB_DESCRIPTOR_LENGTH_INTERFACE)

usb_status_t
ECM_USB_DeviceGetHidDescriptor(usb_device_handle                       handle,
                               usb_device_get_hid_descriptor_struct_t* hidDescriptor)
{
    if (hidDescriptor->interfaceNumber == USB_HID_INTERFACE_INDEX) {
        hidDescriptor->buffer = &s_config_buffer[HID_DESC_OFFSET_IN_CONFIG];
        hidDescriptor->length = USB_DESCRIPTOR_LENGTH_HID;
        return kStatus_USB_Success;
    }
    return kStatus_USB_InvalidRequest;
}

usb_status_t ECM_USB_DeviceGetHidReportDescriptor(
    usb_device_handle                              handle,
    usb_device_get_hid_report_descriptor_struct_t* hidReportDescriptor)
{
    if (hidReportDescriptor->interfaceNumber == USB_HID_INTERFACE_INDEX) {
        hidReportDescriptor->buffer = g_UsbDeviceHidReportDescriptor;
        hidReportDescriptor->length = sizeof(g_UsbDeviceHidReportDescriptor);
        return kStatus_USB_Success;
    }
    return kStatus_USB_InvalidRequest;
}
#endif /* USB_CONFIG_COMPOSITE */
