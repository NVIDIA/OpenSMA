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

#if USB_DEVICE_CONFIG_CDC_ECM

#ifndef NV_ECM_ETH_ADAPTER_H
#define NV_ECM_ETH_ADAPTER_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*******************************************************************************
 * Definitions
 ******************************************************************************/

// Default MAC address: 54-27-8D is NXP OUI, 45-54-48 is "ETH" in ASCII
#define ETH_ADAPTER_MAC_ADDRESS                                                                \
    {                                                                                          \
        0x54, 0x27, 0x8D, 0x45, 0x54, 0x48                                                     \
    }

// Frame queue sizes for USB <-> Ethernet buffering
// TX queue: USB -> Ethernet
#define ETH_ADAPTER_FRAME_TX_BUFFER_LENGTH (16U)
// RX queue: Ethernet -> USB
#define ETH_ADAPTER_FRAME_RX_BUFFER_LENGTH (16U)

#define ETH_ADAPTER_MAX_FRAME_LEN (1514U)

/*******************************************************************************
 * Types
 ******************************************************************************/

typedef enum _eth_adapter_err
{
    ETH_ADAPTER_OK = 0,
    ETH_ADAPTER_ERROR,
    ETH_ADAPTER_INVALID,
    ETH_ADAPTER_BUSY,
} eth_adapter_err_t;

typedef enum _eth_adapter_dst_frame_type
{
    ETH_ADAPTER_DST_FRAME_UNICAST = 0,
    ETH_ADAPTER_DST_FRAME_MULTICAST,
    ETH_ADAPTER_DST_FRAME_BROADCAST,
} eth_adapter_dst_frame_type_t;

typedef struct _eth_adapter_frame_buf
{
    uint8_t* payload;
    uint32_t len;
} eth_adapter_frame_buf_t;

typedef struct _eth_adapter_frame_queue
{
    uint32_t                 idx;
    uint32_t                 total_len;
    uint32_t                 valid_len;
    eth_adapter_frame_buf_t* queue;
} eth_adapter_frame_queue_t;

typedef void* eth_adapter_callback_param_t;
typedef void (*eth_adapter_callback_t)(eth_adapter_callback_param_t);

typedef struct _eth_adapter_handle
{
    eth_adapter_frame_queue_t    txFrameQueue;
    eth_adapter_frame_queue_t    rxFrameQueue;
    eth_adapter_callback_t       txCallback;
    eth_adapter_callback_param_t txUserInfo;
    eth_adapter_callback_t       rxCallback;
    eth_adapter_callback_param_t rxUserInfo;
    eth_adapter_callback_t       errCallback;
    eth_adapter_callback_param_t errUserInfo;
    volatile bool                unicastFramePass;
    volatile bool                multicastFramePass;
    volatile bool                broadcastFramePass;
} eth_adapter_handle_t;

// External adapter handle (defined in eth_adapter.c)
extern eth_adapter_handle_t g_eth_adapter_handle;

/*******************************************************************************
 * API
 ******************************************************************************/

/**
 * @brief Initialize the ethernet adapter
 * @return ETH_ADAPTER_OK on success
 */
eth_adapter_err_t ETH_ADAPTER_Init(void);

/**
 * @brief Get the MAC address
 * @param address Buffer to store MAC address (6 bytes)
 * @return ETH_ADAPTER_OK on success
 */
eth_adapter_err_t ETH_ADAPTER_GetMacAddress(uint8_t* address);

/**
 * @brief Set the MAC address (call before ETH_ADAPTER_InitMac()).
 * @param address MAC address (6 bytes); must not be NULL
 * @return ETH_ADAPTER_OK on success, ETH_ADAPTER_INVALID if address is NULL or MAC already
 * inited
 */
eth_adapter_err_t ETH_ADAPTER_SetMacAddress(const uint8_t* address);

/**
 * @brief Initialize ENET MAC after hardware (pins/clock/reset) is ready.
 *
 * ENET MAC init is deferred until after USB enumerates, because RMII ref clock
 * may not be stable during early boot and ENET init can hang.
 * PHY runs in default auto-negotiation mode (no MDIO control).
 *
 * @return ETH_ADAPTER_OK on success
 */
eth_adapter_err_t ETH_ADAPTER_InitMac(void);

/**
 * @brief Check if ENET MAC is initialized
 * @return true if MAC is initialized, false otherwise
 */
bool ETH_ADAPTER_IsMacInited(void);

/**
 * @brief Get the MAC speed configured/reported by the ENET adapter.
 *
 * Production builds: always returns 100 Mbps because the PHY is strap-pin
 * configured and there is no MDIO visibility.
 *
 * Devkit (NCSI_USE_PHY_MDIO): returns the speed sampled from the LAN8741
 * during ETH_ADAPTER_InitMac(). This is the speed used for MAC configuration
 * and the USB ECM speed notification; it is not refreshed after runtime
 * renegotiation.
 *
 * @return Speed in bps (10000000 or 100000000)
 */
uint32_t ETH_ADAPTER_GetConfiguredSpeedBps(void);

/**
 * @brief Send an ethernet frame
 * @param buffer Frame buffer to send
 * @return ETH_ADAPTER_OK on success
 */
eth_adapter_err_t ETH_ADAPTER_SendFrame(eth_adapter_frame_buf_t* buffer);

/**
 * @brief Receive an ethernet frame
 * @param buffer Frame buffer to receive into
 * @param maxLength Maximum frame length
 * @return ETH_ADAPTER_OK on success
 */
eth_adapter_err_t ETH_ADAPTER_RecvFrame(eth_adapter_frame_buf_t* buffer, uint32_t maxLength);

/**
 * @brief Send all queued frames
 * @return ETH_ADAPTER_OK on success
 */
eth_adapter_err_t ETH_ADAPTER_SendFrameQueue(void);

/**
 * @brief Receive frames into queue
 * @return ETH_ADAPTER_OK on success
 */
eth_adapter_err_t ETH_ADAPTER_RecvFrameQueue(void);

/**
 * @brief Identify destination frame type (unicast/multicast/broadcast)
 * @param buffer Frame buffer
 * @param type Pointer to store frame type
 * @return ETH_ADAPTER_OK on success
 */
eth_adapter_err_t ETH_ADAPTER_IdentifyDstFrameType(eth_adapter_frame_buf_t*      buffer,
                                                   eth_adapter_dst_frame_type_t* type);

/*******************************************************************************
 * Frame Queue Helpers
 ******************************************************************************/

/**
 * @brief Reset a frame queue with static allocation
 * @param queue The queue to reset
 * @param staticQueue Pre-allocated static frame buffer descriptor array
 * @param buffer Pre-allocated static data buffer
 * @param length Number of frames in the queue
 * @param bufferUnitLength Size of each frame buffer
 */
eth_adapter_err_t ETH_ADAPTER_FrameQueueReset(eth_adapter_frame_queue_t* queue,
                                              eth_adapter_frame_buf_t*   staticQueue,
                                              uint8_t*                   buffer,
                                              uint32_t                   length,
                                              uint32_t                   bufferUnitLength);

/**
 * @brief Get the front frame from queue without removing
 */
eth_adapter_err_t ETH_ADAPTER_FrameQueueGet(eth_adapter_frame_queue_t* queue,
                                            eth_adapter_frame_buf_t**  buffer);

/**
 * @brief Push a frame to the queue
 */
eth_adapter_err_t ETH_ADAPTER_FrameQueuePush(eth_adapter_frame_queue_t* queue,
                                             eth_adapter_frame_buf_t*   buffer);

/**
 * @brief Pop a frame from the queue
 */
eth_adapter_err_t ETH_ADAPTER_FrameQueuePop(eth_adapter_frame_queue_t* queue,
                                            eth_adapter_frame_buf_t**  buffer);

/**
 * @brief Allocate space for a new frame in the queue
 */
eth_adapter_err_t ETH_ADAPTER_FrameQueueAlloc(eth_adapter_frame_queue_t* queue,
                                              eth_adapter_frame_buf_t**  buffer);

/**
 * @brief Drop the last allocated frame
 */
eth_adapter_err_t ETH_ADAPTER_FrameQueueDrop(eth_adapter_frame_queue_t* queue,
                                             eth_adapter_frame_buf_t**  buffer);

/**
 * @brief Clear all frames in the queue
 */
eth_adapter_err_t ETH_ADAPTER_FrameQueueClear(eth_adapter_frame_queue_t* queue);

#ifdef __cplusplus
}
#endif

#endif  // NV_ECM_ETH_ADAPTER_H

#endif  // USB_DEVICE_CONFIG_CDC_ECM
