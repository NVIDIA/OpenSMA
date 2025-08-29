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

#include "usb_device_mctp.h"

#include "usb_device.h"
#include "usb_device_class.h"
#include "usb_device_config.h"

#include "nv/common/preproc.h"

static usb_status_t USB_DeviceMctpAllocateHandle(usb_device_mctp_struct_t** handle);
static usb_status_t USB_DeviceMctpFreeHandle(usb_device_mctp_struct_t* handle);
static usb_status_t
USB_DeviceMctpInterruptIn(usb_device_handle                              handle,
                          usb_device_endpoint_callback_message_struct_t* message,
                          void*                                          callbackParam);
static usb_status_t
                    USB_DeviceMctpInterruptOut(usb_device_handle                              handle,
                                               usb_device_endpoint_callback_message_struct_t* message,
                                               void*                                          callbackParam);
static usb_status_t USB_DeviceMctpEndpointsInit(usb_device_mctp_struct_t* mctpHandle);
static usb_status_t USB_DeviceMctpEndpointsDeinit(usb_device_mctp_struct_t* mctpHandle);

NV_SHARED_BSS USB_RAM_ADDRESS_ALIGNMENT(USB_DATA_ALIGN_SIZE) static usb_device_mctp_struct_t
    s_UsbDeviceMctpHandle[USB_DEVICE_CONFIG_MCTP];

static usb_status_t USB_DeviceMctpAllocateHandle(usb_device_mctp_struct_t** handle)
{
    uint32_t count;
    for (count = 0U; count < USB_DEVICE_CONFIG_MCTP; count++) {
        if (NULL == s_UsbDeviceMctpHandle[count].handle) {
            *handle = &s_UsbDeviceMctpHandle[count];
            return kStatus_USB_Success;
        }
    }

    return kStatus_USB_Busy;
}

static usb_status_t USB_DeviceMctpFreeHandle(usb_device_mctp_struct_t* handle)
{
    handle->handle        = NULL;
    handle->configStruct  = (usb_device_class_config_struct_t*)NULL;
    handle->configuration = 0U;
    handle->alternate     = 0U;
    return kStatus_USB_Success;
}

static usb_status_t
USB_DeviceMctpInterruptIn(usb_device_handle                              handle,
                          usb_device_endpoint_callback_message_struct_t* message,
                          void*                                          callbackParam)
{
    usb_device_mctp_struct_t* mctpHandle;
    usb_status_t              status = kStatus_USB_Error;

    /* Get the mctp class handle */
    mctpHandle = (usb_device_mctp_struct_t*)callbackParam;

    if (NULL == mctpHandle) {
        return kStatus_USB_InvalidHandle;
    }
    mctpHandle->interruptInPipeBusy = 0U;
    if ((NULL != mctpHandle->configStruct)
        && (NULL != mctpHandle->configStruct->classCallback)) {
        /* Notify the application data sent by calling the mctp class callback.
           classCallback is initialized in classInit of s_UsbDeviceClassInterfaceMap,it is
           from the second parameter of classInit */
        status = mctpHandle->configStruct->classCallback(
            (class_handle_t)mctpHandle, kUSB_DeviceMctpEventSendResponse, message);
    }

    return status;
}

static usb_status_t
USB_DeviceMctpInterruptOut(usb_device_handle                              handle,
                           usb_device_endpoint_callback_message_struct_t* message,
                           void*                                          callbackParam)
{
    usb_device_mctp_struct_t* mctpHandle;
    usb_status_t              status = kStatus_USB_Error;

    /* Get the mctp class handle */
    mctpHandle = (usb_device_mctp_struct_t*)callbackParam;

    if (NULL == mctpHandle) {
        return kStatus_USB_InvalidHandle;
    }
    mctpHandle->interruptOutPipeBusy = 0U;
    if ((NULL != mctpHandle->configStruct)
        && (NULL != mctpHandle->configStruct->classCallback)) {
        /* Notify the application data sent by calling the mctp class callback.
           classCallback is initialized in classInit of s_UsbDeviceClassInterfaceMap,it is
           from the second parameter of classInit */
        status = mctpHandle->configStruct->classCallback(
            (class_handle_t)mctpHandle, kUSB_DeviceMctpEventRecvResponse, message);
    }

    return status;
}

static usb_status_t USB_DeviceMctpEndpointsInit(usb_device_mctp_struct_t* mctpHandle)
{
    usb_device_interface_list_t*   interfaceList;
    usb_device_interface_struct_t* interface = (usb_device_interface_struct_t*)NULL;
    usb_status_t                   status    = kStatus_USB_Error;
    uint32_t                       count;
    uint32_t                       index;

    /* Check the configuration is valid or not. */
    if (0U == mctpHandle->configuration) {
        return status;
    }

    if (mctpHandle->configuration > mctpHandle->configStruct->classInfomation->configurations) {
        return status;
    }

    /* Get the interface list of the new configuration. */
    if (NULL == mctpHandle->configStruct->classInfomation->interfaceList) {
        return status;
    }
    interfaceList = &mctpHandle->configStruct->classInfomation
                         ->interfaceList[mctpHandle->configuration - 1U];

    /* Find interface by using the alternate setting of the interface. */
    for (count = 0U; count < interfaceList->count; count++) {
        if (USB_DEVICE_CONFIG_MCTP_CLASS_CODE == interfaceList->interfaces[count].classCode) {
            for (index = 0U; index < interfaceList->interfaces[count].count; index++) {
                if (interfaceList->interfaces[count].interface[index].alternateSetting
                    == mctpHandle->alternate) {
                    interface = &interfaceList->interfaces[count].interface[index];
                    break;
                }
            }
            mctpHandle->interfaceNumber = interfaceList->interfaces[count].interfaceNumber;
            break;
        }
    }
    if (NULL == interface) {
        /* Return error if the interface is not found. */
        return status;
    }

    /* Keep new interface handle. */
    mctpHandle->interfaceHandle = interface;

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
            epCallback.callbackFn                 = USB_DeviceMctpInterruptIn;
            mctpHandle->interruptInPipeDataBuffer = (uint8_t*)USB_INVALID_TRANSFER_BUFFER;
            mctpHandle->interruptInPipeStall      = 0U;
            mctpHandle->interruptInPipeDataLen    = 0U;
        }
        else {
            epCallback.callbackFn                  = USB_DeviceMctpInterruptOut;
            mctpHandle->interruptOutPipeDataBuffer = (uint8_t*)USB_INVALID_TRANSFER_BUFFER;
            mctpHandle->interruptOutPipeStall      = 0U;
            mctpHandle->interruptOutPipeDataLen    = 0U;
        }
        epCallback.callbackParam = mctpHandle;

        status = USB_DeviceInitEndpoint(mctpHandle->handle, &epInitStruct, &epCallback);
    }
    return status;
}

static usb_status_t USB_DeviceMctpEndpointsDeinit(usb_device_mctp_struct_t* mctpHandle)
{
    usb_status_t status = kStatus_USB_Error;
    uint32_t     count;

    if (NULL == mctpHandle->interfaceHandle) {
        return status;
    }
    /* De-initialize all endpoints of the interface */
    for (count = 0U; count < mctpHandle->interfaceHandle->endpointList.count; count++) {
        status = USB_DeviceDeinitEndpoint(
            mctpHandle->handle,
            mctpHandle->interfaceHandle->endpointList.endpoint[count].endpointAddress);
    }
    mctpHandle->interfaceHandle = NULL;
    return status;
}

usb_status_t USB_DeviceMctpEvent(void* handle, uint32_t event, void* param)
{
    usb_device_mctp_struct_t* mctpHandle;
    usb_status_t              error = kStatus_USB_Error;
    uint16_t                  interfaceAlternate;
    uint32_t                  count;
    uint8_t*                  temp8;
    uint8_t                   alternate;
    usb_device_class_event_t  eventCode;

    if ((NULL == param) || (NULL == handle)) {
        return kStatus_USB_InvalidHandle;
    }

    /* Verify if the event value is within the valid enumeration range */
    if (event > kUSB_DeviceClassEventClearEndpointHalt) {
        return kStatus_USB_InvalidParameter;
    }

    eventCode = (usb_device_class_event_t)event;

    /* Get the mctp class handle. */
    mctpHandle = (usb_device_mctp_struct_t*)handle;

    switch (eventCode) {
        case kUSB_DeviceClassEventDeviceReset:
            /* Bus reset, clear the configuration. */
            mctpHandle->configuration        = 0U;
            mctpHandle->interruptInPipeBusy  = 0U;
            mctpHandle->interruptOutPipeBusy = 0U;
            mctpHandle->interfaceHandle      = NULL;
            error                            = kStatus_USB_Success;
            break;
        case kUSB_DeviceClassEventSetConfiguration:
            /* Get the new configuration. */
            temp8 = ((uint8_t*)param);
            if (NULL == mctpHandle->configStruct) {
                break;
            }
            if (*temp8 == mctpHandle->configuration) {
                error = kStatus_USB_Success;
                break;
            }

            /* De-initialize the endpoints when current configuration is none zero. */
            if (0U != mctpHandle->configuration) {
                error = USB_DeviceMctpEndpointsDeinit(mctpHandle);
            }
            /* Save new configuration. */
            mctpHandle->configuration = *temp8;
            /* Clear the alternate setting value. */
            mctpHandle->alternate = 0U;

            /* Initialize the endpoints of the new current configuration by using the
             * alternate setting 0. */
            error = USB_DeviceMctpEndpointsInit(mctpHandle);
            break;
        case kUSB_DeviceClassEventSetInterface:
            if (NULL == mctpHandle->configStruct) {
                break;
            }
            /* Get the new alternate setting of the interface */
            interfaceAlternate = *((uint16_t*)param);
            /* Get the alternate setting value */
            alternate = (uint8_t)(interfaceAlternate & 0xFFU);

            /* Whether the interface belongs to the class. */
            if (mctpHandle->interfaceNumber != ((uint8_t)(interfaceAlternate >> 8U))) {
                break;
            }
            /* Only handle new alternate setting. */
            if (alternate == mctpHandle->alternate) {
                error = kStatus_USB_Success;
                break;
            }
            /* De-initialize old endpoints */
            error                 = USB_DeviceMctpEndpointsDeinit(mctpHandle);
            mctpHandle->alternate = alternate;
            /* Initialize new endpoints */
            error = USB_DeviceMctpEndpointsInit(mctpHandle);
            break;
        case kUSB_DeviceClassEventSetEndpointHalt:
            if ((NULL == mctpHandle->configStruct) || (NULL == mctpHandle->interfaceHandle)) {
                break;
            }
            /* Get the endpoint address */
            temp8 = ((uint8_t*)param);
            for (count = 0U; count < mctpHandle->interfaceHandle->endpointList.count; count++) {
                if (*temp8
                    == mctpHandle->interfaceHandle->endpointList.endpoint[count]
                           .endpointAddress) {
                    /* Only stall the endpoint belongs to the class */
                    if (USB_IN
                        == ((mctpHandle->interfaceHandle->endpointList.endpoint[count]
                                 .endpointAddress
                             & USB_DESCRIPTOR_ENDPOINT_ADDRESS_DIRECTION_MASK)
                            >> USB_DESCRIPTOR_ENDPOINT_ADDRESS_DIRECTION_SHIFT)) {
                        mctpHandle->interruptInPipeStall = 1U;
                    }
                    else {
                        mctpHandle->interruptOutPipeStall = 1U;
                    }
                    error = USB_DeviceStallEndpoint(mctpHandle->handle, *temp8);
                }
            }
            break;
        case kUSB_DeviceClassEventClearEndpointHalt:
            if ((NULL == mctpHandle->configStruct) || (NULL == mctpHandle->interfaceHandle)) {
                break;
            }
            /* Get the endpoint address */
            temp8 = ((uint8_t*)param);
            for (count = 0U; count < mctpHandle->interfaceHandle->endpointList.count; count++) {
                if (*temp8
                    == mctpHandle->interfaceHandle->endpointList.endpoint[count]
                           .endpointAddress) {
                    /* Only un-stall the endpoint belongs to the class */
                    error = USB_DeviceUnstallEndpoint(mctpHandle->handle, *temp8);
                    if (USB_IN
                        == (((*temp8) & USB_DESCRIPTOR_ENDPOINT_ADDRESS_DIRECTION_MASK)
                            >> USB_DESCRIPTOR_ENDPOINT_ADDRESS_DIRECTION_SHIFT)) {
                        if (0U != mctpHandle->interruptInPipeStall) {
                            mctpHandle->interruptInPipeStall = 0U;
                            if ((uint8_t*)USB_INVALID_TRANSFER_BUFFER
                                != mctpHandle->interruptInPipeDataBuffer) {
                                error = USB_DeviceSendRequest(
                                    mctpHandle->handle,
                                    (mctpHandle->interfaceHandle->endpointList.endpoint[count]
                                         .endpointAddress
                                     & USB_DESCRIPTOR_ENDPOINT_ADDRESS_NUMBER_MASK),
                                    mctpHandle->interruptInPipeDataBuffer,
                                    mctpHandle->interruptInPipeDataLen);
                                if (kStatus_USB_Success != error) {
                                    usb_device_endpoint_callback_message_struct_t
                                        endpointCallbackMessage;
                                    endpointCallbackMessage
                                        .buffer = mctpHandle->interruptInPipeDataBuffer;
                                    endpointCallbackMessage
                                        .length = mctpHandle->interruptInPipeDataLen;
                                    endpointCallbackMessage.isSetup = 0U;
#if (defined(USB_DEVICE_CONFIG_RETURN_VALUE_CHECK)                                             \
     && (USB_DEVICE_CONFIG_RETURN_VALUE_CHECK > 0U))
                                    if (kStatus_USB_Success
                                        != USB_DeviceMctpInterruptIn(
                                            mctpHandle->handle,
                                            (void*)&endpointCallbackMessage,
                                            handle)) {
                                        return kStatus_USB_Error;
                                    }
#else
                                    (void)USB_DeviceMctpInterruptIn(
                                        mctpHandle->handle,
                                        (void*)&endpointCallbackMessage,
                                        handle);
#endif
                                }
                                mctpHandle->interruptInPipeDataBuffer = (uint8_t*)
                                    USB_INVALID_TRANSFER_BUFFER;
                                mctpHandle->interruptInPipeDataLen = 0U;
                            }
                        }
                    }
                    else {
                        if (0U != mctpHandle->interruptOutPipeStall) {
                            mctpHandle->interruptOutPipeStall = 0U;
                            if ((uint8_t*)USB_INVALID_TRANSFER_BUFFER
                                != mctpHandle->interruptOutPipeDataBuffer) {
                                error = USB_DeviceRecvRequest(
                                    mctpHandle->handle,
                                    (mctpHandle->interfaceHandle->endpointList.endpoint[count]
                                         .endpointAddress
                                     & USB_DESCRIPTOR_ENDPOINT_ADDRESS_NUMBER_MASK),
                                    mctpHandle->interruptOutPipeDataBuffer,
                                    mctpHandle->interruptOutPipeDataLen);
                                if (kStatus_USB_Success != error) {
                                    usb_device_endpoint_callback_message_struct_t
                                        endpointCallbackMessage;
                                    endpointCallbackMessage
                                        .buffer = mctpHandle->interruptOutPipeDataBuffer;
                                    endpointCallbackMessage
                                        .length = mctpHandle->interruptOutPipeDataLen;
                                    endpointCallbackMessage.isSetup = 0U;
#if (defined(USB_DEVICE_CONFIG_RETURN_VALUE_CHECK)                                             \
     && (USB_DEVICE_CONFIG_RETURN_VALUE_CHECK > 0U))
                                    if (kStatus_USB_Success
                                        != USB_DeviceMctpInterruptOut(
                                            mctpHandle->handle,
                                            (void*)&endpointCallbackMessage,
                                            handle)) {
                                        return kStatus_USB_Error;
                                    }
#else
                                    (void)USB_DeviceMctpInterruptOut(
                                        mctpHandle->handle,
                                        (void*)&endpointCallbackMessage,
                                        handle);
#endif
                                }
                                mctpHandle->interruptOutPipeDataBuffer = (uint8_t*)
                                    USB_INVALID_TRANSFER_BUFFER;
                                mctpHandle->interruptOutPipeDataLen = 0U;
                            }
                        }
                        // Always notify application to restart receive operations
                        // when clear halt is received, regardless of previous stall state
                        if ((NULL != mctpHandle->configStruct)
                            && (NULL != mctpHandle->configStruct->classCallback)) {
                            usb_device_endpoint_callback_message_struct_t restartMessage;
                            restartMessage.buffer  = NULL;
                            restartMessage.length  = 0U;
                            restartMessage.isSetup = 0U;
                            // Send RecvResponse event with NULL buffer to signal
                            // restart needed
                            mctpHandle->configStruct->classCallback(
                                (class_handle_t)mctpHandle,
                                kUSB_DeviceMctpEventRecvResponse,
                                &restartMessage);
                        }
                    }
                }
            }
            break;
        case kUSB_DeviceClassEventClassRequest: {
            /* Handle the Mctp class specific request. */
            usb_device_control_request_struct_t*
                controlRequest = (usb_device_control_request_struct_t*)param;

            if ((controlRequest->setup->bmRequestType & USB_REQUEST_TYPE_RECIPIENT_MASK)
                != USB_REQUEST_TYPE_RECIPIENT_INTERFACE) {
                break;
            }

            if ((controlRequest->setup->wIndex & 0xFFU) != mctpHandle->interfaceNumber) {
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

usb_status_t USB_DeviceMctpInit(uint8_t                           controllerId,
                                usb_device_class_config_struct_t* config,
                                class_handle_t*                   handle)
{
    usb_device_mctp_struct_t* mctpHandle;
    usb_status_t              error;

    error = USB_DeviceMctpAllocateHandle(&mctpHandle);

    if (kStatus_USB_Success != error) {
        return error;
    }

    error = USB_DeviceClassGetDeviceHandle(controllerId, &mctpHandle->handle);

    if (kStatus_USB_Success != error) {
        return error;
    }

    if (NULL == mctpHandle->handle) {
        return kStatus_USB_InvalidHandle;
    }

    mctpHandle->configStruct  = config;
    mctpHandle->configuration = 0U;
    mctpHandle->alternate     = 0xffU;

    *handle = (class_handle_t)mctpHandle;
    return error;
}

usb_status_t USB_DeviceMctpDeinit(class_handle_t handle)
{
    usb_device_mctp_struct_t* mctpHandle;
    usb_status_t              error;

    mctpHandle = (usb_device_mctp_struct_t*)handle;

    if (NULL == mctpHandle) {
        return kStatus_USB_InvalidHandle;
    }

    error = USB_DeviceMctpEndpointsDeinit(mctpHandle);

#if (defined(USB_DEVICE_CONFIG_RETURN_VALUE_CHECK)                                             \
     && (USB_DEVICE_CONFIG_RETURN_VALUE_CHECK > 0U))
    if (kStatus_USB_Success != USB_DeviceMctpFreeHandle(mctpHandle)) {
        return kStatus_USB_Error;
    }
#else
    (void)USB_DeviceMctpFreeHandle(mctpHandle);
#endif
    return error;
}

usb_status_t
USB_DeviceMctpSend(class_handle_t handle, uint8_t ep, uint8_t* buffer, uint32_t length)
{
    usb_device_mctp_struct_t* mctpHandle;
    usb_status_t              error;

    if (NULL == handle) {
        return kStatus_USB_InvalidHandle;
    }
    mctpHandle = (usb_device_mctp_struct_t*)handle;

    if (0U != mctpHandle->interruptInPipeBusy) {
        return kStatus_USB_Busy;
    }
    mctpHandle->interruptInPipeBusy = 1U;

    if (0U != mctpHandle->interruptInPipeStall) {
        mctpHandle->interruptInPipeDataBuffer = buffer;
        mctpHandle->interruptInPipeDataLen    = length;
        return kStatus_USB_Success;
    }
    error = USB_DeviceSendRequest(mctpHandle->handle, ep, buffer, length);
    if (kStatus_USB_Success != error) {
        mctpHandle->interruptInPipeBusy = 0U;
    }
    return error;
}

usb_status_t
USB_DeviceMctpRecv(class_handle_t handle, uint8_t ep, uint8_t* buffer, uint32_t length)
{
    usb_device_mctp_struct_t* mctpHandle;
    usb_status_t              error;

    if (NULL == handle) {
        return kStatus_USB_InvalidHandle;
    }
    mctpHandle = (usb_device_mctp_struct_t*)handle;

    if (0U != mctpHandle->interruptOutPipeBusy) {
        return kStatus_USB_Busy;
    }
    mctpHandle->interruptOutPipeBusy = 1U;

    if (0U != mctpHandle->interruptOutPipeStall) {
        mctpHandle->interruptOutPipeDataBuffer = buffer;
        mctpHandle->interruptOutPipeDataLen    = length;
        return kStatus_USB_Success;
    }
    error = USB_DeviceRecvRequest(mctpHandle->handle, ep, buffer, length);
    if (kStatus_USB_Success != error) {
        mctpHandle->interruptOutPipeBusy = 0U;
    }
    return error;
}

#if defined(__cplusplus)
}
#endif /* __cplusplus*/