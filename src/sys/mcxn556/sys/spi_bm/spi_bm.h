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

#pragma once

#include <array>
#include <atomic>
#include <source_location>
#include <span>
#include <type_traits>

#include "fsl_lpspi.h"

#include "nv/spi/spi_bm_config.h"
#include NV_IPC_CONFIG_H
#include "nv/spi/port.h"
#include "nv/spi/spi_types.h"
#include "nv/gpio/common.h"
#include "ecm_types.h"
#include "nv/common/literals.h"

namespace sys::spi::bm {

using nv::operator""_bit;

// Only used to reference back to this class from the SDK callback
struct MasterTransferContext
{
    void* driver;
};

class Driver
{
public:
    /**
     * @brief Initializes the driver and binds it to a port and event.
     * @param port Port to bind to
     * @param event Event to bind to
     * @param event_bit Event bit to bind to
     * @param cs_pins Up to nv::spi::bm::SpiMaxCsPins CS GPIOs. Slots set to
     *                {InvalidGpioPort, InvalidGpioPin} are ignored at assert/deassert time, so
     *                projects with fewer CS pins can fill the unused slots with the sentinel.
     */
    void bind(nv::spi::Port                                         port,
              volatile uint32_t*                                    event,
              nv::ecm_bm::AppEvent                                  event_bit,
              std::array<nv::ipc::Gpios, nv::spi::bm::SpiMaxCsPins> cs_pins);

    /**
     * @brief Transfer data over SPI. Caller must rearm with get_result() after transfer is
     * complete.
     * @param cs CS pin to operate on for this transfer
     * @param read_length Number of bytes to capture on MISO (0 for write-only)
     * @param send_buffer Bytes to clock out on MOSI (empty for read-only)
     * @param flags Flags to configure the transfer
     * @return Status of the transfer
     */
    nv::spi::bm::SpiStatus transfer(nv::spi::bm::CsPins      cs,
                                    uint32_t                 read_length,
                                    const std::span<uint8_t> send_buffer,
                                    nv::spi::bm::SpiFlags    flags);

    /**
     * @brief Logs a write rejected because the handshake GPIO is still low.
     */
    void reject_gpio_busy(nv::spi::bm::CsPins   cs,
                          uint32_t              read_length,
                          size_t                write_length,
                          nv::spi::bm::SpiFlags flags);

    /**
     * @brief Get the result of the transfer. Driver will rearm for next transfer.
     * @return Result of the transfer. Calling transfer() after this will overwrite the result.
     */
    nv::spi::bm::SpiResult& get_result();

    /**
     * @brief Callback from SDK that has to be public. Do not use.
     * @param sdk_status Status of the transfer
     */
    void callback(status_t sdk_status);

    /**
     * @brief Wait for GPIO is complete. Will resume transfer.
     */
    void spi_gpio_wait_complete();

    /**
     * @brief Checks whether an active wait-GPIO transfer timed out.
     */
    void check_gpio_wait_timeout(uint32_t now_ms, uint32_t timeout_ms);

    /**
     * @brief Marks a completed read as timed out because READY_N never returned high.
     */
    void fail_gpio_idle_timeout(uint32_t elapsed_ms);

    /**
     * @brief Returns true when a transfer is blocked waiting for the GPIO handshake.
     */
    bool is_gpio_wait_pending() const;

    /**
     * @brief Poll READY_N, timers, and recovery. No-op unless WaitGpioSpiWar.
     */
    void poll_gpio_war(uint32_t now_ms,
                       uint32_t ready_timeout_ms,
                       uint32_t idle_timeout_ms,
                       uint32_t recovery_timeout_ms);

    /**
     * @brief True while the driver is draining an orphaned response (dummy read).
     */
    bool is_recovery() const;

    /**
     * @brief True while a completed read is held until READY_N goes high.
     */
    bool is_awaiting_ready_release() const;

    bool is_busy();

private:
    struct TraceEntry;

    nv::spi::Port                                         _port;
    volatile uint32_t*                                    _event;
    nv::ecm_bm::AppEvent                                  _event_bit;
    std::array<nv::ipc::Gpios, nv::spi::bm::SpiMaxCsPins> _cs_gpios{};
    nv::spi::bm::CsPins                                   _current_cs{nv::spi::bm::CsPins::Cs0};
    bool                                                  _is_busy;

    nv::spi::bm::Buffer _tx_buffer;

    nv::spi::bm::SpiResult _result;

    MasterTransferContext _master_xfer_ctx;
    LPSPI_Type*           _base;
    lpspi_master_handle_t _handle;

    // Failure context captured where it happens (including in ISR context) and
    // logged later from the main loop, since logging goes over C2C IPC.
    uint32_t                    _last_sdk_status{0};
    size_t                      _write_length{0};
    nv::spi::bm::SpiErrorReason _fail_reason{nv::spi::bm::SpiErrorReason::None};
    bool                        _error_logged{false};

    /// Packs the active CS index and transfer flags into one log byte.
    uint8_t cs_and_flags() const;

    void log_error(nv::spi::bm::SpiStatus      status,
                   nv::spi::bm::SpiErrorReason reason,
                   size_t                      read_length,
                   size_t                      write_length,
                   uint32_t                    sdk_status,
                   const std::source_location& location = std::source_location::current());

    void log_timeout(uint32_t                    elapsed_ms,
                     nv::spi::bm::SpiErrorReason reason,
                     const std::source_location& location = std::source_location::current());

    /// Records one LSTP SPI call in the diagnostic history.
    void begin_trace(uint32_t read_length, size_t write_length);

    /// Adds completed stages to the active call's trace.
    void trace_phase(uint8_t phases);

    /// Captures the terminal status and reason for the active call.
    void trace_error(nv::spi::bm::SpiStatus status, nv::spi::bm::SpiErrorReason reason);

    /// Dumps up to four calls before the current error, then the error.
    void log_trace(const std::source_location& location = std::source_location::current());

    /// Logs one trace entry at its position relative to the error.
    void log_trace_entry(const TraceEntry&           entry,
                         uint8_t                     position,
                         const std::source_location& location);

    /// Arms logging for the next two calls after an error.
    void arm_followup_trace();

    /// Logs a completed call when post-error capture is armed.
    bool
    log_followup_trace(const std::source_location& location = std::source_location::current());

    static void master_callback(LPSPI_Type*            base,
                                lpspi_master_handle_t* handle,
                                status_t               status,
                                void*                  userData);

    static void get_dma_request_mux(nv::spi::Port port, uint32_t& tx_mux, uint32_t& rx_mux);
    static void
    get_dma_channels(nv::spi::Port port, uint32_t& tx_channel, uint32_t& rx_channel);
    static LPSPI_Type*            get_base(nv::spi::Port port);
    static nv::spi::bm::SpiStatus get_status(status_t status);

    void deassert_all_cs();
    void assert_cs(nv::spi::bm::CsPins cs);
    void deassert_cs(nv::spi::bm::CsPins cs);

    // This is all to support "Write > Wait GPIO > Read" WAR
    bool _is_wait_gpio_transaction{false};
    enum class GpioWarState : uint8_t
    {
        Idle = 0,
        RequestForwarded,
        WaitReadyAssert,
        Recovery,
    };
    enum WaitGpioEvents : uint8_t
    {
        WriteDone    = 1_bit,
        GpioWaitDone = 2_bit,
        ReadDone     = 3_bit,
        Ready        = WriteDone | GpioWaitDone,
    };
    enum TracePhases : uint8_t
    {
        TraceRequested  = 1U << 0U,
        TraceWriteStart = 1U << 1U,
        TraceWriteDone  = 1U << 2U,
        TraceGpioWait   = 1U << 3U,
        TraceGpioDone   = 1U << 4U,
        TraceReadStart  = 1U << 5U,
        TraceReadDone   = 1U << 6U,
        TraceResultRead = 1U << 7U,
    };
    struct TraceEntry
    {
        uint16_t                    sequence{0};
        uint8_t                     kind{0};
        uint8_t                     cs_flags{0};
        std::atomic<uint8_t>        phases{0};
        nv::spi::bm::SpiStatus      status{nv::spi::bm::SpiStatus::Ok};
        nv::spi::bm::SpiErrorReason reason{nv::spi::bm::SpiErrorReason::None};
    };
    static constexpr size_t  TraceDepth         = 5U;
    static constexpr uint8_t TraceErrorPosition = 4U;
    static constexpr uint8_t TraceFollowupCount = 2U;
    static constexpr uint8_t TraceFirstAfter    = TraceErrorPosition + 1U;

    struct EnabledTraceStorage
    {
        std::array<TraceEntry, TraceDepth> entries{};
        size_t                             head{0};
        size_t                             count{0};
        uint16_t                           sequence{0};
        uint16_t                           last_capture_sequence{0};
        uint8_t                            followups_remaining{0};
        uint8_t                            followup_position{TraceFirstAfter};
    };
    struct DisabledTraceStorage
    {};
    using TraceStorage = std::conditional_t<nv::spi::SpiBmConfig<>::EnableTrace,
                                            EnabledTraceStorage,
                                            DisabledTraceStorage>;

    std::atomic<uint8_t>               _wait_gpio_flags{0};
    bool                               _wait_gpio_timeout_active{false};
    uint32_t                           _wait_gpio_start_ms{0};
    GpioWarState                       _gpio_war_state{GpioWarState::Idle};
    nv::gpio::GpioPort                 _ready_gpio_port{nv::gpio::InvalidGpioPort};
    nv::gpio::GpioPin                  _ready_gpio_pin{nv::gpio::InvalidGpioPin};
    bool                               _forwarded_timed_out{false};
    bool                               _recovery_dummy_pending{false};
    bool                               _recovery_dummy_active{false};
    [[no_unique_address]] TraceStorage _trace{};
    void                               try_read_after_wait();

    void stop_timer();
    bool timer_expired(uint32_t now_ms, uint32_t timeout_ms);
    void enter_recovery();
    void start_recovery_dummy_read();
    void complete_recovery();
    bool gpio_war_reject(nv::spi::bm::CsPins      cs,
                         uint32_t                 read_length,
                         const std::span<uint8_t> send_buffer,
                         nv::spi::bm::SpiFlags    flags,
                         nv::spi::bm::SpiStatus&  status);
    void reject_immediate(nv::spi::bm::CsPins         cs,
                          uint32_t                    read_length,
                          size_t                      write_length,
                          nv::spi::bm::SpiFlags       flags,
                          nv::spi::bm::SpiStatus      status,
                          nv::spi::bm::SpiErrorReason reason);
};

}  // namespace sys::spi::bm
