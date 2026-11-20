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
#include <functional>
#include <optional>
#include <span>

#include "nv/i2c/common.h"
#include "nv/i2c/eeprom_cache.h"
#include "nv/i2c/i2c_forward.h"
#include "nv/i2c/port.h"
#include "nv/i2c/queue_map.h"
#include "nv/i2c/recovery.h"
#include "nv/ipc/queue.h"
#include "nv/ipc/task.h"
#include "nv/ipc/tasknotify.h"
#include "nv/ipc/timer.h"
#include "nv/mctp/interface.h"
#include "nv/mctp/router.h"
#include "nv/perf_mon/perf_mon.h"
#include "sys/i2c/i2c.h"
namespace nv::i2c {

class Task : public nv::ipc::Task
{
public:
    static constexpr size_t  DataSize                = nv::i2c::I2cBufferSize + 12;
    static constexpr uint8_t DynAddr                 = 0;
    static constexpr uint8_t ByteMask                = 0xFF;
    static constexpr uint8_t MaxSMBusBlockReadLength = 33;

    enum class Error : uint8_t
    {
        Ok,
        CtrlBusBusy,
        CtrlNak,
        CtrlArbLost,
        CtrlError,
        CtrlTimeout,
        TargetRxError,
    };

    struct Config
    {
        nv::ipc::TaskId          task_id;
        std::string_view         task_name;
        nv::mctp::Client         client;
        Port                     port_id;
        uint8_t                  target_addr;
        nv::ipc::BootedEventBits boot_event;
        nv::ipc::TimerId         timer_id;
        nv::ipchandler::Id       ipchandler_id;
        nv::ipc::TimerId         repeated_start_timer_id;
        // MCTP endpoint client driven by the AP-status probe. Defaults to `client`
        // (probe target == data path). Set explicitly when the liveness probe runs on
        // this I2C bus but must gate a different endpoint (e.g. an EROT whose MCTP data
        // path is on SPI: probe over I2C, gate the Spi0 endpoint).
        nv::mctp::Client ap_status_client;
        // When true, the AP-status liveness probe uses a payload-free i2cdetect-style ping
        // (address-phase ACK only) instead of the MCTP-over-SMBus GetEndpointID. Required for
        // targets that are not MCTP-over-SMBus endpoints (e.g. EROT whose MCTP path is on SPI),
        // where sending MCTP control traffic on the I2C bus is invalid and can wedge the
        // device.
        bool plain_status_probe;
        // Inverts the meaning of the probe result. When false (default), an ACK means the
        // endpoint is up. When true, the probe address is a recovery-mode indicator: an ACK
        // means the target is in recovery and its real data path (e.g. EROT MCTP over SPI)
        // is DOWN (remove the endpoint), and a NAK means the data path is expected to work
        // (enumerate the endpoint). Used for the EROT recovery-mode address (0x63).
        bool probe_ack_is_down;
        // Dedicated address for the AP-status liveness probe, when it differs from the
        // task's device address `target_addr`. 0 means "use target_addr". Set for the EROT
        // whose device/LSTP/recovery address is 0x62 but whose recovery-mode indicator (the
        // probe target) is 0x63; keeping them separate leaves recovery/TX on 0x62 untouched.
        uint8_t ap_probe_addr;
        // Task that owns emulation devices, used for SlaveFunction::Emulation
        nv::ipc::TaskId emul_task_id;
        constexpr Config(nv::ipc::TaskId          task_id_,
                         std::string_view         task_name_,
                         nv::mctp::Client         client_,
                         Port                     port_id_,
                         uint8_t                  target_addr_,
                         nv::ipc::BootedEventBits boot_event_,
                         nv::ipc::TimerId         timer_id_        = nv::ipc::TimerId::End,
                         nv::ipchandler::Id       ipchandler_id_   = nv::ipchandler::Id::Unuse,
                         nv::ipc::TimerId repeated_start_timer_id_ = nv::ipc::TimerId::End,
                         nv::mctp::Client ap_status_client_        = nv::mctp::Client::End,
                         bool             plain_status_probe_      = false,
                         bool             probe_ack_is_down_       = false,
                         uint8_t          ap_probe_addr_           = 0,
                         nv::ipc::TaskId  emul_task_id_            = nv::ipc::TaskId::End)
        : task_id(task_id_)
        , task_name(task_name_)
        , client(client_)
        , port_id(port_id_)
        , target_addr(target_addr_)
        , boot_event(boot_event_)
        , timer_id(timer_id_)
        , ipchandler_id(ipchandler_id_)
        , repeated_start_timer_id(repeated_start_timer_id_)
        , ap_status_client(ap_status_client_)
        , plain_status_probe(plain_status_probe_)
        , probe_ack_is_down(probe_ack_is_down_)
        , ap_probe_addr(ap_probe_addr_)
        , emul_task_id(emul_task_id_)
        {}
    };

    enum Event : nv::ipc::TaskNotify::Bits
    {
        CtrlDone          = 1_bit,
        CtrlBusBusy       = 2_bit,
        CtrlNak           = 3_bit,
        CtrlArbLost       = 4_bit,
        CtrlError         = 5_bit,
        CtrlTimeout       = 6_bit,
        CtrlFifoError     = 7_bit,
        CtrlBitError      = 8_bit,
        CtrlPinLowTimeout = 9_bit,
        CtrlDmaError      = 10_bit,
        CtrlNoTransfer    = 11_bit,
    };

    enum class RequestType : uint16_t
    {
        MctpRx,
        MctpTx,
        MctpUpdateRoutingTable,
        MctpRxError,
        UpdateSensor,
        WdtEvent,
        I2cRequest,
        I2cResponse,
        I2cRecovery,
        CheckApStatus,
        EepromUpdate,
        CheckTargetTimeout,
        VrFaultDump
    };

    enum class Status
    {
        Ok,
        InvalidInterface,
        FailToPutTxPacket,
        FailToSetEvent,
    };

    struct [[gnu::packed]] Request
    {
        RequestType type;
        uint16_t    additional_info;
        uint8_t     data[DataSize];
    };

    static void         make(Config config);
    static void         entrypoint(void* params);
    static Task::Status tx(const nv::mctp::Packet& packet, uint16_t additional_info = 0);
    static Task::Status update_routing_table(mctp::Client client);
    static void         update_sensor(nv::ipc::Timer& timer);
    static void         refresh_smbus_cache(nv::ipc::Timer& timer);
    static void         eeprom_update(nv::ipc::Timer& timer);
    static Task::Status wdt_notify(nv::watchdog::TaskMonitorIndex taskId);
    static Task::Status send_recovery_request(RecoveryCmd cmd, mctp::Client client);
    /// Safe to call from ISR context. The owning task runs poll_vr_fault_dump() for its port.
    static Task::Status send_vr_fault_dump(nv::ipc::QueueId queue_id);
    static void         check_ap_status(nv::ipc::Timer& timer);
    static void         check_target_timeout(nv::ipc::Timer& timer);
    static void         repeated_start_timeout(nv::ipc::Timer& timer);
    static bool         to_i2c(ipchandler::Id    src_id,
                               uint8_t           address,
                               ipchandler::Id    i2c_ipchandler_id,
                               uint16_t          write_length,
                               uint16_t          read_length,
                               I2cFlags          flags,
                               ipc::Queue::Item& item);

    static bool to_i2c_recovery(ipchandler::Id src_id, ipchandler::Id i2c_ipchandler_id);

    static bool to_i2c_from_isr(ipchandler::Id    src_id,
                                uint8_t           address,
                                ipchandler::Id    i2c_ipchandler_id,
                                uint8_t           write_length,
                                uint16_t          read_length,
                                ipc::Queue::Item& item);

    Task(Config config) noexcept;
    [[noreturn]] void                 start();
    void                              rx(std::span<uint8_t> buffer);
    void                              rx_error();
    Task::Status                      set_event(Event event);
    EepromCache<nv::ipc::EepromSize>& eeprom_cache()
    {
        NV_ASSERT(_cache.has_value());
        return _cache.value().get();
    }
    void set_cache(EepromCache<nv::ipc::EepromSize>& cache) { _cache = cache; }
    void handle_erot_recovery(uint8_t reg, uint8_t value);

    /// I2cForward shadow home for forwards whose destination bus is owned by
    /// this task; sits inside the task's private MPU block by construction
    /// (see make_task_by_port), so no shared RAM is needed. The source-port
    /// ISR reaches it via the per-project weak sys::i2c::forward_task_id()
    /// (EepromTaskId pattern).
    nv::i2c::forward::State& forward_state() { return _forward_state; }

private:
    /// add 1 byte for PEC
    constexpr static size_t MctpI2cpayload = sizeof(nv::mctp::Packet::msg) + 1;
    using RoutingTable = std::array<mctp::ShardRoutingTable, ipc::RoutingTableSize>;

    struct [[gnu::packed]] I2cHeader
    {
        uint8_t rw_dst   : 1;
        uint8_t dst_addr : 7;
        uint8_t cmd_code;
        uint8_t byte_cnt;
        uint8_t ipmi_src : 1;
        uint8_t src_addr : 7;
    };

    struct [[gnu::packed]] I2cPacket
    {
        I2cHeader        i2c_hdr;
        nv::mctp::Header mctp_hdr;
        uint8_t          msg[MctpI2cpayload];
    };

    enum class APStatus : uint8_t
    {
        Querying,
        ApDown,
        ApUpNotEnumerated,
        ApUpEnumerated
    };

    RoutingTable                        _routing_table{};
    nv::mctp::Client                    _client;
    nv::mctp::Client                    _ap_status_client;
    nv::ipc::QueueId                    _queue_id;  // Primary queue
    Port                                _port;
    sys::i2c::Driver                    _driver;
    std::array<uint8_t, UINT8_MAX + 1U> _eid_addr_map;
    uint8_t                             _target_addr;
    nv::ipc::BootedEventBits            _boot_event;
    nv::ipc::Timer                      _timer;
    nv::ipc::Timer                      _repeated_start_timer;
    nv::ipc::Timer                      _target_timeout_timer;
    nv::ipchandler::Id                  _ipchandler_id;
    bool                                _has_slave_function{false};
    APStatus                            _ap_status{APStatus::Querying};
    // Consecutive failed I2C probes required before declaring the AP down. Debounces
    // transient NAKs since the probe bus (I2C) differs from the EROT data path (SPI).
    static constexpr uint8_t ProbeDownThreshold = 3;
    uint8_t                  _probe_miss_count{0};
    // Consecutive successful I2C probes required before declaring the AP up (and
    // triggering (re-)enumeration). The I2C interface recovers faster than the EROT's
    // SPI MCTP path after a reset, so requiring more than one consecutive alive probe
    // gives the SPI path time to settle before SetEID is sent.
    static constexpr uint8_t ProbeUpThreshold = 2;
    uint8_t                  _probe_hit_count{0};
    // Use a payload-free i2cdetect-style ping (vs MCTP-over-SMBus GetEndpointID) for the
    // AP-status liveness probe. Set for non-MCTP-over-SMBus targets (e.g. EROT on SPI).
    bool _plain_status_probe{false};
    // Invert the probe result: an ACK means the target is in recovery mode (its real data
    // path is down, remove the endpoint) and a NAK means the data path should work
    // (enumerate the endpoint). Set for the EROT recovery-mode address (0x63).
    bool _probe_ack_is_down{false};
    // Dedicated AP-status probe address; 0 means "use _target_addr". Keeps the probe target
    // (EROT 0x63) separate from the device/LSTP/recovery address (_target_addr, EROT 0x62).
    uint8_t _ap_probe_addr{0};
    // True only when the bus is known to be in an idle/STOP state. Cleared while an LSTP
    // transaction is in progress and on any error path that may leave the bus held (e.g. a
    // failed NoStop/repeated-start sequence); set again once a STOP-terminated transaction
    // completes or the bus is forced back to idle. The AP-status liveness ping is gated on
    // this so it can never drive a START into a half-finished LSTP block/repeated-start.
    bool _bus_idle{true};

    static nv::ipc::Queue::Status
    send_request(nv::ipc::QueueId      queue_id,
                 const Request&        request,
                 nv::ipc::Queue::Usecs timeout = nv::ipc::Queue::Usecs::max(),
                 bool                  is_isr  = false);

    void handle_rx(std::span<uint8_t> buffer);
    void handle_tx(std::span<uint8_t> buffer, uint16_t additional_info);
    void handle_update_sensor(std::span<uint8_t> buffer);
    void handle_wdt_event();
    bool process_rx_packet(std::span<uint8_t> buffer,
                           nv::mctp::Packet&  packet,
                           uint8_t&           command_code);
    bool
    process_tx_packet(std::span<uint8_t> buffer, I2cPacket& packet, uint16_t additional_info);
    void handle_update_routing_table();
    void forward(nv::mctp::Packet& packet, uint8_t command_code);
    bool transmit(const I2cPacket& packet);
    void handle_error(I2cStatus status, uint8_t src_id = UINT8_MAX);
    void set_tmp_sensor();
    void handle_i2c_request(std::span<uint8_t> buffer);
    bool handle_eeprom_cache_request(const I2cRequest&  request,
                                     std::span<uint8_t> read_buffer,
                                     size_t             write_len,
                                     size_t             read_len);
    void handle_i2c_response(std::span<uint8_t> buffer);
    void handle_i2c_recovery(std::span<uint8_t> buffer);

    I2cStatus smbus_block_read_wrapper(uint8_t            address,
                                       std::span<uint8_t> write_buffer,
                                       std::span<uint8_t> read_buffer,
                                       I2cFlags           flags,
                                       size_t&            read_len);
    I2cStatus i2c_cmd(uint8_t            address,
                      std::span<uint8_t> write_buffer,
                      std::span<uint8_t> read_buffer,
                      I2cFlags           flags,
                      size_t&            read_len);
    /// After a successful xfer: keep the bus held (restart the repeated-start
    /// timer) or mark it idle. Errors are returned unchanged.
    I2cStatus hold_or_idle(I2cStatus status, I2cFlags flags);
    I2cStatus wait_for_i2c_completion();
    void      handle_ap_status(APStatus set_status);
    // True for the decoupled EROT-style liveness probe: a dedicated probe address
    // (_ap_probe_addr, e.g. EROT 0x63) on an I2C bus that is not the MCTP data path.
    // Selects the dedicated handle_erot_liveness() path and keeps the common
    // single-address probe used by every other product unchanged.
    bool is_erot_ap_probe() const { return _ap_probe_addr != 0; }
    // Common periodic liveness check used by all non-EROT products: a single
    // MCTP-over-SMBus GetEndpointID probe on _target_addr with immediate up/down (no
    // bus-idle gating, no debounce), preserving the original pre-EROT behavior.
    void handle_common_ap_status();
    // EROT-specific liveness check: bus-idle gated, two-stage 0x63/0x62 probe with inverted
    // recovery-mode semantics, fed through the shared state machine with miss-count debounce.
    void handle_erot_liveness();
    // Issue one liveness probe to `address` (payload-free ping or MCTP-over-SMBus probe
    // depending on _plain_status_probe), trace the result, and return true on ACK.
    bool probe_once(uint8_t address);
    // Feed a single liveness observation into the AP-status state machine. The thresholds
    // select how many consecutive misses or hits are required before changing state. Common
    // products pass 1 for immediate transitions; the EROT path uses debounce thresholds.
    void apply_ap_liveness(bool alive, uint8_t down_threshold, uint8_t up_threshold);
    // Emit the status change to the log and (de)enumerate the AP-status endpoint on the
    // MCTP driver when the resolved status differs from the current one.
    void commit_ap_status(APStatus status);
    void handle_check_target_timeout();
    void handle_eeprom_update();

    // OOB Bus error telemetry
    nv::perf_mon::OobBus                                                    _oob_bus;
    std::optional<std::reference_wrapper<EepromCache<nv::ipc::EepromSize>>> _cache;
    nv::i2c::forward::State                                                 _forward_state{};
};

template<uint16_t CacheSize>
struct TaskWithCache
{
    EepromCache<CacheSize> cache;
    Task                   task;

    explicit TaskWithCache(Task::Config config) : cache(), task(config)
    {
        cache.init();
        if constexpr (CacheSize == nv::ipc::EepromSize) {
            task.set_cache(cache);
        }
    }
};

// Template function to create an I2C task for a specific port
template<Port PortId>
static void make_task_by_port(Task::Config config)
{
    // leave ~232/~208 bytes of margin above the worst-case observed stack usage
    constexpr auto i2c_task_stack_size = (nv::lstp::EnableI2c && !nv::lstp::I2cSmallBuffer)
                                           ? 864
                                           : 672;
    constexpr auto StackSize = std::max(i2c_task_stack_size, int(configMINIMAL_STACK_SIZE));
    NV_STACK static sys::ipc::TaskStack<StackSize> stack;
    if constexpr (nv::ipc::EnableEepromBridge) {
        constexpr uint16_t CacheSize = (PortId == nv::ipc::EepromDstPort) ? nv::ipc::EepromSize
                                                                          : MinEepromCacheSize;
        NV_TASK_DATA static TaskWithCache<CacheSize> data(config);
        // NOLINTNEXTLINE(*-reinterpret-cast)
        const std::span<uint8_t> priv(reinterpret_cast<uint8_t*>(&data), sizeof(data));
        data.task.setup(stack.span(), priv, Task::Priority::I2c, Task::entrypoint);
    }
    else {
        NV_TASK_DATA static Task task(config);
        // NOLINTNEXTLINE(*-reinterpret-cast)
        const std::span<uint8_t> priv(reinterpret_cast<uint8_t*>(&task), sizeof(task));
        task.setup(stack.span(), priv, Task::Priority::I2c, Task::entrypoint);
    }
}

void on_ap_status_endpoint_change(uint8_t endpoint_index, uint8_t status);

/// Called by the task that owns `port` when it handles a VrFaultDump request, so VR controller
/// reads stay serialized with LSTP traffic on that bus. `bus_idle` is false while a transfer is
/// held open with NoStop or after a failed transfer. Weak no-op by default.
void poll_vr_fault_dump(Port port, bool bus_idle);

}  // namespace nv::i2c
