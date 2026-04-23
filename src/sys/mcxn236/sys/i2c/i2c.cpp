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
#include "sys/i2c/i2c.h"

#include <array>
#include <cstring>
#include <optional>
#include <ranges>

#include "fsl_debug_console.h"

#include "nv/i2c/task.h"
#include "nv/i2c/smb_direct.h"
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
        case kLPI2C_SlaveTransmitEvent:
            context->transmit  = true;
            transfer->data     = context->buffer.data();
            transfer->dataSize = context->buffer.size();
            if (nv::i2c::on_smbus_direct(context->buffer[2], context->buffer)) {}
            else {
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
            break;
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
    _base                    = get_base(port);
    _controller_context.task = task;
    _target_context.task     = task;
}

void Driver::init()
{
    LPI2C_MasterTransferCreateHandle(
        _base, &_lpi2c_handle.controller, controller_callback, &_controller_context);
    LPI2C_SlaveTransferCreateHandle(
        _base, &_lpi2c_handle.target, target_callback, &_target_context);
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
            return;
        }
    }
}

void Driver::peripheral_recovery([[maybe_unused]] bool enable_target) {}

bool Driver::write(std::span<uint8_t> data)
{
    std::copy(data.begin(), data.end(), _controller_context.buffer.begin());
    lpi2c_master_transfer_t transfer = {
        .flags        = kLPI2C_TransferDefaultFlag,
        .slaveAddress = static_cast<uint8_t>(_controller_context.buffer[0] >> 1U),
        .direction    = kLPI2C_Write,
        .data         = &_controller_context.buffer.data()[1],
        .dataSize     = data.size() - 1,
    };
    const status_t Status = LPI2C_MasterTransferNonBlocking(
        _base, &_lpi2c_handle.controller, &transfer);
    if (Status != kStatus_Success) {
        return false;
    }
    return true;
}

bool Driver::get_status(uint8_t address)
{
    lpi2c_master_transfer_t transfer = {
        .flags        = kLPI2C_TransferDefaultFlag,
        .slaveAddress = address,
        .direction    = kLPI2C_Write,
        .data         = nullptr,
        .dataSize     = 0,
    };
    const status_t Status = LPI2C_MasterTransferBlocking(_base, &transfer);
    if (Status != kStatus_Success) {
        return false;
    }
    return true;
}

uint8_t Driver::address()
{
    return static_cast<uint8_t>(_base->SAMR) >> 1U;
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
