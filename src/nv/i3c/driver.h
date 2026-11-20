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
#include <cstdint>
#include <span>

#include "nv/common/literals.h"
#include "nv/i2c/common.h"
#include "nv/i3c/driver_status.h"
#include "nv/ipc/event.h"
#include "nv/ipchandler/enums.h"
#include "nv/lstp/lstp_common.h"
#include "nv/perf_mon/perf_mon.h"
#include "sys/i3c/driver.h"

namespace nv::i3c {

/**
 * True when the project puts an I2C bus behind an I3C controller, which is the only situation
 * where Task::to_i2c has to fit a whole nv::i2c::I2cRequest into an I3C buffer. Deriving this
 * from tables every project config already declares keeps a large nv::i2c::I2cBufferSize from
 * inflating every I3C queue item on projects that never forward I2C over I3C.
 *
 * The three routes that can hand an I3C handler id to usb::send_to_i2c_backend or
 * LstpRouter::receive_i2c are an LSTP channel of type I2c, a CP2112 virtual address mapping,
 * and the unmapped-address fallback. The fallback is counted even when I2cManualNackMode makes
 * it unreachable: erring toward the larger buffer keeps this a sizing decision rather than a
 * behavioural one for projects that still name an I3C handler there.
 */
constexpr bool EnableI2cOverI3c = []() constexpr {
    auto names_i3c = [](ipchandler::Id id) constexpr {
        return id == ipchandler::Id::I3c0 || id == ipchandler::Id::I3c1;
    };
    for (const auto& entry : nv::lstp::LstpChannels) {
        if (entry.info.type == nv::lstp::LstpChannelType::I2c
            && names_i3c(static_cast<ipchandler::Id>(entry.info.id))) {
            return true;
        }
    }
    for (const auto& item : nv::ipc::I2cVirtualAddressMappingTable) {
        if (names_i3c(item.ipchandler_id)) {
            return true;
        }
    }
    return names_i3c(nv::ipc::I2cDefaultInhandlerId);
}();

class Driver : protected sys::i3c::Driver
{
public:
    /// Largest MCTP-over-I3C payload the task moves through a queue item.
    constexpr static size_t MctpBufferSize = 71;
    constexpr static size_t BufferSize     = EnableI2cOverI3c
                                               ? std::max<size_t>(MctpBufferSize,
                                                              sizeof(nv::i2c::I2cRequest))
                                               : MctpBufferSize;

    static constexpr size_t  IbiDataSize   = 8;
    static constexpr uint8_t TargetAddress = 0x31;
    using I3cBuffer                        = std::array<uint8_t, Driver::BufferSize>;
    using IbiBuffer                        = std::array<uint8_t, Driver::IbiDataSize>;
    using Status                           = nv::i3c::DriverStatus;

    enum class Port : uint8_t
    {
        Zero,
        One,
    };

    enum Event : nv::ipc::Event::Bits
    {
        Success = 0_bit,
        Nack    = 1_bit,
        Error   = 2_bit,
    };

    struct Freq
    {
        uint32_t i2c;
        uint32_t i2c_od;
        uint32_t i3c_pp;
    };

    Driver(Port             port,
           Freq             freq,
           bool             is_gpu,
           void*            task,
           nv::ipc::EventId event_id,
           uint32_t         clock = 0);
    constexpr Port port() const { return _port; }
    constexpr bool is_gpu() const { return _is_gpu; }
    Status         last_status() const { return _last_status; }
    void           set_event(Event event);
    void           init();
    void           deinit();
    // CCC
    bool force_bus_idle();
    bool disec();
    bool reset_daa(bool ping);
    bool get_status(uint8_t address, uint16_t& status);
    bool process_daa(std::span<uint8_t> address_list);
    bool enec();
    bool enec(uint8_t address);
    // Register the target address without changing the current IBI response.
    // The address is remembered so init() can re-arm IBI delivery after a
    // controller reset.
    bool register_ibi(uint8_t address);
    // Set the controller's default IBI response to NACK.
    void set_ibi_nack();
    // Answer target IBIs and read their mandatory data byte.
    void set_ibi_ack_mandatory();
    bool set_mrl_mwl(uint8_t address);
    // Re-arm the strong (deinit/init) bus recovery. Called before a fresh DAA
    // pass so recovery never permanently gives up while the bus is down.
    void reset_recovery_count() { _consecutive_recoveries = 0; }
    // If the I3C ISR detected a SLVSTART interrupt storm and disabled the IRQ,
    // recover the controller in task context (abort/drain/re-init/re-enable IRQ)
    // and return true. No-op returning false otherwise. Called periodically from
    // the I3C task so the storm is broken without a watchdog reset.
    bool recover_irq_storm_if_pending();
    // Write/Read/Interrupt
    bool write(uint8_t address, std::span<uint8_t> data);
    bool read(uint8_t address, std::span<uint8_t> data, uint8_t& length);
    void on_ibi(void* args);
    void on_master_request_ibi(void* args);
    // I2C APIs
    nv::i2c::I2cStatus
         i2c(uint8_t address, std::span<uint8_t> write_buffer, std::span<uint8_t> read_buffer);
    bool ocp_query_interface_mastering(uint8_t address, bool& enable);
    bool ocp_enable_interface_mastering(uint8_t address);
    bool gpu_query_i3c_mode(uint8_t address, bool& i3c);
    bool gpu_configure_cms1(uint8_t address);
    bool gpu_program_cms1(uint8_t address, std::span<uint8_t> buffer);
    bool gpu_read_cms1(uint8_t address, std::span<uint8_t> buffer);

    uint32_t                  _error_log_count{};
    uint32_t                  _i2c_error_log_count{};
    uint32_t                  _i2c_arblost_log_count{};
    static constexpr uint32_t ErrorLogThreshold        = 100;
    static constexpr uint32_t MaxConsecutiveRecoveries = 5;

    // assign a Port with a OobBus
    static nv::perf_mon::OobBus i3c_port_to_oob_bus(Port port);

protected:
    Port               _port;
    void*              _task = NULL;
    nv::ipc::Event&    _event;
    I3cBuffer          _w_buffer{};
    I3cBuffer          _r_buffer{};
    IbiBuffer          _ibi_data{};
    uint8_t            _ibi_address            = 0;
    bool               _is_gpu                 = false;
    bool               _is_init                = false;
    bool               _daa_done               = false;
    uint32_t           _consecutive_recoveries = 0;
    Status             _last_status            = Status::Success;
    Status             transfer(void* args, uint8_t& length);
    void               try_recover();
    nv::i2c::I2cStatus i2c_read(uint8_t address, std::span<uint8_t> buffer);
    nv::i2c::I2cStatus i2c_write(uint8_t address, std::span<uint8_t> buffer);
    nv::i2c::I2cStatus i2c_write_read(uint8_t            address,
                                      std::span<uint8_t> write_buffer,
                                      std::span<uint8_t> read_buffer);
    // I2C status to I3C status mapping
    Status to_driver_status(nv::i2c::I2cStatus status);

    // Sdk status to I3C status mapping
    Status to_driver_status(uint32_t status);
};

}  // namespace nv::i3c
