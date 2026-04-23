/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES.
 * All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef NV_ECM_USB_DESCRIPTOR_H
#define NV_ECM_USB_DESCRIPTOR_H

#include NV_IPC_CONFIG_H  // For UsbDeviceVid, UsbDevicePid (same as Core0)

#include "usb_device.h"
#include "usb_device_class.h"

#ifdef __cplusplus
extern "C" {
#endif

/*******************************************************************************
 * Definitions
 ******************************************************************************/
#define USB_DEVICE_SPECIFIC_BCD_VERSION (0x0200U)
#define USB_DEVICE_DEMO_BCD_VERSION     (0x0001U)

/* Composite device class (Miscellaneous with IAD) */
#define USB_DEVICE_CLASS    (0xEFU) /* Miscellaneous Device Class */
#define USB_DEVICE_SUBCLASS (0x02U) /* Common Class */
#define USB_DEVICE_PROTOCOL (0x01U) /* IAD Protocol */

/* NVIDIA VID/PID - from project config.h (UsbDeviceVid/UsbDevicePid) */

#define USB_DEVICE_MAX_POWER (0x32U) /* 100mA */

#define USB_DEVICE_STRING_COUNT               (0x05U)
#define USB_DEVICE_MANUFACTURER_STRING_INDEX  (0x01U)
#define USB_DEVICE_PRODUCT_STRING_INDEX       (0x02U)
#define USB_DEVICE_SERIAL_NUMBER_STRING_INDEX (0x03U)
#define USB_DEVICE_MAC_ADDRESS_STRING_INDEX   (0x04U)

#define USB_DEVICE_LANGID               (0x0409U)
#define USB_DEVICE_MANUFACTURER_STRING  "NVIDIA Corporation"
#define USB_DEVICE_PRODUCT_STRING       "MCU USB Composite Device"
#define USB_DEVICE_SERIAL_NUMBER_STRING "00001"

#define USB_DEVICE_CONFIGURATION_COUNT (0x01U)

/*******************************************************************************
 * Composite device interface layout
 *
 * Supported configurations:
 *   MCTP(0) + HID(1)
 *   MCTP(0) + LSTP(1)
 *   MCTP(0) + HID(1) + LSTP(2)
 *   MCTP(0) + HID(1) + CDC-ECM(2-3)
 *   MCTP(0) + HID(1) + CDC-ECM(2-3) + CDC-ACM(4-5)
 *   MCTP(0) + HID(1) + CDC-ECM(2-3) + LSTP(4)
 *
 * CDC-ECM can be visible or hidden depending on MAC provisioning.
 *
 * Configuration knobs:
 *   USB_CONFIG_COMPOSITE    -- HID
 *   USB_CONFIG_LSTP         -- LSTP
 *   USB_DEVICE_CONFIG_CDC_ECM -- CDC-ECM (set via USB_CONFIG_ECM)
 *   USB_DEVICE_CONFIG_CDC_ACM -- CDC-ACM (set via USB_CONFIG_UART_BRIDGE)
 *
 * Descriptor byte order: MCTP -> HID -> CDC-ECM -> LSTP -> CDC-ACM
 ******************************************************************************/

#define USB_MCTP_INTERFACE_COUNT (1U)

#if USB_CONFIG_COMPOSITE
#define USB_HID_INTERFACE_COUNT (1U)
#else
#define USB_HID_INTERFACE_COUNT (0U)
#endif

#if USB_DEVICE_CONFIG_CDC_ECM
#define USB_CDC_ECM_INTERFACE_COUNT (2U)
#else
#define USB_CDC_ECM_INTERFACE_COUNT (0U)
#endif

#if USB_CONFIG_LSTP
#define USB_LSTP_INTERFACE_COUNT (1U)
#else
#define USB_LSTP_INTERFACE_COUNT (0U)
#endif

#if USB_DEVICE_CONFIG_CDC_ACM
#define USB_CDC_ACM_INTERFACE_COUNT (2U)
#else
#define USB_CDC_ACM_INTERFACE_COUNT (0U)
#endif

#define USB_COMPOSITE_INTERFACE_COUNT                                                          \
    (USB_MCTP_INTERFACE_COUNT + USB_HID_INTERFACE_COUNT + USB_CDC_ECM_INTERFACE_COUNT          \
     + USB_LSTP_INTERFACE_COUNT + USB_CDC_ACM_INTERFACE_COUNT)

#if USB_DEVICE_CONFIG_CDC_ECM
#define USB_COMPOSITE_CONFIGURE_INDEX USB_DEVICE_CDC_ECM_CONFIGURATION_VALUE
#else
#define USB_COMPOSITE_CONFIGURE_INDEX (0x01U)
#endif

/*******************************************************************************
 * MCTP Interface (Interface 0)
 ******************************************************************************/
#define USB_MCTP_INTERFACE_INDEX       (0x00U)
#define USB_MCTP_INTERFACE_ALTERNATE_0 (0x00U)
#define USB_MCTP_ENDPOINT_COUNT        (0x02U)

#define USB_MCTP_GENERIC_CLASS    (0x14U) /* MCTP over USB */
#define USB_MCTP_GENERIC_SUBCLASS (0x00U)
#define USB_MCTP_GENERIC_PROTOCOL (0x01U)

#define USB_MCTP_ENDPOINT_IN  (0x01U)
#define USB_MCTP_ENDPOINT_OUT (0x01U) /* Same EP number as IN (separate IN/OUT pipes) */

#define USB_MCTP_IN_BUFFER_LENGTH  (512U)
#define USB_MCTP_OUT_BUFFER_LENGTH (512U)

#define HS_MCTP_CLASS_IN_PACKET_SIZE  (512U)
#define HS_MCTP_CLASS_OUT_PACKET_SIZE (512U)
#define FS_MCTP_CLASS_IN_PACKET_SIZE  (64U)
#define FS_MCTP_CLASS_OUT_PACKET_SIZE (64U)

/*******************************************************************************
 * HID Interface (Interface 1) -- only when USB_CONFIG_COMPOSITE
 ******************************************************************************/
#if USB_CONFIG_COMPOSITE
#define USB_HID_INTERFACE_INDEX     (0x01U)
#define USB_HID_INTERFACE_ALTERNATE (0x00U)
#define USB_HID_ENDPOINT_COUNT      (0x02U)

#define USB_HID_GENERIC_CLASS    (0x03U)
#define USB_HID_GENERIC_SUBCLASS (0x00U)
#define USB_HID_GENERIC_PROTOCOL (0x00U)

#define USB_HID_ENDPOINT_IN  (0x02U)
#define USB_HID_ENDPOINT_OUT (0x02U)

#define USB_HID_IN_BUFFER_LENGTH  (64U)
#define USB_HID_OUT_BUFFER_LENGTH (64U)

#define HS_HID_INTERRUPT_IN_PACKET_SIZE  (64U)
#define HS_HID_INTERRUPT_OUT_PACKET_SIZE (64U)
#define FS_HID_INTERRUPT_IN_PACKET_SIZE  (64U)
#define FS_HID_INTERRUPT_OUT_PACKET_SIZE (64U)

#define HS_HID_INTERRUPT_IN_INTERVAL  (0x04U) /* 2^(4-1) = 1ms */
#define HS_HID_INTERRUPT_OUT_INTERVAL (0x04U)
#define FS_HID_INTERRUPT_IN_INTERVAL  (0x01U)
#define FS_HID_INTERRUPT_OUT_INTERVAL (0x01U)

#define USB_DESCRIPTOR_LENGTH_HID (9U)
#endif /* USB_CONFIG_COMPOSITE */

/*******************************************************************************
 * CDC-ECM Communication Interface (Interface 2)
 * Only compiled when USB_DEVICE_CONFIG_CDC_ECM is enabled (per-project via -D flag)
 ******************************************************************************/
#if USB_DEVICE_CONFIG_CDC_ECM
#define USB_DEVICE_CDC_CLASS_COMM_CODE   (0x02U)
#define USB_DEVICE_CDC_CLASS_DATA_CODE   (0x0AU)
#define USB_DEVICE_CDC_ECM_SUBCLASS_CODE (0x06U)
#define USB_DEVICE_CDC_ECM_PROTOCOL_CODE (0x00U)

#define USB_DEVICE_CDC_ECM_INTERFACE_COUNT     (0x02U)
#define USB_DEVICE_CDC_ECM_CONFIGURATION_VALUE (0x01U)

#define USB_DEVICE_CDC_ECM_COMM_INTERFACE_NUMBER          (0x02U)
#define USB_DEVICE_CDC_ECM_COMM_INTERFACE_ALTERNATE       (0x00U)
#define USB_DEVICE_CDC_ECM_COMM_INTERFACE_ALTERNATE_COUNT (0x01U)
#define USB_DEVICE_CDC_ECM_COMM_INTERFACE_CLASS_CODE      (USB_DEVICE_CDC_CLASS_COMM_CODE)
#define USB_DEVICE_CDC_ECM_COMM_INTERFACE_SUBCLASS_CODE   (USB_DEVICE_CDC_ECM_SUBCLASS_CODE)
#define USB_DEVICE_CDC_ECM_COMM_INTERFACE_PROTOCOL_CODE   (USB_DEVICE_CDC_ECM_PROTOCOL_CODE)
#define USB_DEVICE_CDC_ECM_COMM_ENDPOINT_NUMBER           (0x01U)

/* Notification endpoint - EP3 */
#define USB_DEVICE_CDC_ECM_COMM_INTERRUPT_IN_EP_NUMBER      (0x03U)
#define USB_DEVICE_CDC_ECM_COMM_INTERRUPT_IN_EP_MAXPKT_SIZE (0x0010U)
#define USB_DEVICE_CDC_ECM_COMM_INTERRUPT_IN_EP_INTERVAL_FS (0x08U)
#define USB_DEVICE_CDC_ECM_COMM_INTERRUPT_IN_EP_INTERVAL_HS (0x07U)

/*******************************************************************************
 * CDC-ECM Data Interface (Interface 3)
 ******************************************************************************/
#define USB_DEVICE_CDC_ECM_DATA_INTERFACE_NUMBER          (0x03U)
#define USB_DEVICE_CDC_ECM_DATA_INTERFACE_ALTERNATE_COUNT (0x02U)
#define USB_DEVICE_CDC_ECM_DATA_INTERFACE_ALTERNATE0      (0x00U)
#define USB_DEVICE_CDC_ECM_DATA_INTERFACE_ALTERNATE1      (0x01U)
#define USB_DEVICE_CDC_ECM_DATA_INTERFACE_CLASS_CODE      (USB_DEVICE_CDC_CLASS_DATA_CODE)
#define USB_DEVICE_CDC_ECM_DATA_INTERFACE_SUBCLASS_CODE   (0x00U)
#define USB_DEVICE_CDC_ECM_DATA_INTERFACE_PROTOCOL_CODE   (0x00U)
#define USB_DEVICE_CDC_ECM_DATA_ENDPOINT_NUMBER           (0x02U)

/* Bulk endpoints - EP4/EP5 */
#define USB_DEVICE_CDC_ECM_DATA_BULK_IN_EP_NUMBER         (0x04U)
#define USB_DEVICE_CDC_ECM_DATA_BULK_OUT_EP_NUMBER        (0x05U)
#define USB_DEVICE_CDC_ECM_DATA_BULK_IN_EP_MAXPKT_SIZE_FS (0x0040U)
#define USB_DEVICE_CDC_ECM_DATA_BULK_IN_EP_MAXPKT_SIZE_HS (0x0200U)
#define USB_DEVICE_CDC_ECM_DATA_BULK_IN_EP_INTERVAL       (0x00U)

#define USB_DEVICE_CDC_ECM_DATA_BULK_OUT_EP_MAXPKT_SIZE_FS (0x0040U)
#define USB_DEVICE_CDC_ECM_DATA_BULK_OUT_EP_MAXPKT_SIZE_HS (0x0200U)
#define USB_DEVICE_CDC_ECM_DATA_BULK_OUT_EP_INTERVAL       (0x00U)

/*******************************************************************************
 * IAD (Interface Association Descriptor) for CDC-ECM
 ******************************************************************************/
#define USB_DESCRIPTOR_LENGTH_IAD (8U)

/* CDC functional descriptors */
#define USB_DEVICE_CDC_FUNC_LENGTH                      (0x05U)
#define USB_DEVICE_CDC_FUNC_TYPE_CS_INTERFACE           (0x24U)
#define USB_DEVICE_CDC_FUNC_SUBTYPE_HEADER              (0x00U)
#define USB_DEVICE_CDC_FUNC_SUBTYPE_UNION_FUNC          (0x06U)
#define USB_DEVICE_CDC_FUNC_SUBTYPE_ETHERNET_NETWORKING (0x0FU)

#define USB_DEVICE_CDC_FUNC_UNION_LENGTH         (0x05U)
#define USB_DEVICE_CDC_FUNC_UNION_TYPE           (USB_DEVICE_CDC_FUNC_TYPE_CS_INTERFACE)
#define USB_DEVICE_CDC_FUNC_UNION_SUBTYPE        (USB_DEVICE_CDC_FUNC_SUBTYPE_UNION_FUNC)
#define USB_DEVICE_CDC_FUNC_UNION_CTRL_INTERFACE (0x00U)

/* ECM class descriptor */
#define USB_DEVICE_CDC_ECM_CLASS_DESCRIPTOR_LENGTH (0x0DU)
#define USB_DEVICE_CDC_ECM_CLASS_DESCRIPTOR_TYPE   (USB_DEVICE_CDC_FUNC_TYPE_CS_INTERFACE)
#define USB_DEVICE_CDC_ECM_CLASS_DESCRIPTOR_SUBTYPE                                            \
    (USB_DEVICE_CDC_FUNC_SUBTYPE_ETHERNET_NETWORKING)
#define USB_DEVICE_CDC_ECM_CLASS_DESCRIPTOR_MAC_ADDRESS_STRING_INDEX                           \
    (USB_DEVICE_MAC_ADDRESS_STRING_INDEX)
#define USB_DEVICE_CDC_ECM_CLASS_DESCRIPTOR_ETHERNET_STATISTICS      (0x00000000U)
#define USB_DEVICE_CDC_ECM_CLASS_DESCRIPTOR_MAX_SEGMENT_SIZE         (0x05EAU) /* 1514 bytes */
#define USB_DEVICE_CDC_ECM_CLASS_DESCRIPTOR_MULTICAST_FILTERS_NUMBER (0x0000U)
#define USB_DEVICE_CDC_ECM_CLASS_DESCRIPTOR_POWER_FILTERS_NUMBER     (0x00U)

#endif  // USB_DEVICE_CONFIG_CDC_ECM

/*******************************************************************************
 * CDC-ACM Communication Interface (Interface 4) - UART Bridge
 * Only compiled when USB_DEVICE_CONFIG_CDC_ACM is enabled (per-project via -D flag)
 ******************************************************************************/
#if USB_DEVICE_CONFIG_CDC_ACM

#define USB_DEVICE_CDC_ACM_COMM_INTERFACE_NUMBER          (0x04U)
#define USB_DEVICE_CDC_ACM_COMM_INTERFACE_ALTERNATE       (0x00U)
#define USB_DEVICE_CDC_ACM_COMM_INTERFACE_ALTERNATE_COUNT (0x01U)
#define USB_DEVICE_CDC_ACM_COMM_ENDPOINT_NUMBER           (0x01U)

#define USB_DEVICE_CDC_ACM_COMM_CLASS_CODE    (0x02U) /* Communications */
#define USB_DEVICE_CDC_ACM_COMM_SUBCLASS_CODE (0x02U) /* Abstract Control Model */
#define USB_DEVICE_CDC_ACM_COMM_PROTOCOL_CODE (0x00U)

/* Notification endpoint - EP6 */
#define USB_DEVICE_CDC_ACM_COMM_INTERRUPT_IN_EP_NUMBER      (0x06U)
#define USB_DEVICE_CDC_ACM_COMM_INTERRUPT_IN_EP_MAXPKT_SIZE (0x0010U) /* 16 bytes */
#define USB_DEVICE_CDC_ACM_COMM_INTERRUPT_IN_EP_INTERVAL_FS (0x08U)
#define USB_DEVICE_CDC_ACM_COMM_INTERRUPT_IN_EP_INTERVAL_HS (0x07U) /* 2^(7-1) = 8ms */

/*******************************************************************************
 * CDC-ACM Data Interface (Interface 5) - UART Bridge
 ******************************************************************************/
#define USB_DEVICE_CDC_ACM_DATA_INTERFACE_NUMBER          (0x05U)
#define USB_DEVICE_CDC_ACM_DATA_INTERFACE_ALTERNATE       (0x00U)
#define USB_DEVICE_CDC_ACM_DATA_INTERFACE_ALTERNATE_COUNT (0x01U)
#define USB_DEVICE_CDC_ACM_DATA_ENDPOINT_NUMBER           (0x02U)

#define USB_DEVICE_CDC_ACM_DATA_CLASS_CODE    (0x0AU) /* CDC Data */
#define USB_DEVICE_CDC_ACM_DATA_SUBCLASS_CODE (0x00U)
#define USB_DEVICE_CDC_ACM_DATA_PROTOCOL_CODE (0x00U)

#define USB_DEVICE_CDC_ACM_INTERFACE_COUNT (0x02U) /* Comm + Data */

/* Bulk endpoints - EP7 */
#define USB_DEVICE_CDC_ACM_DATA_BULK_IN_EP_NUMBER         (0x07U)
#define USB_DEVICE_CDC_ACM_DATA_BULK_OUT_EP_NUMBER        (0x07U)
#define USB_DEVICE_CDC_ACM_DATA_BULK_IN_EP_MAXPKT_SIZE_FS (0x0040U) /* 64 bytes */
#define USB_DEVICE_CDC_ACM_DATA_BULK_IN_EP_MAXPKT_SIZE_HS (0x0200U) /* 512 bytes */
#define USB_DEVICE_CDC_ACM_DATA_BULK_IN_EP_INTERVAL       (0x00U)

#define USB_DEVICE_CDC_ACM_DATA_BULK_OUT_EP_MAXPKT_SIZE_FS (0x0040U)
#define USB_DEVICE_CDC_ACM_DATA_BULK_OUT_EP_MAXPKT_SIZE_HS (0x0200U)
#define USB_DEVICE_CDC_ACM_DATA_BULK_OUT_EP_INTERVAL       (0x00U)

/*******************************************************************************
 * CDC-ACM Buffer Sizes
 ******************************************************************************/
#define USB_ACM_IN_BUFFER_LENGTH  (512U) /* TX to host buffer */
#define USB_ACM_OUT_BUFFER_LENGTH (512U) /* RX from host buffer */

#endif /* USB_DEVICE_CONFIG_CDC_ACM */

/*******************************************************************************
 * Low-Speed Transfer Protocol
 * Only compiled when USB_CONFIG_LSTP is enabled (per-project via -D flag).
 * Interface/endpoint indices shift depending on which features precede LSTP.
 ******************************************************************************/
#if USB_CONFIG_LSTP
#if USB_DEVICE_CONFIG_CDC_ECM
/* MCTP(0) + HID(1) + ECM(2-3) + LSTP(4) */
#define USB_LSTP_INTERFACE_INDEX (0x04U)
#define USB_LSTP_ENDPOINT_IN     (0x06U)
#define USB_LSTP_ENDPOINT_OUT    (0x06U)
#elif USB_CONFIG_COMPOSITE
/* MCTP(0) + HID(1) + LSTP(2) */
#define USB_LSTP_INTERFACE_INDEX (0x02U)
#define USB_LSTP_ENDPOINT_IN     (0x03U)
#define USB_LSTP_ENDPOINT_OUT    (0x03U)
#else
/* MCTP(0) + LSTP(1) */
#define USB_LSTP_INTERFACE_INDEX (0x01U)
#define USB_LSTP_ENDPOINT_IN     (0x02U)
#define USB_LSTP_ENDPOINT_OUT    (0x02U)
#endif

#define USB_LSTP_INTERFACE_ALTERNATE_0 (0x00U)
#define USB_LSTP_ENDPOINT_COUNT        (0x02U)

#define USB_LSTP_GENERIC_CLASS    (0xFFU) /* Vendor specific class */
#define USB_LSTP_GENERIC_SUBCLASS (0x3FU)
#define USB_LSTP_GENERIC_PROTOCOL (0x01U)

#define USB_LSTP_IN_BUFFER_LENGTH  (512U)
#define USB_LSTP_OUT_BUFFER_LENGTH (512U)

#define USB_LSTP_CLASS_IN_PACKET_SIZE  (512U)
#define USB_LSTP_CLASS_OUT_PACKET_SIZE (512U)

#define USB_LSTP_INTERVAL (0x01U)
#endif /* USB_CONFIG_LSTP */

/*******************************************************************************
 * Variables
 ******************************************************************************/
#if USB_CONFIG_COMPOSITE
extern usb_device_class_struct_t g_ecm_hid_class;
#endif
#if USB_DEVICE_CONFIG_CDC_ECM
extern usb_device_class_struct_t g_ecm_cdc_class;
#endif
#if USB_DEVICE_CONFIG_CDC_ACM
extern usb_device_class_struct_t g_acm_cdc_class;
#endif

#if USB_CONFIG_COMPOSITE
extern uint8_t  g_UsbDeviceHidReportDescriptor[];
extern uint32_t g_UsbDeviceHidReportDescriptorLength;
#endif

/*******************************************************************************
 * API
 ******************************************************************************/
usb_status_t ECM_USB_DeviceSetSpeed(usb_device_handle handle, uint8_t speed);
usb_status_t
             ECM_USB_DeviceGetDeviceDescriptor(usb_device_handle                          handle,
                                               usb_device_get_device_descriptor_struct_t* deviceDescriptor);
usb_status_t ECM_USB_DeviceGetConfigurationDescriptor(
    usb_device_handle                                 handle,
    usb_device_get_configuration_descriptor_struct_t* configurationDescriptor);
usb_status_t
     ECM_USB_DeviceGetStringDescriptor(usb_device_handle                          handle,
                                       usb_device_get_string_descriptor_struct_t* stringDescriptor);
void ECM_USB_FillStringDescriptorBuffer(void);

#if USB_CONFIG_COMPOSITE
usb_status_t
             ECM_USB_DeviceGetHidDescriptor(usb_device_handle                       handle,
                                            usb_device_get_hid_descriptor_struct_t* hidDescriptor);
usb_status_t ECM_USB_DeviceGetHidReportDescriptor(
    usb_device_handle                              handle,
    usb_device_get_hid_report_descriptor_struct_t* hidReportDescriptor);
#endif

#if USB_DEVICE_CONFIG_CDC_ECM
bool ECM_USB_IsEcmInterfaceVisible(void);
#endif
void ECM_USB_SetEcmInterfaceVisible(bool visible);

#ifdef __cplusplus
}
#endif

#endif /* NV_ECM_USB_DESCRIPTOR_H */
