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

#include "ecm_types.h"
#include "eth_adapter.h"
#include "ecm_usb_descriptor.h"

// Bare-metal IPC driver
#include "sys/ipc_bm/driver.h"

#include <cstring>

#if USB_DEVICE_CONFIG_USE_TASK
#include "FreeRTOS.h"
#include "task.h"
#endif

extern "C" {
#include "usb_device_config.h"
#include "usb_device.h"
#include "usb_device_class.h"
#include "usb_device_cdc_acm.h"
#include "usb_device_hid.h"

#if USB_DEVICE_CONFIG_CDC_ECM
#include "usb_device_cdc_ecm.h"
#endif

// Global ECM device handle for IRQ handler (exported for usb.cpp)
usb_device_handle g_ecm_device_handle = nullptr;
}

// Controller ID based on USB device config
#if defined(USB_DEVICE_CONFIG_EHCI) && (USB_DEVICE_CONFIG_EHCI > 0)
#define CONTROLLER_ID kUSB_ControllerEhci0
#elif defined(USB_DEVICE_CONFIG_KHCI) && (USB_DEVICE_CONFIG_KHCI > 0)
#define CONTROLLER_ID kUSB_ControllerKhci0
#elif defined(USB_DEVICE_CONFIG_LPCIP3511FS) && (USB_DEVICE_CONFIG_LPCIP3511FS > 0U)
#define CONTROLLER_ID kUSB_ControllerLpcIp3511Fs0
#elif defined(USB_DEVICE_CONFIG_LPCIP3511HS) && (USB_DEVICE_CONFIG_LPCIP3511HS > 0U)
#define CONTROLLER_ID kUSB_ControllerLpcIp3511Hs0
#else
#define CONTROLLER_ID kUSB_ControllerLpcIp3511Hs0
#endif

namespace {

// ECM context - encapsulates all module-level state
struct EcmContext
{
    nv::ecm_bm::EcmNicHandle* handle   = nullptr;
    volatile uint32_t*        appEvent = nullptr;

#if USB_DEVICE_CONFIG_CDC_ECM
    // Data buffers for ECM notifications
    USB_DMA_NONINIT_DATA_ALIGN(USB_DATA_ALIGN_SIZE)
    uint8_t notifyNetworkConnectionReq[sizeof(usb_setup_struct_t)];

    USB_DMA_NONINIT_DATA_ALIGN(USB_DATA_ALIGN_SIZE)
    uint8_t notifyConnectionSpeedChangeReq[sizeof(usb_setup_struct_t) + 8];

    // USB RX buffer for CDC-ECM data
    USB_DMA_NONINIT_DATA_ALIGN(USB_DATA_ALIGN_SIZE)
    uint8_t ecmRxBuffer[nv::ecm_bm::EthFrameMaxLength];
#endif

    static EcmContext& instance()
    {
        static EcmContext ctx;
        return ctx;
    }
};

// Convenience accessors
inline EcmContext& ctx()
{
    return EcmContext::instance();
}

}  // namespace

extern "C" {

// Flag: need to re-arm USB receive from main loop (callback re-arm may fail)
volatile uint8_t g_ecm_rx_need_rearm = 0;

// USB endpoint buffers and state (grouped per endpoint)
#include "usb_buffers.h"

static struct usb_mctp_bufs g_mctp = {};
#if USB_CONFIG_COMPOSITE
static struct usb_hid_bufs g_hid = {};
#endif
#if USB_DEVICE_CONFIG_CDC_ACM
static struct usb_acm_bufs g_acm = {};
#endif /* USB_DEVICE_CONFIG_CDC_ACM */
#if USB_CONFIG_LSTP
static struct usb_lstp_bufs g_lstp = {.tx_busy_ch_id = nv::lstp::LstpNumChannels};
#endif /* USB_CONFIG_LSTP */

static void clear_in_endpoint_busy(void)
{
    g_mctp.tx_busy = 0;
#if USB_CONFIG_COMPOSITE
    g_hid.tx_busy = 0;
#endif
#if USB_DEVICE_CONFIG_CDC_ACM
    g_acm.tx_busy = 0;
#endif
#if USB_CONFIG_LSTP
    USB_CLEAR_LSTP_TX_BUSY(g_lstp);
#endif
}

struct usb_mctp_bufs* get_usb_mctp_bufs(void)
{
    return &g_mctp;
}
#if USB_CONFIG_COMPOSITE
struct usb_hid_bufs* get_usb_hid_bufs(void)
{
    return &g_hid;
}
#endif
#if USB_DEVICE_CONFIG_CDC_ACM
struct usb_acm_bufs* get_usb_acm_bufs(void)
{
    return &g_acm;
}

// Called from ipc_bm when Core0 signals UART TX done (InterCoreAcmTxDone)
void ncsi_signal_acm_rx_rearm(void)
{
    g_acm.rx_need_rearm = 1;
}
#endif

#if USB_CONFIG_LSTP
struct usb_lstp_bufs* get_usb_lstp_bufs(void)
{
    return &g_lstp;
}
#endif /* USB_CONFIG_LSTP */

#if USB_DEVICE_CONFIG_CDC_ECM
// Forward declarations
extern eth_adapter_handle_t g_eth_adapter_handle;

// USB CDC-ECM class callback
usb_status_t USB_DeviceCdcEcmCallback(class_handle_t handle, uint32_t event, void* param)
{
    usb_status_t status = kStatus_USB_Success;
    usb_device_control_request_struct_t*
        request = reinterpret_cast<usb_device_control_request_struct_t*>(param);
    usb_device_endpoint_callback_message_struct_t*
        epMsg = reinterpret_cast<usb_device_endpoint_callback_message_struct_t*>(param);

    auto& ecm = ctx();
    if (!ecm.handle) {
        return kStatus_USB_Error;
    }

    switch (event) {
        case kUSB_DeviceCdcEcmEventSendResponse:
            // TX complete - nothing to do, main loop will send next frame
            break;

        case kUSB_DeviceCdcEcmEventRecvResponse:
            // Receive complete - push to Ethernet TX queue
            if (epMsg && epMsg->length != USB_CANCELLED_TRANSFER_LENGTH && epMsg->length > 0
                && epMsg->length <= nv::ecm_bm::EthFrameMaxLength) {
                // Allocate buffer in TX frame queue
                eth_adapter_frame_buf_t* frame_buf = nullptr;
                if (ETH_ADAPTER_FrameQueueAlloc(&g_eth_adapter_handle.txFrameQueue, &frame_buf)
                    == ETH_ADAPTER_OK) {
                    // Copy received data to frame buffer
                    memcpy(frame_buf->payload, ecm.ecmRxBuffer, epMsg->length);
                    frame_buf->len = epMsg->length;
                }
            }

            // Always restart USB receive (even on zero-length or error)
            if (ecm.handle && ecm.handle->cdcEcmHandle) {
                usb_status_t recv_status = USB_DeviceCdcEcmRecv(
                    ecm.handle->cdcEcmHandle,
                    USB_DEVICE_CDC_ECM_DATA_BULK_OUT_EP_NUMBER,
                    ecm.ecmRxBuffer,
                    nv::ecm_bm::EthFrameMaxLength);
                g_ecm_rx_need_rearm = (recv_status != kStatus_USB_Success) ? 1 : 0;
            }
            break;

        case kUSB_DeviceCdcEcmEventNotifyResponse:
            // Notification sent
            break;

        case kUSB_DeviceCdcEcmEventSetEthernetPacketFilter:
            // Like SDK: just set attach status and packet filter flags
            ecm.handle->attachStatus = 1U;

            if (request->setup->wValue & USB_DEVICE_CDC_ECM_PACKET_TYPE_PROMISCUOUS_MASK) {
                g_eth_adapter_handle.broadcastFramePass = true;
                g_eth_adapter_handle.multicastFramePass = true;
                g_eth_adapter_handle.unicastFramePass   = true;
            }
            else {
                g_eth_adapter_handle
                    .multicastFramePass = (request->setup->wValue
                                           & USB_DEVICE_CDC_ECM_PACKET_TYPE_ALL_MULTICAST_MASK)
                                       != 0;
                g_eth_adapter_handle
                    .unicastFramePass = (request->setup->wValue
                                         & USB_DEVICE_CDC_ECM_PACKET_TYPE_DIRECTED_MASK)
                                     != 0;
                g_eth_adapter_handle
                    .broadcastFramePass = (request->setup->wValue
                                           & USB_DEVICE_CDC_ECM_PACKET_TYPE_BROADCAST_MASK)
                                       != 0;
            }
            // Trigger (re)notify to ensure carrier comes up
            if (ecm.appEvent) {
                nv::ecm_bm::ecm_event_set(*ecm.appEvent,
                                          nv::ecm_bm::EcmEvent::NotifyNetworkChange);
            }
            break;

        default: status = kStatus_USB_InvalidRequest; break;
    }

    return status;
}
#endif /* USB_DEVICE_CONFIG_CDC_ECM */

/*******************************************************************************
 * MCTP Endpoint Callbacks (raw bulk endpoints)
 ******************************************************************************/

// Received data lengths are in rx_length of the corresponding buffer

// MCTP OUT endpoint callback (data received from host)
usb_status_t USB_DeviceMctpOutCallback(usb_device_handle                              handle,
                                       usb_device_endpoint_callback_message_struct_t* message,
                                       void* callbackParam)
{
    (void)handle;
    (void)callbackParam;

    auto& ecm = ctx();
    if (message->length != USB_CANCELLED_TRANSFER_LENGTH && ecm.appEvent) {
        g_mctp.rx_length = message->length;
        APP_EVENT_SET(*ecm.appEvent, nv::ecm_bm::kAPP_MctpRxReady);
    }

    return kStatus_USB_Success;
}

// MCTP IN endpoint callback (data sent to host)
usb_status_t USB_DeviceMctpInCallback(usb_device_handle                              handle,
                                      usb_device_endpoint_callback_message_struct_t* message,
                                      void* callbackParam)
{
    (void)handle;
    (void)message;
    (void)callbackParam;
    g_mctp.tx_busy = 0;
    return kStatus_USB_Success;
}

#if USB_CONFIG_COMPOSITE
/*******************************************************************************
 * HID Endpoint Callbacks (raw interrupt endpoints)
 ******************************************************************************/

// HID OUT endpoint callback (data received from host)
usb_status_t USB_DeviceHidOutCallback(usb_device_handle                              handle,
                                      usb_device_endpoint_callback_message_struct_t* message,
                                      void* callbackParam)
{
    (void)handle;
    (void)callbackParam;

    auto& ecm = ctx();
    if (message->length != USB_CANCELLED_TRANSFER_LENGTH && ecm.appEvent) {
        g_hid.rx_length = message->length;
        APP_EVENT_SET(*ecm.appEvent, nv::ecm_bm::kAPP_HidRxReady);
    }

    return kStatus_USB_Success;
}

// HID IN endpoint callback (data sent to host)
usb_status_t USB_DeviceHidInCallback(usb_device_handle                              handle,
                                     usb_device_endpoint_callback_message_struct_t* message,
                                     void* callbackParam)
{
    (void)handle;
    (void)message;
    (void)callbackParam;
    g_hid.tx_busy = 0;
    return kStatus_USB_Success;
}
#endif /* USB_CONFIG_COMPOSITE */

#if USB_CONFIG_LSTP
/*******************************************************************************
 * LSTP Endpoint Callbacks (raw bulk endpoints)
 ******************************************************************************/

usb_status_t USB_DeviceLstpOutCallback(usb_device_handle                              handle,
                                       usb_device_endpoint_callback_message_struct_t* message,
                                       void* callbackParam)
{
    (void)handle;
    (void)callbackParam;

    auto& ecm = ctx();
    if (message->length != USB_CANCELLED_TRANSFER_LENGTH && ecm.appEvent) {
        g_lstp.rx_length = message->length;
        APP_EVENT_SET(*ecm.appEvent, nv::ecm_bm::kAPP_LstpRxReady);
    }

    return kStatus_USB_Success;
}

usb_status_t USB_DeviceLstpInCallback(usb_device_handle                              handle,
                                      usb_device_endpoint_callback_message_struct_t* message,
                                      void* callbackParam)
{
    (void)handle;
    (void)message;
    (void)callbackParam;
    USB_CLEAR_LSTP_TX_BUSY(g_lstp);
    return kStatus_USB_Success;
}
#endif /* USB_CONFIG_LSTP */

#if USB_DEVICE_CONFIG_CDC_ACM
/*******************************************************************************
 * ACM Endpoint Callbacks (UART Bridge)
 ******************************************************************************/

// ACM OUT endpoint callback (data received from host)
usb_status_t USB_DeviceAcmOutCallback(usb_device_handle                              handle,
                                      usb_device_endpoint_callback_message_struct_t* message,
                                      void* callbackParam)
{
    (void)handle;
    (void)callbackParam;

    auto& ecm = ctx();
    if (message->length != USB_CANCELLED_TRANSFER_LENGTH && ecm.appEvent) {
        g_acm.rx_length = message->length;
        APP_EVENT_SET(*ecm.appEvent, nv::ecm_bm::kAPP_AcmRxReady);
    }
    // Re-arm is done in main loop only when Core0 signals UART TX done
    // (InterCoreAcmTxDone), not here.

    return kStatus_USB_Success;
}

// ACM IN endpoint callback (data sent to host)
usb_status_t USB_DeviceAcmInCallback(usb_device_handle                              handle,
                                     usb_device_endpoint_callback_message_struct_t* message,
                                     void* callbackParam)
{
    (void)handle;
    (void)message;
    (void)callbackParam;
    // TX complete - clear busy flag so main loop can send next packet
    g_acm.tx_busy = 0;
    return kStatus_USB_Success;
}
#endif /* USB_DEVICE_CONFIG_CDC_ACM */

// USB device callback
usb_status_t USB_DeviceCallback(usb_device_handle handle, uint32_t event, void* param)
{
    usb_status_t status = kStatus_USB_Error;
    auto&        ecm    = ctx();

    if (!ecm.handle) {
        return status;
    }

    switch (event) {
        case kUSB_DeviceEventBusReset:
#if (defined(USB_DEVICE_CONFIG_LPCIP3511HS) && (USB_DEVICE_CONFIG_LPCIP3511HS > 0U))
#if !((defined FSL_FEATURE_SOC_USBPHY_COUNT) && (FSL_FEATURE_SOC_USBPHY_COUNT > 0U))
            USB_DeviceHsPhyChirpIssueWorkaround();
#endif
#endif

#if (defined(USB_DEVICE_CONFIG_EHCI) && (USB_DEVICE_CONFIG_EHCI > 0U))                         \
    || (defined(USB_DEVICE_CONFIG_LPCIP3511HS) && (USB_DEVICE_CONFIG_LPCIP3511HS > 0U))
            if (USB_DeviceClassGetSpeed(CONTROLLER_ID, &ecm.handle->deviceSpeed)
                == kStatus_USB_Success) {
                ECM_USB_DeviceSetSpeed(handle, ecm.handle->deviceSpeed);
            }
#endif

            ecm.handle->configuration = 0U;
            ecm.handle->attachStatus  = 0U;
            ecm.handle->linkStatus    = 0U;
            clear_in_endpoint_busy();

            if (ecm.appEvent) {
                nv::ecm_bm::ecm_event_clear(*ecm.appEvent);
            }

            status = kStatus_USB_Success;
            break;

#if (defined(USB_DEVICE_CONFIG_DETACH_ENABLE) && (USB_DEVICE_CONFIG_DETACH_ENABLE > 0U))
        case kUSB_DeviceEventDetach:
            ecm.handle->attachStatus = 0U;
            clear_in_endpoint_busy();
            if (ecm.appEvent) {
                nv::ecm_bm::ecm_event_clear(*ecm.appEvent);
            }
            status = kStatus_USB_Success;
            break;
#endif

        case kUSB_DeviceEventGetDeviceDescriptor:
            if (param) {
                status = ECM_USB_DeviceGetDeviceDescriptor(
                    handle,
                    reinterpret_cast<usb_device_get_device_descriptor_struct_t*>(param));
            }
            break;

        case kUSB_DeviceEventGetConfigurationDescriptor:
            if (param) {
                status = ECM_USB_DeviceGetConfigurationDescriptor(
                    handle,
                    reinterpret_cast<usb_device_get_configuration_descriptor_struct_t*>(param));
            }
            break;

        case kUSB_DeviceEventGetConfiguration:
            if (param) {
                *reinterpret_cast<uint8_t*>(param) = ecm.handle->configuration;
                status                             = kStatus_USB_Success;
            }
            break;

        case kUSB_DeviceEventSetConfiguration:
            ecm.handle->configuration = *reinterpret_cast<uint8_t*>(param);
            clear_in_endpoint_busy();

            if (*reinterpret_cast<uint8_t*>(param) == USB_COMPOSITE_CONFIGURE_INDEX) {
                // Initialize MCTP bulk endpoints with callbacks
                usb_device_endpoint_init_struct_t mctpOutEpInit = {
                    .maxPacketSize   = HS_MCTP_CLASS_OUT_PACKET_SIZE,
                    .endpointAddress = USB_MCTP_ENDPOINT_OUT | (USB_OUT << 7U),
                    .transferType    = USB_ENDPOINT_BULK,
                    .zlt             = 0U,
                    .interval        = 0U,
                };
                usb_device_endpoint_callback_struct_t mctpOutEpCb = {
                    .callbackFn    = USB_DeviceMctpOutCallback,
                    .callbackParam = nullptr,
                    .isBusy        = 0U,
                };
                USB_DeviceInitEndpoint(ecm.handle->deviceHandle, &mctpOutEpInit, &mctpOutEpCb);

                usb_device_endpoint_init_struct_t mctpInEpInit = {
                    .maxPacketSize   = HS_MCTP_CLASS_IN_PACKET_SIZE,
                    .endpointAddress = USB_MCTP_ENDPOINT_IN | (USB_IN << 7U),
                    .transferType    = USB_ENDPOINT_BULK,
                    .zlt             = 0U,
                    .interval        = 0U,
                };
                usb_device_endpoint_callback_struct_t mctpInEpCb = {
                    .callbackFn    = USB_DeviceMctpInCallback,
                    .callbackParam = nullptr,
                    .isBusy        = 0U,
                };
                USB_DeviceInitEndpoint(ecm.handle->deviceHandle, &mctpInEpInit, &mctpInEpCb);

#if USB_CONFIG_COMPOSITE
                // Initialize HID interrupt endpoints with callbacks
                usb_device_endpoint_init_struct_t hidOutEpInit = {
                    .maxPacketSize   = HS_HID_INTERRUPT_OUT_PACKET_SIZE,
                    .endpointAddress = USB_HID_ENDPOINT_OUT | (USB_OUT << 7U),
                    .transferType    = USB_ENDPOINT_INTERRUPT,
                    .zlt             = 0U,
                    .interval        = HS_HID_INTERRUPT_OUT_INTERVAL,
                };
                usb_device_endpoint_callback_struct_t hidOutEpCb = {
                    .callbackFn    = USB_DeviceHidOutCallback,
                    .callbackParam = nullptr,
                    .isBusy        = 0U,
                };
                USB_DeviceInitEndpoint(ecm.handle->deviceHandle, &hidOutEpInit, &hidOutEpCb);

                usb_device_endpoint_init_struct_t hidInEpInit = {
                    .maxPacketSize   = HS_HID_INTERRUPT_IN_PACKET_SIZE,
                    .endpointAddress = USB_HID_ENDPOINT_IN | (USB_IN << 7U),
                    .transferType    = USB_ENDPOINT_INTERRUPT,
                    .zlt             = 0U,
                    .interval        = HS_HID_INTERRUPT_IN_INTERVAL,
                };
                usb_device_endpoint_callback_struct_t hidInEpCb = {
                    .callbackFn    = USB_DeviceHidInCallback,
                    .callbackParam = nullptr,
                    .isBusy        = 0U,
                };
                USB_DeviceInitEndpoint(ecm.handle->deviceHandle, &hidInEpInit, &hidInEpCb);
#endif /* USB_CONFIG_COMPOSITE */

#if USB_CONFIG_LSTP
                // Initialize LSTP bulk endpoints with callbacks
                usb_device_endpoint_init_struct_t lstpOutEpInit = {
                    .maxPacketSize   = USB_LSTP_CLASS_OUT_PACKET_SIZE,
                    .endpointAddress = USB_LSTP_ENDPOINT_OUT | (USB_OUT << 7U),
                    .transferType    = USB_ENDPOINT_BULK,
                    .zlt             = 0U,
                    .interval        = 0U,
                };
                usb_device_endpoint_callback_struct_t lstpOutEpCb = {
                    .callbackFn    = USB_DeviceLstpOutCallback,
                    .callbackParam = nullptr,
                    .isBusy        = 0U,
                };
                USB_DeviceInitEndpoint(ecm.handle->deviceHandle, &lstpOutEpInit, &lstpOutEpCb);

                usb_device_endpoint_init_struct_t lstpInEpInit = {
                    .maxPacketSize   = USB_LSTP_CLASS_IN_PACKET_SIZE,
                    .endpointAddress = USB_LSTP_ENDPOINT_IN | (USB_IN << 7U),
                    .transferType    = USB_ENDPOINT_BULK,
                    .zlt             = 0U,
                    .interval        = 0U,
                };
                usb_device_endpoint_callback_struct_t lstpInEpCb = {
                    .callbackFn    = USB_DeviceLstpInCallback,
                    .callbackParam = nullptr,
                    .isBusy        = 0U,
                };
                USB_DeviceInitEndpoint(ecm.handle->deviceHandle, &lstpInEpInit, &lstpInEpCb);
#endif /* USB_CONFIG_LSTP */

#if USB_DEVICE_CONFIG_CDC_ECM
                if (ecm.handle->cdcEcmHandle) {
                    ecm.handle->attachStatus = 0U;
                }
#endif

#if USB_DEVICE_CONFIG_CDC_ACM
                // Initialize ACM bulk OUT like SDK CDC ACM (zlt=0). How ZLT causes NYET:
                // - App passes epInit->zlt=0. EHCI driver sets QH capabilities.zlt =
                // (zlt==0)?1:0
                //   (usb_device_ehci.c USB_DeviceEhciEndpointInit), so QH gets ZLT=1.
                // - QH lives in RAM; USBHS controller reads it via EPLISTADDR. When an OUT
                // packet
                //   fills the current DTD and there is no next DTD, hardware uses QH ZLT: if 1,
                //   respond NYET (not ready for next); if 0, respond ACK. So zlt=0 here → NYET.
                // - Request length = maxPacketSize (one packet) so after 512 bytes there is no
                // next
                //   buffer; with QH ZLT=1 the controller responds NYET until we re-arm.
                usb_device_endpoint_init_struct_t acmOutEpInit = {
                    .maxPacketSize   = USB_DEVICE_CDC_ACM_DATA_BULK_OUT_EP_MAXPKT_SIZE_HS,
                    .endpointAddress = USB_DEVICE_CDC_ACM_DATA_BULK_OUT_EP_NUMBER
                                     | (USB_OUT << 7U),
                    .transferType = USB_ENDPOINT_BULK,
                    .zlt          = 0U,
                    .interval     = 0U,
                };
                usb_device_endpoint_callback_struct_t acmOutEpCb = {
                    .callbackFn    = USB_DeviceAcmOutCallback,
                    .callbackParam = nullptr,
                    .isBusy        = 0U,
                };
                USB_DeviceInitEndpoint(ecm.handle->deviceHandle, &acmOutEpInit, &acmOutEpCb);

                usb_device_endpoint_init_struct_t acmInEpInit = {
                    .maxPacketSize   = USB_DEVICE_CDC_ACM_DATA_BULK_IN_EP_MAXPKT_SIZE_HS,
                    .endpointAddress = USB_DEVICE_CDC_ACM_DATA_BULK_IN_EP_NUMBER
                                     | (USB_IN << 7U),
                    .transferType = USB_ENDPOINT_BULK,
                    .zlt          = 1U,
                    .interval     = 0U,
                };
                usb_device_endpoint_callback_struct_t acmInEpCb = {
                    .callbackFn    = USB_DeviceAcmInCallback,
                    .callbackParam = nullptr,
                    .isBusy        = 0U,
                };
                USB_DeviceInitEndpoint(ecm.handle->deviceHandle, &acmInEpInit, &acmInEpCb);
#endif /* USB_DEVICE_CONFIG_CDC_ACM */

                USB_DeviceRecvRequest(ecm.handle->deviceHandle,
                                      USB_MCTP_ENDPOINT_OUT,
                                      g_mctp.rx_buffer,
                                      USB_MCTP_OUT_BUFFER_LENGTH);
#if USB_CONFIG_COMPOSITE
                USB_DeviceRecvRequest(ecm.handle->deviceHandle,
                                      USB_HID_ENDPOINT_OUT,
                                      g_hid.rx_buffer,
                                      USB_HID_OUT_BUFFER_LENGTH);
#endif
#if USB_CONFIG_LSTP
                USB_DeviceRecvRequest(ecm.handle->deviceHandle,
                                      USB_LSTP_ENDPOINT_OUT,
                                      g_lstp.rx_buffer,
                                      USB_LSTP_OUT_BUFFER_LENGTH);
#endif /* USB_CONFIG_LSTP */
#if USB_DEVICE_CONFIG_CDC_ACM
                // Request exactly one max-sized packet (like sys usb vcom_rearm_rx) so
                // controller can respond NYET when 512 bytes received and no next buffer
                // primed.
                USB_DeviceRecvRequest(ecm.handle->deviceHandle,
                                      USB_DEVICE_CDC_ACM_DATA_BULK_OUT_EP_NUMBER,
                                      g_acm.rx_buffer,
                                      USB_DEVICE_CDC_ACM_DATA_BULK_OUT_EP_MAXPKT_SIZE_HS);
#endif
            }

            status = kStatus_USB_Success;
            break;

        case kUSB_DeviceEventGetInterface:
            if (param) {
                uint8_t interface = USB_SHORT_GET_HIGH(*reinterpret_cast<uint16_t*>(param));
                if (interface < USB_COMPOSITE_INTERFACE_COUNT) {
                    *reinterpret_cast<uint16_t*>(param) |= 0;
                    status                               = kStatus_USB_Success;
                }
            }
            break;

        case kUSB_DeviceEventSetInterface: {
            uint8_t interface  = USB_SHORT_GET_HIGH(*reinterpret_cast<uint16_t*>(param));
            uint8_t altSetting = static_cast<uint8_t>(
                USB_SHORT_GET_LOW(*reinterpret_cast<uint16_t*>(param)));

            if (interface == USB_MCTP_INTERFACE_INDEX) {
                status = kStatus_USB_Success;
            }
#if USB_CONFIG_COMPOSITE
            else if (interface == USB_HID_INTERFACE_INDEX) {
                status = kStatus_USB_Success;
            }
#endif
#if USB_CONFIG_LSTP
            else if (interface == USB_LSTP_INTERFACE_INDEX) {
                status = kStatus_USB_Success;
            }
#endif
#if USB_DEVICE_CONFIG_CDC_ECM
            else if (interface == USB_DEVICE_CDC_ECM_COMM_INTERFACE_NUMBER) {
                if (altSetting < USB_DEVICE_CDC_ECM_COMM_INTERFACE_ALTERNATE_COUNT) {
                    status = kStatus_USB_Success;
                }
            }
            else if (interface == USB_DEVICE_CDC_ECM_DATA_INTERFACE_NUMBER) {
                if (altSetting < USB_DEVICE_CDC_ECM_DATA_INTERFACE_ALTERNATE_COUNT) {
                    status = kStatus_USB_Success;

                    if (altSetting == USB_DEVICE_CDC_ECM_DATA_INTERFACE_ALTERNATE1) {
                        ecm.handle->attachStatus = 1U;

                        // Start receiving into static buffer
                        usb_status_t init_recv_status = USB_DeviceCdcEcmRecv(
                            ecm.handle->cdcEcmHandle,
                            USB_DEVICE_CDC_ECM_DATA_BULK_OUT_EP_NUMBER,
                            ecm.ecmRxBuffer,
                            nv::ecm_bm::EthFrameMaxLength);
                        g_ecm_rx_need_rearm = (init_recv_status != kStatus_USB_Success) ? 1 : 0;
                    }
                    else {
                        ecm.handle->attachStatus = 0U;
                    }
                }
            }
#endif /* USB_DEVICE_CONFIG_CDC_ECM */
#if USB_DEVICE_CONFIG_CDC_ACM
            else if (interface == USB_DEVICE_CDC_ACM_COMM_INTERFACE_NUMBER) {
                if (altSetting < USB_DEVICE_CDC_ACM_COMM_INTERFACE_ALTERNATE_COUNT) {
                    status = kStatus_USB_Success;
                }
            }
            else if (interface == USB_DEVICE_CDC_ACM_DATA_INTERFACE_NUMBER) {
                if (altSetting < USB_DEVICE_CDC_ACM_DATA_INTERFACE_ALTERNATE_COUNT) {
                    status = kStatus_USB_Success;
                }
            }
#endif /* USB_DEVICE_CONFIG_CDC_ACM */
        } break;

        case kUSB_DeviceEventGetStringDescriptor:
            if (param) {
                status = ECM_USB_DeviceGetStringDescriptor(
                    handle,
                    reinterpret_cast<usb_device_get_string_descriptor_struct_t*>(param));
            }
            break;

#if USB_CONFIG_COMPOSITE
        case kUSB_DeviceEventGetHidDescriptor:
            if (param) {
                status = ECM_USB_DeviceGetHidDescriptor(
                    handle, reinterpret_cast<usb_device_get_hid_descriptor_struct_t*>(param));
            }
            break;

        case kUSB_DeviceEventGetHidReportDescriptor:
            if (param) {
                status = ECM_USB_DeviceGetHidReportDescriptor(
                    handle,
                    reinterpret_cast<usb_device_get_hid_report_descriptor_struct_t*>(param));
            }
            break;

        case kUSB_DeviceEventGetHidPhysicalDescriptor:
            status = kStatus_USB_InvalidRequest;
            break;
#endif /* USB_CONFIG_COMPOSITE */

        default: status = kStatus_USB_InvalidRequest; break;
    }

    return status;
}

}  // extern "C"

namespace nv::ecm_bm {

#if USB_CONFIG_COMPOSITE
// HID report buffer for GET_REPORT requests (CP2112 compatible)
static uint8_t s_hid_report_buffer[64] = {0};

// HID class callback (minimal - just handles class requests for descriptors)
static usb_status_t USB_DeviceHidCallback(class_handle_t handle, uint32_t event, void* param)
{
    (void)handle;
    (void)param;

    usb_status_t status = kStatus_USB_InvalidRequest;

    switch (event) {
        case kUSB_DeviceHidEventSendResponse:
        case kUSB_DeviceHidEventRecvResponse:
            // Data transfer events - handled via raw endpoint callbacks
            status = kStatus_USB_Success;
            break;

        case kUSB_DeviceHidEventGetReport:
            // CP2112 driver sends GET_REPORT during initialization
            // Must provide valid responses for each report ID
            if (param) {
                usb_device_hid_report_struct_t*
                    report = reinterpret_cast<usb_device_hid_report_struct_t*>(param);

                switch (report->reportId) {
                    case 2:                             // Report ID 2: CP2112 GPIO Config
                        s_hid_report_buffer[0] = 0x02;  // Report ID
                        s_hid_report_buffer[1] = 0xFF;  // GPIO direction (all input)
                        s_hid_report_buffer[2] = 0x00;  // Push-pull/Open-drain
                        s_hid_report_buffer[3] = 0x00;  // Special function
                        s_hid_report_buffer[4] = 0x00;  // Clock divider
                        report->reportBuffer   = s_hid_report_buffer;
                        report->reportLength   = 5;
                        status                 = kStatus_USB_Success;
                        break;

                    case 3:                             // Report ID 3: CP2112 Get GPIO
                        s_hid_report_buffer[0] = 0x03;  // Report ID
                        s_hid_report_buffer[1] = 0x00;  // GPIO latch value
                        report->reportBuffer   = s_hid_report_buffer;
                        report->reportLength   = 2;
                        status                 = kStatus_USB_Success;
                        break;

                    case 5:  // Report ID 5: SMBus Config
                        s_hid_report_buffer[0] = 0x05;
                        s_hid_report_buffer[1] = 0x0C;
                        s_hid_report_buffer[2] = 0x03;
                        report->reportBuffer   = s_hid_report_buffer;
                        report->reportLength   = 3;
                        status                 = kStatus_USB_Success;
                        break;

                    case 6:  // Report ID 6: I2C Config
                        s_hid_report_buffer[0]  = 0x06;
                        s_hid_report_buffer[1]  = 0x00;
                        s_hid_report_buffer[2]  = 0x01;
                        s_hid_report_buffer[3]  = 0x86;
                        s_hid_report_buffer[4]  = 0xA0;
                        s_hid_report_buffer[5]  = 0x02;
                        s_hid_report_buffer[6]  = 0x00;
                        s_hid_report_buffer[7]  = 0x00;
                        s_hid_report_buffer[8]  = 0x00;
                        s_hid_report_buffer[9]  = 0x00;
                        s_hid_report_buffer[10] = 0x00;
                        s_hid_report_buffer[11] = 0x00;
                        s_hid_report_buffer[12] = 0x00;
                        s_hid_report_buffer[13] = 0x00;
                        report->reportBuffer    = s_hid_report_buffer;
                        report->reportLength    = 14;
                        status                  = kStatus_USB_Success;
                        break;

                    case 32:  // Report ID 32
                        s_hid_report_buffer[0] = 0x20;
                        s_hid_report_buffer[1] = 0xFF;
                        report->reportBuffer   = s_hid_report_buffer;
                        report->reportLength   = 2;
                        status                 = kStatus_USB_Success;
                        break;

                    default:
                        // Unknown report ID - return empty buffer
                        s_hid_report_buffer[0] = report->reportId;
                        report->reportBuffer   = s_hid_report_buffer;
                        report->reportLength   = 64;
                        status                 = kStatus_USB_Success;
                        break;
                }
            }
            break;

        case kUSB_DeviceHidEventRequestReportBuffer:
            // Provide buffer for receiving OUT report data
            if (param) {
                usb_device_hid_report_struct_t*
                    report           = reinterpret_cast<usb_device_hid_report_struct_t*>(param);
                report->reportBuffer = s_hid_report_buffer;
                report->reportLength = sizeof(s_hid_report_buffer);
                status               = kStatus_USB_Success;
            }
            break;

        case kUSB_DeviceHidEventSetReport:
            // SET_REPORT request - return success to acknowledge
            status = kStatus_USB_Success;
            break;

        case kUSB_DeviceHidEventGetIdle:
        case kUSB_DeviceHidEventSetIdle:
        case kUSB_DeviceHidEventGetProtocol:
        case kUSB_DeviceHidEventSetProtocol:
            // Optional HID class requests - return success for compatibility
            status = kStatus_USB_Success;
            break;
        default: break;
    }

    return status;
}
#endif /* USB_CONFIG_COMPOSITE */

#if USB_DEVICE_CONFIG_CDC_ACM
// CDC-ACM class callback (UART bridge)
// Handles ACM class requests (SET_LINE_CODING, GET_LINE_CODING, SET_CONTROL_LINE_STATE)
static usb_status_t USB_DeviceCdcAcmCallback(class_handle_t handle, uint32_t event, void* param)
{
    (void)handle;

    usb_status_t status = kStatus_USB_InvalidRequest;

    // CDC line coding structure (7 bytes per USB CDC PSTN spec)
    // SDK does not define this struct; define locally.
    struct LineCoding
    {
        uint32_t dwDTERate;    // baud rate
        uint8_t  bCharFormat;  // 0=1stop, 1=1.5stop, 2=2stop
        uint8_t  bParityType;  // 0=none, 1=odd, 2=even, 3=mark, 4=space
        uint8_t  bDataBits;    // 5, 6, 7, 8, 16
    } __attribute__((packed));

    // Static line coding buffer (115200, 8N1 default)
    static LineCoding s_lineCoding = {
        115200U,  // dwDTERate
        0U,       // bCharFormat: 1 stop bit
        0U,       // bParityType: No parity
        8U,       // bDataBits
    };

    usb_device_cdc_acm_request_param_struct_t*
        acmReqParam = reinterpret_cast<usb_device_cdc_acm_request_param_struct_t*>(param);

    switch (event) {
        case kUSB_DeviceCdcEventSendResponse:
            // TX data sent
            status = kStatus_USB_Success;
            break;

        case kUSB_DeviceCdcEventRecvResponse:
            // RX data received - handled by raw endpoint callback
            status = kStatus_USB_Success;
            break;

        case kUSB_DeviceCdcEventSerialStateNotif:
            // Notification sent
            status = kStatus_USB_Success;
            break;

        case kUSB_DeviceCdcEventSetLineCoding:
            // Host sets line coding (baud rate, data bits, parity, stop bits)
            // Note: acmReqParam->buffer is uint8_t**, acmReqParam->length is uint32_t*
            if (acmReqParam && acmReqParam->buffer && *acmReqParam->length >= 7U) {
                memcpy(&s_lineCoding, *acmReqParam->buffer, sizeof(s_lineCoding));
            }
            status = kStatus_USB_Success;
            break;

        case kUSB_DeviceCdcEventGetLineCoding:
            // Host gets line coding
            if (acmReqParam) {
                *acmReqParam->buffer = reinterpret_cast<uint8_t*>(&s_lineCoding);
                *acmReqParam->length = sizeof(s_lineCoding);
            }
            status = kStatus_USB_Success;
            break;

        case kUSB_DeviceCdcEventSetControlLineState:
            // DTR/RTS state change - just acknowledge
            status = kStatus_USB_Success;
            break;

        default: break;
    }

    return status;
}
#endif /* USB_DEVICE_CONFIG_CDC_ACM */

// USB class configuration: HID + CDC-ECM + CDC-ACM (configurable)
// MCTP and LSTP use raw endpoints (no class driver)
#if USB_DEVICE_CONFIG_CDC_ECM
static usb_device_class_config_struct_t s_ecm_config_with_ecm[] = {
#if USB_CONFIG_COMPOSITE
    {
                            .classCallback   = USB_DeviceHidCallback,
                            .classHandle     = nullptr,
                            .classInfomation = &g_ecm_hid_class,
                            },
#endif
    {
                            .classCallback   = USB_DeviceCdcEcmCallback,
                            .classHandle     = nullptr,
                            .classInfomation = &g_ecm_cdc_class,
                            },
#if USB_DEVICE_CONFIG_CDC_ACM
    {
                            .classCallback   = USB_DeviceCdcAcmCallback,
                            .classHandle     = nullptr,
                            .classInfomation = &g_acm_cdc_class,
                            },
#endif
};
#endif

static usb_device_class_config_struct_t s_ecm_config_without_ecm[] = {
#if USB_CONFIG_COMPOSITE
    {
                            .classCallback   = USB_DeviceHidCallback,
                            .classHandle     = nullptr,
                            .classInfomation = &g_ecm_hid_class,
                            },
#endif
#if USB_DEVICE_CONFIG_CDC_ACM
    {
                            .classCallback   = USB_DeviceCdcAcmCallback,
                            .classHandle     = nullptr,
                            .classInfomation = &g_acm_cdc_class,
                            },
#endif
};

#define ECM_CONFIG_COUNT (USB_CONFIG_COMPOSITE + USB_DEVICE_CONFIG_CDC_ACM)

static usb_device_class_config_list_struct_t s_ecm_config_list = {
    .config         = s_ecm_config_without_ecm,
    .deviceCallback = USB_DeviceCallback,
    .count          = ECM_CONFIG_COUNT,
};

// Initialize USB composite device with enabled interfaces. CDC-ECM is only enabled when MAC is
// valid. MCTP and LSTP use raw endpoints.
void ecm_usb_init(EcmNicHandle* handle, volatile uint32_t* app_event)
{
    auto& ecm    = ctx();
    ecm.handle   = handle;
    ecm.appEvent = app_event;

#if USB_DEVICE_CONFIG_CDC_ECM
    // Hide ECM interface when core1_cfg_data has no valid NCSI MAC
    const bool ecm_visible = nv::ipc_bm::Driver::hasValidNcsiMac();
    ECM_USB_SetEcmInterfaceVisible(ecm_visible);
    if (!ecm_visible) {
        s_ecm_config_list.config = s_ecm_config_without_ecm;
        s_ecm_config_list.count  = ECM_CONFIG_COUNT;
    }
    else {
        s_ecm_config_list.config = s_ecm_config_with_ecm;
        s_ecm_config_list.count  = ECM_CONFIG_COUNT + 1U;
    }

    // Initialize ethernet adapter (continue even if it fails for USB testing)
    (void)ETH_ADAPTER_Init();
#else
    ECM_USB_SetEcmInterfaceVisible(false);
#endif

    // Initialize USB device class (HID, CDC-ECM, CDC-ACM -- each when enabled)
    usb_status_t ret = USB_DeviceClassInit(
        CONTROLLER_ID, &s_ecm_config_list, &handle->deviceHandle);
    if (ret != kStatus_USB_Success) {
        return;
    }

#if USB_DEVICE_CONFIG_CDC_ECM
    // ECM class handle is at index USB_CONFIG_COMPOSITE (HID=0 when present, ECM next)
    handle->cdcEcmHandle = ecm_visible
                             ? s_ecm_config_list.config[USB_CONFIG_COMPOSITE].classHandle
                             : nullptr;
#endif

    // Export device handle for IRQ handler
    g_ecm_device_handle = handle->deviceHandle;

    // Fill string descriptors
    ECM_USB_FillStringDescriptorBuffer();
}

// Start USB device (separate from init for proper timing)
void ecm_usb_run(EcmNicHandle* handle)
{
    if (handle && handle->deviceHandle) {
        USB_DeviceRun(handle->deviceHandle);
    }
}

// Maximum retry count for USB notification send
constexpr uint32_t kUsbNotificationMaxRetries = 10000U;

#if USB_DEVICE_CONFIG_CDC_ECM
// Send notification to USB host about network connection
void ecm_send_network_notification(EcmNicHandle* handle, bool connected)
{
    if (!handle || !handle->cdcEcmHandle) {
        return;
    }

    auto&               ecm = ctx();
    usb_setup_struct_t* req = reinterpret_cast<usb_setup_struct_t*>(
        ecm.notifyNetworkConnectionReq);
    req->bmRequestType = USB_REQUEST_TYPE_DIR_IN | USB_REQUEST_TYPE_TYPE_CLASS
                       | USB_REQUEST_TYPE_RECIPIENT_INTERFACE;
    req->bRequest = USB_DEVICE_CDC_NETWORK_CONNECTION;
    req->wValue   = connected ? 1 : 0;
    req->wIndex   = USB_DEVICE_CDC_ECM_COMM_INTERFACE_NUMBER;
    req->wLength  = 0;

    for (uint32_t retry = kUsbNotificationMaxRetries; retry > 0U; --retry) {
        if (USB_DeviceCdcEcmSend(handle->cdcEcmHandle,
                                 USB_DEVICE_CDC_ECM_COMM_INTERRUPT_IN_EP_NUMBER,
                                 ecm.notifyNetworkConnectionReq,
                                 8)
            == kStatus_USB_Success) {
            break;
        }
    }
}

// Send connection speed change notification
void ecm_send_speed_notification(EcmNicHandle* handle, uint32_t speed)
{
    if (!handle || !handle->cdcEcmHandle) {
        return;
    }

    auto&               ecm = ctx();
    usb_setup_struct_t* req = reinterpret_cast<usb_setup_struct_t*>(
        ecm.notifyConnectionSpeedChangeReq);
    req->bmRequestType = USB_REQUEST_TYPE_DIR_IN | USB_REQUEST_TYPE_TYPE_CLASS
                       | USB_REQUEST_TYPE_RECIPIENT_INTERFACE;
    req->bRequest = USB_DEVICE_CDC_CONNECTION_SPEED_CHANGE;
    req->wValue   = 0;
    req->wIndex   = USB_DEVICE_CDC_ECM_COMM_INTERFACE_NUMBER;
    req->wLength  = 8;

    uint32_t* speed_data = reinterpret_cast<uint32_t*>(ecm.notifyConnectionSpeedChangeReq
                                                       + sizeof(usb_setup_struct_t));
    speed_data[0]        = speed;  // Downstream bit rate
    speed_data[1]        = speed;  // Upstream bit rate

    for (uint32_t retry = kUsbNotificationMaxRetries; retry > 0U; --retry) {
        if (USB_DeviceCdcEcmSend(handle->cdcEcmHandle,
                                 USB_DEVICE_CDC_ECM_COMM_INTERRUPT_IN_EP_NUMBER,
                                 ecm.notifyConnectionSpeedChangeReq,
                                 16)
            == kStatus_USB_Success) {
            break;
        }
    }
}

// Like SDK's APP_TransferEthernet2USB_USBSend - try to send from queue
void ecm_transfer_eth_to_usb_send(EcmNicHandle* handle)
{
    if (!handle || !handle->cdcEcmHandle || !handle->attachStatus) {
        return;
    }

    eth_adapter_frame_buf_t* data = nullptr;
    if (ETH_ADAPTER_FrameQueueGet(&g_eth_adapter_handle.rxFrameQueue, &data)
        == ETH_ADAPTER_OK) {
        if (!data || data->len == 0) {
            // Use Pop (FIFO) to remove the front frame, not Drop (LIFO)
            ETH_ADAPTER_FrameQueuePop(&g_eth_adapter_handle.rxFrameQueue, nullptr);
        }
        else {
            if (USB_DeviceCdcEcmSend(handle->cdcEcmHandle,
                                     USB_DEVICE_CDC_ECM_DATA_BULK_IN_EP_NUMBER,
                                     data->payload,
                                     data->len)
                == kStatus_USB_Success) {
                ETH_ADAPTER_FrameQueuePop(&g_eth_adapter_handle.rxFrameQueue, nullptr);
            }
        }
    }
}

// Like SDK's APP_TransferUSB2Ethernet_USBRecv - try to start/restart USB receive
void ecm_transfer_usb_recv(EcmNicHandle* handle)
{
    if (!handle || !handle->cdcEcmHandle) {
        return;
    }

    auto& ecm = ctx();
    if (USB_DeviceCdcEcmRecv(handle->cdcEcmHandle,
                             USB_DEVICE_CDC_ECM_DATA_BULK_OUT_EP_NUMBER,
                             ecm.ecmRxBuffer,
                             nv::ecm_bm::EthFrameMaxLength)
        != kStatus_USB_Success) {
        if (!handle->attachStatus) {
            USB_DeviceCancel(handle->deviceHandle,
                             USB_DEVICE_CDC_ECM_DATA_BULK_OUT_EP_NUMBER | 0x00);
        }
    }
}
#endif /* USB_DEVICE_CONFIG_CDC_ECM */

}  // namespace nv::ecm_bm
