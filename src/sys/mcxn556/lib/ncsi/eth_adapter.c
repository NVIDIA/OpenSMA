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

// TODO mduchalski: Split this into separate library
#if USB_DEVICE_CONFIG_CDC_ECM

/*******************************************************************************
 * Includes
 ******************************************************************************/
#include "eth_adapter.h"
#include "hardware_init.h"
#include "fsl_enet.h"
#include "fsl_common.h"

#include <string.h>

/*******************************************************************************
 * Definitions
 ******************************************************************************/
// Buffer descriptors for high throughput
#define ENET_RXBD_NUM    (16)
#define ENET_TXBD_NUM    (16)
#define ENET_RXBUFF_SIZE (ENET_FRAME_MAX_FRAMELEN)

// DMA burst length for high throughput
#define ENET_DMA_BURST_LEN kENET_BurstLen16

#ifndef APP_ENET_BUFF_ALIGNMENT
#define APP_ENET_BUFF_ALIGNMENT (ENET_BUFF_ALIGNMENT)
#endif

// RX buffer count for double buffer mode
#define ENET_RX_BUFFER_COUNT (ENET_RXBD_NUM * 2)

// Critical section macros
#ifdef USB_STACK_FREERTOS
#include "FreeRTOS.h"
#include "task.h"
#define ETH_ADAPTER_ENTER_CRITICAL() taskENTER_CRITICAL()
#define ETH_ADAPTER_EXIT_CRITICAL()  taskEXIT_CRITICAL()
#else
// Bare-metal: save/restore PRIMASK via local variable to safely nest critical
// sections. Plain __disable_irq()/__enable_irq() would prematurely re-enable
// interrupts if called from ISR context (e.g. USB callback -> FrameQueueAlloc).
#define ETH_ADAPTER_ENTER_CRITICAL()                                                           \
    uint32_t _saved_primask = __get_PRIMASK();                                                 \
    __disable_irq()
#define ETH_ADAPTER_EXIT_CRITICAL() __set_PRIMASK(_saved_primask)
#endif

/*******************************************************************************
 * External Variables
 ******************************************************************************/
extern ENET_Type* BOARD_Enet;
extern uint32_t   BOARD_PhySysClock;

/*******************************************************************************
 * Variables
 ******************************************************************************/
// MAC address
static uint8_t s_mac_address[6] = ETH_ADAPTER_MAC_ADDRESS;

// Track whether ENET MAC has been initialized. This prevents early ENET register access
// (which can hang if RMII ref clock from CX9 isn't ready yet).
static volatile bool s_mac_inited = false;

// ENET handle
static enet_handle_t s_enet_handle;

// Buffer descriptors - aligned for DMA
SDK_ALIGN(static enet_rx_bd_struct_t s_rx_buff_descrip[ENET_RXBD_NUM], ENET_BUFF_ALIGNMENT);
SDK_ALIGN(static enet_tx_bd_struct_t s_tx_buff_descrip[ENET_TXBD_NUM], ENET_BUFF_ALIGNMENT);

// RX data buffers for double buffer mode
SDK_ALIGN(static uint8_t s_rx_data_buff[ENET_RX_BUFFER_COUNT][SDK_SIZEALIGN(
              ENET_RXBUFF_SIZE, APP_ENET_BUFF_ALIGNMENT)],
          APP_ENET_BUFF_ALIGNMENT);

// TX dirty info
static enet_tx_reclaim_info_t s_tx_dirty[ENET_TXBD_NUM];

// Frame buffers for queuing
static uint8_t s_tx_frame_buffers[ETH_ADAPTER_FRAME_TX_BUFFER_LENGTH]
                                 [ETH_ADAPTER_MAX_FRAME_LEN];
static uint8_t s_rx_frame_buffers[ETH_ADAPTER_FRAME_RX_BUFFER_LENGTH]
                                 [ETH_ADAPTER_MAX_FRAME_LEN];
static eth_adapter_frame_buf_t s_tx_frame_queue[ETH_ADAPTER_FRAME_TX_BUFFER_LENGTH];
static eth_adapter_frame_buf_t s_rx_frame_queue[ETH_ADAPTER_FRAME_RX_BUFFER_LENGTH];

// Global adapter handle
eth_adapter_handle_t g_eth_adapter_handle;

/*******************************************************************************
 * Prototypes
 ******************************************************************************/
static eth_adapter_err_t ETH_ADAPTER_MAC_Init(void);
static void              ETH_Callback(ENET_Type*              base,
                                      enet_handle_t*          handle,
                                      enet_event_t            event,
                                      uint8_t                 channel,
                                      enet_tx_reclaim_info_t* txReclaimInfo,
                                      void*                   param);

/*******************************************************************************
 * Frame Queue Implementation
 ******************************************************************************/

eth_adapter_err_t ETH_ADAPTER_FrameQueueReset(eth_adapter_frame_queue_t* queue,
                                              eth_adapter_frame_buf_t*   staticQueue,
                                              uint8_t*                   buffer,
                                              uint32_t                   length,
                                              uint32_t                   bufferUnitLength)
{
    if (!queue || !staticQueue || !buffer) {
        return ETH_ADAPTER_INVALID;
    }

    ETH_ADAPTER_ENTER_CRITICAL();

    queue->idx       = 0U;
    queue->valid_len = 0U;
    queue->total_len = length;
    queue->queue     = staticQueue;

    for (uint32_t cnt = 0U; cnt < length; cnt++) {
        queue->queue[cnt].payload = &buffer[cnt * bufferUnitLength];
        queue->queue[cnt].len     = 0U;
    }

    ETH_ADAPTER_EXIT_CRITICAL();
    return ETH_ADAPTER_OK;
}

eth_adapter_err_t ETH_ADAPTER_FrameQueueGet(eth_adapter_frame_queue_t* queue,
                                            eth_adapter_frame_buf_t**  buffer)
{
    if (!queue || !buffer) {
        return ETH_ADAPTER_INVALID;
    }

    ETH_ADAPTER_ENTER_CRITICAL();

    if (queue->valid_len) {
        *buffer = &queue->queue[queue->idx];
    }
    else {
        ETH_ADAPTER_EXIT_CRITICAL();
        return ETH_ADAPTER_ERROR;
    }

    ETH_ADAPTER_EXIT_CRITICAL();
    return ETH_ADAPTER_OK;
}

eth_adapter_err_t ETH_ADAPTER_FrameQueuePush(eth_adapter_frame_queue_t* queue,
                                             eth_adapter_frame_buf_t*   buffer)
{
    if (!queue || !buffer) {
        return ETH_ADAPTER_INVALID;
    }

    ETH_ADAPTER_ENTER_CRITICAL();

    if (queue->valid_len < queue->total_len) {
        uint32_t target_idx = (queue->idx + queue->valid_len) % queue->total_len;
        memcpy(queue->queue[target_idx].payload, buffer->payload, buffer->len);
        queue->queue[target_idx].len = buffer->len;
        queue->valid_len++;
    }
    else {
        ETH_ADAPTER_EXIT_CRITICAL();
        return ETH_ADAPTER_ERROR;
    }

    ETH_ADAPTER_EXIT_CRITICAL();
    return ETH_ADAPTER_OK;
}

eth_adapter_err_t ETH_ADAPTER_FrameQueuePop(eth_adapter_frame_queue_t* queue,
                                            eth_adapter_frame_buf_t**  buffer)
{
    if (!queue) {
        return ETH_ADAPTER_INVALID;
    }

    ETH_ADAPTER_ENTER_CRITICAL();

    if (queue->valid_len) {
        if (buffer) {
            *buffer = &queue->queue[queue->idx];
        }

        queue->idx = (queue->idx + 1) % queue->total_len;
        queue->valid_len--;
    }

    ETH_ADAPTER_EXIT_CRITICAL();
    return ETH_ADAPTER_OK;
}

eth_adapter_err_t ETH_ADAPTER_FrameQueueAlloc(eth_adapter_frame_queue_t* queue,
                                              eth_adapter_frame_buf_t**  buffer)
{
    if (!queue || !buffer) {
        return ETH_ADAPTER_INVALID;
    }

    ETH_ADAPTER_ENTER_CRITICAL();

    if (queue->valid_len < queue->total_len) {
        *buffer = &queue->queue[(queue->idx + queue->valid_len) % queue->total_len];
        queue->valid_len++;
    }
    else {
        ETH_ADAPTER_EXIT_CRITICAL();
        return ETH_ADAPTER_INVALID;
    }

    ETH_ADAPTER_EXIT_CRITICAL();
    return ETH_ADAPTER_OK;
}

eth_adapter_err_t ETH_ADAPTER_FrameQueueDrop(eth_adapter_frame_queue_t* queue,
                                             eth_adapter_frame_buf_t**  buffer)
{
    if (!queue) {
        return ETH_ADAPTER_INVALID;
    }

    ETH_ADAPTER_ENTER_CRITICAL();

    if (queue->valid_len) {
        queue->valid_len--;

        if (buffer) {
            *buffer = &queue->queue[(queue->idx + queue->valid_len) % queue->total_len];
        }
    }

    ETH_ADAPTER_EXIT_CRITICAL();
    return ETH_ADAPTER_OK;
}

eth_adapter_err_t ETH_ADAPTER_FrameQueueClear(eth_adapter_frame_queue_t* queue)
{
    if (!queue) {
        return ETH_ADAPTER_INVALID;
    }

    ETH_ADAPTER_ENTER_CRITICAL();
    queue->valid_len = 0U;
    ETH_ADAPTER_EXIT_CRITICAL();

    return ETH_ADAPTER_OK;
}

/*******************************************************************************
 * MAC Initialization
 *
 * PHY runs in default auto-negotiation mode (no MDIO control).
 * MAC is configured for 100Mbps Full Duplex (typical auto-neg result).
 ******************************************************************************/

static eth_adapter_err_t ETH_ADAPTER_MAC_Init(void)
{
    enet_config_t config;
    uint32_t      rxbuffer[ENET_RX_BUFFER_COUNT];

    // Setup RX buffer addresses for double buffer mode
    for (uint8_t index = 0U; index < ENET_RX_BUFFER_COUNT; index++) {
        rxbuffer[index] = (uint32_t)(&s_rx_data_buff[index][0]);
    }

    // Prepare buffer configuration
    enet_buffer_config_t buffConfig[] = {
        {
         ENET_RXBD_NUM, ENET_TXBD_NUM,
         &s_tx_buff_descrip[0],
         &s_tx_buff_descrip[ENET_TXBD_NUM],
         &s_tx_dirty[0],
         &s_rx_buff_descrip[0],
         &s_rx_buff_descrip[ENET_RXBD_NUM],
         &rxbuffer[0],
         sizeof(s_rx_data_buff[0]),
         }
    };

    // Get default configuration
    ENET_GetDefaultConfig(&config);

    // NOTE: Do NOT set multiqueueCfg here!
    // SDK expects 2 buffer configs when multiqueueCfg != NULL, but we only have 1.
    // We'll configure DMA burst length directly after ENET_Init.

    // Enable performance optimizations:
    // - Store-and-forward: better throughput, recommended for CDC ECM
    // - Double buffer: DMA can continue while CPU processes
    // - Rx checksum offload: reduce CPU overhead
    // - Promiscuous mode: accept all frames (needed for NCSI/bridge mode)
    config.specialControl |= kENET_StoreAndForward | kENET_DescDoubleBuffer
                           | kENET_RxChecksumOffloadEnable | kENET_PromiscuousEnable;

    // PHY runs in default auto-negotiation mode
    // Configure MAC for 100M Full Duplex (typical auto-neg result with modern switches)
    config.miiSpeed  = kENET_MiiSpeed100M;
    config.miiDuplex = kENET_MiiFullDuplex;

    // Set MII mode based on PHY interface
#ifdef EXAMPLE_PHY_INTERFACE_RGMII
    config.miiMode = kENET_RgmiiMode;
#else
    config.miiMode = kENET_RmiiMode;
#endif

    // Enable interrupts
    config.interrupt = kENET_DmaTx | kENET_DmaRx | kENET_DmaBusErr;

    // MCU generates 50MHz RMII clock internally (PLL0/3), so standard ENET_Init works
    ENET_Init(BOARD_Enet, &config, &s_mac_address[0], BOARD_PhySysClock);

    // ENET interrupt priority already set in ECM_InitEnetHardware()

    // Initialize descriptors
    if (ENET_DescriptorInit(BOARD_Enet, &config, &buffConfig[0]) != kStatus_Success) {
        return ETH_ADAPTER_ERROR;
    }

    // Create handler
    ENET_CreateHandler(BOARD_Enet, &s_enet_handle, &config, &buffConfig[0], ETH_Callback, NULL);

    // Configure DMA burst length
    {
        uint32_t burstLen = (uint32_t)ENET_DMA_BURST_LEN;
        uint32_t reg;

        reg = BOARD_Enet->DMA_CH[0].DMA_CHX_CTRL;
        if (burstLen & 0x10000U) {
            reg |= ENET_DMA_CH_DMA_CHX_CTRL_PBLx8_MASK;
        }
        else {
            reg &= ~ENET_DMA_CH_DMA_CHX_CTRL_PBLx8_MASK;
        }
        BOARD_Enet->DMA_CH[0].DMA_CHX_CTRL = reg;

        reg  = BOARD_Enet->DMA_CH[0].DMA_CHX_TX_CTRL & ~ENET_DMA_CH_DMA_CHX_TX_CTRL_TxPBL_MASK;
        reg |= ENET_DMA_CH_DMA_CHX_TX_CTRL_TxPBL(burstLen & 0x3FU);
        reg |= ENET_DMA_CH_DMA_CHX_TX_CTRL_OSF_MASK;
        BOARD_Enet->DMA_CH[0].DMA_CHX_TX_CTRL = reg;

        reg  = BOARD_Enet->DMA_CH[0].DMA_CHX_RX_CTRL & ~ENET_DMA_CH_DMA_CHX_RX_CTRL_RxPBL_MASK;
        reg |= ENET_DMA_CH_DMA_CHX_RX_CTRL_RxPBL(burstLen & 0x3FU);
        BOARD_Enet->DMA_CH[0].DMA_CHX_RX_CTRL = reg;
    }

    // Start RX/TX DMA
    ENET_StartRxTx(BOARD_Enet, 1, 1);

    return ETH_ADAPTER_OK;
}

/*******************************************************************************
 * ENET Callback
 ******************************************************************************/

static void ETH_Callback(ENET_Type*              base,
                         enet_handle_t*          handle,
                         enet_event_t            event,
                         uint8_t                 channel,
                         enet_tx_reclaim_info_t* txReclaimInfo,
                         void*                   param)
{
    switch (event) {
        case kENET_TxIntEvent:
            if (g_eth_adapter_handle.txCallback) {
                g_eth_adapter_handle.txCallback(g_eth_adapter_handle.txUserInfo);
            }
            break;

        case kENET_RxIntEvent:
            if (g_eth_adapter_handle.rxCallback) {
                g_eth_adapter_handle.rxCallback(g_eth_adapter_handle.rxUserInfo);
            }
            break;

        default: break;
    }
}

/*******************************************************************************
 * Public API
 ******************************************************************************/

eth_adapter_err_t ETH_ADAPTER_Init(void)
{
    // Initialize TX frame queue
    if (ETH_ADAPTER_FrameQueueReset(&g_eth_adapter_handle.txFrameQueue,
                                    s_tx_frame_queue,
                                    (uint8_t*)s_tx_frame_buffers,
                                    ETH_ADAPTER_FRAME_TX_BUFFER_LENGTH,
                                    ETH_ADAPTER_MAX_FRAME_LEN)
        != ETH_ADAPTER_OK) {
        return ETH_ADAPTER_ERROR;
    }

    // Initialize RX frame queue
    if (ETH_ADAPTER_FrameQueueReset(&g_eth_adapter_handle.rxFrameQueue,
                                    s_rx_frame_queue,
                                    (uint8_t*)s_rx_frame_buffers,
                                    ETH_ADAPTER_FRAME_RX_BUFFER_LENGTH,
                                    ETH_ADAPTER_MAX_FRAME_LEN)
        != ETH_ADAPTER_OK) {
        return ETH_ADAPTER_ERROR;
    }

    // Initialize handle fields
    g_eth_adapter_handle.txCallback         = NULL;
    g_eth_adapter_handle.txUserInfo         = NULL;
    g_eth_adapter_handle.rxCallback         = NULL;
    g_eth_adapter_handle.rxUserInfo         = NULL;
    g_eth_adapter_handle.errCallback        = NULL;
    g_eth_adapter_handle.errUserInfo        = NULL;
    g_eth_adapter_handle.unicastFramePass   = true;
    g_eth_adapter_handle.multicastFramePass = true;
    g_eth_adapter_handle.broadcastFramePass = true;

    // Defer ENET MAC init until ETH_ADAPTER_InitMac() is called.
    // This allows USB to enumerate first before touching ENET
    // (RMII ref clock may not be stable during early boot).
    s_mac_inited = false;
    return ETH_ADAPTER_OK;
}

eth_adapter_err_t ETH_ADAPTER_InitMac(void)
{
    if (s_mac_inited) {
        return ETH_ADAPTER_OK;
    }

    if (ETH_ADAPTER_MAC_Init() != ETH_ADAPTER_OK) {
        return ETH_ADAPTER_ERROR;
    }

    s_mac_inited = true;
    return ETH_ADAPTER_OK;
}

bool ETH_ADAPTER_IsMacInited(void)
{
    return s_mac_inited;
}

eth_adapter_err_t ETH_ADAPTER_GetMacAddress(uint8_t* address)
{
    if (!address) {
        return ETH_ADAPTER_INVALID;
    }

    // Return static MAC address
    memcpy(address, s_mac_address, 6);
    return ETH_ADAPTER_OK;
}

eth_adapter_err_t ETH_ADAPTER_SetMacAddress(const uint8_t* address)
{
    if (!address) {
        return ETH_ADAPTER_INVALID;
    }

    if (s_mac_inited) {
        return ETH_ADAPTER_INVALID;  // Do not change MAC after ENET init
    }

    /* just memcpy since address and s_mac_address are big-endian (MSB first) */
    memcpy(s_mac_address, address, 6);
    return ETH_ADAPTER_OK;
}

eth_adapter_err_t ETH_ADAPTER_SendFrame(eth_adapter_frame_buf_t* buffer)
{
    if (!buffer || !buffer->payload || buffer->len == 0) {
        return ETH_ADAPTER_INVALID;
    }

    if (!s_mac_inited) {
        // ENET MAC not ready yet - return BUSY so frame stays in queue for retry
        return ETH_ADAPTER_BUSY;
    }

    enet_buffer_struct_t txBuff = {
        .buffer = buffer->payload,
        .length = buffer->len,
    };

    enet_tx_frame_struct_t txFrame = {
        .context     = buffer->payload,
        .txBuffArray = &txBuff,
        .txBuffNum   = 1,
        .txConfig    = {
                        .intEnable    = true,
                        .tsEnable     = false,
                        .txOffloadOps = kENET_TxOffloadAll,  // Enable TX checksum offload
        }
    };

    status_t status = ENET_SendFrame(BOARD_Enet, &s_enet_handle, &txFrame, 0);

    if (status != kStatus_Success) {
        if (status == kStatus_ENET_TxFrameBusy) {
            return ETH_ADAPTER_BUSY;
        }
        return ETH_ADAPTER_ERROR;
    }

    return ETH_ADAPTER_OK;
}

eth_adapter_err_t ETH_ADAPTER_RecvFrame(eth_adapter_frame_buf_t* buffer, uint32_t maxLength)
{
    if (!s_mac_inited) {
        (void)maxLength;
        if (buffer) {
            buffer->len = 0;
        }
        return ETH_ADAPTER_OK;
    }

    if (!buffer) {
        // Discard frame
        (void)ENET_ReadFrame(BOARD_Enet, &s_enet_handle, NULL, 0, 0, NULL);
        return ETH_ADAPTER_OK;
    }

    // Get received frame size
    status_t status = ENET_GetRxFrameSize(BOARD_Enet, &s_enet_handle, &buffer->len, 0);

    if (buffer->len != 0) {
        if (buffer->len > maxLength) {
            // Frame too large, discard
            (void)ENET_ReadFrame(BOARD_Enet, &s_enet_handle, NULL, 0, 0, NULL);
            return ETH_ADAPTER_ERROR;
        }
        else {
            // Read frame
            if (ENET_ReadFrame(
                    BOARD_Enet, &s_enet_handle, buffer->payload, buffer->len, 0, NULL)
                != kStatus_Success) {
                return ETH_ADAPTER_ERROR;
            }

            // Check frame type and filter
            eth_adapter_dst_frame_type_t type;
            bool                         forwardUp = false;

            if (ETH_ADAPTER_IdentifyDstFrameType(buffer, &type) == ETH_ADAPTER_OK) {
                switch (type) {
                    case ETH_ADAPTER_DST_FRAME_UNICAST:
                        forwardUp = g_eth_adapter_handle.unicastFramePass;
                        break;

                    case ETH_ADAPTER_DST_FRAME_MULTICAST:
                        forwardUp = g_eth_adapter_handle.multicastFramePass;
                        break;

                    case ETH_ADAPTER_DST_FRAME_BROADCAST:
                        forwardUp = g_eth_adapter_handle.broadcastFramePass;
                        break;

                    default: break;
                }
            }

            if (!forwardUp) {
                buffer->len = 0U;
            }
        }
    }
    else if (status == kStatus_ENET_RxFrameError) {
        // Update buffer on error frame
        (void)ENET_ReadFrame(BOARD_Enet, &s_enet_handle, NULL, 0, 0, NULL);
        return ETH_ADAPTER_ERROR;
    }

    return ETH_ADAPTER_OK;
}

eth_adapter_err_t ETH_ADAPTER_SendFrameQueue(void)
{
    eth_adapter_err_t        status = ETH_ADAPTER_OK;
    eth_adapter_frame_buf_t* frame;

    while (g_eth_adapter_handle.txFrameQueue.valid_len) {
        status = ETH_ADAPTER_FrameQueueGet(&g_eth_adapter_handle.txFrameQueue, &frame);
        if (status != ETH_ADAPTER_OK) {
            break;
        }

        status = ETH_ADAPTER_SendFrame(frame);
        if (status != ETH_ADAPTER_OK) {
            break;
        }
        else {
            status = ETH_ADAPTER_FrameQueuePop(&g_eth_adapter_handle.txFrameQueue, NULL);
            if (status != ETH_ADAPTER_OK) {
                break;
            }
        }
    }

    return status;
}

eth_adapter_err_t ETH_ADAPTER_RecvFrameQueue(void)
{
    eth_adapter_err_t        status = ETH_ADAPTER_OK;
    eth_adapter_frame_buf_t* data;

    while (g_eth_adapter_handle.rxFrameQueue.valid_len
           < g_eth_adapter_handle.rxFrameQueue.total_len) {
        status = ETH_ADAPTER_FrameQueueAlloc(&g_eth_adapter_handle.rxFrameQueue, &data);
        if (status != ETH_ADAPTER_OK) {
            break;
        }

        status = ETH_ADAPTER_RecvFrame(data, ETH_ADAPTER_MAX_FRAME_LEN);
        if (status != ETH_ADAPTER_OK) {
            // Drop the alloc'd slot to avoid zombie frame in queue
            ETH_ADAPTER_FrameQueueDrop(&g_eth_adapter_handle.rxFrameQueue, NULL);
            break;
        }

        if (!data->len) {
            status = ETH_ADAPTER_FrameQueueDrop(&g_eth_adapter_handle.rxFrameQueue, NULL);
            break;
        }
    }

    return status;
}

// Broadcast MAC address for comparison
static const uint8_t s_broadcast_addr[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

eth_adapter_err_t ETH_ADAPTER_IdentifyDstFrameType(eth_adapter_frame_buf_t*      buffer,
                                                   eth_adapter_dst_frame_type_t* type)
{
    if (!buffer || !buffer->payload || !type) {
        return ETH_ADAPTER_INVALID;
    }

    // Check for broadcast (all 0xFF) using memcmp for better performance
    if (memcmp(buffer->payload, s_broadcast_addr, 6) == 0) {
        *type = ETH_ADAPTER_DST_FRAME_BROADCAST;
        return ETH_ADAPTER_OK;
    }

    // Check for unicast (LSB of first byte is 0)
    if ((buffer->payload[0] & 0x01) == 0) {
        *type = ETH_ADAPTER_DST_FRAME_UNICAST;
        return ETH_ADAPTER_OK;
    }

    // Otherwise it's multicast
    *type = ETH_ADAPTER_DST_FRAME_MULTICAST;
    return ETH_ADAPTER_OK;
}

#endif  // USB_DEVICE_CONFIG_CDC_ECM
