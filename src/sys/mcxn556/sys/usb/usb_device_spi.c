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
#if defined(__cplusplus)
extern "C" {
#endif

#include "usb_device_spi.h"

#include "usb_device.h"
#include "usb_device_class.h"
#include "usb_device_config.h"
#include "usb_device_descriptor.h"
#include "nv/common/preproc.h"

static usb_status_t USB_DeviceSpiAllocateHandle(usb_device_spi_struct_t** handle);
static usb_status_t USB_DeviceSpiFreeHandle(usb_device_spi_struct_t* handle);
static usb_status_t
USB_DeviceSpiEndpointIn(usb_device_handle                              handle,
                        usb_device_endpoint_callback_message_struct_t* message,
                        void*                                          callbackParam);
static usb_status_t
                    USB_DeviceSpiEndpointOut(usb_device_handle                              handle,
                                             usb_device_endpoint_callback_message_struct_t* message,
                                             void*                                          callbackParam);
static usb_status_t USB_DeviceSpiEndpointsInit(usb_device_spi_struct_t* spiHandle);
static usb_status_t USB_DeviceSpiEndpointsDeinit(usb_device_spi_struct_t* spiHandle);

NV_SHARED_BSS USB_RAM_ADDRESS_ALIGNMENT(USB_DATA_ALIGN_SIZE) static usb_device_spi_struct_t
    s_UsbDeviceSpiHandle[1];

static usb_status_t USB_DeviceSpiAllocateHandle(usb_device_spi_struct_t** handle)
{
    uint32_t count;
    for (count = 0U; count < 1; count++) {
        if (NULL == s_UsbDeviceSpiHandle[count].handle) {
            *handle = &s_UsbDeviceSpiHandle[count];
            return kStatus_USB_Success;
        }
    }

    return kStatus_USB_Busy;
}

static usb_status_t USB_DeviceSpiFreeHandle(usb_device_spi_struct_t* handle)
{
    handle->handle        = NULL;
    handle->configStruct  = (usb_device_class_config_struct_t*)NULL;
    handle->configuration = 0U;
    handle->alternate     = 0U;
    return kStatus_USB_Success;
}

static usb_status_t
USB_DeviceSpiEndpointIn(usb_device_handle                              handle,
                        usb_device_endpoint_callback_message_struct_t* message,
                        void*                                          callbackParam)
{
    usb_device_spi_struct_t* spiHandle;
    usb_status_t             status = kStatus_USB_Error;
    /* Get the spi class handle */
    spiHandle = (usb_device_spi_struct_t*)callbackParam;

    if (NULL == spiHandle) {
        return kStatus_USB_InvalidHandle;
    }
    spiHandle->bulkInPipeBusy = 0U;
    if ((NULL != spiHandle->configStruct) && (NULL != spiHandle->configStruct->classCallback)) {
        /* Notify the application data sent by calling the spi class callback.
           classCallback is initialized in classInit of s_UsbDeviceClassInterfaceMap,it is
           from the second parameter of classInit */
        status = spiHandle->configStruct->classCallback(
            (class_handle_t)spiHandle, kUSB_DeviceSpiEventSendResponse, message);
    }

    return status;
}

static usb_status_t
USB_DeviceSpiEndpointOut(usb_device_handle                              handle,
                         usb_device_endpoint_callback_message_struct_t* message,
                         void*                                          callbackParam)
{
    usb_device_spi_struct_t* spiHandle;
    usb_status_t             status = kStatus_USB_Error;

    /* Get the spi class handle */
    spiHandle = (usb_device_spi_struct_t*)callbackParam;

    if (NULL == spiHandle) {
        return kStatus_USB_InvalidHandle;
    }
    spiHandle->bulkOutPipeBusy = 0U;
    if ((NULL != spiHandle->configStruct) && (NULL != spiHandle->configStruct->classCallback)) {
        /* Notify the application data sent by calling the spi class callback.
           classCallback is initialized in classInit of s_UsbDeviceClassInterfaceMap,it is
           from the second parameter of classInit */
        status = spiHandle->configStruct->classCallback(
            (class_handle_t)spiHandle, kUSB_DeviceSpiEventRecvResponse, message);
    }

    return status;
}

static usb_status_t USB_DeviceSpiEndpointsInit(usb_device_spi_struct_t* spiHandle)
{
    usb_device_interface_list_t*   interfaceList;
    usb_device_interface_struct_t* interface = (usb_device_interface_struct_t*)NULL;
    usb_status_t                   status    = kStatus_USB_Error;
    uint32_t                       count;
    uint32_t                       index;

    /* Check the configuration is valid or not. */
    if (0U == spiHandle->configuration) {
        return status;
    }

    if (spiHandle->configuration > spiHandle->configStruct->classInfomation->configurations) {
        return status;
    }

    /* Get the interface list of the new configuration. */
    if (NULL == spiHandle->configStruct->classInfomation->interfaceList) {
        return status;
    }
    interfaceList = &spiHandle->configStruct->classInfomation
                         ->interfaceList[spiHandle->configuration - 1U];

    /* Find interface by using the alternate setting of the interface. */
    for (count = 0U; count < interfaceList->count; count++) {
        if (USB_DEVICE_CONFIG_NV_SMA_SPI_CLASS_CODE
            == interfaceList->interfaces[count].classCode) {
            for (index = 0U; index < interfaceList->interfaces[count].count; index++) {
                if (interfaceList->interfaces[count].interface[index].alternateSetting
                    == spiHandle->alternate) {
                    interface = &interfaceList->interfaces[count].interface[index];
                    break;
                }
            }
            spiHandle->interfaceNumber = interfaceList->interfaces[count].interfaceNumber;
            break;
        }
    }
    if (NULL == interface) {
        /* Return error if the interface is not found. */
        return status;
    }

    /* Keep new interface handle. */
    spiHandle->interfaceHandle = interface;

    /* Initialize the endpoints of the new interface. */
    for (count = 0U; count < interface->endpointList.count; count++) {
        usb_device_endpoint_init_struct_t     epInitStruct;
        usb_device_endpoint_callback_struct_t epCallback;
        epInitStruct.zlt             = 0U;
        epInitStruct.interval        = interface->endpointList.endpoint[count].interval;
        epInitStruct.endpointAddress = interface->endpointList.endpoint[count].endpointAddress;
        epInitStruct.maxPacketSize   = interface->endpointList.endpoint[count].maxPacketSize;
        epInitStruct.transferType    = interface->endpointList.endpoint[count].transferType;

        if (USB_IN
            == ((epInitStruct.endpointAddress & USB_DESCRIPTOR_ENDPOINT_ADDRESS_DIRECTION_MASK)
                >> USB_DESCRIPTOR_ENDPOINT_ADDRESS_DIRECTION_SHIFT)) {
            epCallback.callbackFn           = USB_DeviceSpiEndpointIn;
            spiHandle->bulkInPipeDataBuffer = (uint8_t*)USB_INVALID_TRANSFER_BUFFER;
            spiHandle->bulkInPipeStall      = 0U;
            spiHandle->bulkInPipeDataLen    = 0U;
        }
        else {
            epCallback.callbackFn            = USB_DeviceSpiEndpointOut;
            spiHandle->bulkOutPipeDataBuffer = (uint8_t*)USB_INVALID_TRANSFER_BUFFER;
            spiHandle->bulkOutPipeStall      = 0U;
            spiHandle->bulkOutPipeDataLen    = 0U;
        }
        epCallback.callbackParam = spiHandle;

        status = USB_DeviceInitEndpoint(spiHandle->handle, &epInitStruct, &epCallback);
    }
    return status;
}

static usb_status_t USB_DeviceSpiEndpointsDeinit(usb_device_spi_struct_t* spiHandle)
{
    usb_status_t status = kStatus_USB_Error;
    uint32_t     count;

    if (NULL == spiHandle->interfaceHandle) {
        return status;
    }
    /* De-initialize all endpoints of the interface */
    for (count = 0U; count < spiHandle->interfaceHandle->endpointList.count; count++) {
        status = USB_DeviceDeinitEndpoint(
            spiHandle->handle,
            spiHandle->interfaceHandle->endpointList.endpoint[count].endpointAddress);
    }
    spiHandle->interfaceHandle = NULL;
    return status;
}

usb_status_t USB_DeviceSpiEvent(void* handle, uint32_t event, void* param)
{
    usb_device_spi_struct_t* spiHandle;
    usb_status_t             error = kStatus_USB_Error;
    uint16_t                 interfaceAlternate;
    uint32_t                 count;
    uint8_t*                 temp8;
    uint8_t                  alternate;
    usb_device_class_event_t eventCode = (usb_device_class_event_t)event;

    if ((NULL == param) || (NULL == handle)) {
        return kStatus_USB_InvalidHandle;
    }

    /* Get the spi class handle. */
    spiHandle = (usb_device_spi_struct_t*)handle;

    switch (eventCode) {
        case kUSB_DeviceClassEventDeviceReset:
            /* Bus reset, clear the configuration. */
            spiHandle->configuration   = 0U;
            spiHandle->bulkInPipeBusy  = 0U;
            spiHandle->bulkOutPipeBusy = 0U;
            spiHandle->interfaceHandle = NULL;
            error                      = kStatus_USB_Success;
            break;
        case kUSB_DeviceClassEventSetConfiguration:
            /* Get the new configuration. */
            temp8 = ((uint8_t*)param);
            if (NULL == spiHandle->configStruct) {
                break;
            }
            if (*temp8 == spiHandle->configuration) {
                error = kStatus_USB_Success;
                break;
            }

            /* De-initialize the endpoints when current configuration is none zero. */
            if (0U != spiHandle->configuration) {
                error = USB_DeviceSpiEndpointsDeinit(spiHandle);
            }
            /* Save new configuration. */
            spiHandle->configuration = *temp8;
            /* Clear the alternate setting value. */
            spiHandle->alternate = 0U;

            /* Initialize the endpoints of the new current configuration by using the
             * alternate setting 0. */
            error = USB_DeviceSpiEndpointsInit(spiHandle);
            break;
        case kUSB_DeviceClassEventSetInterface:
            if (NULL == spiHandle->configStruct) {
                break;
            }
            /* Get the new alternate setting of the interface */
            interfaceAlternate = *((uint16_t*)param);
            /* Get the alternate setting value */
            alternate = (uint8_t)(interfaceAlternate & 0xFFU);

            /* Whether the interface belongs to the class. */
            if (spiHandle->interfaceNumber != ((uint8_t)(interfaceAlternate >> 8U))) {
                break;
            }
            /* Only handle new alternate setting. */
            if (alternate == spiHandle->alternate) {
                error = kStatus_USB_Success;
                break;
            }
            /* De-initialize old endpoints */
            error                = USB_DeviceSpiEndpointsDeinit(spiHandle);
            spiHandle->alternate = alternate;
            /* Initialize new endpoints */
            error = USB_DeviceSpiEndpointsInit(spiHandle);
            break;
        case kUSB_DeviceClassEventSetEndpointHalt:
            if ((NULL == spiHandle->configStruct) || (NULL == spiHandle->interfaceHandle)) {
                break;
            }
            /* Get the endpoint address */
            temp8 = ((uint8_t*)param);
            for (count = 0U; count < spiHandle->interfaceHandle->endpointList.count; count++) {
                if (*temp8
                    == spiHandle->interfaceHandle->endpointList.endpoint[count]
                           .endpointAddress) {
                    /* Only stall the endpoint belongs to the class */
                    if (USB_IN
                        == ((spiHandle->interfaceHandle->endpointList.endpoint[count]
                                 .endpointAddress
                             & USB_DESCRIPTOR_ENDPOINT_ADDRESS_DIRECTION_MASK)
                            >> USB_DESCRIPTOR_ENDPOINT_ADDRESS_DIRECTION_SHIFT)) {
                        spiHandle->bulkInPipeStall = 1U;
                    }
                    else {
                        spiHandle->bulkOutPipeStall = 1U;
                    }
                    error = USB_DeviceStallEndpoint(spiHandle->handle, *temp8);
                }
            }
            break;
        case kUSB_DeviceClassEventClearEndpointHalt:
            if ((NULL == spiHandle->configStruct) || (NULL == spiHandle->interfaceHandle)) {
                break;
            }
            /* Get the endpoint address */
            temp8 = ((uint8_t*)param);
            for (count = 0U; count < spiHandle->interfaceHandle->endpointList.count; count++) {
                if (*temp8
                    == spiHandle->interfaceHandle->endpointList.endpoint[count]
                           .endpointAddress) {
                    /* Only un-stall the endpoint belongs to the class */
                    error = USB_DeviceUnstallEndpoint(spiHandle->handle, *temp8);
                    if (USB_IN
                        == (((*temp8) & USB_DESCRIPTOR_ENDPOINT_ADDRESS_DIRECTION_MASK)
                            >> USB_DESCRIPTOR_ENDPOINT_ADDRESS_DIRECTION_SHIFT)) {
                        if (0U != spiHandle->bulkInPipeStall) {
                            spiHandle->bulkInPipeStall = 0U;
                            if ((uint8_t*)USB_INVALID_TRANSFER_BUFFER
                                != spiHandle->bulkInPipeDataBuffer) {
                                error = USB_DeviceSendRequest(
                                    spiHandle->handle,
                                    (spiHandle->interfaceHandle->endpointList.endpoint[count]
                                         .endpointAddress
                                     & USB_DESCRIPTOR_ENDPOINT_ADDRESS_NUMBER_MASK),
                                    spiHandle->bulkInPipeDataBuffer,
                                    spiHandle->bulkInPipeDataLen);
                                if (kStatus_USB_Success != error) {
                                    usb_device_endpoint_callback_message_struct_t
                                        endpointCallbackMessage;
                                    endpointCallbackMessage.buffer = spiHandle
                                                                         ->bulkInPipeDataBuffer;
                                    endpointCallbackMessage.length = spiHandle
                                                                         ->bulkInPipeDataLen;
                                    endpointCallbackMessage.isSetup = 0U;
#if (defined(USB_DEVICE_CONFIG_RETURN_VALUE_CHECK)                                             \
     && (USB_DEVICE_CONFIG_RETURN_VALUE_CHECK > 0U))
                                    if (kStatus_USB_Success
                                        != USB_DeviceSpiEndpointIn(
                                            spiHandle->handle,
                                            (void*)&endpointCallbackMessage,
                                            handle)) {
                                        return kStatus_USB_Error;
                                    }
#else
                                    (void)USB_DeviceSpiEndpointIn(
                                        spiHandle->handle,
                                        (void*)&endpointCallbackMessage,
                                        handle);
#endif
                                }
                                spiHandle->bulkInPipeDataBuffer = (uint8_t*)
                                    USB_INVALID_TRANSFER_BUFFER;
                                spiHandle->bulkInPipeDataLen = 0U;
                            }
                        }
                    }
                    else {
                        if (0U != spiHandle->bulkOutPipeStall) {
                            spiHandle->bulkOutPipeStall = 0U;
                            if ((uint8_t*)USB_INVALID_TRANSFER_BUFFER
                                != spiHandle->bulkOutPipeDataBuffer) {
                                error = USB_DeviceRecvRequest(
                                    spiHandle->handle,
                                    (spiHandle->interfaceHandle->endpointList.endpoint[count]
                                         .endpointAddress
                                     & USB_DESCRIPTOR_ENDPOINT_ADDRESS_NUMBER_MASK),
                                    spiHandle->bulkOutPipeDataBuffer,
                                    spiHandle->bulkOutPipeDataLen);
                                if (kStatus_USB_Success != error) {
                                    usb_device_endpoint_callback_message_struct_t
                                        endpointCallbackMessage;
                                    endpointCallbackMessage
                                        .buffer = spiHandle->bulkOutPipeDataBuffer;
                                    endpointCallbackMessage.length = spiHandle
                                                                         ->bulkOutPipeDataLen;
                                    endpointCallbackMessage.isSetup = 0U;
#if (defined(USB_DEVICE_CONFIG_RETURN_VALUE_CHECK)                                             \
     && (USB_DEVICE_CONFIG_RETURN_VALUE_CHECK > 0U))
                                    if (kStatus_USB_Success
                                        != USB_DeviceSpiEndpointOut(
                                            spiHandle->handle,
                                            (void*)&endpointCallbackMessage,
                                            handle)) {
                                        return kStatus_USB_Error;
                                    }
#else
                                    (void)USB_DeviceSpiEndpointOut(
                                        spiHandle->handle,
                                        (void*)&endpointCallbackMessage,
                                        handle);
#endif
                                }
                                spiHandle->bulkOutPipeDataBuffer = (uint8_t*)
                                    USB_INVALID_TRANSFER_BUFFER;
                                spiHandle->bulkOutPipeDataLen = 0U;
                            }
                        }
                    }
                }
            }
            break;
        case kUSB_DeviceClassEventClassRequest: {
            /* Handle the Spi class specific request. */
            usb_device_control_request_struct_t*
                controlRequest = (usb_device_control_request_struct_t*)param;

            if ((controlRequest->setup->bmRequestType & USB_REQUEST_TYPE_RECIPIENT_MASK)
                != USB_REQUEST_TYPE_RECIPIENT_INTERFACE) {
                break;
            }

            if ((controlRequest->setup->wIndex & 0xFFU) != spiHandle->interfaceNumber) {
                break;
            }

            error = kStatus_USB_InvalidRequest;
            switch (controlRequest->setup->bRequest) {
                default:
                    /* no action, return kStatus_USB_InvalidRequest */
                    break;
            }
        } break;
        default:
            /*no action*/
            break;
    }
    return error;
}

usb_status_t USB_DeviceSpiInit(uint8_t                           controllerId,
                               usb_device_class_config_struct_t* config,
                               class_handle_t*                   handle)
{
    usb_device_spi_struct_t* spiHandle;
    usb_status_t             error;

    error = USB_DeviceSpiAllocateHandle(&spiHandle);
    if (kStatus_USB_Success != error) {
        return error;
    }

    error = USB_DeviceClassGetDeviceHandle(controllerId, &spiHandle->handle);
    if (kStatus_USB_Success != error) {
        (void)USB_DeviceSpiFreeHandle(spiHandle);
        return error;
    }

    if (NULL == spiHandle->handle) {
        return kStatus_USB_InvalidHandle;
    }

    spiHandle->configStruct  = config;
    spiHandle->configuration = 0U;
    spiHandle->alternate     = 0xffU;

    *handle = (class_handle_t)spiHandle;
    return error;
}

usb_status_t USB_DeviceSpiDeinit(class_handle_t handle)
{
    usb_device_spi_struct_t* spiHandle;
    usb_status_t             error;

    spiHandle = (usb_device_spi_struct_t*)handle;

    if (NULL == spiHandle) {
        return kStatus_USB_InvalidHandle;
    }

    error = USB_DeviceSpiEndpointsDeinit(spiHandle);

#if (defined(USB_DEVICE_CONFIG_RETURN_VALUE_CHECK)                                             \
     && (USB_DEVICE_CONFIG_RETURN_VALUE_CHECK > 0U))
    if (kStatus_USB_Success != USB_DeviceSpiFreeHandle(spiHandle)) {
        return kStatus_USB_Error;
    }
#else
    (void)USB_DeviceSpiFreeHandle(spiHandle);
#endif
    return error;
}

usb_status_t
USB_DeviceSpiSend(class_handle_t handle, uint8_t ep, uint8_t* buffer, uint32_t length)
{
    usb_device_spi_struct_t* spiHandle;
    usb_status_t             error = kStatus_USB_Error;

    if (NULL == handle) {
        return kStatus_USB_InvalidHandle;
    }
    spiHandle = (usb_device_spi_struct_t*)handle;

    if (0U != spiHandle->bulkInPipeBusy) {
        return kStatus_USB_Busy;
    }
    spiHandle->bulkInPipeBusy = 1U;

    if (0U != spiHandle->bulkInPipeStall) {
        spiHandle->bulkInPipeDataBuffer = buffer;
        spiHandle->bulkInPipeDataLen    = length;
        return kStatus_USB_Success;
    }
    error = USB_DeviceSendRequest(spiHandle->handle, ep, buffer, length);
    if (kStatus_USB_Success != error) {
        spiHandle->bulkInPipeBusy = 0U;
    }
    return error;
}

usb_status_t
USB_DeviceSpiRecv(class_handle_t handle, uint8_t ep, uint8_t* buffer, uint32_t length)
{
    usb_device_spi_struct_t* spiHandle;
    usb_status_t             error;

    if (NULL == handle) {
        return kStatus_USB_InvalidHandle;
    }
    spiHandle = (usb_device_spi_struct_t*)handle;

    if (0U != spiHandle->bulkOutPipeBusy) {
        return kStatus_USB_Busy;
    }
    spiHandle->bulkOutPipeBusy = 1U;

    if (0U != spiHandle->bulkOutPipeStall) {
        spiHandle->bulkOutPipeDataBuffer = buffer;
        spiHandle->bulkOutPipeDataLen    = length;
        return kStatus_USB_Success;
    }
    error = USB_DeviceRecvRequest(spiHandle->handle, ep, buffer, length);
    if (kStatus_USB_Success != error) {
        spiHandle->bulkOutPipeBusy = 0U;
    }
    return error;
}

#if defined(__cplusplus)
}
#endif /* __cplusplus*/