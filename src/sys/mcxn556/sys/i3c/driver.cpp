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
#include "nv/i3c/driver.h"

#include <algorithm>
#include <cstring>
#include <fsl_debug_console.h>

#include "nv/i3c/task.h"
#include "nv/logger/log.h"
#include "nv/nv.h"
#include "fsl_i3c_edma.h"
#include "sys/common/common.h"
#include "sys/smartdma/driver.h"

namespace {

static void ibi_callback([[maybe_unused]] I3C_Type* base,
                         i3c_master_edma_handle_t*  handle,
                         i3c_ibi_type_t             ibi_type,
                         i3c_ibi_state_t            ibi_state)
{
    auto&         driver  = *static_cast<nv::i3c::Driver*>(handle->userData);
    const uint8_t port_id = static_cast<uint8_t>(I3C_GetInstance(base));
    if (ibi_state != kI3C_IbiReady) {
        nv::logger::Logger::add_from_isr(nv::logger::Event::I3CIgnoreIbiStateV2.unique_id,
                                         nv::logger::Level::Info,
                                         {port_id, static_cast<uint8_t>(ibi_state)});
        return;
    }
    switch (ibi_type) {
        case kI3C_IbiNormal: driver.on_ibi(static_cast<void*>(handle)); break;
        default:
            nv::logger::Logger::add_from_isr(nv::logger::Event::I3CIgnoreIbiTypeV2.unique_id,
                                             nv::logger::Level::Info,
                                             {port_id, static_cast<uint8_t>(ibi_type)});
            return;
    }
}

static void i3c_callback(I3C_Type*                 base,
                         i3c_master_edma_handle_t* handle,
                         status_t                  status,
                         [[maybe_unused]] void*    user_data)
{
    using namespace nv;
    auto& driver = *static_cast<i3c::Driver*>(handle->userData);
    switch (status) {
        case kStatus_Success:
            if (handle->transfer.direction == kI3C_Read) {
                EDMA_StopTransfer(handle->rxDmaHandle);
                size_t length = 0;
                I3C_MasterTransferGetCountEDMA(base, handle, &length);
                handle->transferCount = length;
                I3C_MasterGetFifoCounts(handle->base, &length, nullptr);
                while (length > 0) {
                    // coverity[cert_int31_c_violation] MRDATAB FIFO read one byte a time
                    static_cast<uint8_t*>(
                        handle->transfer.data)[handle->transferCount++] = handle->base->MRDATAB;
                    length--;
                }
                EDMA_AbortTransfer(handle->rxDmaHandle);
                if (handle->transfer.flags & kI3C_TransferNoStopFlag) {
                    (void)I3C_MasterStop(base);
                }
            }
            driver.set_event(i3c::Driver::Event::Success);
            break;
        case kStatus_I3C_Nak:
#if 0
            // Only log when 5 retry failed case
            logger::Logger::add_from_isr(logger::Event::I3CNackV2,
                                         logger::Level::Info,
                                         {static_cast<uint8_t>(I3C_GetInstance(base)),
                                          static_cast<uint8_t>(handle->state)});
#endif
            EDMA_AbortTransfer(handle->rxDmaHandle);

            /* Reset fifos. These flags clear automatically. */
            base->MDATACTRL |= I3C_MDATACTRL_FLUSHTB_MASK | I3C_MDATACTRL_FLUSHFB_MASK;
            driver.set_event(i3c::Driver::Event::Nack);
            break;
        case kStatus_I3C_IBIWon: break;
        // coverity[unterminated_case] expected no break
        case kStatus_I3C_Timeout:
#if 0
            // Only log when 5 retry failed case
            logger::Logger::add_from_isr(logger::Event::I3CTimeoutV2,
                                         logger::Level::Info,
                                         {static_cast<uint8_t>(I3C_GetInstance(base)),
                                          static_cast<uint8_t>(handle->state)});
#endif
            EDMA_AbortTransfer(handle->rxDmaHandle);

            /* Reset fifos. These flags clear automatically. */
            base->MDATACTRL |= I3C_MDATACTRL_FLUSHTB_MASK | I3C_MDATACTRL_FLUSHFB_MASK;

            /* Send a stop command to finalize the transfer. */
            (void)I3C_MasterStop(base);
            // TODO : Add break due to "warning: this statement may fall through
            // [-Wimplicit-fallthrough=]"
            break;
        default:
            driver.set_event(i3c::Driver::Event::Error);
            if (driver._error_log_count < i3c::Driver::ErrorLogThreshold) {
                driver._error_log_count++;
                const uint8_t           port_id = static_cast<uint8_t>(I3C_GetInstance(base));
                const uint8_t           hstate  = static_cast<uint8_t>(handle->state);
                const uint32_t          result_u32 = static_cast<uint32_t>(status);
                const uint32_t          mstatus    = base->MSTATUS;
                const logger::EventData err_data   = {
                    port_id,
                    hstate,
                    static_cast<uint8_t>(result_u32 >> 0 & 0xFF),
                    static_cast<uint8_t>(result_u32 >> 8 & 0xFF),
                    static_cast<uint8_t>(result_u32 >> 16 & 0xFF),
                    static_cast<uint8_t>(result_u32 >> 24 & 0xFF),
                };
                // coverity[cert_int31_c_violation] log raw value
                logger::Logger::add_from_isr(logger::Event::I3CUnhandledErrorV2.unique_id,
                                             logger::Level::Info,
                                             err_data);
                const logger::EventData mstatus_data = {
                    port_id,
                    hstate,
                    static_cast<uint8_t>(mstatus >> 0 & 0xFF),
                    static_cast<uint8_t>(mstatus >> 8 & 0xFF),
                    static_cast<uint8_t>(mstatus >> 16 & 0xFF),
                    static_cast<uint8_t>(mstatus >> 24 & 0xFF),
                };
                logger::Logger::add_from_isr(logger::Event::I3CUnhandledErrorMStatus.unique_id,
                                             logger::Level::Info,
                                             mstatus_data);
            }
    }
}

}  // namespace

nv::i3c::Driver::Driver(
    Port port, Freq freq, bool is_gpu, void* task, nv::ipc::EventId event_id, uint32_t clock)
: sys::i3c::Driver()
, _port(port)
, _task(task)
, _event(nv::ipc::Event::make(event_id))
, _is_gpu(is_gpu)
{
#ifdef CPU_MCXN556SCDF
    static_assert(nv::ipc::EnableSmartDMA, "SmartDMA is not enabled for MCXN556SCDF");
#endif
    if (clock != 0) {
        _clock = clock;
    }
    I3C_MasterGetDefaultConfig(&_master_config);
    _master_config.baudRate_Hz.i2cBaud          = freq.i2c;
    _master_config.baudRate_Hz.i3cPushPullBaud  = freq.i3c_pp;
    _master_config.baudRate_Hz.i3cOpenDrainBaud = freq.i2c_od;
    _master_config.enableOpenDrainStop          = false;
    _master_config.disableTimeout               = true;
    // CX8 uses 50:50 duty cycle
    _master_config.enableOpenDrainHigh = is_gpu ? true : false;

    DMA_Type* dma            = nullptr;
    uint32_t  dma_tx_channel = 0;
    uint32_t  dma_rx_channel = 0;
    int32_t   dma_tx_mux     = 0;
    int32_t   dma_rx_mux     = 0;
    switch (port) {
        case Port::Zero:
            _base          = I3C0;
            dma            = DMA0;
            dma_tx_channel = 0;
            dma_rx_channel = 1;
            dma_tx_mux     = kDma0RequestMuxI3c0Tx;
            dma_rx_mux     = kDma0RequestMuxI3c0Rx;
            break;
        case Port::One:
            _base          = I3C1;
            dma            = DMA1;
            dma_tx_channel = 0;
            dma_rx_channel = 1;
            dma_tx_mux     = kDma1RequestMuxI3c1Tx;
            dma_rx_mux     = kDma1RequestMuxI3c1Rx;
            break;
        default: nv::info("unsport port %d\n", port); return;
    }
    // init controller
    EDMA_CreateHandle(&_tx_edma_handle, dma, dma_tx_channel);
    EDMA_CreateHandle(&_rx_edma_handle, dma, dma_rx_channel);
    EDMA_SetChannelMux(dma, dma_tx_channel, dma_tx_mux);
    EDMA_SetChannelMux(dma, dma_rx_channel, dma_rx_mux);
}

void nv::i3c::Driver::init()
{
    if (_is_init) {
        return;
    }
    IRQn_Type const                  kI3cIrqs[] = I3C_IRQS;
    const i3c_master_edma_callback_t Callback   = {
          .slave2Master = nullptr, .ibiCallback = ibi_callback, .transferComplete = i3c_callback};
    const auto instance = I3C_GetInstance(_base);
    I3C_MasterInit(_base, &_master_config, _clock);

    static_assert(nv::ipc::I3CInterruptPriority >= configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY,
                  "Priority cannot be set higher than the FreeRTOS maximum for this platform");
    NVIC_SetPriority(kI3cIrqs[instance], nv::ipc::I3CInterruptPriority);

    // coverity[cert_exp60_cpp_violation] waive this until I have a good solution
    I3C_MasterTransferCreateHandleEDMA(
        _base, &_i3c_m_handle, &Callback, this, &_rx_edma_handle, &_tx_edma_handle);
    _i3c_m_handle.ibiBuff = _ibi_data.data();
    _is_init              = true;
    nv::logger::info(nv::logger::Event::I3cDriverInit, {static_cast<uint8_t>(_port), true});
}

void nv::i3c::Driver::deinit()
{
    if (!_is_init) {
        return;
    }
    I3C_MasterDeinit(_base);
    _is_init = false;
    nv::logger::info(nv::logger::Event::I3cDriverInit, {static_cast<uint8_t>(_port), false});
}

bool nv::i3c::Driver::reset_daa(bool ping)
{
    constexpr uint8_t     BoradcastAddress = 0x7EU;
    constexpr uint8_t     CccRstdaa        = 0x06U;
    i3c_master_transfer_t xfer{};
    xfer.slaveAddress   = BoradcastAddress;
    xfer.subaddress     = CccRstdaa;
    xfer.subaddressSize = 1U;
    xfer.direction      = kI3C_Write;
    xfer.busType        = kI3C_TypeI3CSdr;
    xfer.flags          = kI3C_TransferDefaultFlag;
    xfer.ibiResponse    = kI3C_IbiRespNack;
    auto result         = I3C_MasterTransferBlocking(_i3c_m_handle.base, &xfer);
    if (result != kStatus_Success) {
        I3C_MasterStop(_i3c_m_handle.base);
        nv::info("fail to reset daa %d\n", result);
        if (!ping) {
            logger::info(logger::Event::I3CFailedToResetDaaV2,
                         {static_cast<uint8_t>(_port),
                          static_cast<uint8_t>(result >> 0 & 0xFF),
                          static_cast<uint8_t>(result >> 8 & 0xFF),
                          static_cast<uint8_t>(result >> 16 & 0xFF),
                          static_cast<uint8_t>(result >> 24 & 0xFF)});
        }
        return false;
    }
    return true;
}

bool nv::i3c::Driver::enec()
{
    constexpr uint8_t BoradcastAddress = 0x7EU;
    constexpr uint8_t CccRstdaa        = 0x00;
    constexpr uint8_t Enint            = 0x01;
    I3cBuffer         buffer{
        Enint,
    };
    i3c_master_transfer_t xfer{};
    xfer.slaveAddress   = BoradcastAddress;
    xfer.subaddress     = CccRstdaa;
    xfer.subaddressSize = 1U;
    xfer.data           = buffer.data();
    xfer.dataSize       = 1;
    xfer.direction      = kI3C_Write;
    xfer.busType        = kI3C_TypeI3CSdr;
    xfer.flags          = kI3C_TransferDefaultFlag;
    xfer.ibiResponse    = kI3C_IbiRespNack;
    auto result         = I3C_MasterTransferBlocking(_i3c_m_handle.base, &xfer);
    if (result != kStatus_Success) {
        I3C_MasterStop(_i3c_m_handle.base);
        const logger::EventData data = {
            static_cast<uint8_t>(_port),
            CccRstdaa,
            static_cast<uint8_t>(result >> 0 & 0xFF),
            static_cast<uint8_t>(result >> 8 & 0xFF),
            static_cast<uint8_t>(result >> 16 & 0xFF),
            static_cast<uint8_t>(result >> 24 & 0xFF),
        };
        logger::info(logger::Event::I3CCccErrorV2, data);

        auto status = to_driver_status(static_cast<uint32_t>(result));
        if (status != Status::Success) {
            const auto& task = *static_cast<nv::i3c::Task*>(_task);
            task.record_error(static_cast<uint8_t>(status));
        }
        return false;
    }
    return true;
}

bool nv::i3c::Driver::get_status(uint8_t address, uint16_t& status)
{
    constexpr uint8_t     BoradcastAddress = 0x7EU;
    constexpr uint8_t     CccGetStatus     = 0x90U;
    constexpr uint16_t    WordMask         = 0xFFFF;
    I3cBuffer             buffer{};
    i3c_master_transfer_t xfer{};
    xfer.slaveAddress   = BoradcastAddress;
    xfer.subaddress     = CccGetStatus;
    xfer.subaddressSize = 1U;
    xfer.direction      = kI3C_Write;
    xfer.busType        = kI3C_TypeI3CSdr;
    xfer.flags          = kI3C_TransferNoStopFlag;
    xfer.ibiResponse    = kI3C_IbiRespAckMandatory;
    auto result         = I3C_MasterTransferBlocking(_i3c_m_handle.base, &xfer);
    if (result != kStatus_Success) {
        I3C_MasterEmitRequest(_i3c_m_handle.base, kI3C_RequestForceExit);
        const logger::EventData data = {
            static_cast<uint8_t>(_port),
            CccGetStatus,
            static_cast<uint8_t>(result >> 0 & 0xFF),
            static_cast<uint8_t>(result >> 8 & 0xFF),
            static_cast<uint8_t>(result >> 16 & 0xFF),
            static_cast<uint8_t>(result >> 24 & 0xFF),
        };
        logger::info(logger::Event::I3CCccErrorV2, data);
        auto driver_status = to_driver_status(static_cast<uint32_t>(result));
        if (driver_status != Status::Success) {
            const auto& task = *static_cast<nv::i3c::Task*>(_task);
            task.record_error(static_cast<uint8_t>(driver_status));
        }
        return false;
    }
    memset(&xfer, 0, sizeof(xfer));
    xfer.slaveAddress = address;
    xfer.data         = buffer.data();
    xfer.dataSize     = 2;
    xfer.direction    = kI3C_Read;
    xfer.busType      = kI3C_TypeI3CSdr;
    xfer.flags        = kI3C_TransferDefaultFlag;
    xfer.ibiResponse  = kI3C_IbiRespAckMandatory;
    result            = I3C_MasterTransferBlocking(_i3c_m_handle.base, &xfer);
    if (result != kStatus_Success) {
        I3C_MasterStop(_i3c_m_handle.base);
        const logger::EventData data = {
            static_cast<uint8_t>(_port),
            CccGetStatus,
            static_cast<uint8_t>(result >> 0 & 0xFF),
            static_cast<uint8_t>(result >> 8 & 0xFF),
            static_cast<uint8_t>(result >> 16 & 0xFF),
            static_cast<uint8_t>(result >> 24 & 0xFF),
        };
        logger::info(logger::Event::I3CCccErrorV2, data);
        auto driver_status = to_driver_status(static_cast<uint32_t>(result));
        if (driver_status != Status::Success) {
            const auto& task = *static_cast<nv::i3c::Task*>(_task);
            task.record_error(static_cast<uint8_t>(driver_status));
        }
        return false;
    }
    status = (buffer[0] << 8 | buffer[1]) & WordMask;
    return true;
}

bool nv::i3c::Driver::process_daa(std::span<uint8_t> address_list)
{
    /// reset list of devices
    I3C_MasterClearDeviceCount(_i3c_m_handle.base);
    /// start DAA porcess
    auto result = I3C_MasterProcessDAA(
        _i3c_m_handle.base, address_list.data(), address_list.size());
    if (result != kStatus_Success) {
        I3C_MasterStop(_i3c_m_handle.base);
        nv::info("fail to process daa %d\n", result);
        logger::info(logger::Event::I3CFailedToProcessDaaV2,
                     {static_cast<uint8_t>(_port),
                      static_cast<uint8_t>(result >> 0 & 0xFF),
                      static_cast<uint8_t>(result >> 8 & 0xFF),
                      static_cast<uint8_t>(result >> 16 & 0xFF),
                      static_cast<uint8_t>(result >> 24 & 0xFF)});
        return false;
    }
    /// list I3C devices
    i3c_device_info_t* list  = nullptr;
    uint8_t            count = 0;
    list                     = I3C_MasterGetDeviceListAfterDAA(_i3c_m_handle.base, &count);
    for (uint8_t index = 0; index < count; index++) {
        nv::info("I3C device [%d], vendor#: 0x%04x, part#: 0x%8x, addr: 0x%2x\n",
                 index,
                 list[index].vendorID,
                 list[index].partNumber,
                 list[index].dynamicAddr);
        nv::logger::info(
            nv::logger::Event::I3CSetAddrV2,
            {static_cast<uint8_t>(_port), static_cast<uint8_t>(list[index].dynamicAddr)});
    }
    if (count != address_list.size() && _is_gpu == false) {
        logger::info(logger::Event::I3CDaaMismatchV2,
                     {static_cast<uint8_t>(_port), count, address_list.size()});
        return false;
    }
    return true;
}

bool nv::i3c::Driver::write(uint8_t address, std::span<uint8_t> buffer)
{
    uint8_t               length = 0;
    i3c_master_transfer_t xfer{};
    xfer.slaveAddress  = address;
    xfer.data          = buffer.data();
    xfer.dataSize      = buffer.size();
    xfer.direction     = kI3C_Write;
    xfer.busType       = kI3C_TypeI3CSdr;
    xfer.flags         = kI3C_TransferDefaultFlag;
    xfer.ibiResponse   = kI3C_IbiRespAckMandatory;
    auto        status = transfer(&xfer, length);
    const auto& task   = *static_cast<nv::i3c::Task*>(_task);
    if (status != Status::Success) {
        if (_error_log_count < ErrorLogThreshold) {
            _error_log_count++;
            const uint32_t              mstatus = _i3c_m_handle.base->MSTATUS;
            const nv::logger::EventData data    = {
                static_cast<uint8_t>(_port),
                static_cast<uint8_t>(status),
                static_cast<uint8_t>(_i3c_m_handle.state),
                static_cast<uint8_t>(mstatus >> 0 & 0xFF),
                static_cast<uint8_t>(mstatus >> 8 & 0xFF),
                static_cast<uint8_t>(mstatus >> 16 & 0xFF),
                static_cast<uint8_t>(mstatus >> 24 & 0xFF),
            };
            nv::logger::info(nv::logger::Event::I3CWriteFailV2, data);
        }

        task.record_error(static_cast<uint8_t>(status));
    }
    return status == Status::Success;
}

bool nv::i3c::Driver::read(uint8_t address, std::span<uint8_t> buffer, uint8_t& length)
{
    i3c_master_transfer_t xfer{};
    xfer.slaveAddress  = address;
    xfer.data          = buffer.data();
    xfer.dataSize      = buffer.size();
    xfer.direction     = kI3C_Read;
    xfer.busType       = kI3C_TypeI3CSdr;
    xfer.flags         = kI3C_TransferDefaultFlag;
    xfer.ibiResponse   = kI3C_IbiRespAckMandatory;
    auto        status = transfer(&xfer, length);
    const auto& task   = *static_cast<nv::i3c::Task*>(_task);
    if (status != Status::Success) {
        if (_error_log_count < ErrorLogThreshold) {
            _error_log_count++;
            const uint32_t              mstatus = _i3c_m_handle.base->MSTATUS;
            const nv::logger::EventData data    = {
                static_cast<uint8_t>(_port),
                static_cast<uint8_t>(status),
                static_cast<uint8_t>(_i3c_m_handle.state),
                static_cast<uint8_t>(mstatus >> 0 & 0xFF),
                static_cast<uint8_t>(mstatus >> 8 & 0xFF),
                static_cast<uint8_t>(mstatus >> 16 & 0xFF),
                static_cast<uint8_t>(mstatus >> 24 & 0xFF),
            };
            nv::logger::info(nv::logger::Event::I3CReadFailV2, data);
        }
        task.record_error(static_cast<uint8_t>(status));
    }
    return status == Status::Success;
}

nv::i3c::Driver::Status nv::i3c::Driver::transfer(void* args, uint8_t& length)
{
    using namespace std::chrono;
    const auto&    task   = *static_cast<nv::i3c::Task*>(_task);
    const uint8_t  Retry  = 3;
    const uint32_t Mask   = Event::Success | Event::Error | Event::Nack;
    Status         status = Status::Error;
    for (uint8_t i = Retry; i > 0; i--) {
        status_t result = kStatus_Success;
        // WAR SDK API cannot be interrupted
        taskENTER_CRITICAL();
        result = I3C_MasterTransferEDMA(
            _i3c_m_handle.base, &_i3c_m_handle, static_cast<i3c_master_transfer_t*>(args));
        taskEXIT_CRITICAL();
        if (result != kStatus_Success) {
            status = Status::Busy;
            task.delay(10ms);
            continue;
        }
        auto events = _event.wait(Mask, true, false, 100ms);
        if (events.value() & Event::Success) {
            length = static_cast<uint8_t>(_i3c_m_handle.transferCount & UINT8_MAX);
            status = Status::Success;
            break;
        }
        if (events.value() & Event::Nack) {
            status = Status::Nack;
            task.delay(10ms);
            continue;
        }
        if (events.value() & Event::Error) {
            status = Status::Error;
            task.delay(10ms);
            continue;
        }
        if (!events.value()) {
            if constexpr (nv::ipc::EnableSmartDMA) {
                sys::smartdma::Driver::log_status(_i3c_m_handle);
            }
            status = Status::Timeout;
            task.delay(10ms);
        }
    }
    return status;
}

void nv::i3c::Driver::on_ibi(void* args)
{
    auto& task   = *static_cast<nv::i3c::Task*>(_task);
    auto& handle = *static_cast<i3c_master_edma_handle_t*>(args);
    if constexpr (nv::ipc::EnableSmartDMA) {
        auto instance = I3C_GetInstance(handle.base);
        auto ibi_data = sys::smartdma::Driver::get_ibi_data(instance);
        task.on_ibi(
            ibi_data->address,
            std::span<volatile uint8_t>(static_cast<volatile uint8_t*>(&ibi_data->buf[0]),
                                        static_cast<size_t>(ibi_data->payload_size)));
    }
    else {
        task.on_ibi(handle.ibiAddress,
                    std::span<uint8_t>(handle.ibiBuff, handle.ibiPayloadSize));
    }
}

void nv::i3c::Driver::set_event(Event event)
{
    (void)_event.set(event);
    // TODO error log?
}

nv::i2c::I2cStatus nv::i3c::Driver::i2c(uint8_t            address,
                                        std::span<uint8_t> write_buffer,
                                        std::span<uint8_t> read_buffer)
{
    auto result = nv::i2c::I2cStatus::Error;
    if (write_buffer.size() != 0 && read_buffer.size() == 0) {
        result = i2c_write(address, write_buffer);
    }
    else if (write_buffer.size() == 0 && read_buffer.size() != 0) {
        result = i2c_read(address, read_buffer);
    }
    else if (write_buffer.size() != 0 && read_buffer.size() != 0) {
        result = i2c_write_read(address, write_buffer, read_buffer);
    }
    else {
        /// TODO quick write/read
    }
    return result;
}

nv::i2c::I2cStatus nv::i3c::Driver::i2c_write(uint8_t address, std::span<uint8_t> buffer)
{
    i3c_master_transfer_t xfer{
        .flags        = kI3C_TransferDefaultFlag,
        .slaveAddress = address,
        .direction    = kI3C_Write,
        .data         = buffer.data(),
        .dataSize     = buffer.size(),
        .busType      = kI3C_TypeI2C,
    };
    nv::i2c::I2cStatus result = nv::i2c::I2cStatus::Ok;
    taskENTER_CRITICAL();
    auto status = I3C_MasterTransferBlocking(_i3c_m_handle.base, &xfer);
    taskEXIT_CRITICAL();
    result = to_status(status);
    if (result != nv::i2c::I2cStatus::Ok) {
        i2c_stop();
        if (_i2c_error_log_count < ErrorLogThreshold) {
            _i2c_error_log_count++;
            const uint32_t              mstatus = _i3c_m_handle.base->MSTATUS;
            const nv::logger::EventData data    = {
                static_cast<uint8_t>(_port),
                static_cast<uint8_t>(result),
                address,
                static_cast<uint8_t>(_i3c_m_handle.state),
                static_cast<uint8_t>(mstatus >> 0 & 0xFF),
                static_cast<uint8_t>(mstatus >> 8 & 0xFF),
                static_cast<uint8_t>(mstatus >> 16 & 0xFF),
                static_cast<uint8_t>(mstatus >> 24 & 0xFF),
            };
            nv::logger::info(nv::logger::Event::I3CI2CWriteFailV2, data);
        }
        const auto& task = *static_cast<nv::i3c::Task*>(_task);
        task.record_error((static_cast<uint8_t>(to_driver_status(result))));
    }
    return result;
}

nv::i2c::I2cStatus nv::i3c::Driver::i2c_read(uint8_t address, std::span<uint8_t> buffer)
{
    i3c_master_transfer_t xfer{
        .flags        = kI3C_TransferDefaultFlag,
        .slaveAddress = address,
        .direction    = kI3C_Read,
        .data         = buffer.data(),
        .dataSize     = buffer.size(),
        .busType      = kI3C_TypeI2C,
    };
    nv::i2c::I2cStatus result = nv::i2c::I2cStatus::Ok;
    taskENTER_CRITICAL();
    result = to_status(I3C_MasterTransferBlocking(_i3c_m_handle.base, &xfer));
    taskEXIT_CRITICAL();
    if (result != nv::i2c::I2cStatus::Ok) {
        i2c_stop();
        if (_i2c_error_log_count < ErrorLogThreshold) {
            _i2c_error_log_count++;
            const uint32_t              mstatus = _i3c_m_handle.base->MSTATUS;
            const nv::logger::EventData data    = {
                static_cast<uint8_t>(_port),
                static_cast<uint8_t>(result),
                address,
                static_cast<uint8_t>(_i3c_m_handle.state),
                static_cast<uint8_t>(mstatus >> 0 & 0xFF),
                static_cast<uint8_t>(mstatus >> 8 & 0xFF),
                static_cast<uint8_t>(mstatus >> 16 & 0xFF),
                static_cast<uint8_t>(mstatus >> 24 & 0xFF),
            };
            nv::logger::info(nv::logger::Event::I3CI2CReadFailV2, data);
        }

        const auto& task = *static_cast<nv::i3c::Task*>(_task);
        task.record_error((static_cast<uint8_t>(to_driver_status(result))));
    }
    return result;
}

nv::i2c::I2cStatus nv::i3c::Driver::i2c_write_read(uint8_t            address,
                                                   std::span<uint8_t> write_buffer,
                                                   std::span<uint8_t> read_buffer)
{
    i3c_master_transfer_t xfer{
        .slaveAddress = address,
        .busType      = kI3C_TypeI2C,
    };
    /// write phase
    xfer.direction            = kI3C_Write;
    xfer.data                 = write_buffer.data();
    xfer.dataSize             = write_buffer.size();
    xfer.flags                = kI3C_TransferNoStopFlag;
    nv::i2c::I2cStatus result = nv::i2c::I2cStatus::Ok;
    taskENTER_CRITICAL();
    result = to_status(I3C_MasterTransferBlocking(_i3c_m_handle.base, &xfer));
    taskEXIT_CRITICAL();
    if (result != nv::i2c::I2cStatus::Ok) {
        i2c_stop();
        if (_i2c_error_log_count < ErrorLogThreshold) {
            _i2c_error_log_count++;
            const uint32_t              mstatus = _i3c_m_handle.base->MSTATUS;
            const nv::logger::EventData data    = {
                static_cast<uint8_t>(_port),
                static_cast<uint8_t>(result),
                address,
                static_cast<uint8_t>(_i3c_m_handle.state),
                static_cast<uint8_t>(mstatus >> 0 & 0xFF),
                static_cast<uint8_t>(mstatus >> 8 & 0xFF),
                static_cast<uint8_t>(mstatus >> 16 & 0xFF),
                static_cast<uint8_t>(mstatus >> 24 & 0xFF),
            };
            nv::logger::info(nv::logger::Event::I3CI2CWriteFailV2, data);
        }
        const auto& task = *static_cast<nv::i3c::Task*>(_task);
        task.record_error((static_cast<uint8_t>(to_driver_status(result))));
        return result;
    }
    /// read phase
    xfer.direction = kI3C_Read;
    xfer.data      = read_buffer.data();
    xfer.dataSize  = read_buffer.size();
    xfer.flags     = kI3C_TransferRepeatedStartFlag;
    taskENTER_CRITICAL();
    result = to_status(I3C_MasterTransferBlocking(_i3c_m_handle.base, &xfer));
    taskEXIT_CRITICAL();
    if (result != nv::i2c::I2cStatus::Ok) {
        i2c_stop();
        if (_i2c_error_log_count < ErrorLogThreshold) {
            _i2c_error_log_count++;
            const uint32_t              mstatus = _i3c_m_handle.base->MSTATUS;
            const nv::logger::EventData data    = {
                static_cast<uint8_t>(_port),
                static_cast<uint8_t>(result),
                address,
                static_cast<uint8_t>(_i3c_m_handle.state),
                static_cast<uint8_t>(mstatus >> 0 & 0xFF),
                static_cast<uint8_t>(mstatus >> 8 & 0xFF),
                static_cast<uint8_t>(mstatus >> 16 & 0xFF),
                static_cast<uint8_t>(mstatus >> 24 & 0xFF),
            };
            nv::logger::info(nv::logger::Event::I3CI2CReadFailV2, data);
        }
        const auto& task = *static_cast<nv::i3c::Task*>(_task);
        task.record_error((static_cast<uint8_t>(to_driver_status(result))));
        return result;
    }
    return nv::i2c::I2cStatus::Ok;
}

void sys::i3c::Driver::i2c_stop()
{
    /// WAR driver didn't send stop correctly when NACK
    auto mctrl_val             = _i3c_m_handle.base->MCTRL;
    mctrl_val                 &= ~I3C_MCTRL_TYPE_MASK;
    mctrl_val                 |= I3C_MCTRL_TYPE(kI3C_TypeI2C);
    _i3c_m_handle.base->MCTRL  = mctrl_val;
    I3C_MasterEmitRequest(_i3c_m_handle.base, kI3C_RequestEmitStop);
}

nv::i2c::I2cStatus sys::i3c::Driver::to_status(status_t status)
{
    switch (status) {
        case kStatus_Success   : return nv::i2c::I2cStatus::Ok;
        case kStatus_I3C_Busy  : return nv::i2c::I2cStatus::Busy;
        case kStatus_I3C_Nak   : return nv::i2c::I2cStatus::Nak;
        case kStatus_I3C_IBIWon: return nv::i2c::I2cStatus::ArbLost;
        default                : return nv::i2c::I2cStatus::Error;
    }
}

nv::i3c::Driver::Status nv::i3c::Driver::to_driver_status(nv::i2c::I2cStatus status)
{
    switch (status) {
        case nv::i2c::I2cStatus::Ok     : return nv::i3c::Driver::Status::Success;
        case nv::i2c::I2cStatus::Busy   : return nv::i3c::Driver::Status::Busy;
        case nv::i2c::I2cStatus::Nak    : return nv::i3c::Driver::Status::Nack;
        case nv::i2c::I2cStatus::Timeout: return nv::i3c::Driver::Status::Timeout;
        default                         : return nv::i3c::Driver::Status::Error;
    }
}

nv::i3c::Driver::Status nv::i3c::Driver::to_driver_status(uint32_t status)
{
    switch (status) {
        case kStatus_Success    : return nv::i3c::Driver::Status::Success;
        case kStatus_I3C_Busy   : return nv::i3c::Driver::Status::Busy;
        case kStatus_I3C_Nak    : return nv::i3c::Driver::Status::Nack;
        case kStatus_I3C_Timeout: return nv::i3c::Driver::Status::Timeout;
        default                 : return nv::i3c::Driver::Status::Error;
    }
}

bool nv::i3c::Driver::ocp_query_interface_mastering(uint8_t address, bool& enable)
{
    constexpr uint8_t  Command = 0x25;
    nv::i2c::I2cBuffer write_buffer{};
    nv::i2c::I2cBuffer read_buffer{};
    write_buffer[0] = Command;
    auto status     = i2c(address,
                      std::span<uint8_t>(write_buffer.data(), 1),
                      std::span<uint8_t>(read_buffer.data(), 4));
    if (status != nv::i2c::I2cStatus::Ok) {
        return false;
    }
    enable = read_buffer[3] == 1 ? true : false;
    return true;
}

bool nv::i3c::Driver::ocp_enable_interface_mastering(uint8_t address)
{
    constexpr uint8_t  Command = 0x25;
    constexpr uint8_t  Length  = 0x03;
    nv::i2c::I2cBuffer write_buffer{};
    nv::i2c::I2cBuffer read_buffer{};
    write_buffer[0] = Command;
    write_buffer[1] = Length;
    write_buffer[2] = 0x00;
    write_buffer[3] = 0x00;
    write_buffer[4] = 0x01;
    auto status     = i2c(address,
                      std::span<uint8_t>(write_buffer.data(), 5),
                      std::span<uint8_t>(read_buffer.data(), 0));
    return status == nv::i2c::I2cStatus::Ok ? true : false;
}

bool nv::i3c::Driver::gpu_query_i3c_mode(uint8_t address, bool& i3c)
{
    constexpr uint8_t  Register = 0x00;
    bool               result   = false;
    nv::i2c::I2cBuffer write_buffer{};
    nv::i2c::I2cBuffer read_buffer{};
    write_buffer[0] = Register;
    auto status     = i2c(address,
                      std::span<uint8_t>(write_buffer.data(), 1),
                      std::span<uint8_t>(read_buffer.data(), 1));
    switch (status) {
        case nv::i2c::I2cStatus::Ok:
            i3c    = false;
            result = true;
            break;
        case nv::i2c::I2cStatus::Nak:
            i3c    = true;
            result = true;
            break;
        default: result = false;
    }
    return result;
}

bool nv::i3c::Driver::gpu_configure_cms1(uint8_t address)
{
    constexpr uint8_t  Command = 0x29;
    nv::i2c::I2cBuffer write_buffer{};
    nv::i2c::I2cBuffer read_buffer{};
    write_buffer[0] = Command;
    write_buffer[1] = 0x06;
    write_buffer[2] = 0x01;
    write_buffer[3] = 0x00;
    write_buffer[4] = 0x00;
    write_buffer[5] = 0x00;
    write_buffer[6] = 0x00;
    write_buffer[7] = 0x00;
    auto status     = i2c(address,
                      std::span<uint8_t>(write_buffer.data(), 8),
                      std::span<uint8_t>(read_buffer.data(), 0));
    return status == nv::i2c::I2cStatus::Ok ? true : false;
}

bool nv::i3c::Driver::gpu_program_cms1(uint8_t address, std::span<uint8_t> buffer)
{
    constexpr uint8_t  Command = 0x2b;
    nv::i2c::I2cBuffer write_buffer{};
    nv::i2c::I2cBuffer read_buffer{};
    write_buffer[0] = Command;
    write_buffer[1] = static_cast<uint8_t>(buffer.size());
    std::memcpy(write_buffer.data() + 2, buffer.data(), buffer.size());
    auto status = i2c(address,
                      std::span<uint8_t>(write_buffer.data(), 2 + buffer.size()),
                      std::span<uint8_t>(read_buffer.data(), 0));
    return status == nv::i2c::I2cStatus::Ok ? true : false;
}

bool nv::i3c::Driver::gpu_read_cms1(uint8_t address, std::span<uint8_t> buffer)
{
    constexpr uint8_t  Command = 0x2b;
    nv::i2c::I2cBuffer write_buffer{};
    nv::i2c::I2cBuffer read_buffer{};
    write_buffer[0] = Command;
    auto status     = i2c(address,
                      std::span<uint8_t>(write_buffer.data(), 1),
                      std::span<uint8_t>(read_buffer.data(), buffer.size() + 1));
    if (status == nv::i2c::I2cStatus::Ok) {
        std::memcpy(buffer.data(), read_buffer.data() + 1, buffer.size());
        return true;
    }
    return false;
}
