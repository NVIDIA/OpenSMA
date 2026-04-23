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

#include "sys/usb/usb.h"

#include "usb_config_wrapper.h"
#include "usb_device.h"
#include "usb_device_class.h"
#include "usb_device_hid.h"

#include "nv/common/console.h"
#include "nv/common/debuglevel.h"
#include "nv/common/system.h"
#include "nv/logger/log.h"
#include "nv/iox/iox.h"
#include "nv/mainbox/mailbox.h"
#include "nv/usb/task.h"
#include "sys/usb/usb_device_config.h"
#include "sys/usb/usb_device_descriptor.h"
#include "sys/usb/usb_device_mctp.h"
#include "sys/usb/usb_device_spi.h"
#include "mpu_syscall_numbers.h"

#if defined(USB_CONFIG_UART_BRIDGE)
#include "usb_device_cdc_acm.h"
#endif

#if defined(__cplusplus)
extern "C" {
#endif

// NOLINTBEGIN

extern usb_status_t
USB_DeviceGetDeviceDescriptor(usb_device_handle                          handle,
                              usb_device_get_device_descriptor_struct_t* deviceDescriptor);

extern usb_status_t USB_DeviceGetConfigurationDescriptor(
    usb_device_handle                                 handle,
    usb_device_get_configuration_descriptor_struct_t* configurationDescriptor);

extern usb_status_t
USB_DeviceGetStringDescriptor(usb_device_handle                          handle,
                              usb_device_get_string_descriptor_struct_t* stringDescriptor);

extern void                      USB_DeviceEhciIsrFunction(void* device_handle);
extern void                      USB_DeviceClockInit(void);
extern void                      USB_DeviceIsrEnable(void);
extern usb_device_class_struct_t g_UsbDeviceMctpGenericConfig;

#if defined(USB_CONFIG_COMPOSITE)
extern usb_device_class_struct_t g_UsbDeviceHidGenericConfig;
#if defined(USB_DEVICE_CONFIG_VENDOR_SPECIFIC)
extern usb_device_class_struct_t g_UsbDeviceSpiConfig;
#endif
#endif

#if defined(USB_CONFIG_UART_BRIDGE)
extern usb_device_class_struct_t    g_UsbDeviceCdcVcomConfig;
extern usb_device_endpoint_struct_t g_cdcVcomDicEndpoints[];
#endif

#if defined(__cplusplus)
extern "C" {
#endif

#if defined(__cplusplus)
}
#endif

namespace sys::usb {

NV_SHARED_BSS usb_composite_struct_t g_UsbDevice;

#if defined(USB_CONFIG_UART_BRIDGE)
typedef struct _usb_cdc_acm_info
{
    /**
     * NOTIF_PACKET_SIZE: bmRequestType=[A1]
     *                    bNotification=[20] (SERIAL_STATE)
     *                    wValue=[00 00]
     *                    wIndex=[xx xx]
     *                    wLength=[02 00]
     *UART_BITMAP_SIZE bytes: UartStateBitmap=[xx xx]
     */
    uint8_t serialStateBuf[NOTIF_PACKET_SIZE + UART_BITMAP_SIZE]; /* Serial state buffer of the
                                                                     CDC device to notify the
                                                                     serial state to host. */
    /**
     * DTE: Data Terminal Equipment = host (PC)
     * DCE: Data Circuit-terminating Equipment = device (MCXN236)
     */
    bool     dtePresent;       /* A flag to indicate whether DTE is present.         */
    uint16_t breakDuration;    /* Length of time in milliseconds of the break signal */
    uint8_t  dteStatus;        /* Status of data terminal equipment                  */
    uint8_t  currentInterface; /* Current interface index.                           */
    uint16_t uartState;        /* UART state of the CDC device.                      */
} usb_cdc_acm_info_t;

/**
 * @brief Line coding of cdc device
 * @note Line Coding data structure
 *  dwDTERate: [00 C2 01 00] (0x0001C200)
 *  bCharFormat: [00] (1 stop bit)
 *  bParityType: [00] (no parity)
 *  bDataBits: [08] (8 data bits)
 */
USB_DMA_INIT_DATA_ALIGN(USB_DATA_ALIGN_SIZE)
static uint8_t s_lineCoding[LINE_CODING_SIZE] = {
    /* E.g. 0x00,0xC2,0x01,0x00 : 0x0001C200 is 115200 bits per second */
    (LINE_CODING_DTERATE >> 0U) & 0x000000FFU,
    (LINE_CODING_DTERATE >> 8U) & 0x000000FFU,
    (LINE_CODING_DTERATE >> 16U) & 0x000000FFU,
    (LINE_CODING_DTERATE >> 24U) & 0x000000FFU,
    LINE_CODING_CHARFORMAT,
    LINE_CODING_PARITYTYPE,
    LINE_CODING_DATABITS};

/**
 * @brief Abstract state of cdc device
 * @note NOT USED as COMMON FEATURE is NOT SUPPORTED
 */
USB_DMA_INIT_DATA_ALIGN(USB_DATA_ALIGN_SIZE)
static uint8_t s_abstractState[COMM_FEATURE_DATA_SIZE] = {
    (STATUS_ABSTRACT_STATE >> 0U) & 0x00FFU, (STATUS_ABSTRACT_STATE >> 8U) & 0x00FFU};

/**
 * @brief Country code of cdc device
 * @note NOT USED so far
 */
USB_DMA_INIT_DATA_ALIGN(USB_DATA_ALIGN_SIZE)
static uint8_t s_countryCode[COMM_FEATURE_DATA_SIZE] = {(COUNTRY_SETTING >> 0U) & 0x00FFU,
                                                        (COUNTRY_SETTING >> 8U) & 0x00FFU};

/* CDC ACM information */
NV_SHARED_BSS
USB_DMA_NONINIT_DATA_ALIGN(USB_DATA_ALIGN_SIZE) static usb_cdc_acm_info_t s_usbCdcAcmInfo;

// Receive buffer for USB VCOM (backpressure mode: one packet at a time)
NV_SHARED_BSS USB_DMA_NONINIT_DATA_ALIGN(USB_DATA_ALIGN_SIZE) static uint8_t
    s_currRecvBuf[HS_CDC_VCOM_BULK_OUT_PACKET_SIZE];

// VCOM state: handle and ready flag
static void* s_vcomHandle = nullptr;
static bool  s_vcomReady  = false;

// Weak hooks for upper-layer handling (no runtime callback registration)
extern "C" __attribute__((weak)) uint8_t nv_usb_vcom_rx(uint8_t* data, uint32_t length)
{
    (void)data;
    (void)length;
    return 0;
}

extern "C" __attribute__((weak)) void nv_usb_vcom_close() {}

/*!
 * @brief CDC class specific callback function.
 *
 * This function handles the CDC class specific requests.
 *
 * @param handle          The CDC ACM class handle.
 * @param event           The CDC ACM class event type.
 * @param param           The parameter of the class specific request.
 *
 * @return A USB error code or kStatus_USB_Success.
 */
usb_status_t Driver::usb_devicevcomcallback(class_handle_t handle, uint32_t event, void* param)
{
    usb_status_t                               error = kStatus_USB_InvalidRequest;
    uint32_t                                   len;
    uint8_t*                                   uartBitmap;
    usb_cdc_acm_info_t*                        acmInfo = &s_usbCdcAcmInfo;
    usb_device_cdc_acm_request_param_struct_t* acmReqParam;  // definition of parameters for CDC
                                                             // ACM request
    usb_device_endpoint_callback_message_struct_t* epCbParam;  // endpoint callback message
                                                               // structure
    acmReqParam = (usb_device_cdc_acm_request_param_struct_t*)param;
    epCbParam   = (usb_device_endpoint_callback_message_struct_t*)param;

    // DbgConsole_Printf("vcom cb event=[%04x]\r\n", event);

    switch (event) {
        case kUSB_DeviceCdcEventSendResponse: {
            if ((epCbParam->length != 0)
                && (!(epCbParam->length % g_cdcVcomDicEndpoints[0].maxPacketSize))) {
                /**
                 * If the last packet is the size of endpoint, then send also zero-ended packet,
                 * meaning that we want to inform the host that we do not have any additional
                 * data, so it can flush the output.
                 *
                 * After the zero-ended packet is sent, another kUSB_DeviceCdcEventSendResponse
                 * event will be triggered, leading to enter the else if block below.
                 */
                error = USB_DeviceCdcAcmSend(
                    handle, USB_CDC_VCOM_DIC_BULK_IN_ENDPOINT, NULL, 0);
            }
        } break;
        case kUSB_DeviceCdcEventRecvResponse: {
            if ((1 == g_UsbDevice.attach) && s_vcomReady) {
                error = static_cast<usb_status_t>(
                    nv_usb_vcom_rx(s_currRecvBuf, epCbParam->length));
            }
        } break;
        /**
         * @brief serial state has been sent to the host
         *        serial state is sent to the host through interrupt IN endpoint
         *        hasSentState is set to 1 to record that there is a serial state notification
         * scheduled hasSentState is set to 0 to indicate that the serial state notification has
         * been sent
         */
        case kUSB_DeviceCdcEventSerialStateNotif:
            ((usb_device_cdc_acm_struct_t*)handle)->hasSentState = 0;
            error                                                = kStatus_USB_Success;
            break;
        /*! TODO: handle encapsulated command */
        case kUSB_DeviceCdcEventSendEncapsulatedCommand: break;
        /*! TODO: handle encapsulated response */
        case kUSB_DeviceCdcEventGetEncapsulatedResponse: break;
        /*! NOTE: Common Feature NOT supported */
        case kUSB_DeviceCdcEventSetCommFeature:
            if (USB_DEVICE_CDC_FEATURE_ABSTRACT_STATE == acmReqParam->setupValue) {
                if (1 == acmReqParam->isSetup) {
                    *(acmReqParam->buffer) = s_abstractState;
                    *(acmReqParam->length) = sizeof(s_abstractState);
                }
                else {
                    /* no action, data phase, s_abstractState has been assigned */
                }
                error = kStatus_USB_Success;
            }
            else if (USB_DEVICE_CDC_FEATURE_COUNTRY_SETTING == acmReqParam->setupValue) {
                if (1 == acmReqParam->isSetup) {
                    *(acmReqParam->buffer) = s_countryCode;
                    *(acmReqParam->length) = sizeof(s_countryCode);
                }
                else {
                    /* no action, data phase, s_countryCode has been assigned */
                }
                error = kStatus_USB_Success;
            }
            else {
                /* no action, return kStatus_USB_InvalidRequest */
            }
            break;
        /*! NOTE: Common Feature NOT supported */
        case kUSB_DeviceCdcEventGetCommFeature:
            if (USB_DEVICE_CDC_FEATURE_ABSTRACT_STATE == acmReqParam->setupValue) {
                *(acmReqParam->buffer) = s_abstractState;
                *(acmReqParam->length) = COMM_FEATURE_DATA_SIZE;
                error                  = kStatus_USB_Success;
            }
            else if (USB_DEVICE_CDC_FEATURE_COUNTRY_SETTING == acmReqParam->setupValue) {
                *(acmReqParam->buffer) = s_countryCode;
                *(acmReqParam->length) = COMM_FEATURE_DATA_SIZE;
                error                  = kStatus_USB_Success;
            }
            else {
                /* no action, return kStatus_USB_InvalidRequest */
            }
            break;
        /*! NOTE: Common Feature NOT supported */
        case kUSB_DeviceCdcEventClearCommFeature: break;
        /*! @brief host requests to get line coding */
        case kUSB_DeviceCdcEventGetLineCoding:
            *(acmReqParam->buffer) = s_lineCoding;
            *(acmReqParam->length) = LINE_CODING_SIZE;
            error                  = kStatus_USB_Success;
            break;
        /*! @brief host requests to set line coding */
        case kUSB_DeviceCdcEventSetLineCoding: {
            if (1U == acmReqParam->isSetup) {
                *(acmReqParam->buffer) = s_lineCoding;
                *(acmReqParam->length) = sizeof(s_lineCoding);
            }
            else {
                /* no action, data phase, s_lineCoding has been assigned */
            }
            error = kStatus_USB_Success;
        } break;
        /*! @brief host requests to set control line state */
        case kUSB_DeviceCdcEventSetControlLineState: {
            error = kStatus_USB_Success;
            /**
             * setupValue: The wValue field of the setup packet.
             * 16bits, but only the bit1 and bit0 is used
             *   - bit15 ~ bit2: reserved to 0
             *   - bit1: RTS signal, indicating host is requesting to send data
             *   - bit0: DTR signal, indicating host is ready to communicate
             */
            s_usbCdcAcmInfo.dteStatus = acmReqParam->setupValue;
            /* activate/deactivate Tx carrier */
            if (acmInfo->dteStatus & USB_DEVICE_CDC_CONTROL_SIG_BITMAP_CARRIER_ACTIVATION) {
                acmInfo->uartState |= USB_DEVICE_CDC_UART_STATE_TX_CARRIER;
            }
            else {
                acmInfo->uartState &= (uint16_t)~USB_DEVICE_CDC_UART_STATE_TX_CARRIER;
            }

            /* activate carrier and DTE. Com port of terminal tool running on PC is open now */
            if (acmInfo->dteStatus & USB_DEVICE_CDC_CONTROL_SIG_BITMAP_DTE_PRESENCE) {
                acmInfo->uartState |= USB_DEVICE_CDC_UART_STATE_RX_CARRIER;
                s_vcomReady         = true;
            }
            /* Com port of terminal tool running on PC is closed now */
            else {
                acmInfo->uartState &= (uint16_t)~USB_DEVICE_CDC_UART_STATE_RX_CARRIER;
                s_vcomReady         = false;
                // Notify upper layer to flush pending data
                nv_usb_vcom_close();
            }

            /* Indicates to DCE if DTE is present or not */
            acmInfo->dtePresent = (acmInfo->dteStatus
                                   & USB_DEVICE_CDC_CONTROL_SIG_BITMAP_DTE_PRESENCE)
                                    ? true
                                    : false;

            /* Initialize the serial state buffer */
            acmInfo->serialStateBuf[0] = NOTIF_REQUEST_TYPE;                /* bmRequestType */
            acmInfo->serialStateBuf[1] = USB_DEVICE_CDC_NOTIF_SERIAL_STATE; /* bNotification */
            acmInfo->serialStateBuf[2] = 0x00;                              /* wValue */
            acmInfo->serialStateBuf[3] = 0x00;
            acmInfo->serialStateBuf[4] = 0x00; /* wIndex */
            acmInfo->serialStateBuf[5] = 0x00;
            acmInfo->serialStateBuf[6] = UART_BITMAP_SIZE; /* wLength */
            acmInfo->serialStateBuf[7] = 0x00;
            /* Notify to host the line state */
            acmInfo->serialStateBuf[4] = acmReqParam->interfaceIndex;
            /* Lower byte of UART BITMAP */
            uartBitmap = (uint8_t*)&acmInfo
                             ->serialStateBuf[NOTIF_PACKET_SIZE + UART_BITMAP_SIZE - 2];
            uartBitmap[0] = acmInfo->uartState & 0xFFu;
            uartBitmap[1] = (acmInfo->uartState >> 8) & 0xFFu;
            len           = (uint32_t)(NOTIF_PACKET_SIZE + UART_BITMAP_SIZE);
            if (0 == ((usb_device_cdc_acm_struct_t*)handle)->hasSentState) {
                error = USB_DeviceCdcAcmSend(handle,
                                             USB_CDC_VCOM_CIC_INTERRUPT_IN_ENDPOINT,
                                             acmInfo->serialStateBuf,
                                             len);
                if (kStatus_USB_Success != error) {
                    usb_echo("kUSB_DeviceCdcEventSetControlLineState error!");
                }
                ((usb_device_cdc_acm_struct_t*)handle)->hasSentState = 1;
            }

            /* Update status */
            if (acmInfo->dteStatus & USB_DEVICE_CDC_CONTROL_SIG_BITMAP_CARRIER_ACTIVATION) {
                /*  To do: CARRIER_ACTIVATED */
            }
            else {
                /* To do: CARRIER_DEACTIVATED */
            }

            error = kStatus_USB_Success;
        } break;
        case kUSB_DeviceCdcEventSendBreak: break;
        default                          : break;
    }

    return error;
}

/*!
 * @brief Virtual COM device set configuration function.
 *
 * This function sets configuration for CDC class.
 *
 * @param handle The CDC ACM class handle.
 * @param configure The CDC ACM class configure index.
 *
 * @return A USB error code or kStatus_USB_Success.
 */
usb_status_t Driver::usb_vcomsetconfigure(class_handle_t handle, uint8_t configure)
{
    if (SYS_USB_COMPOSITE_CONFIGURE_INDEX == configure) {
        /* Schedule buffer for receive - use endpoint max packet size (dynamic based on USB
         * speed) */
        Driver::vcom_rearm_rx(s_vcomHandle, s_currRecvBuf);
    }
    return kStatus_USB_Success;
}

void Driver::vcom_rearm_rx(void* handle, uint8_t* buffer)
{
    USB_DeviceCdcAcmRecv(handle,
                         USB_CDC_VCOM_DIC_BULK_OUT_ENDPOINT,
                         buffer,
                         g_cdcVcomDicEndpoints[1].maxPacketSize);
}

bool Driver::vcom_send(void* handle, uint8_t* data, uint32_t length)
{
    return USB_DeviceCdcAcmSend(handle, USB_CDC_VCOM_DIC_BULK_IN_ENDPOINT, data, length)
        == kStatus_USB_Busy;
}

void* Driver::get_vcom_handle()
{
    return s_vcomHandle;
}

bool Driver::is_vcom_ready()
{
    return s_vcomReady;
}
#endif

#if defined(USB_CONFIG_COMPOSITE)
usb_status_t Driver::usb_devicehidcallback(class_handle_t handle, uint32_t event, void* param)
{
    usb_status_t error = kStatus_USB_InvalidRequest;
    switch (event) {
        case kUSB_DeviceHidEventSendResponse: {
            error = kStatus_USB_Success;
        } break;

        case kUSB_DeviceHidEventRecvResponse: {
            if (g_UsbDevice.attach) {
                usb_device_endpoint_callback_message_struct_t*
                    message = (usb_device_endpoint_callback_message_struct_t*)param;

                if (message->length != USB_CANCELLED_TRANSFER_LENGTH) {
                    // Process received data
                    nv::usb::Task::set_hid_rx_event();  // Signal received data
                }
            }
            error = kStatus_USB_Success;
        } break;

        case kUSB_DeviceHidEventGetReport: {
            usb_device_hid_report_struct_t* hidReportParam = (usb_device_hid_report_struct_t*)
                param;

            if (hidReportParam != NULL) {
                switch (hidReportParam->reportId) {
                    case 2:  // Report ID 2 (CP2112 GPIO Config)
                        if constexpr (nv::ipc::EnableCP2112NativeGpio) {
                            // CP2112 returns:
                            // Byte 0: Report ID (0x02)
                            // Byte 1: GPIO direction (bitmask, 1 = output, 0 = input)
                            // Byte 2: Push-pull/Open-drain (bitmask, 1 = push-pull, 0 =
                            // open-drain) Byte 3: Special function (bitmask) Byte 4: Clock
                            // divider (if applicable, else 0)
                            g_UsbDevice.hid_buffer[2][0] = 0x02;  // Report ID
                            g_UsbDevice.hid_buffer[2][1] = 0xFF;  // GPIO direction (all
                                                                  // input)
                            g_UsbDevice.hid_buffer[2][2] = 0x00;  // Push-pull/Open-drain (all
                                                                  // open-drain)
                            g_UsbDevice.hid_buffer[2][3] = 0x00;  // Special function
                            g_UsbDevice.hid_buffer[2][4] = 0x00;  // Clock divider
                            hidReportParam->reportBuffer = g_UsbDevice.hid_buffer[2];
                            hidReportParam->reportLength = 5;
                            error                        = kStatus_USB_Success;
                        }
                        break;
                    case 3:  // Report ID 3 (CP2112 Get GPIO)
                        if constexpr (nv::ipc::EnableCP2112NativeGpio) {
                            // CP2112 Get GPIO format:
                            // Byte 0: Report ID (0x03)
                            // Byte 1-2: Latch values (2 bytes)
                            g_UsbDevice.hid_buffer[2][0] = 0x03;  // Report ID
                            g_UsbDevice.hid_buffer[2][1] = nv::iox::Iox::get_gpio_value();
                            hidReportParam->reportBuffer = g_UsbDevice.hid_buffer[2];
                            hidReportParam->reportLength = 2;
                            error                        = kStatus_USB_Success;
                        }
                        break;

                    case 5:  // Report ID 5
                        g_UsbDevice.hid_buffer[2][0] = 0x05;
                        g_UsbDevice.hid_buffer[2][1] = 0x0C;
                        g_UsbDevice.hid_buffer[2][2] = 0x03;

                        hidReportParam->reportBuffer = g_UsbDevice.hid_buffer[2];
                        hidReportParam->reportLength = 3;
                        error                        = kStatus_USB_Success;
                        break;

                    case 6:  // Report ID 6
                        g_UsbDevice.hid_buffer[2][0]  = 0x06;
                        g_UsbDevice.hid_buffer[2][1]  = 0x00;
                        g_UsbDevice.hid_buffer[2][2]  = 0x01;
                        g_UsbDevice.hid_buffer[2][3]  = 0x86;
                        g_UsbDevice.hid_buffer[2][4]  = 0xA0;
                        g_UsbDevice.hid_buffer[2][5]  = 0x02;
                        g_UsbDevice.hid_buffer[2][6]  = 0x00;
                        g_UsbDevice.hid_buffer[2][7]  = 0x00;
                        g_UsbDevice.hid_buffer[2][8]  = 0x00;
                        g_UsbDevice.hid_buffer[2][9]  = 0x00;
                        g_UsbDevice.hid_buffer[2][10] = 0x00;
                        g_UsbDevice.hid_buffer[2][11] = 0x00;
                        g_UsbDevice.hid_buffer[2][12] = 0x00;
                        g_UsbDevice.hid_buffer[2][13] = 0x00;
                        hidReportParam->reportBuffer  = g_UsbDevice.hid_buffer[2];
                        hidReportParam->reportLength  = 14;
                        error                         = kStatus_USB_Success;
                        break;

                    case 32:  // Report ID 32
                        g_UsbDevice.hid_buffer[2][0] = 0x20;
                        g_UsbDevice.hid_buffer[2][1] = 0xFF;

                        hidReportParam->reportBuffer = g_UsbDevice.hid_buffer[2];
                        hidReportParam->reportLength = 2;
                        error                        = kStatus_USB_Success;
                        break;

                    default:
                        nv::info("Unknown Report ID: %d\r\n", hidReportParam->reportId);
                        break;
                }
            }
        } break;
        case kUSB_DeviceHidEventRequestReportBuffer: {
            usb_device_hid_report_struct_t* hidReportParam = (usb_device_hid_report_struct_t*)
                param;

            if (hidReportParam) {
                hidReportParam->reportBuffer = g_UsbDevice.hid_buffer.at(
                    g_UsbDevice.buffer_index);
                hidReportParam->reportLength = 64;
                return kStatus_USB_Success;
            }
            return kStatus_USB_InvalidRequest;
        } break;

        case kUSB_DeviceHidEventSetReport: {
            usb_device_hid_report_struct_t* hidReportParam = (usb_device_hid_report_struct_t*)
                param;

            if (hidReportParam && hidReportParam->reportBuffer) {
                // TODO:
                //
                // process_smbus_config(hidReportParam->reportBuffer,
                //                     hidReportParam->reportLength);
                g_UsbDevice.hid_buffer[2][0] = 0x20;
                g_UsbDevice.hid_buffer[2][1] = 0xff;
            }
            error = USB_DeviceHidSend(g_UsbDevice.hid_handle,
                                      USB_HID_GENERIC_ENDPOINT_IN,
                                      g_UsbDevice.hid_buffer[2],
                                      2);
        } break;

        case kUSB_DeviceHidEventGetIdle:
        case kUSB_DeviceHidEventGetProtocol:
        case kUSB_DeviceHidEventSetIdle:
        case kUSB_DeviceHidEventSetProtocol: error = kStatus_USB_Success; break;

        default: break;
    }

    return error;
}
#endif
/* Composite class configuration list */
NV_SHARED_DATA usb_device_class_config_struct_t g_UsbDeviceCompositeConfig[] = {
    {
     Driver::usb_devicemctpcallback,/* MCTP class callback pointer */
(class_handle_t) nullptr,  /* MCTP class handle */
  &g_UsbDeviceMctpGenericConfig,  /* MCTP class information */
    },
#if defined(USB_CONFIG_COMPOSITE)
    {
     Driver::usb_devicehidcallback,
     (class_handle_t) nullptr,        /* HID class handle */
        &g_UsbDeviceHidGenericConfig, /* HID class information */
    },
#if defined(USB_DEVICE_CONFIG_VENDOR_SPECIFIC)
    {
     Driver::usb_deviceSpicallback,
     (class_handle_t) nullptr,        /* NV SMA SPI class handle */
        &g_UsbDeviceSpiConfig,    /* NV SMA SPI class information */
    },
#endif
#endif
#if defined(USB_CONFIG_UART_BRIDGE)
    {
     Driver::usb_devicevcomcallback, /* VCOM class callback pointer */
 (class_handle_t) nullptr,        /* VCOM class handle */
        &g_UsbDeviceCdcVcomConfig,      /* VCOM class information */
    },
#endif
};

/* Composite class configuration list */
NV_SHARED_DATA usb_device_class_config_list_struct_t g_UsbDeviceCompositeConfigList = {
    g_UsbDeviceCompositeConfig,    /* Class configurations */
    Driver::usb_devicecallback,    /* Device callback pointer */
    SYS_USB_COMPOSITE_CLASS_COUNT, /* Class count */
};

NV_PRIVILEGED_FUNCTION uint8_t Driver::init(void* mctp_buffer0,
                                            void* mctp_buffer1,
                                            void* hid_buffer0,
                                            void* hid_buffer1,
                                            void* hid_buffer2,
                                            void* spi_buffer0,
                                            void* spi_buffer1,
                                            void* spi_rx_len)
{
    ::USB_DeviceClockInit();

    /* Set mctp generic to default state */
    g_UsbDevice.speed          = USB_SPEED_FULL;
    g_UsbDevice.attach         = 0U;
    g_UsbDevice.mctp_handle    = (class_handle_t) nullptr;
    g_UsbDevice.device_handle  = nullptr;
    g_UsbDevice.mctp_buffer[0] = (uint8_t*)mctp_buffer0;
    g_UsbDevice.mctp_buffer[1] = (uint8_t*)mctp_buffer1;
#if defined(USB_CONFIG_COMPOSITE)
    g_UsbDevice.hid_handle    = (class_handle_t) nullptr;
    g_UsbDevice.hid_buffer[0] = (uint8_t*)hid_buffer0;
    g_UsbDevice.hid_buffer[1] = (uint8_t*)hid_buffer1;
    g_UsbDevice.hid_buffer[2] = (uint8_t*)hid_buffer2;
#if defined(USB_DEVICE_CONFIG_VENDOR_SPECIFIC)
    g_UsbDevice.spi_handle    = (class_handle_t) nullptr;
    g_UsbDevice.spi_buffer[0] = (uint8_t*)spi_buffer0;
    g_UsbDevice.spi_buffer[1] = (uint8_t*)spi_buffer1;
    g_UsbDevice.spi_rx_len    = (uint32_t*)spi_rx_len;
#endif
#endif

    usb_status_t status = kStatus_USB_Success;

    /* Initialize the usb stack and class drivers */
    status = USB_DeviceClassInit(
        CONTROLLER_ID, &g_UsbDeviceCompositeConfigList, &g_UsbDevice.device_handle);

    if (status != kStatus_USB_Success) {
        nv::info("USB device class init failed with status: %d\r\n", status);
    }
    else {
        g_UsbDevice.mctp_handle = g_UsbDeviceCompositeConfigList.config[0].classHandle;
#if defined(USB_CONFIG_COMPOSITE)
        g_UsbDevice.hid_handle = g_UsbDeviceCompositeConfigList.config[1].classHandle;
#if defined(USB_DEVICE_CONFIG_VENDOR_SPECIFIC)
        g_UsbDevice.spi_handle = g_UsbDeviceCompositeConfigList.config[2].classHandle;
#endif
#endif

#if defined(USB_CONFIG_UART_BRIDGE)
#if defined(USB_CONFIG_MCTP)
        s_vcomHandle = g_UsbDeviceCompositeConfigList.config[1].classHandle;
#endif
#if defined(USB_CONFIG_COMPOSITE)
#if defined(USB_DEVICE_CONFIG_VENDOR_SPECIFIC)
        s_vcomHandle = g_UsbDeviceCompositeConfigList.config[3].classHandle;
#else
        s_vcomHandle = g_UsbDeviceCompositeConfigList.config[2].classHandle;
#endif
#endif
#endif

        nv::info("USB device class init pass!\r\n");

        ::USB_DeviceIsrEnable();

        SDK_DelayAtLeastUs(5000, SDK_DEVICE_MAXIMUM_CPU_CLOCK_FREQUENCY);

        status = USB_DeviceRun(g_UsbDevice.device_handle);
        if (status != kStatus_USB_Success) {
            nv::info("USB device run failed with status: %d\r\n", status);
        }
    }

    return static_cast<uint8_t>(status);
}

NV_PRIVILEGED_FUNCTION uint8_t Driver::write_mctp(uint8_t* data, uint32_t length)
{
    taskENTER_CRITICAL();
    auto result = USB_DeviceMctpSend(
        g_UsbDevice.mctp_handle, USB_MCTP_ENDPOINT_IN, data, length);
    taskEXIT_CRITICAL();
    return result;
}

NV_PRIVILEGED_FUNCTION usb_status_t Driver::enable_mctp_rx()
{
    taskENTER_CRITICAL();
    auto result = USB_DeviceMctpRecv(g_UsbDevice.mctp_handle,
                                     USB_MCTP_ENDPOINT_OUT,
                                     (uint8_t*)&g_UsbDevice.mctp_buffer[0][0],
                                     USB_MCTP_OUT_BUFFER_LENGTH);
    taskEXIT_CRITICAL();
    return result;
}

NV_PRIVILEGED_FUNCTION usb_status_t Driver::enable_hid_rx()
{
    usb_status_t result = kStatus_USB_Success;
#if defined(USB_CONFIG_COMPOSITE)
    result = USB_DeviceHidRecv(g_UsbDevice.hid_handle,
                               USB_HID_GENERIC_ENDPOINT_OUT,
                               (uint8_t*)&g_UsbDevice.hid_buffer[0][0],
                               USB_HID_GENERIC_OUT_BUFFER_LENGTH);
#endif
    return result;
}

uint8_t Driver::write_hid(uint8_t* data, uint32_t length)
{
    usb_status_t result = kStatus_USB_Success;
#if defined(USB_CONFIG_COMPOSITE)
    taskENTER_CRITICAL();
    result = USB_DeviceHidSend(
        g_UsbDevice.hid_handle, USB_HID_GENERIC_ENDPOINT_IN, data, length);
    taskEXIT_CRITICAL();
#endif
    return result;
}

NV_PRIVILEGED_FUNCTION uint8_t Driver::write_spi(uint8_t* data, uint32_t length)
{
    usb_status_t result = kStatus_USB_Success;
#if defined(USB_DEVICE_CONFIG_VENDOR_SPECIFIC)
    result = USB_DeviceSpiSend(
        g_UsbDevice.spi_handle, USB_NV_SMA_SPI_ENDPOINT_IN, data, length);
#endif
    return result;
}

NV_PRIVILEGED_FUNCTION usb_status_t Driver::enable_spi_rx()
{
    usb_status_t result = kStatus_USB_Success;
#if defined(USB_DEVICE_CONFIG_VENDOR_SPECIFIC)
    result = USB_DeviceSpiRecv(g_UsbDevice.spi_handle,
                               USB_NV_SMA_SPI_ENDPOINT_OUT,
                               (uint8_t*)&g_UsbDevice.spi_buffer[0][0],
                               USB_NV_SMA_SPI_OUT_BUFFER_LENGTH);
#endif
    return result;
}

void Driver::recover_spi_endpoint()
{
#if defined(USB_DEVICE_CONFIG_VENDOR_SPECIFIC)
    // Get the SPI handle to check and clear busy flag
    usb_device_spi_struct_t* spiHandle = (usb_device_spi_struct_t*)g_UsbDevice.spi_handle;
    if (spiHandle != nullptr && spiHandle->bulkInPipeBusy) {
        spiHandle->bulkInPipeBusy = 0U;

        // Cancel any pending IN transfer
        USB_DeviceCancel(g_UsbDevice.device_handle, USB_NV_SMA_SPI_ENDPOINT_IN | 0x80);
    }
#endif
}

usb_status_t Driver::usb_deviceSpicallback(class_handle_t handle, uint32_t event, void* param)
{
    usb_status_t error = kStatus_USB_InvalidRequest;
#if defined(USB_DEVICE_CONFIG_VENDOR_SPECIFIC)
    usb_device_endpoint_callback_message_struct_t* ep_cb_param;
    ep_cb_param = (usb_device_endpoint_callback_message_struct_t*)param;

    switch (event) {
        case kUSB_DeviceSpiEventSendResponse:
            nv::usb::Task::set_lstp_tx_done_event();
            error = kStatus_USB_Success;
            break;

        case kUSB_DeviceSpiEventRecvResponse:
            if (g_UsbDevice.attach
                && (ep_cb_param->length != (USB_CANCELLED_TRANSFER_LENGTH))) {
                if (g_UsbDevice.buffer_index == 0) {
                    *g_UsbDevice.spi_rx_len = ep_cb_param->length;
                    nv::usb::Task::set_lstp_rx_event();
                }
            }
            error = kStatus_USB_Success;
            break;
        default: break;
    }
#endif
    return error;
}

bool Driver::check_vbus()
{
    volatile USBHS_Type* ehciRegisterBase;
    uint32_t             usbhsBaseAddrs[] = USBHS_BASE_ADDRS;
    ehciRegisterBase = (USBHS_Type*)usbhsBaseAddrs[CONTROLLER_ID - kUSB_ControllerEhci0];

    auto is_vbus_on = (ehciRegisterBase->OTGSC & USBHS_OTGSC_BSV_MASK) >> USBHS_OTGSC_BSV_SHIFT;

    return is_vbus_on;
}

bool Driver::is_device_connected()
{
    return g_UsbDevice.attach != 0U && g_UsbDevice.current_configuration != 0U;
}

#if (defined(USB_DEVICE_CONFIG_EHCI) && (USB_DEVICE_CONFIG_EHCI > 0U))
void USB1_HS_IRQHandler(void)
{
    USB_DeviceEhciIsrFunction(g_UsbDevice.device_handle);
}
#endif

usb_status_t Driver::usb_devicemctpcallback(class_handle_t handle, uint32_t event, void* param)
{
    usb_status_t                                   error = kStatus_USB_InvalidRequest;
    usb_device_endpoint_callback_message_struct_t* ep_cb_param;
    ep_cb_param = (usb_device_endpoint_callback_message_struct_t*)param;

    switch (event) {
        case kUSB_DeviceMctpEventSendResponse:
            nv::usb::Task::set_mctp_tx_done_event();
            error = kStatus_USB_Success;
            break;
        case kUSB_DeviceMctpEventRecvResponse:
            if (g_UsbDevice.attach
                && (ep_cb_param->length != (USB_CANCELLED_TRANSFER_LENGTH))) {
#if defined(USB_RAW_DATA_CHECK)
                // NV USB hook echo function
                USB_DeviceMctpSend(
                    g_UsbDevice.mctp_handle,
                    USB_MCTP_ENDPOINT_IN,
                    (uint8_t*)&g_UsbDevice.mctp_buffer.at(g_UsbDevice.buffer_index)[0],
                    USB_MCTP_OUT_BUFFER_LENGTH);
                for (unsigned int i = 0; i < USB_MCTP_OUT_BUFFER_LENGTH; i++) {
                    usb_echo("%02x ", g_UsbDevice.mctp_buffer.at(g_UsbDevice.buffer_index)[i]);
                }
#endif
                if (g_UsbDevice.buffer_index == 0) {
                    nv::usb::Task::set_mctp_rx0_event();
                }
            }
            break;
        default: break;
    }

    return error;
}

usb_status_t Driver::usb_devicecallback(usb_device_handle handle, uint32_t event, void* param)
{
    usb_status_t error  = kStatus_USB_InvalidRequest;
    uint8_t*     temp8  = (uint8_t*)param;
    uint16_t*    temp16 = (uint16_t*)param;

    switch (event) {
        case kUSB_DeviceEventBusReset: {
            if constexpr (nv::ipc::EnableUsbPortResetSelfReset) {
                bool was_enumerated = (g_UsbDevice.attach != 0U)
                                   || (g_UsbDevice.current_configuration != 0U);
                if (was_enumerated) {
                    // Record in mailbox
                    nv::mainbox::write_mailbox_u32(nv::mainbox::MainBoxMemoryType::UsbPortReset,
                                                   nv::usb::UsbPortResetMagicNumber);
                    // Trigger software reset
                    nv::bootloader::Driver::self_reset();  // Never returns
                }
            }
            nv::usb::Task::reset_all_event_bits();
            g_UsbDevice.attach                = 0U;
            g_UsbDevice.current_configuration = 0U;
            error                             = kStatus_USB_Success;
#if (defined(USB_DEVICE_CONFIG_EHCI) && (USB_DEVICE_CONFIG_EHCI > 0U))                         \
    || (defined(USB_DEVICE_CONFIG_LPCIP3511HS) && (USB_DEVICE_CONFIG_LPCIP3511HS > 0U))
            /* Get USB speed to configure the device, including max packet size and
             * interval of the endpoints. */
            if (kStatus_USB_Success
                == USB_DeviceClassGetSpeed(CONTROLLER_ID, &g_UsbDevice.speed)) {
                USB_DeviceSetSpeed(handle, g_UsbDevice.speed);
            }
#endif
        } break;
        case kUSB_DeviceEventSetConfiguration:
            if (0U == (*temp8)) {
                g_UsbDevice.attach                = 0U;
                g_UsbDevice.current_configuration = 0U;
                error                             = kStatus_USB_Success;
            }
            else if (USB_MCTP_CONFIGURE_INDEX == (*temp8)) {
                /* Set device configuration request */
                nv::usb::Task::set_device_attach_event();
                g_UsbDevice.attach                = 1U;
                g_UsbDevice.current_configuration = *temp8;

                // Initialize MCTP endpoint
                error = USB_DeviceMctpRecv(g_UsbDevice.mctp_handle,
                                           USB_MCTP_ENDPOINT_OUT,
                                           g_UsbDevice.mctp_buffer.at(g_UsbDevice.buffer_index),
                                           USB_MCTP_OUT_BUFFER_LENGTH);
#if defined(USB_CONFIG_UART_BRIDGE)
                // Initialize vcom endpoint with separate buffer
                Driver::usb_vcomsetconfigure(s_vcomHandle, *temp8);
#endif

#if defined(USB_CONFIG_COMPOSITE)

                // Initialize HID endpoint with separate buffer
                const auto HidError = USB_DeviceHidRecv(
                    g_UsbDevice.hid_handle,
                    USB_HID_GENERIC_ENDPOINT_OUT,
                    g_UsbDevice.hid_buffer.at(g_UsbDevice.buffer_index),
                    USB_HID_GENERIC_OUT_BUFFER_LENGTH);
                if (HidError != kStatus_USB_Success) {
                    error = HidError;
                }
#if defined(USB_DEVICE_CONFIG_VENDOR_SPECIFIC)
                // Initialize SPI endpoint with separate buffer
                const auto spiError = USB_DeviceSpiRecv(
                    g_UsbDevice.spi_handle,
                    USB_NV_SMA_SPI_ENDPOINT_OUT,
                    g_UsbDevice.spi_buffer.at(g_UsbDevice.buffer_index),
                    USB_NV_SMA_SPI_OUT_BUFFER_LENGTH);
                if (spiError != kStatus_USB_Success) {
                    error = spiError;
                }
#endif
#endif
            }
            else {
                /* no action required, the default return value is
                 * kStatus_USB_InvalidRequest.
                 */
            }
            break;
        case kUSB_DeviceEventSetInterface:
            if (g_UsbDevice.attach) {
                /* Set device interface request */
                uint8_t interface        = (uint8_t)((*temp16 & 0xFF00U) >> 0x08U);
                uint8_t alternateSetting = (uint8_t)(*temp16 & 0x00FFU);

                if (USB_MCTP_INTERFACE_INDEX == interface) {
                    if (alternateSetting < USB_MCTP_INTERFACE_ALTERNATE_COUNT) {
                        g_UsbDevice
                            .current_interface_alternate_setting[interface] = alternateSetting;
                        if (alternateSetting == USB_MCTP_INTERFACE_ALTERNATE_0) {
                            error = USB_DeviceMctpRecv(g_UsbDevice.mctp_handle,
                                                       USB_MCTP_ENDPOINT_OUT,
                                                       (uint8_t*)&g_UsbDevice.mctp_buffer.at(
                                                           g_UsbDevice.buffer_index)[0],
                                                       USB_MCTP_OUT_BUFFER_LENGTH);
                        }
                    }
                }

#if defined(USB_CONFIG_UART_BRIDGE)
                else if (interface == USB_CDC_VCOM_CIC_INTERFACE_INDEX) {
                    if (alternateSetting < USB_CDC_VCOM_CIC_INTERFACE_ALTERNATE_COUNT) {
                        g_UsbDevice
                            .current_interface_alternate_setting[interface] = alternateSetting;
                        error = kStatus_USB_Success;
                    }
                }
                else if (interface == USB_CDC_VCOM_DIC_INTERFACE_INDEX) {
                    if (alternateSetting < USB_CDC_VCOM_DIC_INTERFACE_ALTERNATE_COUNT) {
                        g_UsbDevice
                            .current_interface_alternate_setting[interface] = alternateSetting;
                        error = kStatus_USB_Success;
                    }
                }
#endif

#if defined(USB_CONFIG_COMPOSITE)
                else if (USB_HID_GENERIC_INTERFACE_INDEX == interface) {
                    if (alternateSetting < USB_MCTP_INTERFACE_ALTERNATE_COUNT) {
                        g_UsbDevice
                            .current_interface_alternate_setting[interface] = alternateSetting;
                        if (alternateSetting == USB_MCTP_INTERFACE_ALTERNATE_0) {
                            error = kStatus_USB_Success;
                        }
                    }
                }
#if defined(USB_DEVICE_CONFIG_VENDOR_SPECIFIC)
                else if (USB_NV_SMA_SPI_INTERFACE_INDEX == interface) {
                    if (alternateSetting < USB_MCTP_INTERFACE_ALTERNATE_COUNT) {
                        g_UsbDevice
                            .current_interface_alternate_setting[interface] = alternateSetting;
                        if (alternateSetting == USB_MCTP_INTERFACE_ALTERNATE_0) {
                            error = kStatus_USB_Success;
                        }
                    }
                }
#endif
#endif
                else {
                    /* no action, return kStatus_USB_InvalidRequest. */
                }
            }
            break;
        case kUSB_DeviceEventGetConfiguration:
            if (param) {
                *temp8 = g_UsbDevice.current_configuration;
                error  = kStatus_USB_Success;
            }
            break;
        case kUSB_DeviceEventGetInterface:
            if (param) {
                /* Get current alternate setting of the interface request */
                uint8_t interface = (uint8_t)((*temp16 & 0xFF00U) >> 0x08U);
                if (interface < SYS_USB_COMPOSITE_INTERFACE_COUNT) {
                    *temp16 = (*temp16 & 0xFF00U)
                            | g_UsbDevice.current_interface_alternate_setting[interface];
                    error = kStatus_USB_Success;
                }
            }
            break;
        case kUSB_DeviceEventGetDeviceDescriptor:
            if (param) {
                error = USB_DeviceGetDeviceDescriptor(
                    handle, (usb_device_get_device_descriptor_struct_t*)param);
            }
            break;
        case kUSB_DeviceEventGetConfigurationDescriptor:
            if (param) {
                error = USB_DeviceGetConfigurationDescriptor(
                    handle, (usb_device_get_configuration_descriptor_struct_t*)param);
            }
            break;
        case kUSB_DeviceEventGetStringDescriptor:
            if (param) {
                error = USB_DeviceGetStringDescriptor(
                    handle, (usb_device_get_string_descriptor_struct_t*)param);
            }
            break;
#if ((defined(USB_DEVICE_CONFIG_REMOTE_WAKEUP)) && (USB_DEVICE_CONFIG_REMOTE_WAKEUP > 0U))
        case kUSB_DeviceEventSetRemoteWakeup:
            if (param) {
                g_UsbDevice.remote_wakeup = *temp8;
                error                     = kStatus_USB_Success;
            }
            break;
#endif

#if defined(USB_CONFIG_UART_BRIDGE)
            // more case TO-BE-IMPLEMENTED
#endif

#if defined(USB_CONFIG_COMPOSITE)
        case kUSB_DeviceEventGetHidDescriptor:
            if (param) {
                /* Get hid descriptor request */
                error = USB_DeviceGetHidDescriptor(
                    handle, (usb_device_get_hid_descriptor_struct_t*)param);
            }
            break;
        case kUSB_DeviceEventGetHidReportDescriptor:
            if (param) {
                /* Get hid report descriptor request */
                error = USB_DeviceGetHidReportDescriptor(
                    handle, (usb_device_get_hid_report_descriptor_struct_t*)param);
            }
            break;
        case kUSB_DeviceEventGetHidPhysicalDescriptor:
            if (param) {
                /* Get hid physical descriptor request */
                error = USB_DeviceGetHidPhysicalDescriptor(
                    handle, (usb_device_get_hid_physical_descriptor_struct_t*)param);
            }
            break;
#endif
        default: break;
    }

    return error;
}
}  // namespace sys::usb

#if defined(__cplusplus)
}
#endif /* __cplusplus*/

// NOLINTEND
