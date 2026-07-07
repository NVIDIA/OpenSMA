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

#include "nv/common/uuid.h"
#include "nv/flash/driver.h"
#include "nv/ipc/event.h"
#include "nv/ipc/queue.h"
#include "nv/ipc/supervisor.h"
#include "nv/ipc/task.h"
#include "nv/mctp/interface.h"
#include "nv/mctp/router.h"
#include "nv/spdm/cert_library.h"
#include "nv/spdm/spdm_crypto_helper.h"
#include "nv/spdm/spdm_get_measurement.h"
#include "nv/spdm/spdm_cert_chain.h"
#include "sys/ipc/event.h"
#include "corepdk/modules/spdm/src/app/pdk-spdm-app-res-library.h"
namespace nv::spdm {

constexpr size_t SpdmResponderNumber = nv::ipc::SpdmI2cResponder == true ? 2 : 1;
constexpr auto   DualCoreSpdmCryptoHelperQueueMaxItems = 2;

enum class Status
{
    Ok,             ///< success
    QueueSendFail,  ///< queue send return fail
    EventSetFail,   ///< event set fail
    InvalidInterface,
    Timeout,       ///< no response within deadline (used by crypto::request)
    Busy,          ///< another caller holds the SPDM idle lock
    Unknown,       ///< an unknown error occurred
    NotSupported,  ///< the request is not supported
};
struct CertificateChain
{
    std::array<uint8_t, nv::spdm::cert::MaxCertChainLength> certificate_chain{};
    size_t                                                  certificate_chain_length{};
};

class Task : public ipc::Task
{
public:
    Task();
    enum EventBits : nv::ipc::Event::Bits
    {
        UsbRxBit = 0_bit,
        I2cRxBit = 1_bit,
        // for authenticate usage
        AuthenticateRequestBit    = 4_bit,  // notify the spdm task to auth
        AuthenticateTaskIdle      = 5_bit,  // act as a lock to access spdm auth functionality
        CryptoPrimitiveRequestBit = 6_bit,
        CertReadyBit              = 22_bit,
    };
    static constexpr nv::ipc::Event::Bits
        CryptoRequestEventBit = static_cast<nv::ipc::Event::Bits>(
                                    spdm::Task::EventBits::AuthenticateRequestBit)
                              | static_cast<nv::ipc::Event::Bits>(
                                    spdm::Task::EventBits::CryptoPrimitiveRequestBit);
    static constexpr auto RxWaitEventBit = (nv::ipc::SpdmI2cResponder == true
                                                ? spdm::Task::EventBits::UsbRxBit
                                                      | spdm::Task::EventBits::I2cRxBit
                                                : spdm::Task::EventBits::UsbRxBit)
                                         | CryptoRequestEventBit;

    static void make();
    static void entrypoint(void* params);

    static nv::spdm::Status spdm_tx(const nv::mctp::Packet& packet);

    // get_xxx function is for access the variable of spdm task
    // save mctp header for sending back usage
    static nv::mctp::Header&        get_last_header();
    static nv::mctp::PrivateHeader& get_last_private_header();

    // this is save all of the measurement that need to catch
    static measurement::MeasurementCacheT& get_measurement_cache();

    static nv::spdm::Task& get_task();

    [[noreturn]] void main();

    inline uint32_t enable_bit(uint32_t bits, uint32_t event_bit) { return bits | event_bit; }
    inline uint32_t disable_bit(uint32_t bits, uint32_t event_bit) { return bits & ~event_bit; }

    nv::mctp::Header        last_request_header{};
    nv::mctp::PrivateHeader last_request_private_header{};

    std::array<pdk::spdm::app::res::library::SpdmEngineContext, SpdmResponderNumber>
        spdm_engine_context{};

    // for cache the certificate chain
    uint8_t                        certificate_chain_slot_mask = 0;
    CertificateChain               certificate_chain_slot_0{};
    measurement::MeasurementCacheT measurement_cache{};

    std::array<uint8_t, 200>  spdm_library_recv_buffer{};
    std::array<uint8_t, 2048> spdm_library_send_buffer{};

    // currently only use for sha512 context
    static constexpr size_t MaxCryptoContextSize      = sizeof(mbedtls_sha512_context);
    static constexpr size_t MaxCryptoContextItemCount = 3;
    // for spdm context and scratch buffer
    static constexpr size_t MaxSpdmContextItemSize  = 2200;  // last check need 2080
    static constexpr size_t MaxSpdmContextItemCount = nv::ipc::SpdmI2cResponder == true ? 2 : 1;
    static constexpr size_t MaxSpdmScratchBufferItemSize  = 4200;  // last check need 4141
    static constexpr size_t MaxSpdmScratchBufferItemCount = nv::ipc::SpdmI2cResponder == true
                                                              ? 2
                                                              : 1;

    // for crypto context
    struct CryptoContextAllocateItem
    {
        std::array<uint8_t, MaxCryptoContextSize> data{};
        size_t                                    used_size{};
    };
    // for spdm context
    struct SpdmContextAllocateItem
    {
        std::array<uint8_t, MaxSpdmContextItemSize> data{};
        size_t                                      used_size{};
    };
    // for spdm scratch buffer
    struct SpdmScratchBufferAllocateItem
    {
        std::array<uint8_t, MaxSpdmScratchBufferItemSize> data{};
        size_t                                            used_size{};
    };

    // for crypto context
    std::array<CryptoContextAllocateItem, MaxCryptoContextItemCount>
        crypto_context_allocate_item{};
    // for spdm context
    std::array<SpdmContextAllocateItem, MaxSpdmContextItemCount> spdm_context_allocate_item{};
    // for spdm scratch buffer
    std::array<SpdmScratchBufferAllocateItem, MaxSpdmScratchBufferItemCount>
        spdm_scratch_buffer_allocate_item{};

protected:
    ipc::Event& _event;
    ipc::Queue& _tx;
    ipc::Queue& _rx;

public:  // static api section
};
}  // namespace nv::spdm
