/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES.
 * All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 *
 * Bare-Metal IPC Driver for Core1
 *
 * Compatible with Core0's existing ipc::task::Task
 * Uses MCMGR for startup data exchange and event notification
 * Uses shared StreamBuffer-compatible memory for data transfer
 */

#ifndef NV_IPC_BM_DRIVER_H
#define NV_IPC_BM_DRIVER_H

#include <cstdint>
#include <cstddef>

// Include project config for QueueId (same as Core0)
// CORE1_BARE_METAL flag in config.h skips Core0-only includes
#include NV_IPC_CONFIG_H

// Shared wire format definitions for C2C IPC
#include "nv/ipc/wire_format.h"

namespace nv::ipc_bm {

/*******************************************************************************
 * Status Codes
 ******************************************************************************/
enum class Status : uint32_t
{
    Ok = 0,
    NotInitialized,
    NotReady,
    Timeout,
    InvalidParam,
    McmgrError,
    SendFailed,
    RecvFailed,
    Error,
};

/*******************************************************************************
 * Request Types - Use project config's nv::ipc::QueueId
 * This ensures Core0 and Core1 use identical QueueId values for C2C IPC.
 ******************************************************************************/
using QueueId = ::nv::ipc::QueueId;

/*******************************************************************************
 * Driver Class
 ******************************************************************************/
class Driver
{
public:
    /**
     * @brief Initialize IPC driver (call from Core1 bare-metal main)
     *
     * This will:
     * 1. Get startup data (shared memory address) from Core0 via MCMGR
     * 2. Initialize shared memory access
     * 3. Notify Core0 that Core1 is ready
     *
     * @return Status
     */
    static Status init();

    /**
     * @brief Check if IPC is initialized
     */
    static bool isInitialized();

    /**
     * @brief Get shared memory base address (C2C buffers)
     */
    static uint32_t getSharedMemoryAddress();

    /**
     * @brief Get address of read-only block for Core1 (set by Core0 before start; 0 if unused)
     */
    static uint32_t getCore1CfgDataAddress();

    /**
     * @brief Send queue data to Core0
     * @param queue_id Target queue
     * @param data Data to send
     * @param length Data length
     * @param is_front Send to front of queue
     * @return Status
     */
    static Status
    sendToQueue(QueueId queue_id, const uint8_t* data, size_t length, bool is_front = false);

    /**
     * @brief Send event to Core0
     * @param event_id Event ID
     * @param bits Event bits
     * @param is_set true to set, false to clear
     * @return Status
     */
    static Status sendEvent(uint8_t event_id, uint32_t bits, bool is_set);

    /**
     * @brief Check if data is available from Core0
     * @return Number of bytes available
     */
    static size_t bytesAvailable();

    /**
     * @brief Read data from Core0
     * @param buffer Output buffer
     * @param max_length Maximum bytes to read
     * @return Actual bytes read
     */
    static size_t read(uint8_t* buffer, size_t max_length);

    /**
     * @brief Notify Core0 (trigger interrupt)
     */
    static void notifyCore0();

    /**
     * @brief Check if data available flag is set (from MCMGR callback)
     */
    static bool hasDataAvailable();

    /**
     * @brief Clear the data available flag
     */
    static void clearDataAvailable();

    /**
     * @brief Check cfg data
     */
    static uint8_t* getCore1CfgNcsiMac();

    /**
     * @brief Return true if Core1 config has a valid NCSI/ECM MAC address to use
     */
    static bool hasValidNcsiMac();

private:
    static Status initMcmgr();
    static Status getStartupData(uint32_t* data);
};

}  // namespace nv::ipc_bm

#endif  // NV_IPC_BM_DRIVER_H
