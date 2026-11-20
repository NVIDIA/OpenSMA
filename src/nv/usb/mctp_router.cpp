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

#include "nv/usb/mctp_router.h"

#include <chrono>

#include "nv/i2c/task.h"
#include "nv/i3c/task.h"
#include "nv/ipc/mctp_bridge_config.h"
#include "nv/logger/log.h"
#include "nv/mctp/driver.h"
#include "nv/perf_mon/perf_mon.h"
#include "nv/spi/task.h"

using namespace std::chrono_literals;

namespace nv::usb {

RouteResult route_mctp_to_downstream(mctp::Packet& pkt, const RoutingTable& routing_table)
{
    for (auto& entry : routing_table) {
        if (entry.is_enumerated && entry.assigned_eid == pkt.hdr.dst_eid) {
            // coverity[cert_int31_c_violation] number of client < 256
            pkt.priv.packet_interface = static_cast<uint8_t>(entry.client);

            // Check if bridging is allowed (drop SetEID/RoutingInfoUpdate)
            if (!mctp::Driver::is_allow_bridge(pkt)) {
                auto& ctl_pkt = mctp::Control::PktReq::from(pkt);
                logger::info(
                    logger::Event::MctpMcuActAsBridgePacketDrop,
                    {static_cast<uint8_t>(static_cast<uint16_t>(mctp::Client::UsUsb)
                                          & UINT8_MAX),
                     static_cast<uint8_t>(static_cast<uint16_t>(mctp::Client::UsUsb) >> 8U),
                     pkt.priv.packet_interface,
                     static_cast<uint8_t>(ctl_pkt.command_code),
                     ctl_pkt.data[1]});
                return RouteResult::Dropped;
            }

            // Forward to appropriate downstream task
            if (entry.client == mctp::Client::DsI2c0 || entry.client == mctp::Client::DsI2c1
                || entry.client == mctp::Client::DsI2c2 || entry.client == mctp::Client::DsI2c3
                || entry.client == mctp::Client::DsI2c4 || entry.client == mctp::Client::DsI2c5
                || entry.client == mctp::Client::DsI2c6
                || entry.client == mctp::Client::DsI2c7) {
                auto status = i2c::Task::tx(pkt);
                if (status != i2c::Task::Status::Ok) {
                    logger::error(logger::Event::UsbCannotSend,
                                  {static_cast<uint8_t>(entry.client)});
                    return RouteResult::Dropped;
                }
            }
            else if (entry.client == mctp::Client::DsI3c0
                     || entry.client == mctp::Client::DsI3c1) {
                perf_mon::Driver::log_pkt_latency(
                    perf_mon::Driver::LatencyEvent::UsbSendTxPktToTask);
                auto status = i3c::Task::tx(pkt);
                if (status != i3c::Task::Status::Ok) {
                    logger::error(logger::Event::UsbCannotSend,
                                  {static_cast<uint8_t>(entry.client)});
                    return RouteResult::Dropped;
                }
            }
            else if (entry.client == mctp::Client::Spi0 || entry.client == mctp::Client::Spi1
                     || entry.client == mctp::Client::Spi2) {
                if constexpr (nv::ipc::Spi_Available) {
                    auto status = spi::Task::tx(pkt);
                    if (status != spi::Task::Status::Ok) {
                        logger::error(logger::Event::UsbCannotSend,
                                      {static_cast<uint8_t>(entry.client)});
                        return RouteResult::Dropped;
                    }
                }
            }
            else if constexpr (nv::ipc::MctpBridgeConfig<>::UsI3cBridgeMode) {
                if (entry.client == mctp::Client::UsI3c) {
                    // BMC traffic for the CPU bus owner crosses to the upstream I3C
                    // channel in bridge mode. Two cases land here:
                    //   (a) a BMC-originated REQUEST to the CPU, addressed to the CPU
                    //       alias EID (matched entry is the CPU-alias, bridge_real_eid
                    //       = CPU real EID), and
                    //   (b) a BMC RESPONSE to a CPU request, addressed to the CPU's
                    //       real EID (matched entry is the UsI3c upstream slot,
                    //       bridge_real_eid = 0).
                    //
                    // Dual-bus-owner bridge EID translation:
                    //  * dst: alias -> real, so the CPU receives it on its real EID
                    //    (case (a): Ca -> CPU real; case (b): no-op, bridge_real_eid
                    //    = 0).
                    //  * src: real -> alias, so the CPU sees the BMC as the alias EID
                    //    it knows (reverse-lookup the entry whose bridge_real_eid ==
                    //    src).
                    if (entry.bridge_real_eid != 0) {
                        pkt.hdr.dst_eid = entry.bridge_real_eid;
                    }
                    for (auto& alias : routing_table) {
                        if (alias.is_enumerated && alias.bridge_real_eid != 0
                            && alias.bridge_real_eid == pkt.hdr.src_eid) {
                            pkt.hdr.src_eid = alias.assigned_eid;
                            break;
                        }
                    }
                    perf_mon::Driver::log_pkt_latency(
                        perf_mon::Driver::LatencyEvent::UsbSendTxPktToTask);
                    auto status = i3c::Task::tx(pkt);
                    if (status != i3c::Task::Status::Ok) {
                        logger::error(logger::Event::UsbCannotSend,
                                      {static_cast<uint8_t>(entry.client)});
                        return RouteResult::Dropped;
                    }
                }
            }

            return RouteResult::Forwarded;
        }
    }

    // A CPU alias EID is present in the routing table from the moment the BMC
    // allocates the pool, but stays unbacked until I3C DAA binds a real EID to
    // it. DSP0236 8.7 requires a bridge to discard packets it cannot route, so
    // falling through to the local control handler would answer on behalf of an
    // endpoint that is not there and hand the BMC the MCU's own EID.
    if constexpr (nv::ipc::MctpBridgeConfig<>::UsI3cBridgeMode) {
        for (uint8_t i = 0; i < nv::ipc::MctpBridgeConfig<>::BridgeCpuAliasNum; i++) {
            const auto& alias = routing_table.at(nv::ipc::MctpBridgeConfig<>::CpuAliasSlotBase
                                                 + i);
            if (alias.client == mctp::Client::UsI3c && alias.assigned_eid != 0
                && alias.assigned_eid == pkt.hdr.dst_eid
                && !(alias.is_enumerated && alias.bridge_real_eid != 0)) {
                logger::info(
                    logger::Event::MctpMcuActAsBridgePacketDrop,
                    {static_cast<uint8_t>(static_cast<uint16_t>(mctp::Client::UsUsb)
                                          & UINT8_MAX),
                     static_cast<uint8_t>(static_cast<uint16_t>(mctp::Client::UsUsb) >> 8U),
                     static_cast<uint8_t>(mctp::Client::UsI3c),
                     pkt.hdr.dst_eid,
                     0});
                return RouteResult::Dropped;
            }
        }
    }

    return RouteResult::NotRouted;
}

void update_routing_table(RoutingTable& routing_table)
{
    ipc::Queue::Item item(std::bit_cast<uint8_t*>(&routing_table), sizeof(routing_table));
    auto&            router_queue = ipc::Queue::make(ipc::QueueId::RoutingTable);
    auto             queue_status = router_queue.recv(item, 100ms);

    if (queue_status != ipc::Queue::Status::Ok) {
        logger::error(logger::Event::UsbUpdateRoutingTableFailed,
                      {static_cast<uint8_t>(queue_status)});
    }
}

}  // namespace nv::usb
