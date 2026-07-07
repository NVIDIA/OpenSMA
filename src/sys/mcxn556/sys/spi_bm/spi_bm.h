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
#include <span>

#include "fsl_lpspi.h"

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

    bool is_busy();

private:
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
    enum WaitGpioEvents : uint8_t
    {
        WriteDone    = 1_bit,
        GpioWaitDone = 2_bit,
        ReadDone     = 3_bit,
        Ready        = WriteDone | GpioWaitDone,
    };
    std::atomic<uint8_t> _wait_gpio_flags{0};
    void                 try_read_after_wait();
};

}  // namespace sys::spi::bm
