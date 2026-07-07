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
#include "pdk-mctp-app-validator.h"

#include "pdk-mctp-app-control.h"
#include "pdk-mctp-app-router-plat.h"
#include "pdk-mctp-app-vendor.h"
#include "pdk-mctp-platforms-nsm.h"
#include "pdk/cmn/log/log.h"

using namespace pdk::mctp::app;
using namespace pdk::cmn;
using enum pdk::cmn::log::Destination;  // or spell out Destination::Console

bool Validator::validate(const Packet& pkt, platforms::Interface interface)
{
    auto& ctl = Control::PktReq::from(pkt);
    if (ctl.hdr_ver != 0x01) {
        pdk::cmn::log::hide().warn<Console>("invalid rsvd / hdr version\n");
        return false;
    }

    if (interface >= platforms::Interface::End) {
        return false;
    }

    auto eid_ok = (ctl.dst_eid == NULL_EID) || (ctl.dst_eid == BROADCAST_EID);
    if (!eid_ok) {
        for (uint8_t i = 0; i < _router.ec.cur_eid.size(); i++) {
            if (get_cur_eid(_router, i) == ctl.dst_eid) {
                eid_ok = true;
                break;
            }
        }
    }

    // Single packet Control message
    if ((ctl.msg_type == MsgType::Control) && ctl.som && ctl.eom) {
        if (!eid_ok) {
            pdk::cmn::log::hide().warn<Console>("invalid dst eid for control message\n");
            return false;
        }
        auto type = Control::get_packet_type(pkt);
        if (type != PacketType::Request && type != PacketType::Response) {
            pdk::cmn::log::hide().warn<Console>("packet is neither request nor response\n");
            return false;
        }
        _msg_type = MsgType::Control;
        return true;
    }
    // Start of PLDM message
    if ((ctl.msg_type == MsgType::Pldm) && ctl.som) {
        if (!eid_ok) {
            pdk::cmn::log::hide().warn<Console>(
                "SOM=1,EOM=0/1 with invalid dst eid for pldm message\n");
            return false;
        }
        // Skip persist on EOM=1 so a single-packet message does not clobber
        // an in-flight fragmented flow.
        if (!ctl.eom) {
            _msg_type = ctl.msg_type;
            _msg_tag  = ctl.msg_tag;
            _pkt_seq  = ctl.pkt_seq;
            _eid      = ctl.dst_eid;
        }
        return true;
    }

    // Start of SPDM message
    if ((ctl.msg_type == MsgType::Spdm) && ctl.som) {
        if (!eid_ok) {
            pdk::cmn::log::hide().warn<Console>(
                "SOM=1,EOM=0/1 with invalid dst eid for spdm message\n");
            return false;
        }
        if (!ctl.eom) {
            _msg_type = ctl.msg_type;
            _msg_tag  = ctl.msg_tag;
            _pkt_seq  = ctl.pkt_seq;
            _eid      = ctl.dst_eid;
        }
        return true;
    }

    // Start of VendorPci (NSM) message
    if ((ctl.msg_type == MsgType::VendorPci) && ctl.som) {
        if (!eid_ok) {
            pdk::cmn::log::hide().warn<Console>("invalid dst eid for vendor pci message\n");
            return false;
        }

        auto& nrx       = platforms::NsmPktReq::from(pkt);
        auto  vendor_id = nrx.pci_vendor_id;
        if (vendor_id != platforms::NvMctpPciVendorId) {
            pdk::cmn::log::hide().warn<Console>("NvMctpPciVendorId invalid");
            return false;
        }

        // Persist state only for fragmented NSM starts. Single-packet NSM
        // traffic can interleave with another fragmented message and must not
        // clobber the continuation state for that flow.
        if (!ctl.eom) {
            _msg_type = ctl.msg_type;
            _msg_tag  = ctl.msg_tag;
            _pkt_seq  = ctl.pkt_seq;
            _eid      = ctl.dst_eid;
        }
        return true;
    }

    if (ctl.msg_type == MsgType::VendorIani) {
        auto& vdr = VendorPktReq::from(pkt);
        // TODO: remove little endian
        if (vdr.iana == dmtf_iana_be || vdr.iana == dmtf_iana_le) {
            if (eid_ok) {
                if (vdr.pkt_seq != 0) {
                    pdk::cmn::log::hide().warn<Console>("packet sequence invalid\n");
                    return false;
                }
                auto type = Vendor::get_vendor_packet_type(pkt);
                if (type != PacketType::Request && type != PacketType::Response) {
                    pdk::cmn::log::hide().warn<Console>(
                        "packet is neither request nor response\n");
                    return false;
                }
                _msg_type = MsgType::VendorIani;
                // Start of VDM message
                if (ctl.som && !ctl.eom) {
                    _msg_tag = ctl.msg_tag;
                    _pkt_seq = ctl.pkt_seq;
                    _eid     = ctl.dst_eid;
                }
                return true;
            }
            else {
                pdk::cmn::log::hide().warn<Console>(
                    "SOM=1,EOM=0/1 with invalid dst eid for vendor message\n");
                return false;
            }
        }
    }

    // Multi packet message
    if (ctl.som && !ctl.eom) {
        if (eid_ok && (_eid == ctl.dst_eid)) {
            _pkt_seq = ctl.pkt_seq;
        }
        else {
            pdk::cmn::log::hide().warn<Console>("invalid dst eid for multi packet message\n");
            return false;
        }
        return true;
    }
    else if (!ctl.som || ctl.eom) {
        if (eid_ok && (_eid == ctl.dst_eid)) {
            if (ctl.msg_tag != _msg_tag) {
                pdk::cmn::log::hide().warn<Console>("message tag mismatch");
                return false;
            }
            // coverity[cert_int31_c_violation] -- safe: result is masked to 2 bits
            _pkt_seq = static_cast<uint8_t>(_pkt_seq + 1) & static_cast<uint8_t>(0b11);
            if (ctl.pkt_seq != _pkt_seq) {
                pdk::cmn::log::hide().warn<Console>("packet sequence mismatch");
                return false;
            }
        }
        else {
            pdk::cmn::log::hide().warn<Console>("invalid dst eid for multi packet message\n");
            return false;
        }
        return true;
    }

    pdk::cmn::log::hide().warn<Console>("invalid packet\n");
    return false;
}

void Validator::reset()
{
    _pkt_seq  = 0;
    _msg_tag  = 0;
    _eid      = 0;
    _msg_type = MsgType::Control;
}
