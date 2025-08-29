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
#include "corepdk/modules/spdm/src/app/pdk-spdm-app-res-ada-library.h"

namespace nv::spdm {

constexpr size_t SpdmResponderNumber = 2;
enum class Status
{
    Ok,             ///< success
    QueueSendFail,  ///< queue send return fail
    EventSetFail,   ///< event set fail
    InvalidInterface,
    Unknown,  ///< an unknown error occurred
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
        AuthenticateRequestBit = 4_bit,  // notify the spdm task to auth
        AuthenticateTaskIdle   = 5_bit,  // act as a lock to access spdm auth functionality
        CertReadyBit           = 22_bit,
    };
    static constexpr auto RxWaitEventBit = nv::ipc::SpdmI2cResponder == true
                                             ? spdm::Task::EventBits::UsbRxBit
                                                   | spdm::Task::EventBits::I2cRxBit
                                             : spdm::Task::EventBits::UsbRxBit;

    static void make();
    static void entrypoint(void* params);

    static nv::spdm::Status spdm_tx(const nv::mctp::Packet& packet);

    // get_xxx function is for access the variable of spdm task
    // save mctp header for sending back usage
    static nv::mctp::Header&        get_last_header();
    static nv::mctp::PrivateHeader& get_last_private_header();

    // spdm_c_responder.adb context usage
    static std::array<pdk::spdm::platforms::res::ada::library::PlatformContext,
                      SpdmResponderNumber>&
    get_context_instance();

    // this is save all of the measurement that need to catch
    static measurement::MeasurementCacheT& get_measurement_cache();

    static nv::spdm::Task& get_task();

    [[noreturn]] void main();

    inline uint32_t enable_bit(uint32_t bits, uint32_t event_bit) { return bits | event_bit; }
    inline uint32_t disable_bit(uint32_t bits, uint32_t event_bit) { return bits & ~event_bit; }

    nv::mctp::Header        last_request_header{};
    nv::mctp::PrivateHeader last_request_private_header{};
    std::array<pdk::spdm::platforms::res::ada::library::PlatformContext, SpdmResponderNumber>
        context_instance{
            pdk::spdm::platforms::res::ada::library::PlatformContext{.inst_num = 255},
            pdk::spdm::platforms::res::ada::library::PlatformContext{.inst_num = 255}};
    measurement::MeasurementCacheT measurement_cache{};

protected:
    ipc::Event& _event;
    ipc::Queue& _tx;
    ipc::Queue& _rx;

public:  // static api section
};
}  // namespace nv::spdm
