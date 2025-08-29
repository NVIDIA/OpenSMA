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

#ifndef __USB_DEVICE_MCTP_H__
#define __USB_DEVICE_MCTP_H__

#include "usb_device_descriptor.h"

#define USB_DEVICE_CONFIG_MCTP_CLASS_CODE (0x14U)

typedef enum _usb_device_mctp_event
{
    kUSB_DeviceMctpEventSendResponse = 0x01U, /*!< Send data completed or cancelled etc*/
    kUSB_DeviceMctpEventRecvResponse,         /*!< Data received or cancelled etc*/
} usb_device_mctp_event_t;

typedef struct _usb_device_mctp_struct
{
    usb_device_handle                 handle;       /*!< The device handle */
    usb_device_class_config_struct_t* configStruct; /*!< The configuration of the class.
                                                     */
    usb_device_interface_struct_t* interfaceHandle; /*!< Current interface handle */
    uint8_t* interruptInPipeDataBuffer;  /*!< IN pipe data buffer backup when stall */
    uint32_t interruptInPipeDataLen;     /*!< IN pipe data length backup when stall  */
    uint8_t* interruptOutPipeDataBuffer; /*!< OUT pipe data buffer backup when stall */
    uint32_t interruptOutPipeDataLen;    /*!< OUT pipe data length backup when stall  */
    uint8_t  configuration;              /*!< Current configuration */
    uint8_t  interfaceNumber;            /*!< The interface number of the class */
    uint8_t  alternate;                  /*!< Current alternate setting of the interface */
    uint8_t  protocol;                   /*!< Current protocol */
    uint8_t  interruptInPipeBusy;        /*!< Interrupt IN pipe busy flag */
    uint8_t  interruptOutPipeBusy;       /*!< Interrupt OUT pipe busy flag */
    uint8_t  interruptInPipeStall;       /*!< Interrupt IN pipe stall flag */
    uint8_t  interruptOutPipeStall;      /*!< Interrupt OUT pipe stall flag */
} usb_device_mctp_struct_t;

#if defined(__cplusplus)
extern "C" {
#endif

/*!
 * @brief Initializes the mctp class.
 *
 * This function is used to initialize the mctp class. This function only can be called by
 * #USB_DeviceClassInit.
 *
 * @param[in] controllerId   The controller ID of the USB IP. See the enumeration
 * #usb_controller_index_t.
 * @param[in] config          The class configuration information.
 * @param[out] handle          An parameter used to return pointer of the mctp class
 * handle to the caller.
 *
 * @return A USB error code or kStatus_USB_Success.
 */
extern usb_status_t USB_DeviceMctpInit(uint8_t                           controllerId,
                                       usb_device_class_config_struct_t* config,
                                       class_handle_t*                   handle);

/*!
 * @brief Deinitializes the device mctp class.
 *
 * The function deinitializes the device mctp class. This function only can be called by
 * #USB_DeviceClassDeinit.
 *
 * @param[in] handle The mctp class handle got from
 * usb_device_class_config_struct_t::classHandle.
 *
 * @return A USB error code or kStatus_USB_Success.
 */
extern usb_status_t USB_DeviceMctpDeinit(class_handle_t handle);

/*!
 * @brief Handles the event passed to the mctp class.
 *
 * This function handles the event passed to the mctp class. This function only can be
 * called by #USB_DeviceClassEvent.
 *
 * @param[in] handle          The mctp class handle received from the
 * usb_device_class_config_struct_t::classHandle.
 * @param[in] event           The event codes. See the enumeration
 * usb_device_class_event_t.
 * @param[in,out] param           The parameter type is determined by the event code.
 *
 * @return A USB error code or kStatus_USB_Success.
 * @retval kStatus_USB_Success              Free device handle successfully.
 * @retval kStatus_USB_InvalidParameter     The device handle not be found.
 * @retval kStatus_USB_InvalidRequest       The request is invalid, and the control pipe
 * is stalled by the caller.
 */
extern usb_status_t USB_DeviceMctpEvent(void* handle, uint32_t event, void* param);

/*!
 * @name USB device mctp class APIs
 * @{
 */

/*!
 * @brief Sends data through a specified endpoint.
 *
 * The function is used to send data through a specified endpoint.
 * The function calls #USB_DeviceSendRequest internally.
 *
 * @param[in] handle The mctp class handle received from
 * usb_device_class_config_struct_t::classHandle.
 * @param[in] ep     Endpoint index.
 * @param[in] buffer The memory address to hold the data need to be sent.
 * @param[in] length The data length to be sent.
 *
 * @return A USB error code or kStatus_USB_Success.
 *
 * @note The function can only be called in the same context.
 *
 * @note The return value indicates whether the sending request is successful or not. The
 * transfer done is notified by usb_device_mctp_interrupt_in. Currently, only one transfer
 * request can be supported for one specific endpoint. If there is a specific requirement
 * to support multiple transfer requests for a specific endpoint, the application should
 * implement a queue in the application level. The subsequent transfer can begin only when
 * the previous transfer is done (a notification is received through the endpoint
 * callback).
 */
extern usb_status_t
USB_DeviceMctpSend(class_handle_t handle, uint8_t ep, uint8_t* buffer, uint32_t length);

/*!
 * @brief Receives data through a specified endpoint.
 *
 * The function is used to receive data through a specified endpoint.
 * The function calls #USB_DeviceRecvRequest internally.
 *
 * @param[in] handle The mctp class handle received from the
 * usb_device_class_config_struct_t::classHandle.
 * @param[in] ep     Endpoint index.
 * @param[in] buffer The memory address to save the received data.
 * @param[in] length The data length to be received.
 *
 * @return A USB error code or kStatus_USB_Success.
 *
 * @note The function can only be called in the same context.
 *
 * @note The return value indicates whether the receiving request is successful or not.
 * The transfer done is notified by usb_device_mctp_interrupt_out. Currently, only one
 * transfer request can be supported for a specific endpoint. If there is a specific
 * requirement to support multiple transfer requests for a specific endpoint, the
 * application should implement a queue in the application level. The subsequent transfer
 * can begin only when the previous transfer is done (a notification is received through
 * the endpoint callback).
 */
extern usb_status_t
USB_DeviceMctpRecv(class_handle_t handle, uint8_t ep, uint8_t* buffer, uint32_t length);

/*! @}*/

#if defined(__cplusplus)
}
#endif

/*! @}*/

#endif /* __USB_DEVICE_MCTP_H__ */

// NOLINTEND