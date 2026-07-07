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

#include <cstdint>

#include "sys/usb/usb_config_wrapper.h"

// Default the CP2112/HID interface flag to enabled for platforms whose USB
// config wrapper does not define it
#ifndef SYS_USB_HID_CP2112
#define SYS_USB_HID_CP2112 (1U)
#endif

namespace nv::usb {
// Magic value to indicate USB port reset in mailbox
constexpr uint32_t UsbPortResetMagicNumber = 0x55534252;  // "USBR"
}  // namespace nv::usb

// When USB handled by Core1 bare-metal, provide stubs that forward to usb_proxy
#if SYS_USB_DISABLED

#include "nv/usb/usb_stub.h"

#else  // SYS_USB_DISABLED == 0 - Original implementation

#include "nv/i2c/common.h"
#include "nv/ipc/event.h"
#include "nv/ipc/queue.h"
#include "nv/ipc/task.h"
#include "nv/ipchandler/ipchandler.h"
#include "nv/mctp/interface.h"
#include "nv/mctp/router.h"
#include "nv/usb/hid_smb.h"
#include "nv/usb/mctp_router.h"
#include "nv/usb/usb_mctp_header.h"
#include "nv/lstp/lstp_router.h"
#include "sys/ipc/event.h"
#include "sys/usb/usb.h"
#include "sys/usb/usb_device_descriptor.h"

namespace nv::usb {

enum class Status
{
    Ok,             ///< success
    QueueSendFail,  ///< queue send return fail
    EventSetFail,   ///< event set fail
    Unknown,        ///< an unknown error occurred
};

class Task : public ipc::Task
{
public:
    Task();

    enum EventBits : nv::ipc::Event::Bits
    {
        MctpTxBit             = 1_bit,
        MctpRxBit             = 2_bit,
        MctpTxDoneBit         = 3_bit,
        AttachBit             = 4_bit,
        ResetBit              = 5_bit,
        UpdateRoutingTableBit = 6_bit,
        HidRxBit              = 7_bit,
        WdtEventBit           = 8_bit,
        // LSTP event bits (conditional)
        LstpRxBit     = 9_bit,
        LstpTxBit     = 10_bit,
        LstpTxDoneBit = 11_bit,
    };

    static void        make();
    static void        entrypoint(void* params);
    static usb::Status set_mctp_rx0_event();
    static usb::Status set_mctp_tx_done_event();
    static usb::Status set_update_routing_table_event();
    static usb::Status set_device_attach_event();
    static usb::Status reset_all_event_bits();
    bool               check_vbus_status();
    void               check_tx_done_status(uint32_t);
    void               clear_queue();

    // LSTP methods (implementation conditional on EnableLstp)
    void               clear_usb_lstp_queue();
    void               recover_lstp_endpoint();
    static usb::Status set_lstp_rx_event();
    static usb::Status set_lstp_tx_done_event();
    static bool        is_lstp_device_connected();
    void               lstp_receive();
    uint8_t            lstp_transmit();

    [[noreturn]] void main();
    void              mctp_receive();
#if (SYS_USB_HID_CP2112 > 0U)
    static usb::Status set_hid_rx_event();
    void               hid_receive();
#endif
    uint8_t transmit();
    void    update_routing_table();

    inline uint32_t enable_bit(uint32_t bits, uint32_t event_bit) { return bits | event_bit; }
    inline uint32_t disable_bit(uint32_t bits, uint32_t event_bit) { return bits & ~event_bit; }

    static void wdt_notify();

protected:
    using MctpHeader                     = UsbMctpHeader;
    constexpr static uint16_t UsbDmtfId  = nv::usb::UsbDmtfId;
    constexpr static uint32_t BufferSize = USB_MCTP_OUT_BUFFER_LENGTH;

    constexpr static uint32_t HidBufferSize = 64;

    using Buffer    = std::array<uint8_t, BufferSize>;
    using HidBuffer = std::array<uint8_t, HidBufferSize>;

    struct [[gnu::packed]] MctpPacket
    {
        MctpHeader                                                    usb_hdr;
        nv::mctp::Header                                              mctp_hdr;
        std::array<uint8_t, mctp::PktBufDataLen - sizeof(MctpHeader)> msg;
    };

#if (SYS_USB_HID_CP2112 > 0U)
    HidSmb _hid_smb;
#endif
    nv::lstp::LstpRouter _lstp_router;
    ipc::Event&          _event;
    ipc::Queue&          _tx;
    Buffer               mctp_rx_buffer{};
    Buffer               mctp_tx_buffer{};
    HidBuffer            hid_rx_buffer{};
    HidBuffer            hid_tx_buffer{};
    HidBuffer            hid_ep0_buffer{};

    // LSTP buffers - conditionally sized via UsbLstpMsgSize
    // When EnableLstp = false: UsbLstpMsgSize = 1
    // When EnableLstp = true: UsbLstpMsgSize = 512, uses full buffers
    std::array<uint8_t, nv::ipc::UsbLstpMsgSize> lstp_rx_buffer{};
    std::array<uint8_t, nv::ipc::UsbLstpMsgSize> lstp_tx_buffer{};
    uint32_t                                     lstp_rx_len{};
    uint32_t                                     lstp_tx_len{};

    sys::usb::Driver _driver;
    RoutingTable     _routing_table{};
    bool             was_vbus_on{};
    bool             is_vbus_on{};
    uint8_t          loop_count{};

    static MctpPacket& from(Buffer& arr, uint32_t offset)
    {
        // coverity[cert_ctr50_cpp_violation] mctp_receive() has already checked the offset
        return *std::bit_cast<MctpPacket*>(&arr[offset]);
    }

public:  // static api section
    using UsbRequest = HidSmb::Request;
    static usb::Status usb_tx(ipc::Queue::Item& item);
    static bool        to_usb(ipchandler::Id    src_id,
                              uint16_t          read_length,
                              ipc::Queue::Item& item,
                              i2c::I2cStatus    result);

    // LSTP interface (implementation conditional on EnableLstp)
    static usb::Status to_usbLstp(std::span<uint8_t>& data);
};

}  // namespace nv::usb

#endif  // !SYS_USB_DISABLED
