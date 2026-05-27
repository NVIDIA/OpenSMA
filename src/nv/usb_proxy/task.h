/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES.
 * All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 *
 * USB Proxy Task for Core0
 *
 * This task handles USB data forwarded from Core1 bare-metal USB implementation.
 * It processes MCTP and HID requests using Core0's existing infrastructure
 * (MCTP driver, I2C tasks, etc.) and sends responses back to Core1.
 *
 * Optimized: Uses existing C2C StreamBuffer instead of separate queues.
 */
#pragma once

#include NV_IPC_CONFIG_H

// Only enable when Core1 bare-metal USB is active
#if NCSI_ENABLE

#include <array>
#include <cstdint>
#include <span>

#include "nv/ipc/event.h"
#include "nv/ipc/queue.h"
#include "nv/ipc/task.h"
#include "nv/i2c/common.h"
#include "nv/lstp/lstp_router.h"
#include "nv/mctp/router.h"
#include "nv/usb/mctp_router.h"

namespace nv::usb_proxy {

/// Queue item for I2C response from I2C/I3C/IOX task to usb_proxy (via UsbI2cResp queue)
struct I2cResponseItem
{
    uint16_t                read_length;
    i2c::I2cStatus          result;
    uint8_t                 data_length;
    std::array<uint8_t, 64> data;
};
static_assert(sizeof(I2cResponseItem) == 68,
              "I2cResponseItem size must match UsbI2cResp queue item_size (68)");

/**
 * @brief USB Proxy Task
 *
 * Receives USB data from Core1 via C2C and processes:
 * - MCTP packets: forwarded to MCTP driver
 * - HID reports: received via UsbHid queue, processed and forwarded to I2C tasks
 * - I2C responses: received via UsbI2cResp queue
 *
 * Responses are sent back to Core1 via C2C StreamBuffer.
 */
class Task : public ipc::Task
{
public:
    Task() noexcept;

    /**
     * @brief Create the task instance
     */
    static void make();

    /**
     * @brief Task entry point
     */
    static void entrypoint(void* params);

    /**
     * @brief Process MCTP data received from C2C
     * @param data Pointer to MCTP data in C2C buffer
     * @param length Data length
     */
    static void process_mctp_from_c2c(const uint8_t* data, size_t length);

    /**
     * @brief Called by I2C task to send response back
     * @param src_id Source IPC handler ID
     * @param read_length Bytes read
     * @param item Response data
     * @param result I2C operation result
     * @return true on success
     */
    static bool to_usb_proxy(ipchandler::Id    src_id,
                             uint16_t          read_length,
                             ipc::Queue::Item& item,
                             i2c::I2cStatus    result);

    /**
     * @brief Forward LSTP data to Core1 via C2C
     * @param data LSTP message data
     * @return true on success
     */
    static bool to_usbLstp_proxy(std::span<uint8_t>& data);

    // Event bits for this task
    enum EventBits : uint32_t
    {
        MctpRxBit             = (1U << 0),
        HidRxBit              = (1U << 1),
        HidI2cRespBit         = (1U << 2),
        WdtBit                = (1U << 3),
        UpdateRoutingTableBit = (1U << 4),
        AcmRxBit              = (1U << 5),
        LstpRxBit             = (1U << 6),
    };

    /**
     * @brief Set event to update routing table
     */
    static void set_update_routing_table_event();

private:
    // Minimal buffer sizes - only for HID state machine
    static constexpr size_t HidBufferSize = 64;
    static constexpr size_t MaxDataSize   = 61;

    // CP2112 HID report header sizes
    static constexpr size_t DataWriteHeaderSize        = 3;  // ReportId + SlaveAddr + WriteLen
    static constexpr size_t DataWriteReadReqHeaderSize = 5;  // ReportId + SlaveAddr + ReadLenHi
                                                             // + ReadLenLo + TarAddrLen

    // HID Report IDs (CP2112 compatible)
    enum class ReportId : uint8_t
    {
        DataReadReq       = 0x10,
        DataWriteReadReq  = 0x11,
        DataReadForceSend = 0x12,
        DataReadRes       = 0x13,
        DataWrite         = 0x14,
        TransferStatusReq = 0x15,
        TransferStatusRes = 0x16,
        CancelTransfer    = 0x17,
    };

    enum class Status0 : uint8_t
    {
        Idle              = 0x0,
        Busy              = 0x1,
        Complete          = 0x2,
        CompleteWithError = 0x3,
    };

    enum class Status1 : uint8_t
    {
        Acked           = 0x0,
        Nacked          = 0x1,
        ReadInProgress  = 0x2,
        WriteInProgress = 0x3,
        TimeoutNacked   = 0x0,
        Scllowtimeout   = 0x1,
        ArbitrationLost = 0x2,
        ReadIncomplete  = 0x3,
        WriteIncomplete = 0x4,
        Succeeded       = 0x5,
    };

    enum class HidState
    {
        Idle,
        Read,
        Write,
    };

    // Minimal HID state - no large buffers
    HidState _hid_state{HidState::Idle};
    uint16_t _hid_read_length{};
    uint16_t _hid_bytes_received{};
    Status0  _hid_status0{Status0::Idle};
    Status1  _hid_status1{Status1::Acked};
    bool     _has_pending_response{false};

    // HID response buffer
    alignas(4) std::array<uint8_t, HidBufferSize> _response_buffer{};

    // I2C response data storage (for read operations)
    alignas(4) std::array<uint8_t, HidBufferSize> _i2c_response_buffer{};
    size_t _i2c_response_data_length{0};

    // Event reference
    ipc::Event& _event;

    // Routing table for MCTP bridging (type from usb::RoutingTable)
    static usb::RoutingTable _routing_table;

    // LSTP router for Core0-owned channels forwarded by Core1 USB.
    nv::lstp::LstpRouter _lstp_router;

    /**
     * @brief Main task loop
     */
    [[noreturn]] void main();

    /**
     * @brief Process received HID data
     */
    void process_hid(const uint8_t* data, size_t length);

    /**
     * @brief Route I2C request to appropriate backend (IOX/I3C/I2C)
     * @param ipchandler_id Target handler ID
     * @param physical_addr Physical I2C address
     * @param write_len Write data length
     * @param read_len Read data length
     * @param buffer Data buffer
     * @return true if request was sent successfully
     */
    bool send_to_backend(ipchandler::Id     ipchandler_id,
                         uint8_t            physical_addr,
                         uint8_t            write_len,
                         uint16_t           read_len,
                         std::span<uint8_t> buffer);

    /**
     * @brief Process I2C response from queue item
     */
    void process_i2c_response(uint16_t       read_length,
                              const uint8_t* data,
                              size_t         data_length,
                              i2c::I2cStatus result);

    /**
     * @brief Get I2C handler ID and physical address from virtual address
     */
    std::pair<ipchandler::Id, uint8_t> i2c_addr_mapping(uint8_t virtual_addr);

    /**
     * @brief Send HID response back to Core1 via C2C
     */
    bool send_response_to_core1(const uint8_t* data, size_t length);

    /**
     * @brief Update routing table from MCTP driver
     */
    void update_routing_table();
};

/**
 * @brief Dispatch C2C data from IPC task to the appropriate USB handler.
 * @param queue_id The queue ID from the C2C request
 * @param data Pointer to data in IPC task's buffer
 * @param length Actual data length from C2C request
 * @return true if handled (USB queue), false if not a USB queue (caller should use generic
 * path)
 */
bool dispatch_c2c_data(ipc::QueueId queue_id, const uint8_t* data, uint16_t length);

/**
 * @brief Forward I2C response to usb_proxy::Task (called from I2C/I3C/IOX task)
 */
bool to_usb_proxy_impl(ipchandler::Id    src_id,
                       uint16_t          read_length,
                       ipc::Queue::Item& item,
                       i2c::I2cStatus    result);

/**
 * @brief Forward LSTP response data to Core1 via C2C
 */
bool to_usbLstp_proxy_impl(std::span<uint8_t>& data);

}  // namespace nv::usb_proxy

#endif  // NCSI_ENABLE
