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
#include "nv/usb/task.h"
#include "sys/usb/usb_device_config.h"
#include "sys/usb/usb_device_descriptor.h"
#include "sys/usb/usb_device_mctp.h"
#include "mpu_syscall_numbers.h"

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
#endif

#if defined(__cplusplus)
extern "C" {
#endif

#if defined(__cplusplus)
}
#endif

namespace sys::usb {

NV_SHARED_BSS usb_composite_struct_t g_UsbDevice;

#if defined(USB_CONFIG_COMPOSITE)
usb_status_t Driver::usb_devicehidcallback(class_handle_t handle, uint32_t event, void* param)
{
    usb_status_t error = kStatus_USB_InvalidRequest;
    switch (event) {
        case kUSB_DeviceHidEventSendResponse: {
            if (param && ((usb_device_hid_report_struct_t*)param)->reportBuffer) {
                error = USB_DeviceSendRequest(
                    g_UsbDevice.hid_handle,
                    USB_HID_GENERIC_ENDPOINT_IN,
                    ((usb_device_hid_report_struct_t*)param)->reportBuffer,
                    ((usb_device_hid_report_struct_t*)param)->reportLength);
            }
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
                    case 5:  // Report ID 5
                        g_UsbDevice.hid_buffer.at(g_UsbDevice.buffer_index)[0] = 0x05;
                        g_UsbDevice.hid_buffer.at(g_UsbDevice.buffer_index)[1] = 0x0C;
                        g_UsbDevice.hid_buffer.at(g_UsbDevice.buffer_index)[2] = 0x03;

                        hidReportParam->reportBuffer = g_UsbDevice.hid_buffer.at(
                            g_UsbDevice.buffer_index);
                        hidReportParam->reportLength = 3;
                        error                        = kStatus_USB_Success;
                        break;

                    case 6:  // Report ID 6
                        g_UsbDevice.hid_buffer.at(g_UsbDevice.buffer_index)[0]  = 0x06;
                        g_UsbDevice.hid_buffer.at(g_UsbDevice.buffer_index)[1]  = 0x00;
                        g_UsbDevice.hid_buffer.at(g_UsbDevice.buffer_index)[2]  = 0x01;
                        g_UsbDevice.hid_buffer.at(g_UsbDevice.buffer_index)[3]  = 0x86;
                        g_UsbDevice.hid_buffer.at(g_UsbDevice.buffer_index)[4]  = 0xA0;
                        g_UsbDevice.hid_buffer.at(g_UsbDevice.buffer_index)[5]  = 0x02;
                        g_UsbDevice.hid_buffer.at(g_UsbDevice.buffer_index)[6]  = 0x00;
                        g_UsbDevice.hid_buffer.at(g_UsbDevice.buffer_index)[7]  = 0x00;
                        g_UsbDevice.hid_buffer.at(g_UsbDevice.buffer_index)[8]  = 0x00;
                        g_UsbDevice.hid_buffer.at(g_UsbDevice.buffer_index)[9]  = 0x00;
                        g_UsbDevice.hid_buffer.at(g_UsbDevice.buffer_index)[10] = 0x00;
                        g_UsbDevice.hid_buffer.at(g_UsbDevice.buffer_index)[11] = 0x00;
                        g_UsbDevice.hid_buffer.at(g_UsbDevice.buffer_index)[12] = 0x00;
                        g_UsbDevice.hid_buffer.at(g_UsbDevice.buffer_index)[13] = 0x00;
                        hidReportParam->reportBuffer = g_UsbDevice.hid_buffer.at(
                            g_UsbDevice.buffer_index);
                        hidReportParam->reportLength = 14;
                        error                        = kStatus_USB_Success;
                        break;

                    case 32:  // Report ID 32
                        g_UsbDevice.hid_buffer.at(g_UsbDevice.buffer_index)[0] = 0x20;
                        g_UsbDevice.hid_buffer.at(g_UsbDevice.buffer_index)[1] = 0xFF;

                        hidReportParam->reportBuffer = g_UsbDevice.hid_buffer.at(
                            g_UsbDevice.buffer_index);
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
                g_UsbDevice.hid_buffer.at(g_UsbDevice.buffer_index)[0] = 0x20;
                g_UsbDevice.hid_buffer.at(g_UsbDevice.buffer_index)[1] = 0xff;
            }
            error = USB_DeviceSendRequest(g_UsbDevice.hid_handle,
                                          USB_HID_GENERIC_ENDPOINT_IN,
                                          g_UsbDevice.hid_buffer.at(g_UsbDevice.buffer_index),
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
(class_handle_t) nullptr, /* MCTP class handle */
 &g_UsbDeviceMctpGenericConfig,  /* MCTP class information */
    },
#if defined(USB_CONFIG_COMPOSITE)
    {
     Driver::usb_devicehidcallback,
     (class_handle_t) nullptr,        /* HID class handle */
        &g_UsbDeviceHidGenericConfig, /* HID class information */
    }
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
                                            void* hid_buffer1)
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
    usb_device_hid_struct_t* hid_handle = (usb_device_hid_struct_t*)g_UsbDevice.hid_handle;
    taskENTER_CRITICAL();
    result = USB_DeviceSendRequest(
        hid_handle->handle, USB_HID_GENERIC_ENDPOINT_IN, data, length);
    taskEXIT_CRITICAL();
#endif
    return result;
}

bool Driver::check_vbus()
{
    volatile USBHS_Type* ehciRegisterBase;
    uint32_t             usbhsBaseAddrs[] = USBHS_BASE_ADDRS;
    ehciRegisterBase = (USBHS_Type*)usbhsBaseAddrs[CONTROLLER_ID - kUSB_ControllerEhci0];

    auto is_vbus_on = (ehciRegisterBase->OTGSC & USBHS_OTGSC_BSV_MASK) >> USBHS_OTGSC_BSV_SHIFT;

    return is_vbus_on;
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
                if (interface < 1) {
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