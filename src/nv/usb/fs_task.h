/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES.
 * All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <algorithm>
#include <array>
#include <cstring>
#include <span>

#include "nv/mctp/interface.h"
#include "nv/usb/usb_mctp_header.h"

namespace nv::usb {

/**
 * @brief Decode one complete MCTP packet from a full-speed USB receive completion.
 *
 * @param received Remaining bytes of the USB receive completion.
 * @param packet Destination packet; unused payload bytes are cleared on success.
 * @return Wire packet length, or zero for an invalid or incomplete packet.
 */
inline uint16_t decode_fs_mctp_packet(std::span<const uint8_t> received, mctp::Packet& packet)
{
    constexpr auto UsbHeaderSize  = sizeof(UsbMctpSpanningHeader);
    constexpr auto MctpHeaderSize = sizeof(mctp::Header);
    constexpr auto MaxPacketSize  = UsbHeaderSize + mctp::PktBufDataLen;
    if (received.size() < UsbHeaderSize) {
        return 0U;
    }
    UsbMctpSpanningHeader header{};
    std::memcpy(&header, received.data(), UsbHeaderSize);
    const auto length = header.packet_length();
    if (header.dmtf_id != UsbDmtfId || length < UsbHeaderSize + MctpHeaderSize + 1U
        || length > MaxPacketSize || length > received.size()) {
        return 0U;
    }

    packet                    = {};
    packet.priv.packet_length = length - UsbHeaderSize;
    std::memcpy(&packet.hdr, received.data() + UsbHeaderSize, MctpHeaderSize);
    std::copy_n(received.data() + UsbHeaderSize + MctpHeaderSize,
                length - UsbHeaderSize - MctpHeaderSize,
                packet.msg.begin());
    return length;
}

/** @brief Assemble one baseline packet; retain it until the caller resets used.
 *
 * Partial packets persist across full receive buffers. At transfer end the
 * caller clears used/discard. A malformed header discards the remaining transfer.
 */
inline bool decode_fs_mctp_stream(std::span<const uint8_t>& received,
                                  std::span<uint8_t>        assembly,
                                  size_t&                   used,
                                  bool&                     discard,
                                  mctp::Packet&             packet)
{
    while (!discard) {
        auto target = sizeof(UsbMctpSpanningHeader);
        if (used >= target) {
            UsbMctpSpanningHeader header{};
            std::memcpy(&header, assembly.data(), sizeof(header));
            target = header.packet_length();
            if (header.dmtf_id != UsbDmtfId
                || target < sizeof(header) + sizeof(mctp::Header) + 1U
                || target > assembly.size()) {
                discard = true;
                break;
            }
        }
        const auto count = std::min(target - used, received.size());
        std::copy_n(received.begin(), count, assembly.begin() + used);
        used     += count;
        received  = received.subspan(count);
        if (used == target && target > sizeof(UsbMctpSpanningHeader)) {
            return decode_fs_mctp_packet(assembly.first(used), packet) != 0U;
        }
        if (received.empty()) {
            return false;
        }
    }
    received = {};
    return false;
}

}  // namespace nv::usb

#if defined(USB_CONFIG_KHCI_MCTP) && (USB_CONFIG_KHCI_MCTP > 0U)

#include <array>
#include <cstdint>

#include "nv/ipc/queue.h"
#include "nv/ipc/task.h"
#include "nv/ipc/tasknotify.h"
#include "nv/mctp/router.h"
#include "nv/usb/mctp_router.h"
#include "nv/usb/task.h"
#include "sys/usb/usb_device_descriptor.h"

namespace nv::usb {

class FsTask : public ipc::Task
{
public:
    enum EventBits : ipc::TaskNotify::Bits
    {
        MctpTxBit             = 1_bit,
        MctpRxBit             = 2_bit,
        MctpTxDoneBit         = 3_bit,
        AttachBit             = 4_bit,
        ResetBit              = 5_bit,
        UpdateRoutingTableBit = 6_bit,
    };

    FsTask();

    static void            make();
    static void            entrypoint(void* params);
    static nv::usb::Status usb_tx(ipc::Queue::Item& item);
    static nv::usb::Status set_mctp_rx_event();
    static nv::usb::Status set_mctp_tx_done_event();
    static nv::usb::Status set_device_attach_event();
    static nv::usb::Status set_update_routing_table_event();
    static nv::usb::Status reset_events();

    [[noreturn]] void main();

private:
    constexpr static uint32_t BufferSize = USB_MCTP_OUT_BUFFER_LENGTH;
    using Buffer                         = std::array<uint8_t, BufferSize>;

    struct [[gnu::packed]] MctpPacket
    {
        UsbMctpSpanningHeader                                                    usb_hdr;
        mctp::Header                                                             mctp_hdr;
        std::array<uint8_t, mctp::PktBufDataLen - sizeof(UsbMctpSpanningHeader)> msg;
    };

    static MctpPacket& from(Buffer& buffer, uint32_t offset);
    void               receive();
    uint8_t            transmit();
    void               clear_queue();
    void               update_routing_table();
    void               reset_rx();

    ipc::Queue& _tx;
    alignas(USB_DATA_ALIGN_SIZE) Buffer _rx_buffer{};
    alignas(USB_DATA_ALIGN_SIZE) Buffer _tx_buffer{};
    RoutingTable _routing_table{};

    std::array<uint8_t, sizeof(UsbMctpSpanningHeader) + mctp::PktBufDataLen> _rx_assembly{};
    size_t                                                                   _rx_used{};
    bool                                                                     _rx_discard{};
    std::span<const uint8_t>                                                 _rx_remaining{};
    bool                                                                     _rx_active{};
    bool                                                                     _rx_transfer_end{};
    uint32_t                                                                 _rx_hold_start{};
    bool                                                                     _rx_held{};
    bool                                                                     _rx_hold_logged{};
};

}  // namespace nv::usb

#endif
