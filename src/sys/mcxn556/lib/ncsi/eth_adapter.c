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

#if defined(NCSI_USE_PHY_MDIO) && (NCSI_USE_PHY_MDIO > 0)
#include "fsl_phy.h"
#endif

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
// BOARD_PhySysClock and BOARD_PhyAddress are declared in hardware_init.h.
// BOARD_PhyOps / BOARD_PhySource depend on fsl_phy.h types and are kept local
// to avoid pulling that header into hardware_init.h's wider consumer set.
extern ENET_Type* BOARD_Enet;

#if defined(NCSI_USE_PHY_MDIO) && (NCSI_USE_PHY_MDIO > 0)
extern const phy_operations_t* BOARD_PhyOps;  // defined in hardware_init.cpp
extern void*                   BOARD_PhySource;
#endif

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

#if defined(NCSI_USE_PHY_MDIO) && (NCSI_USE_PHY_MDIO > 0)
// PHY handle (for LAN8741 driver on devkit)
static phy_handle_t s_phy_handle;

// Negotiated link speed/duplex written by ETH_ADAPTER_PHY_Init and consumed by
// ETH_ADAPTER_MAC_Init. Defaults assume 100M/FD so a MAC brought up before
// auto-neg converges (or if the PHY query fails) at least matches the most
// likely link state on modern switches.
static enet_mii_speed_t  s_phy_speed  = kENET_MiiSpeed100M;
static enet_mii_duplex_t s_phy_duplex = kENET_MiiFullDuplex;

// Wall-clock-bounded auto-neg poll. 3 s covers worst-case 802.3 auto-neg
// convergence (~2 s typical), and ~50 ms between MDIO reads keeps the bus
// idle long enough to not stress the PHY. Independent of SMI clock, unlike
// a naked counter loop.
#define ETH_ADAPTER_AUTONEG_TIMEOUT_MS (3000U)
#define ETH_ADAPTER_AUTONEG_POLL_MS    (50U)
#endif

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

#if defined(NCSI_USE_PHY_MDIO) && (NCSI_USE_PHY_MDIO > 0)
/*******************************************************************************
 * PHY Initialization (devkit, LAN8741 via MDIO)
 ******************************************************************************/

static eth_adapter_err_t ETH_ADAPTER_PHY_Init(void)
{
    phy_config_t phyConfig = {
        .phyAddr  = BOARD_PhyAddress,
        .autoNeg  = true,
        .ops      = BOARD_PhyOps,
        .resource = BOARD_PhySource,
    };

    if (PHY_Init(&s_phy_handle, &phyConfig) != kStatus_Success) {
        return ETH_ADAPTER_ERROR;
    }

    // PHY_Init only kicks auto-neg off; we still have to wait for it to
    // converge before reading the negotiated speed/duplex. SDK_DelayAtLeastUs
    // gives a stable wall-clock-based timeout regardless of how SMI/MDIO clock
    // is configured.
    bool autoneg_done = false;
    for (uint32_t elapsed_ms  = 0U; elapsed_ms < ETH_ADAPTER_AUTONEG_TIMEOUT_MS;
         elapsed_ms          += ETH_ADAPTER_AUTONEG_POLL_MS) {
        if (PHY_GetAutoNegotiationStatus(&s_phy_handle, &autoneg_done) != kStatus_Success) {
            return ETH_ADAPTER_ERROR;
        }
        if (autoneg_done) {
            break;
        }
        SDK_DelayAtLeastUs(ETH_ADAPTER_AUTONEG_POLL_MS * 1000U, SystemCoreClock);
    }

    if (!autoneg_done) {
        // Link never came up. Leave s_phy_speed/s_phy_duplex at the 100M/FD
        // default and let MAC_Init proceed - if the cable is plugged in
        // later, the link is more likely to land on 100M/FD than 10M with a
        // modern switch.
        return ETH_ADAPTER_OK;
    }

    phy_speed_t  speed;
    phy_duplex_t duplex;
    if (PHY_GetLinkSpeedDuplex(&s_phy_handle, &speed, &duplex) != kStatus_Success) {
        return ETH_ADAPTER_ERROR;
    }

    s_phy_speed  = (speed == kPHY_Speed100M) ? kENET_MiiSpeed100M : kENET_MiiSpeed10M;
    s_phy_duplex = (duplex == kPHY_FullDuplex) ? kENET_MiiFullDuplex : kENET_MiiHalfDuplex;
    return ETH_ADAPTER_OK;
}
#endif  // NCSI_USE_PHY_MDIO

/*******************************************************************************
 * MAC Initialization
 *
 * Production boards: PHY is strap-pin configured and we have no MDIO
 * visibility, so MAC is hardcoded to 100M Full Duplex (typical auto-neg
 * result with modern switches; if reality differs, packets drop).
 *
 * Devkit (NCSI_USE_PHY_MDIO): ETH_ADAPTER_PHY_Init() ran before this and
 * populated s_phy_speed / s_phy_duplex from the LAN8741's negotiated link
 * state - MAC tracks that. Falls back to the same 100M/FD default if
 * auto-neg never converged.
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

#if defined(NCSI_USE_PHY_MDIO) && (NCSI_USE_PHY_MDIO > 0)
    // Devkit: use the speed/duplex negotiated by the LAN8741 PHY (populated
    // by ETH_ADAPTER_PHY_Init). Falls back to 100M/FD if auto-neg never
    // converged - see s_phy_speed initialiser.
    config.miiSpeed  = s_phy_speed;
    config.miiDuplex = s_phy_duplex;
#else
    // Production: PHY is strap-pin configured for auto-neg with no MDIO
    // visibility; we assume the typical 100M/FD result.
    config.miiSpeed  = kENET_MiiSpeed100M;
    config.miiDuplex = kENET_MiiFullDuplex;
#endif

    // Set MII mode based on PHY interface
#ifdef EXAMPLE_PHY_INTERFACE_RGMII
    config.miiMode = kENET_RgmiiMode;
#else
    config.miiMode = kENET_RmiiMode;
#endif

    // Enable interrupts
    config.interrupt = kENET_DmaTx | kENET_DmaRx | kENET_DmaBusErr;

    // ENET_Init uses the RMII clock source configured during ENET hardware init.
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

#if defined(NCSI_USE_PHY_MDIO) && (NCSI_USE_PHY_MDIO > 0)
    // Devkit (LAN8741): kick the PHY out of its default state via MDIO so its
    // RX path actually feeds the MAC. Production boards skip this - the PHY is
    // brought up by hardware strap pins.
    if (ETH_ADAPTER_PHY_Init() != ETH_ADAPTER_OK) {
        return ETH_ADAPTER_ERROR;
    }
#endif

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

uint32_t ETH_ADAPTER_GetConfiguredSpeedBps(void)
{
#if defined(NCSI_USE_PHY_MDIO) && (NCSI_USE_PHY_MDIO > 0)
    // Tracks the speed sampled from the LAN8741 during ETH_ADAPTER_InitMac().
    return (s_phy_speed == kENET_MiiSpeed100M) ? 100000000U : 10000000U;
#else
    // Production: PHY is strap-pin configured; MAC is hardcoded to 100M FD
    // and we have no MDIO visibility to confirm or correct.
    return 100000000U;
#endif
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
