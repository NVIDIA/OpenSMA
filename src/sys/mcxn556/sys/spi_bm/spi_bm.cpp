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

#include NV_IPC_CONFIG_H
#include "sys/spi_bm/spi_bm.h"
#include "nv/common/utils.h"
#include "nv/gpio/driver_bm.h"
#include "nv/logger/core1_log.h"
#include "nv/spi/spi_bm_config.h"

namespace sys::spi::bm {

namespace {

constexpr bool    TraceEnabled = nv::spi::SpiBmConfig<>::EnableTrace;
constexpr uint8_t ByteMask     = 0xFFU;
constexpr uint8_t Byte1Shift   = 8U;
constexpr uint8_t FlagsShift   = 4U;
constexpr uint8_t CsFieldMask  = 0x0FU;

/// Lengths are only needed to spot the outlier, so saturate instead of widening.
constexpr uint8_t saturate_u8(size_t value)
{
    return static_cast<uint8_t>((value > ByteMask) ? ByteMask : value);
}

constexpr uint16_t saturate_u16(uint32_t value)
{
    constexpr uint32_t Max = 0xFFFFU;
    return static_cast<uint16_t>((value > Max) ? Max : value);
}

constexpr uint8_t  XferKindWrite           = 0U;
constexpr uint8_t  XferKindRead            = 1U;
constexpr uint8_t  XferKindWriteRead       = 2U;
constexpr uint32_t RecoveryDummyReadLength = 256U;

}  // namespace

uint8_t Driver::cs_and_flags() const
{
    const auto cs    = static_cast<uint8_t>(_current_cs) & CsFieldMask;
    const auto flags = static_cast<uint8_t>(_result.flags);
    return static_cast<uint8_t>((flags << FlagsShift) | cs);
}

/**
 * SpiBmError (0x0b08) payload:
 *   [0] nv::spi::Port
 *   [1] nv::spi::bm::SpiStatus
 *   [2] nv::spi::bm::SpiErrorReason
 *   [3] (SpiFlags << 4) | CS index
 *   [4] requested read length, saturated at 0xFF
 *   [5] requested write length, saturated at 0xFF
 *   [6..7] SDK status_t, little endian (0 when the SDK was not involved)
 */
void Driver::log_error(nv::spi::bm::SpiStatus      status,
                       nv::spi::bm::SpiErrorReason reason,
                       size_t                      read_length,
                       size_t                      write_length,
                       uint32_t                    sdk_status,
                       const std::source_location& location)
{
    const uint16_t sdk = saturate_u16(sdk_status);
    trace_error(status, reason);

    nv::logger::error(nv::logger::CoreEvent::SpiBmError,
                      {static_cast<uint8_t>(_port),
                       static_cast<uint8_t>(status),
                       static_cast<uint8_t>(reason),
                       cs_and_flags(),
                       saturate_u8(read_length),
                       saturate_u8(write_length),
                       static_cast<uint8_t>(sdk & ByteMask),
                       static_cast<uint8_t>(sdk >> Byte1Shift)},
                      location);
    _error_logged = true;
    if (!log_followup_trace(location)) {
        log_trace();
        arm_followup_trace();
    }
}

/**
 * SpiBmTimeout (0x0b09) payload:
 *   [0] nv::spi::Port
 *   [1] nv::spi::bm::SpiStatus
 *   [2] nv::spi::bm::SpiErrorReason
 *   [3] (SpiFlags << 4) | CS index
 *   [4] pending read length, saturated at 0xFF
 *   [5] WaitGpioEvents flags at the point of timeout
 *   [6..7] elapsed ms since the wait started, little endian
 */
void Driver::log_timeout(uint32_t                    elapsed_ms,
                         nv::spi::bm::SpiErrorReason reason,
                         const std::source_location& location)
{
    const uint16_t elapsed = saturate_u16(elapsed_ms);
    trace_error(nv::spi::bm::SpiStatus::EventTimeout, reason);

    nv::logger::error(nv::logger::CoreEvent::SpiBmTimeout,
                      {static_cast<uint8_t>(_port),
                       static_cast<uint8_t>(nv::spi::bm::SpiStatus::EventTimeout),
                       static_cast<uint8_t>(reason),
                       cs_and_flags(),
                       saturate_u8(_result.read_length),
                       _wait_gpio_flags.load(std::memory_order_acquire),
                       static_cast<uint8_t>(elapsed & ByteMask),
                       static_cast<uint8_t>(elapsed >> Byte1Shift)},
                      location);
    _error_logged = true;
    if (!log_followup_trace(location)) {
        log_trace();
        arm_followup_trace();
    }
}

void Driver::begin_trace(uint32_t read_length, size_t write_length)
{
    [&]<typename Storage>(Storage& trace) {
        if constexpr (std::is_same_v<Storage, EnabledTraceStorage>) {
            if (trace.count != 0U) {
                trace.head = (trace.head + 1U) % TraceDepth;
            }
            trace.count = std::min(trace.count + 1U, TraceDepth);

            TraceEntry& entry = trace.entries.at(trace.head);
            entry.sequence    = ++trace.sequence;
            if (read_length > 0U && write_length > 0U) {
                entry.kind = XferKindWriteRead;
            }
            else if (read_length > 0U) {
                entry.kind = XferKindRead;
            }
            else {
                entry.kind = XferKindWrite;
            }
            entry.cs_flags = cs_and_flags();
            entry.phases.store(TraceRequested, std::memory_order_release);
            entry.status = nv::spi::bm::SpiStatus::Ok;
            entry.reason = nv::spi::bm::SpiErrorReason::None;
        }
    }(_trace);
}

void Driver::trace_phase(uint8_t phases)
{
    [&]<typename Storage>(Storage& trace) {
        if constexpr (std::is_same_v<Storage, EnabledTraceStorage>) {
            if (trace.count != 0U) {
                trace.entries.at(trace.head).phases.fetch_or(phases, std::memory_order_acq_rel);
            }
        }
    }(_trace);
}

void Driver::trace_error(nv::spi::bm::SpiStatus status, nv::spi::bm::SpiErrorReason reason)
{
    [&]<typename Storage>(Storage& trace) {
        if constexpr (std::is_same_v<Storage, EnabledTraceStorage>) {
            if (trace.count != 0U) {
                TraceEntry& entry = trace.entries.at(trace.head);
                entry.status      = status;
                entry.reason      = reason;
            }
        }
    }(_trace);
}

/**
 * SpiBmTrace (0x0b0b) payload:
 *   [0] nv::spi::Port
 *   [1] bits 7..2: position relative to the error
 *                  (0..4=Before-4..Error, 5..6=After+1..After+2);
 *       bits 1..0: transfer kind (0=write, 1=read, 2=write-read)
 *   [2] (SpiFlags << 4) | CS index
 *   [3] phase bitmap: request, write start/done, GPIO wait/done,
 *                     read start/done, result consumed
 *   [4] nv::spi::bm::SpiStatus
 *   [5] nv::spi::bm::SpiErrorReason
 *   [6..7] call sequence, little endian
 */
void Driver::log_trace_entry(const TraceEntry&           entry,
                             uint8_t                     position,
                             const std::source_location& location)
{
    if constexpr (TraceEnabled) {
        const uint16_t seq = entry.sequence;
        nv::logger::error(nv::logger::CoreEvent::SpiBmTrace,
                          {static_cast<uint8_t>(_port),
                           static_cast<uint8_t>((position << 2U) | entry.kind),
                           entry.cs_flags,
                           entry.phases.load(std::memory_order_acquire),
                           static_cast<uint8_t>(entry.status),
                           static_cast<uint8_t>(entry.reason),
                           static_cast<uint8_t>(seq & ByteMask),
                           static_cast<uint8_t>(seq >> Byte1Shift)},
                          location);
    }
}

void Driver::log_trace(const std::source_location& location)
{
    [&]<typename Storage>(Storage& trace) {
        if constexpr (std::is_same_v<Storage, EnabledTraceStorage>) {
            const size_t first = (trace.count < TraceDepth) ? 0U
                                                            : ((trace.head + 1U) % TraceDepth);
            for (size_t order = 0; order < trace.count; ++order) {
                const TraceEntry& entry = trace.entries.at((first + order) % TraceDepth);
                const auto position     = static_cast<uint8_t>(TraceErrorPosition + order + 1U
                                                           - trace.count);
                log_trace_entry(entry, position, location);
            }
        }
    }(_trace);
}

void Driver::arm_followup_trace()
{
    [&]<typename Storage>(Storage& trace) {
        if constexpr (std::is_same_v<Storage, EnabledTraceStorage>) {
            if (trace.count == 0U) {
                return;
            }

            trace.last_capture_sequence = trace.entries.at(trace.head).sequence;
            trace.followups_remaining   = TraceFollowupCount;
            trace.followup_position     = TraceFirstAfter;
        }
    }(_trace);
}

bool Driver::log_followup_trace(const std::source_location& location)
{
    return [&]<typename Storage>(Storage& trace) {
        if constexpr (std::is_same_v<Storage, EnabledTraceStorage>) {
            if (trace.count == 0U || trace.followups_remaining == 0U) {
                return false;
            }

            const TraceEntry& entry = trace.entries.at(trace.head);
            if (entry.sequence == trace.last_capture_sequence) {
                return false;
            }

            log_trace_entry(entry, trace.followup_position, location);
            trace.last_capture_sequence = entry.sequence;
            --trace.followups_remaining;
            ++trace.followup_position;
            return true;
        }
        return false;
    }(_trace);
}

nv::spi::bm::SpiStatus Driver::get_status(status_t status)
{
    switch (status) {
        case kStatus_Success         : return nv::spi::bm::SpiStatus::Ok;
        case kStatus_LPSPI_Busy      : return nv::spi::bm::SpiStatus::Ok;
        case kStatus_LPSPI_Error     : return nv::spi::bm::SpiStatus::Error;
        case kStatus_LPSPI_Idle      : return nv::spi::bm::SpiStatus::Ok;
        case kStatus_LPSPI_OutOfRange: return nv::spi::bm::SpiStatus::Error;
        case kStatus_LPSPI_Timeout   : return nv::spi::bm::SpiStatus::EventTimeout;
    }
    return nv::spi::bm::SpiStatus::Error;
}

LPSPI_Type* Driver::get_base(nv::spi::Port port)
{
    constexpr uint8_t Size = nv::common::to_underlying(nv::spi::Port::End);
    // NOLINTNEXTLINE: SDK definition
    std::array<LPSPI_Type*, Size> bases LPSPI_BASE_PTRS;
    return bases.at(nv::common::to_underlying(port));
}

void Driver::bind(nv::spi::Port                                         port,
                  volatile uint32_t*                                    event,
                  nv::ecm_bm::AppEvent                                  event_bit,
                  std::array<nv::ipc::Gpios, nv::spi::bm::SpiMaxCsPins> cs_pins)
{
    _port                     = port;
    _event                    = event;
    _event_bit                = event_bit;
    _cs_gpios                 = cs_pins;
    _current_cs               = nv::spi::bm::CsPins::Cs0;
    _is_busy                  = false;
    _gpio_war_state           = GpioWarState::Idle;
    _forwarded_timed_out      = false;
    _recovery_dummy_pending   = false;
    _recovery_dummy_active    = false;
    _wait_gpio_timeout_active = false;
    _wait_gpio_flags.store(0U, std::memory_order_relaxed);
    _ready_gpio_port = nv::gpio::InvalidGpioPort;
    _ready_gpio_pin  = nv::gpio::InvalidGpioPin;
    if constexpr (nv::spi::WaitGpioSpiWar) {
        for (const auto& info : nv::spi::SpiGpioWaitInfos) {
            if (info.spiPort == port) {
                _ready_gpio_port = info.gpioPort;
                _ready_gpio_pin  = info.pin;
                break;
            }
        }
        if (_ready_gpio_port == nv::gpio::InvalidGpioPort
            || _ready_gpio_pin == nv::gpio::InvalidGpioPin) {
            nv::logger::error(
                nv::logger::CoreEvent::SpiBmError,
                {static_cast<uint8_t>(port),
                 static_cast<uint8_t>(nv::spi::bm::SpiStatus::Error),
                 static_cast<uint8_t>(nv::spi::bm::SpiErrorReason::InvalidReadyGpio),
                 0U,
                 0U,
                 0U,
                 0U,
                 0U});
        }
    }
    [&]<typename Storage>(Storage& trace) {
        if constexpr (std::is_same_v<Storage, EnabledTraceStorage>) {
            trace.head                  = 0U;
            trace.count                 = 0U;
            trace.sequence              = 0U;
            trace.last_capture_sequence = 0U;
            trace.followups_remaining   = 0U;
            trace.followup_position     = TraceFirstAfter;
            for (TraceEntry& entry : trace.entries) {
                entry.phases.store(0U, std::memory_order_relaxed);
            }
        }
    }(_trace);

    _master_xfer_ctx.driver = this;
    _base                   = get_base(port);

    deassert_all_cs();

    LPSPI_MasterTransferCreateHandle(_base, &_handle, master_callback, &_master_xfer_ctx);
    // SpiBindBm (0x0b0a) payload: [0] nv::spi::Port
    nv::logger::info(nv::logger::CoreEvent::SpiBindBm, {static_cast<uint8_t>(port)});
}

nv::spi::bm::SpiStatus Driver::transfer(nv::spi::bm::CsPins      cs,
                                        uint32_t                 read_length,
                                        const std::span<uint8_t> send_buffer,
                                        nv::spi::bm::SpiFlags    flags)
{
    if (_is_busy) {
        return nv::spi::bm::SpiStatus::Busy;
    }

    // Preconditions run before _current_cs/_result are updated, so report the
    // requested CS and flags rather than the previous transfer's context.
    _current_cs   = cs;
    _result.flags = flags;

    if constexpr (nv::spi::WaitGpioSpiWar) {
        nv::spi::bm::SpiStatus war_status = nv::spi::bm::SpiStatus::Ok;
        if (gpio_war_reject(cs, read_length, send_buffer, flags, war_status)) {
            return war_status;
        }
    }

    begin_trace(read_length, send_buffer.size());

    if (read_length > nv::spi::bm::BufferSize || send_buffer.size() > nv::spi::bm::BufferSize) {
        log_error(nv::spi::bm::SpiStatus::Error,
                  nv::spi::bm::SpiErrorReason::RequestTooLarge,
                  read_length,
                  send_buffer.size(),
                  0U);
        return nv::spi::bm::SpiStatus::Error;
    }
    // Asymmetric write-then-read is not supported in a single transfer; caller
    // must split it into separate write and read transfers.
    if (read_length > 0 && send_buffer.size() > 0 && read_length != send_buffer.size()) {
        log_error(nv::spi::bm::SpiStatus::Error,
                  nv::spi::bm::SpiErrorReason::AsymmetricLength,
                  read_length,
                  send_buffer.size(),
                  0U);
        return nv::spi::bm::SpiStatus::Error;
    }

    const auto& [cs_port, cs_pin] = _cs_gpios.at(static_cast<size_t>(cs));
    if (cs_port == nv::gpio::InvalidGpioPort || cs_pin == nv::gpio::InvalidGpioPin) {
        log_error(nv::spi::bm::SpiStatus::Error,
                  nv::spi::bm::SpiErrorReason::InvalidCs,
                  read_length,
                  send_buffer.size(),
                  0U);
        return nv::spi::bm::SpiStatus::Error;
    }

    _is_busy            = true;
    _result.status      = nv::spi::bm::SpiStatus::Ok;
    _result.read_length = read_length;

    _last_sdk_status = 0U;
    _write_length    = send_buffer.size();
    _fail_reason     = nv::spi::bm::SpiErrorReason::None;
    _error_logged    = false;

    const uint32_t data_size = std::max<uint32_t>(read_length,
                                                  static_cast<uint32_t>(send_buffer.size()));

    lpspi_transfer_t masterXfer;
    if (send_buffer.size() > 0) {
        memcpy(_tx_buffer.data(), send_buffer.data(), send_buffer.size());
        masterXfer.txData = _tx_buffer.data();
    }
    else {
        masterXfer.txData = nullptr;
    }
    masterXfer.rxData      = (read_length > 0) ? _result.buffer.data() : nullptr;
    masterXfer.dataSize    = data_size;
    masterXfer.configFlags = kLPSPI_MasterPcs0 | kLPSPI_MasterPcsContinuous
                           | kLPSPI_MasterByteSwap;

    status_t Status = kStatus_Success;
    if constexpr (nv::spi::WaitGpioSpiWar) {
        // Combined write-read waits for READY_N between phases.
        // Split paired READ is Busy/Timeout-gated in gpio_war_reject().
        const bool has_write      = !send_buffer.empty();
        const bool has_read       = read_length > 0U;
        _is_wait_gpio_transaction = has_write && has_read;
        _wait_gpio_timeout_active = false;
        _wait_gpio_flags.store(0U, std::memory_order_release);

        if (_is_wait_gpio_transaction) {
            // This is the write phase; try_read_after_wait() supplies the read buffer.
            masterXfer.rxData = nullptr;
        }

        trace_phase(has_write ? TraceWriteStart : (has_read ? TraceReadStart : 0U));
        if (flags & nv::spi::bm::SpiFlags::CsAssert) {
            deassert_all_cs();
            assert_cs(_current_cs);
        }
        Status = LPSPI_MasterTransferNonBlocking(_base, &_handle, &masterXfer);
        if (Status != kStatus_Success) {
            _is_busy = false;
            if (flags & nv::spi::bm::SpiFlags::CsDeassert) {
                deassert_cs(_current_cs);
            }
            if (read_length > 0U) {
                enter_recovery();
            }
            else {
                _gpio_war_state = GpioWarState::Idle;
            }
            log_error(nv::spi::bm::SpiStatus::EventXferFail,
                      nv::spi::bm::SpiErrorReason::XferStartFail,
                      read_length,
                      send_buffer.size(),
                      static_cast<uint32_t>(Status));
            return nv::spi::bm::SpiStatus::EventXferFail;
        }
    }
    else {
        trace_phase((send_buffer.size() > 0U ? TraceWriteStart : 0U)
                    | (read_length > 0U ? TraceReadStart : 0U));
        if (flags & nv::spi::bm::SpiFlags::CsAssert) {
            deassert_all_cs();
            assert_cs(_current_cs);
        }

        Status = LPSPI_MasterTransferNonBlocking(_base, &_handle, &masterXfer);
        if (Status != kStatus_Success) {
            _is_busy = false;
            if (flags & nv::spi::bm::SpiFlags::CsDeassert) {
                deassert_cs(_current_cs);
            }
            log_error(nv::spi::bm::SpiStatus::EventXferFail,
                      nv::spi::bm::SpiErrorReason::XferStartFail,
                      read_length,
                      send_buffer.size(),
                      static_cast<uint32_t>(Status));
            return nv::spi::bm::SpiStatus::EventXferFail;
        }
    }

    return nv::spi::bm::SpiStatus::Ok;
}

void Driver::reject_gpio_busy(nv::spi::bm::CsPins   cs,
                              uint32_t              read_length,
                              size_t                write_length,
                              nv::spi::bm::SpiFlags flags)
{
    reject_immediate(cs,
                     read_length,
                     write_length,
                     flags,
                     nv::spi::bm::SpiStatus::Busy,
                     nv::spi::bm::SpiErrorReason::GpioBusy);
}

void Driver::reject_immediate(nv::spi::bm::CsPins         cs,
                              uint32_t                    read_length,
                              size_t                      write_length,
                              nv::spi::bm::SpiFlags       flags,
                              nv::spi::bm::SpiStatus      status,
                              nv::spi::bm::SpiErrorReason reason)
{
    _current_cs         = cs;
    _result.status      = status;
    _result.flags       = flags;
    _result.read_length = read_length;
    _write_length       = write_length;
    _last_sdk_status    = 0U;
    _fail_reason        = reason;
    begin_trace(read_length, write_length);
    if (flags & nv::spi::bm::SpiFlags::CsDeassert) {
        deassert_all_cs();
    }
    log_error(status, reason, read_length, write_length, 0U);
}

nv::spi::bm::SpiResult& Driver::get_result()
{
    _is_busy = false;
    trace_phase(TraceResultRead);
    [&]<typename Storage>(Storage& trace) {
        if constexpr (std::is_same_v<Storage, EnabledTraceStorage>) {
            if (trace.count != 0U) {
                trace.entries.at(trace.head).status = _result.status;
            }
        }
    }(_trace);

    // Completion failures are detected in ISR context, so they are reported here
    // on the main loop. _error_logged keeps the timeout path from logging twice.
    const bool failed = (_result.status != nv::spi::bm::SpiStatus::Ok)
                     && (_result.status != nv::spi::bm::SpiStatus::Busy);
    if (failed && !_error_logged) {
        const auto reason = (_fail_reason == nv::spi::bm::SpiErrorReason::None)
                              ? nv::spi::bm::SpiErrorReason::XferComplete
                              : _fail_reason;
        log_error(_result.status, reason, _result.read_length, _write_length, _last_sdk_status);
    }

    log_followup_trace();
    return _result;
}

void Driver::callback(status_t sdk_status)
{
    _result.status   = get_status(sdk_status);
    _last_sdk_status = static_cast<uint32_t>(sdk_status);

    if (_result.status != nv::spi::bm::SpiStatus::Ok) {
        const auto reason = (_fail_reason == nv::spi::bm::SpiErrorReason::None)
                              ? nv::spi::bm::SpiErrorReason::XferComplete
                              : _fail_reason;
        trace_error(_result.status, reason);
    }

    if constexpr (nv::spi::WaitGpioSpiWar) {
        if (_recovery_dummy_active) {
            complete_recovery();
            return;
        }

        if (_is_wait_gpio_transaction) {
            if ((_wait_gpio_flags.load(std::memory_order_acquire) & WaitGpioEvents::ReadDone)
                != WaitGpioEvents::ReadDone) {
                if (_result.status == nv::spi::bm::SpiStatus::Ok) {
                    trace_phase(TraceWriteDone | TraceGpioWait);
                }
                // Need to deassert CS between write and read to avoid bus contention
                deassert_cs(_current_cs);
                _wait_gpio_timeout_active = false;
                _wait_gpio_flags.fetch_or(WaitGpioEvents::WriteDone, std::memory_order_acq_rel);
                try_read_after_wait();
                return;
            }
            if (_result.status == nv::spi::bm::SpiStatus::Ok) {
                trace_phase(TraceReadDone);
            }
            _is_wait_gpio_transaction = false;
        }
        else if (_result.status == nv::spi::bm::SpiStatus::Ok) {
            trace_phase((_write_length > 0U ? TraceWriteDone : 0U)
                        | (_result.read_length > 0U ? TraceReadDone : 0U));
        }

        if (_result.flags & nv::spi::bm::SpiFlags::CsDeassert) {
            deassert_cs(_current_cs);
        }

        if (_result.status == nv::spi::bm::SpiStatus::Ok) {
            if (_result.read_length > 0U) {
                // SPI read finished. Hold the LSTP response until READY_N goes
                // high or the 1 s WaitReadyAssert timer expires.
                _gpio_war_state           = GpioWarState::WaitReadyAssert;
                _wait_gpio_timeout_active = false;
                _is_busy                  = false;
                return;
            }
            if (_write_length > 0U) {
                _gpio_war_state           = GpioWarState::RequestForwarded;
                _wait_gpio_timeout_active = false;
                _forwarded_timed_out      = false;
            }
        }
        else if (_result.read_length > 0U) {
            enter_recovery();
        }
        else {
            _gpio_war_state = GpioWarState::Idle;
        }

        APP_EVENT_SET(*_event, _event_bit);
        return;
    }

    if (_result.status == nv::spi::bm::SpiStatus::Ok) {
        trace_phase((_write_length > 0U ? TraceWriteDone : 0U)
                    | (_result.read_length > 0U ? TraceReadDone : 0U));
    }

    if (_result.flags & nv::spi::bm::SpiFlags::CsDeassert) {
        deassert_cs(_current_cs);
    }

    APP_EVENT_SET(*_event, _event_bit);
}

void Driver::master_callback([[maybe_unused]] LPSPI_Type*            base,
                             [[maybe_unused]] lpspi_master_handle_t* handle,
                             status_t                                status,
                             void*                                   userData)
{
    auto ctx = static_cast<MasterTransferContext*>(userData);
    static_cast<Driver*>(ctx->driver)->callback(status);
}

void Driver::spi_gpio_wait_complete()
{
    if constexpr (nv::spi::WaitGpioSpiWar) {
        if (_is_wait_gpio_transaction) {
            _wait_gpio_timeout_active = false;
            trace_phase(TraceGpioDone);
            _wait_gpio_flags.fetch_or(WaitGpioEvents::GpioWaitDone, std::memory_order_acq_rel);
            try_read_after_wait();
        }
    }
}

void Driver::check_gpio_wait_timeout(uint32_t now_ms, uint32_t timeout_ms)
{
    if constexpr (nv::spi::WaitGpioSpiWar) {
        if (timeout_ms == 0U) {
            _wait_gpio_timeout_active = false;
            return;
        }

        if (!is_gpio_wait_pending()) {
            return;
        }

        if (!_wait_gpio_timeout_active) {
            _wait_gpio_timeout_active = true;
            _wait_gpio_start_ms       = now_ms;
            return;
        }

        if ((now_ms - _wait_gpio_start_ms) < timeout_ms) {
            return;
        }

        log_timeout(now_ms - _wait_gpio_start_ms,
                    nv::spi::bm::SpiErrorReason::WaitGpioReadyTimeout);

        _wait_gpio_timeout_active = false;
        _is_wait_gpio_transaction = false;
        _wait_gpio_flags.store(0U, std::memory_order_release);
        _result.status = nv::spi::bm::SpiStatus::EventTimeout;
        _fail_reason   = nv::spi::bm::SpiErrorReason::WaitGpioReadyTimeout;
        deassert_cs(_current_cs);
        APP_EVENT_SET(*_event, _event_bit);
        enter_recovery();
    }
}

void Driver::fail_gpio_idle_timeout(uint32_t elapsed_ms)
{
    if constexpr (nv::spi::WaitGpioSpiWar) {
        if (_result.status != nv::spi::bm::SpiStatus::Ok) {
            return;
        }

        log_timeout(elapsed_ms, nv::spi::bm::SpiErrorReason::WaitGpioIdleTimeout);
        _result.status = nv::spi::bm::SpiStatus::EventTimeout;
        _fail_reason   = nv::spi::bm::SpiErrorReason::WaitGpioIdleTimeout;
    }
}

bool Driver::is_gpio_wait_pending() const
{
    if constexpr (nv::spi::WaitGpioSpiWar) {
        const uint8_t flags = _wait_gpio_flags.load(std::memory_order_acquire);
        return _is_busy && _is_wait_gpio_transaction
            && ((flags & WaitGpioEvents::WriteDone) != 0U)
            && ((flags & WaitGpioEvents::GpioWaitDone) == 0U)
            && ((flags & WaitGpioEvents::ReadDone) == 0U);
    }

    return false;
}

bool Driver::is_busy()
{
    return _is_busy;
}

bool Driver::is_recovery() const
{
    if constexpr (nv::spi::WaitGpioSpiWar) {
        return (_gpio_war_state == GpioWarState::Recovery) || _recovery_dummy_active
            || _recovery_dummy_pending;
    }
    return false;
}

bool Driver::is_awaiting_ready_release() const
{
    if constexpr (nv::spi::WaitGpioSpiWar) {
        return _gpio_war_state == GpioWarState::WaitReadyAssert;
    }
    return false;
}

void Driver::try_read_after_wait()
{
    if constexpr (nv::spi::WaitGpioSpiWar) {
        uint8_t expected = WaitGpioEvents::Ready;
        if (_wait_gpio_flags.compare_exchange_strong(
                expected, WaitGpioEvents::ReadDone, std::memory_order_acq_rel)) {
            _wait_gpio_timeout_active = false;
            trace_phase(TraceReadStart);
            deassert_all_cs();
            assert_cs(_current_cs);

            lpspi_transfer_t masterXfer;
            masterXfer.txData      = nullptr;
            masterXfer.rxData      = _result.buffer.data();
            masterXfer.dataSize    = _result.read_length;
            masterXfer.configFlags = kLPSPI_MasterPcs0 | kLPSPI_MasterPcsContinuous
                                   | kLPSPI_MasterByteSwap;

            const status_t Status = LPSPI_MasterTransferNonBlocking(
                _base, &_handle, &masterXfer);
            if (Status != kStatus_Success) {
                _fail_reason = nv::spi::bm::SpiErrorReason::ReadStartFail;
                callback(Status);
            }
        }
    }
}

void Driver::stop_timer()
{
    _wait_gpio_timeout_active = false;
}

bool Driver::timer_expired(uint32_t now_ms, uint32_t timeout_ms)
{
    if (timeout_ms == 0U) {
        _wait_gpio_timeout_active = false;
        return false;
    }

    if (!_wait_gpio_timeout_active) {
        _wait_gpio_timeout_active = true;
        _wait_gpio_start_ms       = now_ms;
        return false;
    }

    return (now_ms - _wait_gpio_start_ms) >= timeout_ms;
}

void Driver::enter_recovery()
{
    stop_timer();
    _gpio_war_state           = GpioWarState::Recovery;
    _is_wait_gpio_transaction = false;
    _wait_gpio_flags.store(0U, std::memory_order_release);
    _recovery_dummy_pending = true;
}

void Driver::complete_recovery()
{
    deassert_cs(_current_cs);
    _recovery_dummy_active    = false;
    _recovery_dummy_pending   = false;
    _is_busy                  = false;
    _is_wait_gpio_transaction = false;
    _forwarded_timed_out      = false;
    _result.read_length       = 0U;
    _gpio_war_state           = GpioWarState::Idle;
}

void Driver::start_recovery_dummy_read()
{
    static_assert(RecoveryDummyReadLength <= nv::spi::bm::BufferSize);

    stop_timer();
    _gpio_war_state           = GpioWarState::Recovery;
    _recovery_dummy_pending   = false;
    _recovery_dummy_active    = true;
    _forwarded_timed_out      = false;
    _is_wait_gpio_transaction = false;
    _is_busy                  = true;
    _result.status            = nv::spi::bm::SpiStatus::Ok;
    _result.read_length       = RecoveryDummyReadLength;
    _write_length             = 0U;
    _fail_reason              = nv::spi::bm::SpiErrorReason::None;
    _error_logged             = false;

    deassert_all_cs();
    assert_cs(_current_cs);

    lpspi_transfer_t masterXfer;
    masterXfer.txData      = nullptr;
    masterXfer.rxData      = _result.buffer.data();
    masterXfer.dataSize    = RecoveryDummyReadLength;
    masterXfer.configFlags = kLPSPI_MasterPcs0 | kLPSPI_MasterPcsContinuous
                           | kLPSPI_MasterByteSwap;

    const status_t Status = LPSPI_MasterTransferNonBlocking(_base, &_handle, &masterXfer);
    if (Status != kStatus_Success) {
        complete_recovery();
    }
}

bool Driver::gpio_war_reject(nv::spi::bm::CsPins      cs,
                             uint32_t                 read_length,
                             const std::span<uint8_t> send_buffer,
                             nv::spi::bm::SpiFlags    flags,
                             nv::spi::bm::SpiStatus&  status)
{
    const bool is_write  = !send_buffer.empty();
    const bool is_read   = read_length > 0U;
    const bool ready_low = nv::gpio::bm::Driver::read(_ready_gpio_port, _ready_gpio_pin) == 0U;

    if (_recovery_dummy_active || (_gpio_war_state == GpioWarState::Recovery)) {
        status = nv::spi::bm::SpiStatus::Busy;
        return true;
    }

    switch (_gpio_war_state) {
        case GpioWarState::Idle: {
            if (is_write) {
                if (ready_low) {
                    reject_gpio_busy(cs, read_length, send_buffer.size(), flags);
                    enter_recovery();
                    status = nv::spi::bm::SpiStatus::Busy;
                    return true;
                }
                return false;
            }
            if (is_read) {
                reject_immediate(cs,
                                 read_length,
                                 send_buffer.size(),
                                 flags,
                                 nv::spi::bm::SpiStatus::Error,
                                 nv::spi::bm::SpiErrorReason::UnexpectedRead);
                if (ready_low) {
                    enter_recovery();
                }
                status = nv::spi::bm::SpiStatus::Error;
                return true;
            }
            return false;
        }
        case GpioWarState::RequestForwarded: {
            if (is_write) {
                reject_immediate(cs,
                                 read_length,
                                 send_buffer.size(),
                                 flags,
                                 nv::spi::bm::SpiStatus::Busy,
                                 nv::spi::bm::SpiErrorReason::UnexpectedWrite);
                enter_recovery();
                status = nv::spi::bm::SpiStatus::Busy;
                return true;
            }
            if (is_read) {
                if (_forwarded_timed_out) {
                    _forwarded_timed_out = false;
                    _result.status       = nv::spi::bm::SpiStatus::EventTimeout;
                    _fail_reason         = nv::spi::bm::SpiErrorReason::WaitGpioReadyTimeout;
                    if (!_error_logged) {
                        log_timeout(0U, nv::spi::bm::SpiErrorReason::WaitGpioReadyTimeout);
                    }
                    enter_recovery();
                    status = nv::spi::bm::SpiStatus::EventTimeout;
                    return true;
                }
                if (!ready_low) {
                    status = nv::spi::bm::SpiStatus::Busy;
                    return true;
                }
                stop_timer();
                return false;
            }
            status = nv::spi::bm::SpiStatus::Error;
            return true;
        }
        case GpioWarState::WaitReadyAssert: {
            status = nv::spi::bm::SpiStatus::Busy;
            return true;
        }
        case GpioWarState::Recovery: {
            status = nv::spi::bm::SpiStatus::Busy;
            return true;
        }
    }

    status = nv::spi::bm::SpiStatus::Error;
    return true;
}

void Driver::poll_gpio_war([[maybe_unused]] uint32_t now_ms,
                           [[maybe_unused]] uint32_t ready_timeout_ms,
                           [[maybe_unused]] uint32_t idle_timeout_ms,
                           [[maybe_unused]] uint32_t recovery_timeout_ms)
{
    if constexpr (!nv::spi::WaitGpioSpiWar) {
        return;
    }

    if (is_gpio_wait_pending()) {
        if (nv::gpio::bm::Driver::read(_ready_gpio_port, _ready_gpio_pin) == 0U) {
            spi_gpio_wait_complete();
        }
        else {
            check_gpio_wait_timeout(now_ms, ready_timeout_ms);
        }
        return;
    }

    if (_gpio_war_state == GpioWarState::RequestForwarded) {
        if (!_is_busy && !_forwarded_timed_out && timer_expired(now_ms, ready_timeout_ms)) {
            log_timeout(now_ms - _wait_gpio_start_ms,
                        nv::spi::bm::SpiErrorReason::WaitGpioReadyTimeout);
            _forwarded_timed_out = true;
        }
    }
    else if (_gpio_war_state == GpioWarState::WaitReadyAssert) {
        if (nv::gpio::bm::Driver::read(_ready_gpio_port, _ready_gpio_pin) != 0U) {
            stop_timer();
            _gpio_war_state = GpioWarState::Idle;
            APP_EVENT_SET(*_event, _event_bit);
        }
        else if (timer_expired(now_ms, idle_timeout_ms)) {
            fail_gpio_idle_timeout(now_ms - _wait_gpio_start_ms);
            APP_EVENT_SET(*_event, _event_bit);
            enter_recovery();
        }
    }

    if (_recovery_dummy_pending && !_recovery_dummy_active && !_is_busy
        && (APP_EVENT_CHECK(*_event, _event_bit) == 0U)) {
        if (nv::gpio::bm::Driver::read(_ready_gpio_port, _ready_gpio_pin) == 0U) {
            stop_timer();
            start_recovery_dummy_read();
        }
        else if (timer_expired(now_ms, recovery_timeout_ms)) {
            log_timeout(now_ms - _wait_gpio_start_ms,
                        nv::spi::bm::SpiErrorReason::RecoveryReadyTimeout);
            complete_recovery();
        }
    }
}

void Driver::deassert_all_cs()
{
    for (size_t i = 0; i < nv::spi::bm::SpiMaxCsPins; ++i) {
        deassert_cs(static_cast<nv::spi::bm::CsPins>(i));
    }
}

void Driver::assert_cs(nv::spi::bm::CsPins cs)
{
    const auto& [port, pin] = _cs_gpios.at(static_cast<size_t>(cs));
    if (port == nv::gpio::InvalidGpioPort || pin == nv::gpio::InvalidGpioPin) {
        return;
    }
    nv::gpio::bm::Driver::write(port, pin, 0U);
}

void Driver::deassert_cs(nv::spi::bm::CsPins cs)
{
    const auto& [port, pin] = _cs_gpios.at(static_cast<size_t>(cs));
    if (port == nv::gpio::InvalidGpioPort || pin == nv::gpio::InvalidGpioPin) {
        return;
    }
    nv::gpio::bm::Driver::write(port, pin, 1U);
}

}  // namespace sys::spi::bm
