/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES.
 * All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <array>
#include <cstring>

#include "nv/usb/fs_task.h"
#include "nv/ut/unittest.h"

using namespace nv;
using namespace nv::ut;

class UsbFsReceive : public Fixture
{
public:
    static constexpr auto       HeaderSize = sizeof(usb::UsbMctpSpanningHeader)
                                           + sizeof(mctp::Header);
    std::array<uint8_t, 2048>    buffer{};
    std::array<mctp::Packet, 32> packets{};
    std::size_t                 count{};

    std::array<uint8_t, 72> assembly{};
    size_t used{};
    bool discard{};

    /** @brief Fill unused receive bytes with recognizable stale data. */
    void setup() override { buffer.fill(0xA5U); }

    /**
     * @brief Put a valid wire packet into the receive buffer at the given offset.
     * @param offset Packet start within the buffer.
     * @param payload_length Number of payload bytes, each set to source_eid.
     * @param source_eid Source EID and payload pattern identifying this packet.
     * @return Total wire packet length.
     */
    std::size_t put_packet(std::size_t offset, std::size_t payload_length, uint8_t source_eid)
    {
        const auto                 length = HeaderSize + payload_length;
        usb::UsbMctpSpanningHeader usb_header{};
        usb_header.dmtf_id = usb::UsbDmtfId;
        usb_header.set_packet_length(static_cast<uint16_t>(length));
        mctp::Header header{};
        header.hdr_ver = 1U;
        header.dst_eid = 8U;
        header.src_eid = source_eid;
        header.som     = 1U;
        header.eom     = 1U;
        std::memcpy(buffer.data() + offset, &usb_header, sizeof(usb_header));
        std::memcpy(buffer.data() + offset + sizeof(usb_header), &header, sizeof(header));
        std::fill_n(buffer.begin() + offset + HeaderSize, payload_length, source_eid);
        return length;
    }

    /**
     * @brief Capture packets from the production parser using a completed byte count.
     * @param received_length Length of the USB receive completion.
     * @return Number of complete wire bytes consumed.
     */
    std::size_t parse(std::size_t received_length)
    {
        auto remaining = std::span<const uint8_t>(buffer.data(), received_length);
        while (!remaining.empty()) {
            mctp::Packet packet{};
            const auto   length = usb::decode_fs_mctp_packet(remaining, packet);
            if (length == 0U) {
                break;
            }
            ensure::is_lt(count, packets.size());
            packets[count++] = packet;
            remaining        = remaining.subspan(length);
        }
        return received_length - remaining.size();
    }

    /**
     * @brief Check decoded headers, declared length, payload, and zeroed unused bytes.
     * @param index Captured packet index.
     * @param payload_length Expected payload length.
     * @param source_eid Expected source EID and payload pattern.
     */
    void check_packet(std::size_t index, std::size_t payload_length, uint8_t source_eid)
    {
        const auto& packet = packets[index];
        ensure::is_eq(packet.priv.packet_length, sizeof(mctp::Header) + payload_length);
        ensure::is_eq(static_cast<uint8_t>(packet.hdr.hdr_ver), 1U);
        ensure::is_eq(packet.hdr.dst_eid, 8U);
        ensure::is_eq(packet.hdr.src_eid, source_eid);
        ensure::is_eq(static_cast<uint8_t>(packet.hdr.som), 1U);
        ensure::is_eq(static_cast<uint8_t>(packet.hdr.eom), 1U);
        for (std::size_t i = 0; i < packet.msg.size(); ++i) {
            ensure::is_eq(packet.msg[i], i < payload_length ? source_eid : 0U);
        }
    }
    void reset_stream()
    {
        used    = 0U;
        discard = false;
    }

    /** @brief Feed a receive completion, preserving partial state until transfer end. */
    void feed(std::span<const uint8_t> received, bool transfer_end)
    {
        mctp::Packet packet{};
        while (usb::decode_fs_mctp_stream(received, assembly, used, discard, packet)) {
            used = 0U;
            ensure::is_lt(count, packets.size());
            packets[count++] = packet;
        }
        if (transfer_end) {
            reset_stream();
        }
    }

};

/** @brief A short completion must not forward a valid stale packet after its end. */
TEST_F(UsbFsReceive, ShortTransferIgnoresStalePacket)
{
    const auto length = fixture.put_packet(0U, 3U, 9U);
    fixture.put_packet(length, 4U, 10U);
    ensure::is_eq(fixture.parse(length), length);
    ensure::is_eq(fixture.count, 1U);
    fixture.check_packet(0U, 3U, 9U);
};

/** @brief Incomplete spanning headers must not be completed from stale bytes. */
TEST_F(UsbFsReceive, TruncatedUsbHeader)
{
    fixture.put_packet(0U, 3U, 9U);
    for (std::size_t length = 0U; length < sizeof(usb::UsbMctpSpanningHeader); ++length) {
        ensure::is_eq(fixture.parse(length), 0U);
        ensure::is_eq(fixture.count, 0U);
    }
};

/** @brief Reject packets declaring an incomplete MCTP header or no payload. */
TEST_F(UsbFsReceive, InvalidMinimumPacketLength)
{
    fixture.put_packet(0U, 3U, 9U);
    for (std::size_t length = sizeof(usb::UsbMctpSpanningHeader);
         length <= UsbFsReceive::HeaderSize;
         ++length) {
        usb::UsbMctpSpanningHeader header{};
        header.dmtf_id = usb::UsbDmtfId;
        header.set_packet_length(static_cast<uint16_t>(length));
        std::memcpy(fixture.buffer.data(), &header, sizeof(header));
        ensure::is_eq(fixture.parse(length), 0U);
        ensure::is_eq(fixture.count, 0U);
    }
};

/** @brief Reject every truncated prefix even when the buffer contains a complete packet. */
TEST_F(UsbFsReceive, TruncatedMctpPacket)
{
    const auto length = fixture.put_packet(0U, 4U, 9U);
    for (std::size_t received = sizeof(usb::UsbMctpSpanningHeader); received < length;
         ++received) {
        ensure::is_eq(fixture.parse(received), 0U);
        ensure::is_eq(fixture.count, 0U);
    }
};

/** @brief Decode multiple packets in order and copy only each declared payload. */
TEST_F(UsbFsReceive, MultiplePackets)
{
    const auto first  = fixture.put_packet(0U, 1U, 9U);
    const auto second = fixture.put_packet(first, mctp::Packet{}.msg.size(), 10U);
    ensure::is_eq(fixture.parse(first + second), first + second);
    ensure::is_eq(fixture.count, 2U);
    fixture.check_packet(0U, 1U, 9U);
    fixture.check_packet(1U, mctp::Packet{}.msg.size(), 10U);
};

/** @brief Forward the complete prefix but reject a truncated second packet. */
TEST_F(UsbFsReceive, CompletePacketThenTruncatedPacket)
{
    const auto first  = fixture.put_packet(0U, 3U, 9U);
    const auto second = fixture.put_packet(first, 4U, 10U);
    ensure::is_eq(fixture.parse(first + second - 1U), first);
    ensure::is_eq(fixture.count, 1U);
    fixture.check_packet(0U, 3U, 9U);
};

/** @brief A partial trailing header must not be completed using stale buffer bytes. */
TEST_F(UsbFsReceive, CompletePacketThenPartialHeader)
{
    const auto first = fixture.put_packet(0U, 3U, 9U);
    fixture.put_packet(first, 4U, 10U);
    ensure::is_eq(fixture.parse(first + sizeof(usb::UsbMctpSpanningHeader) - 1U), first);
    ensure::is_eq(fixture.count, 1U);
    fixture.check_packet(0U, 3U, 9U);
};

/** @brief Reject packets with an invalid DMTF identifier. */
TEST_F(UsbFsReceive, InvalidDmtfId)
{
    const auto length  = fixture.put_packet(0U, 3U, 9U);
    fixture.buffer[0] ^= 1U;
    ensure::is_eq(fixture.parse(length), 0U);
    ensure::is_eq(fixture.count, 0U);
};

/** @brief Reject declared payloads exceeding the destination MCTP packet capacity. */
TEST_F(UsbFsReceive, OversizedPacket)
{
    const auto length = fixture.put_packet(0U, mctp::Packet{}.msg.size() + 1U, 9U);
    ensure::is_eq(fixture.parse(length), 0U);
    ensure::is_eq(fixture.count, 0U);
};

/** @brief A minimum-sized allocation must suffice for a minimum-sized wire packet. */
TEST_F(UsbFsReceive, ExactSizeBuffer)
{
    std::array<uint8_t, UsbFsReceive::HeaderSize + 1U> received{};
    fixture.put_packet(0U, 1U, 9U);
    std::copy_n(fixture.buffer.begin(), received.size(), received.begin());
    const auto consumed = usb::decode_fs_mctp_packet(received, fixture.packets[0]);
    ensure::is_eq(consumed, received.size());
    fixture.check_packet(0U, 1U, 9U);
};

/** @brief Every header/body split must reconstruct the packet without stale bytes. */
TEST_F(UsbFsReceive, StreamEverySplit)
{
    const auto length = fixture.put_packet(0U, mctp::Packet{}.msg.size(), 9U);
    const auto wire   = std::span<const uint8_t>(fixture.buffer.data(), length);
    for (std::size_t split = 1U; split < length; ++split) {
        fixture.count = 0U;
        fixture.feed(wire.first(split), false);
        ensure::is_eq(fixture.count, 0U);
        fixture.feed(wire.subspan(split), true);
        ensure::is_eq(fixture.count, 1U);
        fixture.check_packet(0U, mctp::Packet{}.msg.size(), 9U);
    }
};

/** @brief Packed baseline packets survive several 512-byte receive boundaries. */
TEST_F(UsbFsReceive, StreamPackedPacketsAcrossBuffers)
{
    std::size_t length = 0U;
    for (uint8_t i = 0U; i < 24U; ++i) {
        length += fixture.put_packet(length, mctp::Packet{}.msg.size(), i + 8U);
    }
    for (std::size_t offset = 0U; offset < length; offset += 512U) {
        const auto chunk = std::min<std::size_t>(512U, length - offset);
        fixture.feed({fixture.buffer.data() + offset, chunk}, chunk < 512U);
    }
    ensure::is_eq(fixture.count, 24U);
    for (std::size_t i = 0U; i < fixture.count; ++i) {
        fixture.check_packet(i, mctp::Packet{}.msg.size(), i + 8U);
    }
};

/** @brief A complete packet ending at 512 needs no ZLP to be delivered. */
TEST_F(UsbFsReceive, StreamFullBufferThenZlp)
{
    std::size_t length = 0U;
    for (uint8_t i = 0U; i < 8U; ++i) {
        length += fixture.put_packet(length, 56U, i + 8U);
    }
    ensure::is_eq(length, 512U);
    fixture.feed({fixture.buffer.data(), length}, false);
    ensure::is_eq(fixture.count, 8U);
    fixture.feed({}, true);
    ensure::is_eq(fixture.count, 8U);
    fixture.feed({fixture.buffer.data(), 64U}, true);
    ensure::is_eq(fixture.count, 9U);
    fixture.check_packet(8U, 56U, 8U);
};

/** @brief Short transfers and standalone ZLPs discard partial headers and bodies. */
TEST_F(UsbFsReceive, StreamTruncatedTransfer)
{
    const auto length = fixture.put_packet(0U, 64U, 9U);
    const auto wire   = std::span<const uint8_t>(fixture.buffer.data(), length);
    for (std::size_t prefix = 1U; prefix < length; ++prefix) {
        fixture.count = 0U;
        fixture.feed(wire.first(prefix), true);
        fixture.feed(wire, true);
        ensure::is_eq(fixture.count, 1U);
        fixture.check_packet(0U, 64U, 9U);

        fixture.count = 0U;
        fixture.feed(wire.first(prefix), false);
        fixture.feed({}, true);
        fixture.feed(wire, true);
        ensure::is_eq(fixture.count, 1U);
        fixture.check_packet(0U, 64U, 9U);
    }
};

/** @brief Invalid/oversized headers discard through transfer end, then recover. */
TEST_F(UsbFsReceive, StreamMalformedTransfer)
{
    const auto length = fixture.put_packet(0U, 64U, 9U);
    const auto wire   = std::span<const uint8_t>(fixture.buffer.data(), length);
    for (const auto bad_length : {0U, 8U, 73U, 8191U}) {
        usb::UsbMctpSpanningHeader header{};
        header.dmtf_id = usb::UsbDmtfId;
        header.set_packet_length(static_cast<uint16_t>(bad_length));
        fixture.count = 0U;
        fixture.feed({reinterpret_cast<const uint8_t*>(&header), sizeof(header)}, false);
        fixture.feed(wire, false);
        ensure::is_eq(fixture.count, 0U);
        fixture.feed({}, true);
        fixture.feed(wire, true);
        ensure::is_eq(fixture.count, 1U);
    }
    auto invalid   = fixture.buffer;
    invalid[0]    ^= 1U;
    fixture.count  = 0U;
    fixture.feed({invalid.data(), length}, false);
    fixture.feed(wire, true);
    ensure::is_eq(fixture.count, 0U);
    fixture.feed(wire, true);
    ensure::is_eq(fixture.count, 1U);
};

/** @brief USB reset must discard both partial assembly and malformed-transfer state. */
TEST_F(UsbFsReceive, StreamReset)
{
    const auto length = fixture.put_packet(0U, 64U, 9U);
    const auto wire   = std::span<const uint8_t>(fixture.buffer.data(), length);
    fixture.feed(wire.first(3U), false);
    fixture.reset_stream();
    fixture.feed(wire, true);
    ensure::is_eq(fixture.count, 1U);
    fixture.buffer[0] ^= 1U;
    fixture.feed(wire, false);
    fixture.buffer[0] ^= 1U;
    fixture.reset_stream();
    fixture.feed(wire, true);
    ensure::is_eq(fixture.count, 2U);
};
