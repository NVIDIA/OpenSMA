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

#include "sys/i2c/i2c_slave.h"

#include "nv/common/debug.h"
#include "nv/common/utils.h"
#include "nv/logger/log.h"
#include "nv/nv.h"

using namespace nv;
namespace sys::i2c {

I2CSlaveDriver::I2CSlaveDriver(Config& driver_config)
: _target_addresses(driver_config.target_addresses)
, _i2c_bus(driver_config.i2c_bus)
, _handle()
, _base_addr(get_base(_i2c_bus))
, received_address(0x00)
, _rx_buffer{}
, _tx_buffer{}
, _rx_buffer_transfer_size(0U)
, _tx_buffer_transfer_size(0U)
, _rx_addr(0U)
, _parent_task_class(driver_config.parent_task_class)
, _process_data_callback(driver_config.process_data_callback)
, _task_state(Init)
{
    nv::info("Initializing I2C Bus %d Slave Driver\n", _i2c_bus);
    LPI2C_SlaveTransferCreateHandle(_base_addr, &_handle, callback, this);

    LP_FLEXCOMM_SetIRQHandler(
        LPI2C_GetInstance(_base_addr), irq_handler, &_handle, LP_FLEXCOMM_PERIPH_LPI2C);
}

I2CSlaveDriver::I2CSlaveDriver()
: _target_addresses{}
, _i2c_bus()
, _handle()
, _base_addr()
, received_address()
, _rx_buffer{}
, _tx_buffer{}
, _rx_buffer_transfer_size()
, _tx_buffer_transfer_size()
, _rx_addr()
, _parent_task_class()
, _process_data_callback()
, _task_state()
{
    //
}

void I2CSlaveDriver::start()
{
    nv::info("Starting I2C Bus %d Slave Driver\n", _i2c_bus);
    LPI2C_SlaveEnable(_base_addr, true);
    const status_t Status = LPI2C_SlaveTransferNonBlocking(_base_addr, &_handle, EventMask);
    if (Status != kStatus_Success) {
        nv::error("fail to start i2c bus: %d", _i2c_bus);
        return;
    }
    _task_state = Idle;
}

void I2CSlaveDriver::callback([[maybe_unused]] LPI2C_Type* base,
                              lpi2c_slave_transfer_t*      transfer,
                              void*                        user_data)
{
    // NOLINTNEXTLINE: nxp api name
    // nv::info("i2c callback. addr:%d event:%d\n", transfer->receivedAddress, transfer->event);

    auto* this_driver_instance = static_cast<I2CSlaveDriver*>(user_data);
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
        case kLPI2C_SlaveCompletionEvent:
            if (transfer->completionStatus != kStatus_Success) {
                nv::error("I2C driver got status: %d on i2c bus: %d in state: %d\n",
                          transfer->completionStatus,
                          this_driver_instance->_i2c_bus,
                          this_driver_instance->_task_state);
            }
            else if (this_driver_instance->_task_state == Receiving) {
                this_driver_instance->service_rx_data(transfer);
            }
            this_driver_instance->_task_state = Idle;
            break;
        default:
            nv::error("Unexpected/Invalid I2C slave event: %d on bus: %d\n",
                      transfer->event,
                      this_driver_instance->_i2c_bus);
            LPI2C_SlaveTransmitAck(base, false);
            this_driver_instance->_task_state = Idle;
            break;
    }
}

bool I2CSlaveDriver::is_target_address(const uint8_t address)
{
    for (const auto& target_address : _target_addresses) {
        if (target_address == address) {
            return true;
        }
    }
    return false;
}

void I2CSlaveDriver::service_tx_request(lpi2c_slave_transfer_t* transfer)
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

        _process_data_callback(
            address, true, _tx_buffer, _tx_buffer_transfer_size, _parent_task_class, was_idle);

        transfer->data     = _tx_buffer.data();
        transfer->dataSize = _tx_buffer_transfer_size;
        // nv::info("sending size: %d byte1: %x\n", transfer->dataSize, transfer->data[0]);
    }
}

void I2CSlaveDriver::service_rx_buffer_request(lpi2c_slave_transfer_t* transfer)
{
    // nv::info("driver rx buffer req\n");

    // NOLINTNEXTLINE: nxp api name
    const uint8_t Address = transfer->receivedAddress >> 1U;  //  remove r/w bit
    // save address for later since it gets removed when the receive is done for some reason...
    _rx_addr = Address;

    _task_state = Receiving;

    transfer->data     = _rx_buffer.data();
    transfer->dataSize = _rx_buffer.size();
}

void I2CSlaveDriver::service_rx_data(lpi2c_slave_transfer_t* transfer)
{
    // nv::info("driver read req\n");
    _process_data_callback(
        _rx_addr, false, _rx_buffer, transfer->transferredCount, _parent_task_class, true);
}

void I2CSlaveDriver::send_ack_or_nack(lpi2c_slave_transfer_t* transfer)
{
    // nv::info("driver ack req\n");
    switch (_task_state) {
        case Init: {
            // should not be here
            nv::error("Received address ack callback during driver init\n");
            LPI2C_SlaveTransmitAck(_base_addr, false);
            _task_state = Nacking;
            break;
        }
        case Idle: {
            // must be receiving address
            // NOLINTNEXTLINE: nxp api name
            uint8_t    address      = transfer->receivedAddress >> 1U;  // remove r/w bit
            const bool ValidAddress = is_target_address(address);
            LPI2C_SlaveTransmitAck(_base_addr, ValidAddress);
            if (!ValidAddress) {
                _task_state = Nacking;
            }
            break;
        }
        case Nacking: {
            nv::error("having to nack after already nacked\n");
            LPI2C_SlaveTransmitAck(_base_addr, false);
            _task_state = Nacking;
            break;
        }
        case Receiving: {
            // always send ACK
            _process_data_callback(_rx_addr,
                                   false,
                                   _rx_buffer,
                                   transfer->transferredCount,
                                   _parent_task_class,
                                   true);
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
            nv::error("Invalid i2c driver state (%d) during address ACK callback\n", _i2c_bus);
            LPI2C_SlaveTransmitAck(_base_addr, false);
            _task_state = Nacking;
            break;
        }
    }
}

void I2CSlaveDriver::irq_handler(uint32_t instance, void* handle)
{
    NV_ASSERT(instance < nv::common::to_underlying(nv::i2c::Port::End));
    NV_ASSERT(handle != nullptr);
    auto base = get_base(static_cast<nv::i2c::Port>(instance));
    if ((base->SCR & LPI2C_SCR_SEN_MASK) != 0U) {
        LPI2C_SlaveTransferHandleIRQ(instance, handle);
    }
}

LPI2C_Type* I2CSlaveDriver::get_base(nv::i2c::Port port)
{
    constexpr uint8_t Size = nv::common::to_underlying(nv::i2c::Port::End);
    // NOLINTNEXTLINE: SDK definition
    std::array<LPI2C_Type*, Size> bases LPI2C_BASE_PTRS;
    return bases.at(nv::common::to_underlying(port));
}

}  // namespace sys::i2c
