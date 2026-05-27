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
#include "app/pdk-mctp-app-control.h"
#include "app/pdk-mctp-app-router-plat.h"
#include "app/pdk-mctp-app-validator.h"
#include "app/pdk-mctp-app-vendor.h"
#include "pdk-mctp-platforms-nsm.h"
#include "pdk-mctp-platforms-router.h"
#include "ubs/unittest.hpp"

using namespace ubs::unittest;
using namespace pdk::mctp;

UBS_TEST(Validator, InvalidPackets)
{
    platforms::RoutingTable router;
    platforms::set_cur_eid(router, static_cast<uint8_t>(platforms::Interface::UsI2c), 0x02);
    app::Validator v(router);

    // Test invalid header version
    app::Packet invalidHeader{
        .priv = {.packet_length    = 0,
                 .packet_interface = static_cast<uint8_t>(platforms::Interface::UsI2c)},
        .hdr  = {.hdr_ver   = 0x02,
                 .dst_eid   = 0x02,
                 .src_eid   = 0x01,
                 .msg_tag   = 0x1,
                 .tag_owner = 0,
                 .pkt_seq   = 0,
                 .eom       = 1,
                 .som       = 1}
    };
    ensure::is_eq(v.validate(invalidHeader, platforms::Interface::UsI2c), false);

    // Test invalid destination EID
    app::Packet invalidDstEID{
        .priv = {.packet_length    = 0,
                 .packet_interface = static_cast<uint8_t>(platforms::Interface::UsI2c)},
        .hdr  = {.hdr_ver   = 0x01,
                 .dst_eid   = 0x03,
                 .src_eid   = 0x01,
                 .msg_tag   = 0x1,
                 .tag_owner = 0,
                 .pkt_seq   = 0,
                 .eom       = 1,
                 .som       = 1}
    };
    ensure::is_eq(v.validate(invalidDstEID, platforms::Interface::UsI2c), false);

    // Test invalid message type
    app::Packet invalidMsgType{
        .priv = {.packet_length    = 0,
                 .packet_interface = static_cast<uint8_t>(platforms::Interface::UsI2c)},
        .hdr  = {.hdr_ver   = 0x01,
                 .dst_eid   = 0x02,
                 .src_eid   = 0x01,
                 .msg_tag   = 0x1,
                 .tag_owner = 0,
                 .pkt_seq   = 0,
                 .eom       = 1,
                 .som       = 1},
        .msg  = {0x7F}  // Invalid msg_type
    };
    ensure::is_eq(v.validate(invalidMsgType, platforms::Interface::UsI2c), false);

    // Test invalid interface
    app::Packet invalidInterface{
        .priv = {.packet_length    = 0,
                 .packet_interface = static_cast<uint8_t>(platforms::Interface::End)},
        .hdr  = {.hdr_ver   = 0x01,
                 .dst_eid   = 0x02,
                 .src_eid   = 0x01,
                 .msg_tag   = 0x1,
                 .tag_owner = 0,
                 .pkt_seq   = 0,
                 .eom       = 1,
                 .som       = 1}
    };
    ensure::is_eq(v.validate(invalidInterface, platforms::Interface::End), false);

    // Test that when the control packet fields are incorrect (e.g. rq != 1 or d
    // != 0), Control::get_packet_type should return neither Request nor Response,
    // triggering an error.
    app::Packet invalidControlType{
        .priv = {.packet_length    = 0,
                 .packet_interface = static_cast<uint8_t>(platforms::Interface::UsI2c)},
        .hdr  = {.hdr_ver   = 0x01,
                 .dst_eid   = 0x02,
                 .src_eid   = 0x01,
                 .msg_tag   = 0x2,  // tag can be different from others
                 .tag_owner = 1,
                 .pkt_seq   = 0,
                 .eom       = 1,
                 .som       = 1},
        .msg  = {static_cast<uint8_t>(app::MsgType::Control)}
    };
    auto& ctl_invalid = app::Control::PktReq::from(invalidControlType);
    // Intentionally set incorrect control fields
    ctl_invalid.rq = 0;  // Incorrect rq field, should be 1
    ctl_invalid.d  = 1;  // Incorrect d field, should be 0
    v.reset();
    ensure::is_eq(v.validate(invalidControlType, platforms::Interface::UsI2c), false);
};

UBS_TEST(Validator, ValidPackets)
{
    platforms::RoutingTable router;
    platforms::set_cur_eid(router, static_cast<uint8_t>(platforms::Interface::UsI2c), 0x02);
    app::Validator v(router);

    // ====== Test Valid Control Packet (Request) ======
    app::Packet validControl{
        .priv = {.packet_length    = 0,
                 .packet_interface = static_cast<uint8_t>(platforms::Interface::UsI2c)},
        .hdr  = {.hdr_ver   = 0x01,
                 .dst_eid   = 0x02,
                 .src_eid   = 0x01,
                 .msg_tag   = 0x1,
                 .tag_owner = 1,
                 .pkt_seq   = 0,
                 .eom       = 1,
                 .som       = 1},
        .msg  = {static_cast<uint8_t>(app::MsgType::Control)}
    };

    // Set correct fields for a valid Control Request
    auto& ctl     = app::Control::PktReq::from(validControl);
    ctl.rq        = 1;  // Must be 1 for Request
    ctl.d         = 0;  // Must be 0 for Request
    ctl.tag_owner = 1;  // Control messages are tag-owned
    ensure::is_eq(v.validate(validControl, platforms::Interface::UsI2c), true);

    // ====== Test Valid PLDM Start Packet ======
    app::Packet validPldm{
        .priv = {.packet_length    = 0,
                 .packet_interface = static_cast<uint8_t>(platforms::Interface::UsI2c)},
        .hdr  = {.hdr_ver   = 0x01,
                 .dst_eid   = 0x02,
                 .src_eid   = 0x01,
                 .msg_tag   = 0x1,
                 .tag_owner = 0,
                 .pkt_seq   = 0,
                 .eom       = 0,
                 .som       = 1},
        .msg  = {static_cast<uint8_t>(app::MsgType::Pldm)}
    };

    ensure::is_eq(v.validate(validPldm, platforms::Interface::UsI2c), true);

    // ====== Test Valid SPDM Start Packet ======
    app::Packet validSpdm{
        .priv = {.packet_length    = 0,
                 .packet_interface = static_cast<uint8_t>(platforms::Interface::UsI2c)},
        .hdr  = validPldm.hdr, // Copy header from validPldm
        .msg  = {static_cast<uint8_t>(app::MsgType::Spdm)}
    };

    ensure::is_eq(v.validate(validSpdm, platforms::Interface::UsI2c), true);
};

UBS_TEST(Validator, MultiPacketFailures)
{
    platforms::RoutingTable router;
    platforms::set_cur_eid(router, static_cast<uint8_t>(platforms::Interface::UsI2c), 0x02);
    app::Validator v(router);

    // invalid packet sequence
    app::Packet invalidPktSeq{
        .priv = {.packet_length    = 0,
                 .packet_interface = static_cast<uint8_t>(platforms::Interface::UsI2c)},
        .hdr  = {.hdr_ver   = 0x01,
                 .dst_eid   = 0x02,
                 .src_eid   = 0x01,
                 .msg_tag   = 0x1,
                 .tag_owner = 0,
                 .pkt_seq   = 3,
                 .eom       = 1,
                 .som       = 0}
    };
    ensure::is_eq(v.validate(invalidPktSeq, platforms::Interface::UsI2c), false);

    // invalid EID in multi-packet message
    app::Packet invalidEid{
        .priv = {.packet_length    = 0,
                 .packet_interface = static_cast<uint8_t>(platforms::Interface::UsI2c)},
        .hdr  = {.hdr_ver   = 0x01,
                 .dst_eid   = 0x03,
                 .src_eid   = 0x01,
                 .msg_tag   = 0x1,
                 .tag_owner = 0,
                 .pkt_seq   = 0,
                 .eom       = 0,
                 .som       = 1}
    };
    ensure::is_eq(v.validate(invalidEid, platforms::Interface::UsI2c), false);

    // both som and eom set to 0
    app::Packet invalidSomEom{
        .priv = {.packet_length    = 0,
                 .packet_interface = static_cast<uint8_t>(platforms::Interface::UsI2c)},
        .hdr  = {.hdr_ver   = 0x01,
                 .dst_eid   = 0x02,
                 .src_eid   = 0x01,
                 .msg_tag   = 0x1,
                 .tag_owner = 0,
                 .pkt_seq   = 0,
                 .eom       = 0,
                 .som       = 0}
    };
    ensure::is_eq(v.validate(invalidSomEom, platforms::Interface::UsI2c), false);

    // For multi-packet messages, after the starting packet sets _eid and
    // _msg_tag, if subsequent packets have a mismatched msg_tag compared to the
    // starting packet, validation should fail.
    app::Packet startPkt{
        .priv = {.packet_length    = 0,
                 .packet_interface = static_cast<uint8_t>(platforms::Interface::UsI2c)},
        .hdr  = {.hdr_ver   = 0x01,
                 .dst_eid   = 0x02,
                 .src_eid   = 0x01,
                 .msg_tag   = 0x7,  // Starting tag is 0x7
                 .tag_owner = 0,
                 .pkt_seq   = 0,
                 .eom       = 0,
                 .som       = 1},
        .msg  = {static_cast<uint8_t>(app::MsgType::Pldm)}
    };
    v.reset();
    ensure::is_eq(v.validate(startPkt, platforms::Interface::UsI2c), true);

    // Subsequent packet: msg_tag does not match the starting packet (0x8 != 0x7)
    app::Packet tagMismatchPkt{
        .priv = {.packet_length    = 0,
                 .packet_interface = static_cast<uint8_t>(platforms::Interface::UsI2c)},
        .hdr  = {.hdr_ver   = 0x01,
                 .dst_eid   = 0x02,
                 .src_eid   = 0x01,
                 .msg_tag   = 0x6,  // Does not match the previous tag
                 .tag_owner = 0,
                 .pkt_seq   = 1,  // Expected pkt_seq = 1
                 .eom       = 1,
                 .som       = 0},
        .msg  = {static_cast<uint8_t>(app::MsgType::Pldm)}
    };
    ensure::is_eq(v.validate(tagMismatchPkt, platforms::Interface::UsI2c), false);
};

UBS_TEST(Validator, ValidMultiPacketStart)
{
    platforms::RoutingTable router;
    platforms::set_cur_eid(router, static_cast<uint8_t>(platforms::Interface::UsI2c), 0x02);
    app::Validator v(router);

    // First packet (Start of multi-packet sequence)
    app::Packet firstPkt{
        .priv = {.packet_length    = 0,
                 .packet_interface = static_cast<uint8_t>(platforms::Interface::UsI2c)},
        .hdr  = {.hdr_ver   = 0x01,
                 .dst_eid   = 0x02,
                 .src_eid   = 0x01,
                 .msg_tag   = 0x1,
                 .tag_owner = 0,
                 .pkt_seq   = 0,
                 .eom       = 0,
                 .som       = 1},
        .msg  = {static_cast<uint8_t>(app::MsgType::Pldm)}
    };

    ensure::is_eq(v.validate(firstPkt, platforms::Interface::UsI2c),
                  true);  // This must pass

    // Second packet (Continuation of multi-packet)
    app::Packet secondPkt{
        .priv = {.packet_length    = 0,
                 .packet_interface = static_cast<uint8_t>(platforms::Interface::UsI2c)},
        .hdr  = {.hdr_ver   = 0x01,
                 .dst_eid   = 0x02,
                 .src_eid   = 0x01,
                 .msg_tag   = 0x1,
                 .tag_owner = 0,
                 .pkt_seq   = 1,
                 .eom       = 0,
                 .som       = 0},
        .msg  = {static_cast<uint8_t>(app::MsgType::Pldm)}
    };

    ensure::is_eq(v.validate(secondPkt, platforms::Interface::UsI2c),
                  true);  // This must pass
};

UBS_TEST(Validator, ValidMultiPacketMiddle)
{
    platforms::RoutingTable router;
    platforms::set_cur_eid(router, static_cast<uint8_t>(platforms::Interface::UsI2c), 0x02);
    app::Validator v(router);

    // Start Packet (sets _eid)
    app::Packet startPkt{
        .priv = {.packet_length    = 0,
                 .packet_interface = static_cast<uint8_t>(platforms::Interface::UsI2c)},
        .hdr  = {.hdr_ver = 0x01,
                 .dst_eid = platforms::get_cur_eid(
                    router, static_cast<uint8_t>(platforms::Interface::UsI2c)),
                 .src_eid   = 0x01,
                 .msg_tag   = 0x1,
                 .tag_owner = 0,
                 .pkt_seq   = 0,
                 .eom       = 0,
                 .som       = 1},
        .msg  = {static_cast<uint8_t>(app::MsgType::Pldm)}
    };
    ensure::is_eq(v.validate(startPkt, platforms::Interface::UsI2c), true);

    // Middle Packet (not SOM or EOM, same dst_eid)
    app::Packet middlePkt{
        .priv = {.packet_length    = 0,
                 .packet_interface = static_cast<uint8_t>(platforms::Interface::UsI2c)},
        .hdr  = {.hdr_ver   = 0x01,
                 .dst_eid   = startPkt.hdr.dst_eid,  // Must match `_eid`
                 .src_eid   = 0x01,
                 .msg_tag   = 0x1,
                 .tag_owner = 0,
                 .pkt_seq   = 1,
                 .eom       = 0,
                 .som       = 0},
        .msg  = {static_cast<uint8_t>(app::MsgType::Pldm)}
    };
    ensure::is_eq(v.validate(middlePkt, platforms::Interface::UsI2c), true);
};

UBS_TEST(Validator, VendorPackets)
{
    platforms::RoutingTable router;
    platforms::set_cur_eid(router, static_cast<uint8_t>(platforms::Interface::UsI2c), 0x02);
    app::Validator v(router);

    // Valid VendorPci Packet
    app::Packet pkt1{
        .priv = {.packet_length    = 0,
                 .packet_interface = static_cast<uint8_t>(platforms::Interface::UsI2c)},
        .hdr  = {.hdr_ver   = 0x01,
                 .dst_eid   = 0x02,
                 .src_eid   = 0x01,
                 .msg_tag   = 0x1,
                 .tag_owner = 1,
                 .pkt_seq   = 0,
                 .eom       = 0,
                 .som       = 1},
        .msg  = {static_cast<uint8_t>(app::MsgType::VendorPci)}
    };
    auto& nrx1         = platforms::NsmPktReq::from(pkt1);
    nrx1.pci_vendor_id = platforms::NvMctpPciVendorId;
    ensure::is_eq(v.validate(pkt1, platforms::Interface::UsI2c), true);

    // Invalid VendorPci Packet
    app::Packet pkt2   = pkt1;
    auto&       nrx2   = platforms::NsmPktReq::from(pkt2);
    nrx2.pci_vendor_id = 0xFFFF;
    ensure::is_eq(v.validate(pkt2, platforms::Interface::UsI2c), false);

    // Valid VendorIani Packet
    app::Packet pkt3{
        .priv = {.packet_length    = 0,
                 .packet_interface = static_cast<uint8_t>(platforms::Interface::UsI2c)},
        .hdr  = {.hdr_ver   = 0x01,
                 .dst_eid   = 0x02,
                 .src_eid   = 0x01,
                 .msg_tag   = 0x2,
                 .tag_owner = 1,
                 .pkt_seq   = 0,
                 .eom       = 1,
                 .som       = 1},
        .msg  = {static_cast<uint8_t>(app::MsgType::VendorIani)}
    };
    auto& vdr1   = app::VendorPktReq::from(pkt3);
    vdr1.iana    = 0x00001647;
    vdr1.pkt_seq = 0;
    vdr1.rq      = 1;
    vdr1.d       = 0;
    ensure::is_eq(v.validate(pkt3, platforms::Interface::UsI2c), true);

    // Original test case: Invalid VendorIani Packet (invalid iana)
    app::Packet pkt4 = pkt3;
    auto&       vdr2 = app::VendorPktReq::from(pkt4);
    vdr2.iana        = 0x12345678;
    ensure::is_eq(v.validate(pkt4, platforms::Interface::UsI2c), false);

    // VendorIani: invalid dst_eid (not the router's, not NULL, nor BROADCAST)
    app::Packet vendorIaniInvalidEid{
        .priv = {.packet_length    = 0,
                 .packet_interface = static_cast<uint8_t>(platforms::Interface::UsI2c)},
        .hdr  = {.hdr_ver   = 0x01,
                 .dst_eid   = 0x03,  // Invalid, because router.cur_eid[UsI2c] == 0x02
                 .src_eid   = 0x01,
                 .msg_tag   = 0x3,
                 .tag_owner = 1,
                 .pkt_seq   = 0,
                 .eom       = 1,
                 .som       = 1},
        .msg  = {static_cast<uint8_t>(app::MsgType::VendorIani)}
    };
    auto& vdr_invalid_eid = app::VendorPktReq::from(vendorIaniInvalidEid);
    vdr_invalid_eid.iana  = 0x00001647;  // Using big-endian format
    v.reset();
    ensure::is_eq(v.validate(vendorIaniInvalidEid, platforms::Interface::UsI2c), false);

    // VendorIani: pkt_seq is not 0 (should be 0)
    app::Packet vendorIaniInvalidSeq{
        .priv = {.packet_length    = 0,
                 .packet_interface = static_cast<uint8_t>(platforms::Interface::UsI2c)},
        .hdr  = {.hdr_ver   = 0x01,
                 .dst_eid   = 0x02,  // Valid
                 .src_eid   = 0x01,
                 .msg_tag   = 0x4,
                 .tag_owner = 1,
                 .pkt_seq   = 1,  // Not 0
                 .eom       = 1,
                 .som       = 1},
        .msg  = {static_cast<uint8_t>(app::MsgType::VendorIani)}
    };
    auto& vdr_invalid_seq = app::VendorPktReq::from(vendorIaniInvalidSeq);
    vdr_invalid_seq.iana  = 0x47160000;  // Using little-endian format
    v.reset();
    ensure::is_eq(v.validate(vendorIaniInvalidSeq, platforms::Interface::UsI2c), false);

    // VendorIani: simulate Vendor::get_vendor_packet_type returning neither
    // Request nor Response
    app::Packet vendorIaniInvalidType{
        .priv = {.packet_length    = 0,
                 .packet_interface = static_cast<uint8_t>(platforms::Interface::UsI2c)},
        .hdr  = {.hdr_ver   = 0x01,
                 .dst_eid   = 0x02,
                 .src_eid   = 0x01,
                 .msg_tag   = 0x5,
                 .tag_owner = 1,
                 .pkt_seq   = 0,
                 .eom       = 1,
                 .som       = 1},
        .msg  = {0xFF}  // Set to an invalid vendor packet type
    };
    auto& vdr_invalid_type = app::VendorPktReq::from(vendorIaniInvalidType);
    vdr_invalid_type.iana  = 0x00001647;
    v.reset();
    ensure::is_eq(v.validate(vendorIaniInvalidType, platforms::Interface::UsI2c), false);
};

// Regression: VendorPci (NSM) SOM=1 fragment must persist _eid / _msg_tag /
// _pkt_seq so a SOM=0 continuation passes the multi-packet validation branch.
// Before the fix, the SOM branch returned true without saving state and the
// continuation fragment got dropped — silently breaking any NSM message large
// enough to fragment (e.g. InstallToken).
UBS_TEST(Validator, ValidMultiPacketVendorPciStart)
{
    platforms::RoutingTable router;
    platforms::set_cur_eid(router, static_cast<uint8_t>(platforms::Interface::UsI2c), 0x02);
    app::Validator v(router);

    // First packet (SOM=1, EOM=0): start of a multi-fragment NSM message.
    app::Packet firstPkt{
        .priv = {.packet_length    = 0,
                 .packet_interface = static_cast<uint8_t>(platforms::Interface::UsI2c)},
        .hdr  = {.hdr_ver   = 0x01,
                 .dst_eid   = 0x02,
                 .src_eid   = 0x01,
                 .msg_tag   = 0x1,
                 .tag_owner = 1,
                 .pkt_seq   = 0,
                 .eom       = 0,
                 .som       = 1},
        .msg  = {static_cast<uint8_t>(app::MsgType::VendorPci)}
    };
    auto& nrx1         = platforms::NsmPktReq::from(firstPkt);
    nrx1.pci_vendor_id = platforms::NvMctpPciVendorId;
    ensure::is_eq(v.validate(firstPkt, platforms::Interface::UsI2c), true);

    // Second packet (SOM=0, EOM=1): continuation must validate against the
    // _eid/_msg_tag/_pkt_seq the first packet persisted.
    app::Packet secondPkt{
        .priv = {.packet_length    = 0,
                 .packet_interface = static_cast<uint8_t>(platforms::Interface::UsI2c)},
        .hdr  = {.hdr_ver   = 0x01,
                 .dst_eid   = 0x02,
                 .src_eid   = 0x01,
                 .msg_tag   = 0x1,
                 .tag_owner = 1,
                 .pkt_seq   = 1,
                 .eom       = 1,
                 .som       = 0},
        .msg  = {static_cast<uint8_t>(app::MsgType::VendorPci)}
    };
    ensure::is_eq(v.validate(secondPkt, platforms::Interface::UsI2c), true);
};
