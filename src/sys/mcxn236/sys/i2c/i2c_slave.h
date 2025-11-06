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

#pragma once

#include <array>
#include <fsl_lpi2c.h>

#include "nv/i2c/port.h"

#include NV_IPC_CONFIG_H

#ifndef NUM_I2C_TARGET_ADDRESSES
#define NUM_I2C_TARGET_ADDRESSES 16
#endif

namespace sys::i2c {

// Driver Class:
/*
Acts as an I2C Slave on the given I2C bus pins
Abstracts the clock stretching and buffer logic on the NXP I2C HW API

This driver read and write requests are triggered to the master task with callbacks.
    There is only a single buffer so the master task must service the data in the callback
    as the buffer can be overwritten at any time after the callback
Driver will clock stretch for the duration of the callback ensuring no buffer overwriting until
the data is serviced
*/
// -----------------------------------------------------------------------------------------

class I2CSlaveDriver
{
public:
    // Constants
    // ---------------------------------------------

    // Number of I2C target addresses
    constexpr static size_t NumI2cTargetAddresses = static_cast<size_t>(
        NUM_I2C_TARGET_ADDRESSES);

    // Size of the I2C data buffers (RX/TX). Increase this if transactions involve more data
    constexpr static size_t BufferSize = 34;

    // Callback Types
    // --------------------

    // Will call this function on its master task when there is an I2C write request
    // Parent task must service the buffer data during this function call since it will be
    // overwritten by next I2C transaction Cannot block this driver or will be stuck forever
    typedef void (*process_data_callback_t)(
        uint8_t&                         address,    // Address of the I2C transaction
        bool                             is_read,    // R/W Bit
        std::array<uint8_t, BufferSize>& buffer,     // Buffer for sending/receiving
        size_t&                          data_size,  // Size of the send/receive data
        void*                            task,  // Context for parent task managing this driver
        bool new_transaction  // Indicates a new transaction started (not repeated read ack)
    );

    // Configuration for driver initialization
    struct Config
    {
        nv::i2c::Port           i2c_bus;                // I2C hardware peripheral
        process_data_callback_t process_data_callback;  // Invoked on read requests for transmit
                                                        // data
        std::array<uint8_t, NumI2cTargetAddresses> target_addresses;  // Target addresses to
                                                                      // ACK/NACK
        void* parent_task_class;  // Parent task class for running callbacks
    };

    // Public Functions
    // ---------------------------------------------

    // Constructor: Initializes the I2C driver with the given configuration
    I2CSlaveDriver(Config& driver_config);
    I2CSlaveDriver();

    // Starts the I2C slave driver. Must be called after initialization
    void start();

    // Callback from HW interrups to service data
    // Do Not Use: Reserved for NXP HW API callback and testing.
    static void callback(LPI2C_Type* base, lpi2c_slave_transfer_t* transfer, void* user_data);

private:
    // Internal Constants
    // ---------------------------------------------

    // Internal - Used for tracking the state of the driver in callbacks
    enum DriverState
    {
        Init,
        Idle,
        Nacking,
        Receiving,
        Transmitting
    };

    constexpr static uint32_t EventMask =  // Event mask for interrupt service routines
        kLPI2C_SlaveTransmitEvent | kLPI2C_SlaveReceiveEvent | kLPI2C_SlaveCompletionEvent
        | kLPI2C_SlaveTransmitAckEvent;

    // Helper Functions
    // ---------------------------------------------

    // Returns the hardware base address for the specified I2C port
    static LPI2C_Type* get_base(nv::i2c::Port port);

    // Handles HW Interrupts
    static void irq_handler(uint32_t instance, void* handle);

    // Services read request data from the I2C master
    void service_tx_request(lpi2c_slave_transfer_t* transfer);

    // Provides the buffer for incoming I2C transactions
    void service_rx_buffer_request(lpi2c_slave_transfer_t* transfer);

    // Services write request data from the I2C master
    void service_rx_data(lpi2c_slave_transfer_t* transfer);

    // decides whether to send an ACK or not
    void send_ack_or_nack(lpi2c_slave_transfer_t* transfer);

    // Checks if the address is a target address to ACK/NACK
    bool is_target_address(const uint8_t address);

    // Member Variables
    // ---------------------------------------------

    // Target addresses to ACK/NACK
    std::array<uint8_t, NumI2cTargetAddresses> _target_addresses;

    // Hardware Config Settings
    nv::i2c::Port        _i2c_bus;    // I2C hardware port
    lpi2c_slave_handle_t _handle;     // HW slave handle structure
    LPI2C_Type*          _base_addr;  // Base address for the I2C hardware

    // Internal buffers for data transfer
    uint8_t                         received_address;
    std::array<uint8_t, BufferSize> _rx_buffer;  // Receive buffer for incoming data
    std::array<uint8_t, BufferSize> _tx_buffer;  // Transmit buffer for outgoing data
    size_t                          _rx_buffer_transfer_size;  // Size of the last received data
    size_t                          _tx_buffer_transfer_size;  // Size of the transmit data

    uint8_t _rx_addr;  // Address from the last received transaction

    // Parent task callbacks
    void*                   _parent_task_class;      // Parent task class for callback execution
    process_data_callback_t _process_data_callback;  // Callback to master task to service data

    DriverState _task_state;  // Current state of the I2C driver
};
}  // namespace sys::i2c
