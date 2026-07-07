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

#include "nv/spdm/task.h"
#include <array>
#include <chrono>
#include <utility>
#include "sys/spdm/platform_certificate.h"
#include "nv/flash/flash.h"
#include "nv/logger/log.h"
#include "nv/logger/common.h"
#include "nv/bootloader.h"
#include "nv/mctp/interface.h"
#include "nv/nv.h"
#include "nv/i2c/lattice_driver.h"
#include "nv/vrot/interface/interface.h"
#include "library/spdm_lib_config.h"
using namespace std::chrono_literals;
using namespace nv::ipc;
using namespace nv;

namespace nv::spdm {
namespace {
constexpr bool HasVrotAp = !nv::vrot::ApList.empty();

void handle_mcu_auth_request(
    const nv::spdm::crypto::AuthenticateMcuFirmwareRequestParameter& request,
    nv::ipc::CoreId&                                                 request_core_id,
    nv::spdm::crypto::CryptoReqResParameters&                        response_parameters)
{
    request_core_id  = request.request_core_id;
    auto auth_result = nv::spdm::crypto::authenticate_mcu_firmware(
        request.input_parsing_fw_type);
    response_parameters = nv::spdm::crypto::AuthenticateMcuFirmwareResponseParameter{
        .auth_result           = auth_result,
        .input_parsing_fw_type = request.input_parsing_fw_type,
        .request_task_id       = request.request_task_id,
    };
}

bool handle_ap_auth_request(
    const nv::spdm::crypto::AuthenticateApFirmwareRequestParameter& request,
    nv::ipc::CoreId&                                                request_core_id,
    nv::spdm::crypto::CryptoReqResParameters&                       response_parameters)
{
    if constexpr (!HasVrotAp) {
        return false;
    }
    else {
        request_core_id  = request.request_core_id;
        auto auth_result = nv::spdm::crypto::CryptoStatus::FailUnknown;
        if (const auto ap = nv::vrot::find_ap(request.ap_id, nv::vrot::ApList); ap) {
            auth_result = nv::spdm::crypto::authenticate_ap_firmware_with_request_id(
                ap.value(),
                request.input_parsing_fw_type,
                request.request_task_id,
                request.auth_request_id);
        }
        response_parameters = nv::spdm::crypto::AuthenticateApFirmwareResponseParameter{
            .auth_result           = auth_result,
            .input_parsing_fw_type = request.input_parsing_fw_type,
            .ap_id                 = request.ap_id,
            .auth_request_id       = request.auth_request_id,
            .request_task_id       = request.request_task_id,
        };
        return true;
    }
}

bool handle_auth_request(const nv::spdm::crypto::CryptoReqResParameters& request_parameters,
                         nv::ipc::CoreId&                                request_core_id,
                         nv::spdm::crypto::CryptoReqResParameters&       response_parameters)
{
    if (const auto*
            request = std::get_if<nv::spdm::crypto::AuthenticateMcuFirmwareRequestParameter>(
                &request_parameters)) {
        handle_mcu_auth_request(*request, request_core_id, response_parameters);
        return true;
    }

    if (const auto*
            request = std::get_if<nv::spdm::crypto::AuthenticateApFirmwareRequestParameter>(
                &request_parameters)) {
        return handle_ap_auth_request(*request, request_core_id, response_parameters);
    }

    return false;
}

void set_authenticate_task_idle(nv::ipc::Event& event, nv::ipc::CoreId core_id)
{
    if (nv::ipc::Event::Status::Ok
        != event.set(nv::spdm::Task::EventBits::AuthenticateTaskIdle, core_id)) {
        nv::info("set AuthenticateTaskIdle fail for core %d\n", core_id);
    }
}

void refresh_ap_provision_status()
{
    if constexpr (HasVrotAp) {
        for (const auto& ap : nv::vrot::ApList) {
            if (!nv::vrot::supports_ap_provision(ap.type)) {
                continue;
            }
            nv::vrot::set_ap_provision_status(ap);
        }
    }
}
}  // namespace

// namespace {

// enum class SpdmLockFuseBlockCode : uint32_t
// {
//     LockSuccess     = 0,
//     ReadFuseFail    = 1,
//     ProgramFuseFail = 2,
//     AlreadyLock     = 3
// };

// void lock_fuse_block()
// {
//     constexpr uint32_t EfuseLockFieldIndex = 0;
//     uint32_t           current_fuse_data   = 0;
//     // read current fuse lock configuration
//     auto flash_status = nv::flash::Flash::read_efuse(EfuseLockFieldIndex, current_fuse_data);
//     if (nv::flash::Status::Ok != flash_status) {
//         nv::logger::error(nv::logger::Event::SpdmLockFuseBlock,
//                           nv::logger::data_from_two_u32(
//                               std::to_underlying(SpdmLockFuseBlockCode::ReadFuseFail),
//                               std::to_underlying(flash_status)));
//         return;
//     }
//     /**
//      * @brief Lock Fuse Config
//      *
//      * - Byte_0
//      *
//      *   Reserved
//      *
//      * - Byte_1
//      *
//      *   OSCAA_KEY_LOCK[0]\(Bit_7) + PRINCE_CFG_LOCK[2:0] + BOOT_CFG_LOCK[2:0] +
//      Reserved(Bit_0)
//      *
//      * - Byte_2
//      *
//      *   CUST_LOCK1[2:0]\(Bit_7) + CUST_LOCK0[2:0] + OSCAA_KEY_LOCK[2:1]\(Bit_0)
//      *
//      * - Byte_3
//      *
//      *   Reserved[1:0]\(Bit_7) + CUST_LOCK3[2:0] + CUST_LOCK2[2:0]\(Bit_0)
//      *
//      *
//      * Item to be set
//      *
//      * - PRINCE_CFG_LOCK[0]
//      *
//      * - OSCAA_KEY_LOCK[0]
//      *
//      * - CUST_LOCK0[0]
//      *
//      * - CUST_LOCK1[0]
//      *
//      */
//     constexpr uint32_t LockAllFuseConfig = 0x00249000;
//     if ((current_fuse_data & LockAllFuseConfig) == LockAllFuseConfig) {  // do nothing
//                                                                          // when fuse is
//                                                                          // already
//                                                                          // locked.
//         nv::logger::info(nv::logger::Event::SpdmLockFuseBlock,
//                          nv::logger::data_from_two_u32(
//                              std::to_underlying(SpdmLockFuseBlockCode::AlreadyLock),
//                              std::to_underlying(nv::flash::Status::Ok)));
//     }
//     else {  // lock fuse.
//         flash_status = nv::flash::Flash::program_efuse(EfuseLockFieldIndex,
//         LockAllFuseConfig); if (nv::flash::Status::Ok != flash_status) {
//             nv::logger::error(nv::logger::Event::SpdmLockFuseBlock,
//                               nv::logger::data_from_two_u32(
//                                   std::to_underlying(SpdmLockFuseBlockCode::ProgramFuseFail),
//                                   std::to_underlying(flash_status)));
//         }
//         else {
//             nv::logger::info(nv::logger::Event::SpdmLockFuseBlock,
//                              nv::logger::data_from_two_u32(
//                                  std::to_underlying(SpdmLockFuseBlockCode::LockSuccess),
//                                  std::to_underlying(flash_status)));
//         }
//     }
// }
// }  // namespace

nv::mctp::Header& Task::get_last_header()
{
    auto& task = nv::spdm::Task::get_task();
    return task.last_request_header;
}

nv::mctp::PrivateHeader& Task::get_last_private_header()
{
    auto& task = nv::spdm::Task::get_task();
    return task.last_request_private_header;
}

measurement::MeasurementCacheT& Task::get_measurement_cache()
{
    auto& task = nv::spdm::Task::get_task();
    return task.measurement_cache;
}

nv::spdm::Task& Task::get_task()
{
    auto& task = nv::ipc::Supervisor::inst().task(nv::ipc::TaskId::Spdm);
    // NOLINTNEXTLINE(*-reinterpret-cast)
    auto& spdm_task = reinterpret_cast<nv::spdm::Task&>(task);
    return spdm_task;
}

void Task::make()
{
    NV_TASK_DATA static Task task;
    // Minimum stack plus future overhead plus increase to support additional APs.
    constexpr auto StackSize = std::max(2880 + 512 + (HasVrotAp ? 384 : 0),
                                        int(configMINIMAL_STACK_SIZE));
    NV_STACK static sys::ipc::TaskStack<StackSize> stack;
    // NOLINTNEXTLINE(*-reinterpret-cast)
    const std::span<uint8_t> Priv(reinterpret_cast<uint8_t*>(&task), sizeof(Task));
    task.setup(stack.span(), Priv, Priority::Spdm, Task::entrypoint);
    measurement::spdm_init_all_measurement_cache();
}

void Task::entrypoint(void* params)
{
    NV_ASSERT(params != nullptr);
    using namespace std::chrono_literals;
    auto& task = *static_cast<Task*>(params);
    task.main();
}

// send message to the spdm library
nv::spdm::Status Task::spdm_tx(const nv::mctp::Packet& packet)
{
    nv::ipc::Queue& queue           = nv::ipc::Queue::make(nv::ipc::QueueId::SpdmRx);
    const size_t    ItemSize        = queue.item_size();
    const auto      PacketInterface = static_cast<mctp::Client>(packet.priv.packet_interface);

    // drop the packet from incorrect interface
    // support only usb / i2c(need to open configuration in config.h)
    if constexpr (nv::ipc::SpdmI2cResponder) {
        if (PacketInterface != mctp::Client::UsUsb && PacketInterface != mctp::Client::UsI2c) {
            return nv::spdm::Status::InvalidInterface;
        }
    }
    else {
        if (PacketInterface != mctp::Client::UsUsb) {
            return nv::spdm::Status::InvalidInterface;
        }
    }

    nv::ipc::Event&        event        = nv::ipc::Event::make(nv::ipc::EventId::SpdmTask);
    nv::ipc::Event::Status event_status = Event::Status::Ok;
    auto item         = nv::ipc::Queue::Item(std::bit_cast<uint8_t*>(&packet), ItemSize);
    auto queue_status = queue.send(item, 100ms);
    if (queue_status != nv::ipc::Queue::Status::Ok) {
        return nv::spdm::Status::QueueSendFail;
    }

    if (PacketInterface == mctp::Client::UsUsb) {
        event_status = event.set(spdm::Task::EventBits::UsbRxBit);
    }

    if (PacketInterface == mctp::Client::UsI2c) {
        event_status = event.set(spdm::Task::EventBits::I2cRxBit);
    }

    if (event_status != nv::ipc::Event::Status::Ok) {
        return nv::spdm::Status::EventSetFail;
    }

    return nv::spdm::Status::Ok;
}

Task::Task()
: ipc::Task(ipc::TaskId::Spdm, "SPDM")
, _event(ipc::Event::make(ipc::EventId::SpdmTask))
, _tx(ipc::Queue::make(ipc::QueueId::MctpSpdmRequest))
, _rx(ipc::Queue::make(ipc::QueueId::SpdmRx))
{
    // Dual core project will need SpdmCryptoHelper max_items = 2
    // 1 for Core0, 1 for Core1
    static_assert(
        (!nv::ipc::EnableDualCore)
            || std::get<1>(nv::ipc::QueueInfos[int(nv::ipc::QueueId::SpdmCryptoHelper)])
                   == DualCoreSpdmCryptoHelperQueueMaxItems,
        "SpdmCryptoHelper queue max_items is mismatch");
}

[[noreturn]] void Task::main()
{
    nv::bootloader::Driver::set_task_booted(nv::ipc::BootedEventBits::Spdm);

    auto           event            = nv::ipc::Event::make(nv::ipc::EventId::SpdmTask);
    constexpr auto CertReadyTimeout = 10;
    if (auto ret = event.wait(
            EventBits::CertReadyBit, true, false, std::chrono::seconds(CertReadyTimeout));
        !ret) {
        nv::logger::info(nv::logger::Event::SpdmCertReady, {});
    }

    if (event.set(nv::spdm::Task::AuthenticateTaskIdle, nv::ipc::CoreId::Core0)
        != nv::ipc::Event::Status::Ok) {
        nv::info("spdm task start set AuthenticateTaskIdle fail for Core0\n");
    }

    // Only called set event for Core1 if it is dual core project
    if constexpr (nv::ipc::EnableDualCore) {
        if (event.set(nv::spdm::Task::AuthenticateTaskIdle, nv::ipc::CoreId::Core1)
            != nv::ipc::Event::Status::Ok) {
            nv::info("spdm task start set AuthenticateTaskIdle fail for Core1\n");
        }
    }

    {
        uint32_t enf_cnsa     = 0;
        auto     flash_status = nv::flash::Flash::read_enf_cnsa(enf_cnsa);
        if (flash_status == nv::flash::Status::Ok) {
            nv::logger::info(nv::logger::Event::SpdmCmpaEnfCnsa,
                             nv::logger::data_from_u32(enf_cnsa));
        }
        else {
            nv::logger::error(nv::logger::Event::SpdmCmpaEnfCnsa,
                              nv::logger::data_from_u32(std::to_underlying(flash_status)));
        }
    }

    // start to generate the cert
    // use dummy certificate
    if constexpr (nv::ipc::SpdmDummyCertificates == true) {
        nv::info("use dummy certificate\n");
        this->certificate_chain_slot_mask = 0x01;
    }
    else {
        // log the dda ordinal number on efuse
        constexpr uint32_t DdaOrdinalEfuseAddr = sys::spdm::cert::DdaOrdinalEfuseAddr;
        uint32_t           dda_ordinal_number  = 0;
        auto               flash_status = nv::flash::Flash::read_efuse(DdaOrdinalEfuseAddr,
                                                         dda_ordinal_number);

        nv::logger::info(nv::logger::Event::SpdmCertDdaOtpValue,
                         nv::logger::data_from_two_u32(std::to_underlying(flash_status),
                                                       dda_ordinal_number));
        bool could_lock = true;
        if (!nv::spdm::cert::verify_l2_l3_cert()) {
            could_lock = false;
        }
        if (!nv::spdm::cert::generate_l4_cert()) {
            could_lock = false;
        }
        if (!nv::spdm::cert::generate_l5_cert()) {
            could_lock = false;
        }
        if (could_lock) {
            this->certificate_chain_slot_mask = 0x01;
            // lock_fuse_block();
        }
    }

    // update dbg nonce if necessary
    nv::spdm::measurement::update_dbg_token_nonce_meas();

    // setting the spdm engine context
    certificate_chain_slot_0.certificate_chain        = nv::spdm::cert::get_cert_chain();
    certificate_chain_slot_0.certificate_chain_length = nv::spdm::cert::get_cert_chain_length(
        0);

    // setting for each responder interface
    for (size_t i = 0; i < spdm_engine_context.size(); i++) {
        // NOLINTBEGIN(cppcoreguidelines-pro-bounds-constant-array-index)
        auto result = spdm_engine_context[i].spdm_engine_initialize();
        // NOLINTEND(cppcoreguidelines-pro-bounds-constant-array-index)
        if (result != pdk::spdm::app::res::library::SpdmEngineSettingError::SUCCESS) {
            nv::info("spdm engine setting fail for interface %d, error code: %d\n",
                     i,
                     std::to_underlying(result));
            NV_ASSERT(false,
                      "spdm engine setting fail for interface %d, error code: %d\n",
                      i,
                      std::to_underlying(result));
        }
    }

    // Validate MCU pwr-fail I2C debug token against NPDS cache at boot.
    // Ignore return value.
    (void)nv::debugtoken::check_debug_token_subtype_enabled(
        nv::debugtoken::Type::McuDebug, nv::debugtoken::DebugTokenSubtypePwrFailI2cDebug);

    refresh_ap_provision_status();

    // coverity[no_escape] - no escape is expected
    while (true) {
        nv::ipc::Event& event = nv::ipc::Event::make(nv::ipc::EventId::SpdmTask);
        nv::ipc::Queue& queue = nv::ipc::Queue::make(nv::ipc::QueueId::SpdmCryptoHelper);

        auto event_bits = event.wait(nv::spdm::Task::RxWaitEventBit, false, false, 1s).value();

        // check data is recive
        if (event_bits & nv::spdm::Task::EventBits::UsbRxBit) {
            auto status = spdm_engine_context[0].process_message();
            if (status != LIBSPDM_STATUS_SUCCESS) {
                nv::info("libspdm_responder_dispatch_message fail, status: %d\n", status);
            }
        }
        if (event_bits & nv::spdm::Task::EventBits::CryptoPrimitiveRequestBit) {
            (void)event.clear(nv::spdm::Task::EventBits::CryptoPrimitiveRequestBit);
            nv::spdm::crypto::process_primitive_request_queue();
        }
        // need to auth fw
        if (event_bits & nv::spdm::Task::EventBits::AuthenticateRequestBit) {
            (void)event.clear(nv::spdm::Task::EventBits::AuthenticateRequestBit);

            // NOLINTNEXTLINE(misc-const-correctness) queue.recv() writes through Queue::Item.
            nv::spdm::crypto::CryptoReqResParameters request_parameters{};

            auto& request_parameters_arr_view = *std::bit_cast<
                std::array<uint8_t, sizeof(decltype(request_parameters))>*>(
                &request_parameters);

            nv::ipc::Queue::Item item(request_parameters_arr_view.data(),
                                      request_parameters_arr_view.size());

            auto status = queue.recv(item, 100ms);

            if (status != nv::ipc::Queue::Status::Ok) {
                // fail
            }
            else {
                nv::spdm::crypto::CryptoReqResParameters response_parameters{};

                nv::ipc::CoreId request_core_id = nv::ipc::CoreId::Invalid;
                const bool      request_handled = handle_auth_request(
                    request_parameters, request_core_id, response_parameters);
                if (!request_handled) {
                    nv::logger::error(nv::logger::Event::SpdmError,
                                      nv::logger::data_from_u32(
                                          static_cast<uint32_t>(request_parameters.index())));
                    set_authenticate_task_idle(event, nv::ipc::CoreId::Core0);
                    if constexpr (nv::ipc::EnableDualCore) {
                        set_authenticate_task_idle(event, nv::ipc::CoreId::Core1);
                    }
                }
                else {
                    nv::spdm::crypto::send_response_back(response_parameters);

                    // Set the event based on request_core_id
                    set_authenticate_task_idle(event, request_core_id);
                }

                // Set the event when the queue is not empty to handle request again.
                // It's possible because both cores may send queue then set event.
                if (queue.size() != 0) {
                    (void)event.set(nv::spdm::Task::EventBits::AuthenticateRequestBit);
                }
            }
        }
        if constexpr (nv::ipc::SpdmI2cResponder) {
            if (event_bits & nv::spdm::Task::EventBits::I2cRxBit) {
                static_assert(!nv::ipc::SpdmI2cResponder
                                  or std::tuple_size_v<decltype(spdm_engine_context)> >= 2,
                              "spdm_engine_context size should larger than 1 when I2C "
                              "responder is enabled");
                // NOLINTBEGIN(cppcoreguidelines-pro-bounds-constant-array-index)
                auto status = spdm_engine_context[1].process_message();
                // NOLINTEND(cppcoreguidelines-pro-bounds-constant-array-index)
                if (status != LIBSPDM_STATUS_SUCCESS) {
                    nv::info("libspdm_responder_dispatch_message fail, status: %d\n", status);
                }
            }
        }
    }
}
}  // namespace nv::spdm
