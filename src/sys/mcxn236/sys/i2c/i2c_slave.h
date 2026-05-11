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

#include "nv/common/debug.h"
#include "nv/common/utils.h"
#include "nv/ctimer/ctimer.h"
#include "nv/i2c/port.h"
#include "nv/logger/log.h"
#include "nv/nv.h"
#include "sys/i2c/utils.h"

#include NV_IPC_CONFIG_H

#ifndef NUM_I2C_TARGET_ADDRESSES
#define NUM_I2C_TARGET_ADDRESSES 16
#endif

namespace sys::i2c {

// Number of I2C target addresses
constexpr static size_t NumI2cTargetAddresses = static_cast<size_t>(NUM_I2C_TARGET_ADDRESSES);

// Size of the I2C data buffers (RX/TX). Increase this if transactions involve more data
constexpr static size_t I2cSlaveBufferSize = 35;

using I2cSlaveBuffer = std::array<uint8_t, I2cSlaveBufferSize>;

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

template<typename T>
class I2CSlaveDriver
{
public:
    // Public Functions
    // ---------------------------------------------
    I2CSlaveDriver() = default;

    // Starts the I2C slave driver. Must be called after initialization
    void start()
    {
        if (_task_state == Idle) {
            // Already started
            return;
        }

        nv::info("Starting I2C Bus %d Slave Driver\n", _i2c_bus);
        LPI2C_SlaveEnable(_base_addr, true);
        const status_t Status = LPI2C_SlaveTransferNonBlocking(_base_addr, &_handle, EventMask);
        if (Status != kStatus_Success) {
            nv::error("fail to start i2c bus: %d", _i2c_bus);
            return;
        }
        _task_state           = Idle;
        _target_timeout_timer = 0;
    }

    // Binds the driver, for cases where driver config is not used
    void bind(nv::i2c::Port                              port,
              std::array<uint8_t, NumI2cTargetAddresses> target_addresses,
              T*                                         parent)
    {
        _i2c_bus          = port;
        _target_addresses = target_addresses;
        _parent           = parent;
        _base_addr        = get_base(port);

        LPI2C_SlaveTransferCreateHandle(_base_addr, &_handle, callback, this);
    }

    // Callback from HW interrups to service data
    static void callback([[maybe_unused]] LPI2C_Type* base,
                         lpi2c_slave_transfer_t*      transfer,
                         void*                        user_data)
    {
        // NOLINTNEXTLINE: nxp api name
        auto* this_driver_instance = static_cast<I2CSlaveDriver*>(user_data);
        this_driver_instance->update_target_state_time_stamp(transfer->event);
        switch (transfer->event) {
            case kLPI2C_SlaveTransmitEvent:
                this_driver_instance->service_tx_request(transfer);
                break;
            case kLPI2C_SlaveReceiveEvent:
                this_driver_instance->service_rx_buffer_request(transfer);
                break;
            case kLPI2C_SlaveTransmitAckEvent:
                //
                this_driver_instance->send_ack_or_nack(transfer);
                break;
            case kLPI2C_SlaveRepeatedStartEvent:
            case kLPI2C_SlaveCompletionEvent:
                if (transfer->completionStatus != kStatus_Success) {
                    nv::logger::Logger::add_from_isr(
                        nv::logger::Event::I2CSlaveDriverError.unique_id,
                        nv::logger::Level::Error,
                        {static_cast<uint8_t>(transfer->completionStatus),
                         static_cast<uint8_t>(this_driver_instance->_i2c_bus),
                         static_cast<uint8_t>(this_driver_instance->_task_state)});
                }
                else if (this_driver_instance->_task_state == Receiving) {
                    this_driver_instance->service_rx_data(transfer);
                }
                this_driver_instance->_task_state = Idle;
                transfer->data                    = nullptr;
                transfer->dataSize                = 0;
                break;
            default:
                nv::logger::Logger::add_from_isr(
                    nv::logger::Event::I2CSlaveUnexpectedEvent.unique_id,
                    nv::logger::Level::Error,
                    {static_cast<uint8_t>(transfer->event),
                     static_cast<uint8_t>(this_driver_instance->_i2c_bus)});
                LPI2C_SlaveTransmitAck(base, false);
                this_driver_instance->_task_state = Idle;
                break;
        }
    }

    sys::ctimer::Ticks get_target_timeout_elapsed()
    {
        // Read the target timeout timer atomically
        const auto target_timeout_timer = _target_timeout_timer;
        if (target_timeout_timer == 0) {
            return 0;
        }
        return sys::ctimer::Driver::get_counter_difference(
            target_timeout_timer, nv::ctimer::Driver::read_ticks_inline());
    }

    void peripheral_recovery() {}

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
        | kLPI2C_SlaveTransmitAckEvent | kLPI2C_SlaveRepeatedStartEvent;

    // Helper Functions
    // ---------------------------------------------

    // Returns the hardware base address for the specified I2C port
    static LPI2C_Type* get_base(nv::i2c::Port port)
    {
        constexpr uint8_t Size = nv::common::to_underlying(nv::i2c::Port::End);
        // NOLINTNEXTLINE: SDK definition
        std::array<LPI2C_Type*, Size> bases LPI2C_BASE_PTRS;
        return bases.at(nv::common::to_underlying(port));
    }

    void service_tx_request(lpi2c_slave_transfer_t* transfer)
    {
        if (_task_state == Nacking) {
            LPI2C_SlaveTransmitAck(_base_addr, false);
        }
        else {
            // Store current state before changing it for the callback
            const bool was_idle = (_task_state == Idle);

            _task_state = Transmitting;

            // NOLINTNEXTLINE: nxp api name
            uint8_t address          = transfer->receivedAddress >> 1U;  // remove r/w bit
            _tx_buffer_transfer_size = 0;

            _parent->i2c_callback(
                address, true, _tx_buffer, _tx_buffer_transfer_size, _parent, was_idle);
        }

        transfer->data     = _tx_buffer.data();
        transfer->dataSize = _tx_buffer_transfer_size;
    }

    // Provides the buffer for incoming I2C transactions
    void service_rx_buffer_request(lpi2c_slave_transfer_t* transfer)
    {
        // NOLINTNEXTLINE: nxp api name
        const uint8_t Address = transfer->receivedAddress >> 1U;  //  remove r/w bit
        // save address for later since it gets removed when the receive is done for some
        // reason...
        _rx_addr = Address;

        _task_state = Receiving;

        transfer->data     = _rx_buffer.data();
        transfer->dataSize = _rx_buffer.size();
    }

    // Services write request data from the I2C master
    void service_rx_data(lpi2c_slave_transfer_t* transfer)
    {
        _parent->i2c_callback(
            _rx_addr, false, _rx_buffer, transfer->transferredCount, _parent, true);
    }

    // decides whether to send an ACK or not
    void send_ack_or_nack(lpi2c_slave_transfer_t* transfer)
    {
        switch (_task_state) {
            case Init: {
                // should not be here
                nv::logger::Logger::add_from_isr(
                    nv::logger::Event::I2CSlaveAckDuringInit.unique_id,
                    nv::logger::Level::Error,
                    {});
                LPI2C_SlaveTransmitAck(_base_addr, false);
                _task_state = Nacking;
                break;
            }
            case Idle: {
                // must be receiving address
                // NOLINTNEXTLINE: nxp api name
                uint8_t    address  = transfer->receivedAddress >> 1U;  // remove r/w bit
                const bool is_read  = transfer->receivedAddress & 1U;
                bool       ack_nack = is_target_address(address);

                if constexpr (requires(T* p, uint8_t addr, bool read) {
                                  p->i2c_ack_callback(addr, read, p);
                              }) {
                    // Only used for SSIF where address ACK/NACK is part of higher layer
                    // protocol
                    if (ack_nack && !_parent->i2c_ack_callback(address, is_read, _parent)) {
                        ack_nack = false;
                    }
                }

                LPI2C_SlaveTransmitAck(_base_addr, ack_nack);
                if (!ack_nack) {
                    // Clear TX data in case NACK is not transmitted and/or master attempts read
                    // despite NACK
                    _tx_buffer.fill(0);
                    _task_state = Nacking;
                }
                break;
            }
            case Nacking: {
                nv::logger::Logger::add_from_isr(
                    nv::logger::Event::I2CSlaveDoubleNack.unique_id,
                    nv::logger::Level::Error,
                    {});
                LPI2C_SlaveTransmitAck(_base_addr, false);
                _task_state = Nacking;
                break;
            }
            case Receiving: {
                // always send ACK
                _parent->i2c_callback(
                    _rx_addr, false, _rx_buffer, transfer->transferredCount, _parent, true);
                LPI2C_SlaveTransmitAck(_base_addr, true);
                if (transfer->receivedAddress & 0x01U) {
                    service_tx_request(transfer);
                }
                break;
            }
            case Transmitting: {
                // always send ACK
                LPI2C_SlaveTransmitAck(_base_addr, true);
                break;
            }
            default: {
                nv::logger::Logger::add_from_isr(
                    nv::logger::Event::I2CSlaveInvalidState.unique_id,
                    nv::logger::Level::Error,
                    {static_cast<uint8_t>(_i2c_bus)});
                LPI2C_SlaveTransmitAck(_base_addr, false);
                _task_state = Nacking;
                break;
            }
        }
    }

    void update_target_state_time_stamp(lpi2c_slave_transfer_event_t event)
    {
        switch (event) {
            case kLPI2C_SlaveCompletionEvent:
            case kLPI2C_SlaveRepeatedStartEvent: _target_timeout_timer = 0; break;
            case kLPI2C_SlaveTransmitAckEvent  :
            case kLPI2C_SlaveTransmitEvent     :
            case kLPI2C_SlaveReceiveEvent      :
            default                            : _target_timeout_timer = nv::ctimer::Driver::read_ticks_inline(); break;
        }
    }

    // Checks if the address is a target address to ACK/NACK
    bool is_target_address(const uint8_t address)
    {
        for (const auto& target_address : _target_addresses) {
            if (target_address == address) {
                return true;
            }
        }
        return false;
    }

    // Member Variables
    // ---------------------------------------------

    // Target addresses to ACK/NACK
    std::array<uint8_t, NumI2cTargetAddresses> _target_addresses;

    // Hardware Config Settings
    nv::i2c::Port        _i2c_bus;    // I2C hardware port
    lpi2c_slave_handle_t _handle;     // HW slave handle structure
    LPI2C_Type*          _base_addr;  // Base address for the I2C hardware

    // Internal buffers for data transfer
    uint8_t        received_address;
    I2cSlaveBuffer _rx_buffer;                // Receive buffer for incoming data
    I2cSlaveBuffer _tx_buffer;                // Transmit buffer for outgoing data
    size_t         _rx_buffer_transfer_size;  // Size of the last received data
    size_t         _tx_buffer_transfer_size;  // Size of the transmit data

    uint8_t _rx_addr;  // Address from the last received transaction

    // Parent is the higher layer protocol driver, expected to implement
    // - i2c_callback - for TX/RX data processing
    // - i2c_ack_callback  - for ACK/NACK processing
    T* _parent;

    DriverState _task_state;  // Current state of the I2C driver

    volatile sys::ctimer::Ticks _target_timeout_timer{};
};
}  // namespace sys::i2c
