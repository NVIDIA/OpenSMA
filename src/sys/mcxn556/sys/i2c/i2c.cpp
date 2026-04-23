/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES.
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
#include "sys/i2c/i2c.h"

#include "fsl_clock.h"

#include "nv/i2c/mutex.h"
#include "nv/i2c/task.h"
#include "nv/i2c/smb_direct.h"
#include "nv/ipc/mutex.h"
#include "nv/i2c/eeprom_cache.h"
#include "nv/logger/log.h"
#include "nv/logger/task.h"
#include "sys/i2c/utils.h"

#include "nv/nv.h"
#include "nv/ipc/supervisor.h"

using namespace nv;
using namespace sys::i2c;

namespace sys::i2c {

void download_log_in_isr(const uint8_t CurrentSession, I2cBuffer& log_buffer_data)
{
    nv::logger::Dlreq       res_log{};
    const nv::logger::Dlreq req_log{.session = CurrentSession};

    auto& task = nv::ipc::Supervisor::inst().task(nv::ipc::TaskId::Logger);
    // NOLINTNEXTLINE(*-reinterpret-cast)
    auto& logger_task = reinterpret_cast<nv::logger::Task&>(task);
    logger_task.get_logger_obj().process_download(req_log, res_log);

    log_buffer_data[0] = res_log.session;
    log_buffer_data[1] = res_log.size;
    memcpy(&log_buffer_data[2], std::bit_cast<uint8_t*>(res_log.data.data()), res_log.size);
}

SlaveFunction get_slave_function(nv::i2c::Port port, uint8_t address)
{
    // Lookup in slave function table
    for (const auto& entry : nv::ipc::SlaveFunctionTable) {
        if (entry.enabled && entry.port == port && entry.address == address) {
            return entry.function;
        }
    }
    // Default: MCTP/SMBus handling
    return SlaveFunction::Mctp;
}

/// EEPROM slave callback - handles all EEPROM cache operations
void Driver::target_callback_eeprom([[maybe_unused]] LPI2C_Type* base,
                                    lpi2c_slave_transfer_t*      transfer,
                                    void*                        user_data)
{
    using namespace nv::i2c;
    auto  context = static_cast<TargetContex*>(user_data);
    auto& task    = nv::ipc::Supervisor::inst().task(nv::ipc::EepromTaskId);
    // NOLINTNEXTLINE(*-reinterpret-cast)
    auto& eeprom_task = reinterpret_cast<nv::i2c::Task&>(task);
    auto& cache       = eeprom_task.eeprom_cache();

    const std::span<uint8_t> Buffer = context->buffer;

    switch (transfer->event) {
        case kLPI2C_SlaveAddressMatchEvent:
            transfer->data     = nullptr;
            transfer->dataSize = 0;
            if (transfer->receivedAddress & 0x01U) {
                if (cache.use_2byte_addr()) {
                    const uint16_t eeprom_addr = (static_cast<uint16_t>(Buffer[2]) << 8)
                                               | Buffer[3];
                    cache.set_addr_ptr(eeprom_addr);
                }
                else {
                    cache.set_addr_ptr(Buffer[2]);
                }
            }
            break;
        case kLPI2C_SlaveTransmitEvent: {
            context->transmit = true;
            Buffer[0]         = cache.read(cache.get_addr_ptr());
            cache.inc_addr_ptr();
            transfer->data     = Buffer.subspan(0, 1).data();
            transfer->dataSize = 1;
            break;
        }
        case kLPI2C_SlaveReceiveEvent:
            context->transmit  = false;
            transfer->data     = Buffer.subspan(2).data();
            transfer->dataSize = Buffer.subspan(2).size();
            break;
        case kLPI2C_SlaveCompletionEvent:
            if (!context->transmit && transfer->transferredCount > 0) {
                const uint8_t AddrBytes = cache.use_2byte_addr() ? 2 : 1;
                if (transfer->transferredCount >= AddrBytes) {
                    uint16_t eeprom_addr = 0;
                    if (cache.use_2byte_addr()) {
                        eeprom_addr = (static_cast<uint16_t>(Buffer[2]) << 8) | Buffer[3];
                    }
                    else {
                        eeprom_addr = Buffer[2];
                    }
                    cache.set_addr_ptr(eeprom_addr);
                    for (size_t i = AddrBytes; i < transfer->transferredCount; i++) {
                        cache.write(cache.get_addr_ptr(), Buffer[2 + i]);
                        cache.inc_addr_ptr();
                    }
                }
            }
            break;
        default: break;
    }
}

void Driver::target_callback_lookup(LPI2C_Type*             base,
                                    lpi2c_slave_transfer_t* transfer,
                                    void*                   user_data)
{
    auto context = static_cast<TargetContex*>(user_data);

    // Determine slave function on address match
    if (transfer->event == kLPI2C_SlaveAddressMatchEvent) {
        const auto address  = static_cast<uint8_t>(transfer->receivedAddress & 0xFF) >> 1U;
        const auto instance = LPI2C_GetInstance(base);
        NV_ASSERT(instance < nv::common::to_underlying(nv::i2c::Port::End));
        const auto port   = static_cast<nv::i2c::Port>(instance);
        context->function = get_slave_function(port, address);
    }

    // Dispatch to appropriate slave callback
    switch (context->function) {
        case SlaveFunction::Eeprom: target_callback_eeprom(base, transfer, user_data); break;
        case SlaveFunction::Mctp  :
        default                   : target_callback(base, transfer, user_data); break;
    }
}

}  // namespace sys::i2c

LPI2C_Type* Driver::get_base(nv::i2c::Port port)
{
    constexpr uint8_t Size = nv::common::to_underlying(nv::i2c::Port::End);
    // NOLINTNEXTLINE: SDK definition
    std::array<LPI2C_Type*, Size> bases LPI2C_BASE_PTRS;
    return bases.at(nv::common::to_underlying(port));
}

IRQn_Type Driver::get_irq(nv::i2c::Port port)
{
    constexpr uint8_t Size = nv::common::to_underlying(nv::i2c::Port::End);
    // NOLINTNEXTLINE: SDK definition
    std::array<IRQn_Type, Size> irqs LPI2C_IRQS;
    return irqs.at(nv::common::to_underlying(port));
}

void Driver::irq_handler(uint32_t instance, void* handle)
{
    NV_ASSERT(instance < nv::common::to_underlying(nv::i2c::Port::End));
    NV_ASSERT(handle != nullptr);
    auto base         = get_base(static_cast<nv::i2c::Port>(instance));
    auto lpi2c_handle = static_cast<Lpi2cHandle*>(handle);
    if ((base->MCR & LPI2C_MCR_MEN_MASK) != 0U) {
        LPI2C_MasterTransferHandleIRQ(instance, &lpi2c_handle->controller);
    }
    if ((base->SCR & LPI2C_SCR_SEN_MASK) != 0U) {
        LPI2C_SlaveTransferHandleIRQ(instance, &lpi2c_handle->target);
    }
}

void Driver::controller_callback([[maybe_unused]] LPI2C_Type*            base,
                                 [[maybe_unused]] lpi2c_master_handle_t* handle,
                                 status_t                                completion_status,
                                 void*                                   user_data)
{
    using namespace nv::i2c;
    auto context = static_cast<ControllerContex*>(user_data);
    auto task    = static_cast<Task*>(context->task);
    switch (completion_status) {
        case kStatus_Success              : task->set_event(Task::Event::CtrlDone); break;
        case kStatus_LPI2C_Nak            : task->set_event(Task::Event::CtrlNak); break;
        case kStatus_LPI2C_ArbitrationLost: task->set_event(Task::Event::CtrlArbLost); break;
        default:
            /// TODO should we enable pin low detect?
            task->set_event(Task::Event::CtrlError);
            break;
    }
}

void Driver::target_callback(LPI2C_Type*             base,
                             lpi2c_slave_transfer_t* transfer,
                             void*                   user_data)
{
    using namespace nv::i2c;
    constexpr uint8_t        DefaultData   = 0xFF;
    constexpr uint8_t        MinPacketSize = 4;
    auto                     context       = static_cast<TargetContex*>(user_data);
    auto                     task          = static_cast<Task*>(context->task);
    const std::span<uint8_t> Buffer        = context->buffer;
    switch (transfer->event) {
        case kLPI2C_SlaveAddressMatchEvent:
            transfer->data     = nullptr;
            transfer->dataSize = 0;
            break;
        case kLPI2C_SlaveTransmitEvent: {
            context->transmit  = true;
            transfer->data     = context->buffer.data();
            transfer->dataSize = context->buffer.size();

            auto status = false;
            if constexpr (nv::ipc::EnableSmbDirect) {
                status = nv::i2c::on_smbus_direct(context->buffer[2], context->buffer);
            }
            else {
                status = nv::i2c::vr_on_smbus_direct(context->buffer[2],
                                                     context->buffer,
                                                     nv::smb_telemetry::SmbDirect::get_cache());
            }

            if (!status) {
                const auto CmdCode = context->buffer[2];
                if (CmdCode == std::to_underlying(I2cCustomizeCommand::DumpLog)) {
                    const uint8_t RequestSession = context->buffer[3];
                    context->buffer.fill(0x0);
                    download_log_in_isr(RequestSession, context->buffer);
                }
                else {
                    context->buffer[2] = 0x0;
                    context->buffer[0] = DefaultData;
                }
            }
        } break;

        case kLPI2C_SlaveReceiveEvent:
            context->transmit  = false;
            transfer->data     = Buffer.subspan(2).data();
            transfer->dataSize = Buffer.subspan(2).size();
            break;
        case kLPI2C_SlaveCompletionEvent:
            if (transfer->completionStatus != kStatus_Success) {
                task->rx_error();
                break;
            }
            // do not process small transfers and SMBus reads
            if (!context->transmit && transfer->data != nullptr
                && transfer->transferredCount >= MinPacketSize) {
                // [0] = payload_size, [1] = 8 bit target address, [2:] payload
                context->buffer[0] = transfer->transferredCount + 1;
                context->buffer[1] = static_cast<uint8_t>(base->SAMR) | 0U;
                task->rx(context->buffer);
            }
            break;
        case kLPI2C_SlaveRepeatedStartEvent:
            // TODO the driver call AddressMatchEvent instead of RepeatedStartEvent. Is it a
            // BUG?
            break;
        default: nv::error("unknown event %d\n", transfer->event); break;
    }
}

void Driver::bind(nv::i2c::Port port, void* task)
{
    NV_ASSERT(task != nullptr);
    auto i2c_task = static_cast<nv::i2c::Task*>(task);
    nv::info("task %s binds to I2C port %d\n", i2c_task->name().data(), port);
    logger::info(logger::Event::I2CBind,
                 {static_cast<uint8_t>(i2c_task->id()), static_cast<uint8_t>(port)});
    _port                    = port;
    _base                    = get_base(port);
    _controller_context.task = task;
    _target_context.task     = task;

    _peripheral_clk_hz = CLOCK_GetLPFlexCommClkFreq(LPI2C_GetInstance(_base));
    if (_peripheral_clk_hz == 0) {
        nv::error("fail to get clock frequency\n");
        _peripheral_clk_hz = 25000000UL;
    }
}

void Driver::init()
{
    LPI2C_MasterTransferCreateHandle(
        _base, &_lpi2c_handle.controller, controller_callback, &_controller_context);
    if (nv::ipc::SlaveFunctionTableSize > 0) {
        LPI2C_SlaveTransferCreateHandle(
            _base, &_lpi2c_handle.target, target_callback_lookup, &_target_context);
    }
    else {
        LPI2C_SlaveTransferCreateHandle(
            _base, &_lpi2c_handle.target, target_callback, &_target_context);
    }

    LP_FLEXCOMM_SetIRQHandler(
        LPI2C_GetInstance(_base), irq_handler, &_lpi2c_handle, LP_FLEXCOMM_PERIPH_LPI2C);
}

void Driver::start(bool enable_target)
{
    LPI2C_MasterEnable(_base, true);
    if (enable_target) {
        LPI2C_SlaveEnable(_base, true);
        constexpr uint32_t Mask   = kLPI2C_SlaveAddressMatchEvent | kLPI2C_SlaveCompletionEvent;
        const status_t     Status = LPI2C_SlaveTransferNonBlocking(
            _base, &_lpi2c_handle.target, Mask);
        if (Status != kStatus_Success) {
            nv::error("fail to start target\n");
        }
    }
}

void Driver::peripheral_recovery(bool enable_target)
{
    auto& mutex = nv::ipc::Mutex::make(nv::i2c::port_to_mutex_id(_port));
    if (mutex.lock() != nv::ipc::Mutex::Status::Ok) {
        return;
    }

    constexpr uint8_t I2CRecoveryLogReport = 0;
    constexpr uint8_t I2CRecoveryLogResult = 1;
    nv::logger::info(
        nv::logger::Event::I2CSlaveRecovery,
        {I2CRecoveryLogReport,
         static_cast<uint8_t>(nv::common::to_underlying(_port) & 0xFFU),
         static_cast<uint8_t>(_lpi2c_handle.target.isBusy ? 1 : 0),
         static_cast<uint8_t>(_lpi2c_handle.target.transferredCount & 0xFFU),
         static_cast<uint8_t>(_lpi2c_handle.target.transfer.event & 0xFFU),
         static_cast<uint8_t>(_lpi2c_handle.target.transfer.receivedAddress & 0xFFU),
         static_cast<uint8_t>(_lpi2c_handle.target.transfer.completionStatus & 0xFFU)});

    // Abort current transfer and disable slave
    LPI2C_SlaveTransferAbort(_base, &_lpi2c_handle.target);
    LPI2C_SlaveEnable(_base, false);

    // Retrive slave configuration from registers, reset, and re-init slave
    lpi2c_slave_config_t slave_config{};
    slave_config_fill_from_registers(_base, _peripheral_clk_hz, slave_config);
    LPI2C_SlaveInit(_base, &slave_config, _peripheral_clk_hz);

    // Enable slave and interrupts and arm for next transfer
    LPI2C_SlaveEnable(_base, true);
    constexpr uint32_t Mask = kLPI2C_SlaveAddressMatchEvent | kLPI2C_SlaveCompletionEvent;
    const status_t Status = LPI2C_SlaveTransferNonBlocking(_base, &_lpi2c_handle.target, Mask);
    nv::logger::info(
        nv::logger::Event::I2CSlaveRecovery,
        {I2CRecoveryLogResult,
         static_cast<uint8_t>(nv::common::to_underlying(_port) & 0xFFU),
         static_cast<uint8_t>(_lpi2c_handle.target.isBusy ? 1 : 0),
         static_cast<uint8_t>(_lpi2c_handle.target.transferredCount & 0xFFU),
         static_cast<uint8_t>(_lpi2c_handle.target.transfer.event & 0xFFU),
         static_cast<uint8_t>(_lpi2c_handle.target.transfer.receivedAddress & 0xFFU),
         static_cast<uint8_t>(static_cast<uint32_t>(Status) & 0xFFU),
         static_cast<uint8_t>(static_cast<uint32_t>(Status) >> 8U & 0xFFU)});

    mutex.unlock();
}

bool Driver::write(std::span<uint8_t> data)
{
    std::copy(data.begin(), data.end(), _controller_context.buffer.begin());
    lpi2c_master_transfer_t transfer = {
        .flags          = kLPI2C_TransferDefaultFlag,
        .slaveAddress   = static_cast<uint8_t>(_controller_context.buffer[0] >> 1U),
        .direction      = kLPI2C_Write,
        .subaddress     = 0,
        .subaddressSize = 0,
        .data           = &_controller_context.buffer.data()[1],
        .dataSize       = data.size() - 1,
    };

    auto& mutex = nv::ipc::Mutex::make(nv::i2c::port_to_mutex_id(_port));
    // Acquire mutex
    auto mutex_status = mutex.lock();
    if (mutex_status != nv::ipc::Mutex::Status::Ok) {
        return false;
    }
    const status_t Status = LPI2C_MasterTransferNonBlocking(
        _base, &_lpi2c_handle.controller, &transfer);
    mutex.unlock();
    return Status == kStatus_Success;
}

bool Driver::get_status(uint8_t address)
{
    lpi2c_master_transfer_t transfer = {
        .flags          = kLPI2C_TransferDefaultFlag,
        .slaveAddress   = address,
        .direction      = kLPI2C_Write,
        .subaddress     = 0,
        .subaddressSize = 0,
        .data           = nullptr,
        .dataSize       = 0,
    };

    auto& mutex = nv::ipc::Mutex::make(nv::i2c::port_to_mutex_id(_port));
    // Acquire mutex
    auto mutex_status = mutex.lock();
    if (mutex_status != nv::ipc::Mutex::Status::Ok) {
        return false;
    }
    const status_t Status = LPI2C_MasterTransferBlocking(_base, &transfer);
    mutex.unlock();
    return Status == kStatus_Success;
}

uint8_t Driver::address()
{
    return static_cast<uint8_t>(_base->SAMR) >> 1U;
}

void Driver::set_address(nv::i2c::Port port, uint8_t address)
{
    auto base  = get_base(port);
    base->SAMR = static_cast<uint8_t>(address) << 1U;
    logger::info(logger::Event::I2CAddressUpdate,
                 {static_cast<uint8_t>(port), static_cast<uint8_t>(address)});
}

i2c::I2cStatus
Driver::i2c_read(uint8_t address, std::span<uint8_t> buffer, nv::i2c::I2cFlags flags)
{
    uint32_t lpi2c_flags = kLPI2C_TransferDefaultFlag;

    if (flags & nv::i2c::I2cFlags::RecvLen) {
        lpi2c_flags |= kLPI2C_TransferNoStartFlag;
    }

    if (flags & nv::i2c::I2cFlags::NoStop) {
        lpi2c_flags |= kLPI2C_TransferNoStopFlag;
    }

    lpi2c_master_transfer_t xfer{
        .flags        = lpi2c_flags,
        .slaveAddress = address,
        .direction    = kLPI2C_Read,
        .data         = buffer.data(),
        .dataSize     = (flags & nv::i2c::I2cFlags::QuickRead) ? 0 : buffer.size(),
    };

    const status_t status = LPI2C_MasterTransferNonBlocking(
        _base, &_lpi2c_handle.controller, &xfer);
    return sys::i2c::get_status(status);
}

i2c::I2cStatus
Driver::i2c_write(uint8_t address, std::span<uint8_t> buffer, nv::i2c::I2cFlags flags)
{
    lpi2c_master_transfer_t xfer{
        .flags        = (flags & nv::i2c::I2cFlags::NoStop) ? kLPI2C_TransferNoStopFlag
                                                            : kLPI2C_TransferDefaultFlag,
        .slaveAddress = address,
        .direction    = kLPI2C_Write,
        .data         = buffer.data(),
        .dataSize     = (flags & nv::i2c::I2cFlags::QuickWrite) ? 0 : buffer.size(),
    };

    const status_t status = LPI2C_MasterTransferNonBlocking(
        _base, &_lpi2c_handle.controller, &xfer);
    return sys::i2c::get_status(status);
}
