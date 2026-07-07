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
#include <algorithm>
#include <chrono>

#include "nv/spdm/spdm_cert_chain.h"
#include "nv/spdm/spdm_crypto_helper.h"
#include <cstring>
#include "nv/crypto/key_clear_guard.h"
#include "sys/common/utils.h"
#include "nv/debugtoken/debugtoken.h"
#include "nv/ipc/supervisor.h"
#include "nv/spdm/task.h"
#include "nv/nv.h"
#include "nv/flash/flash.h"
#include "sys/crypto/crypto.h"
#include "nv/pldm/task.h"
#include "nv/secure_boot/secure_boot.h"
#include "nv/vrot/interface/interface.h"
#include "nv/logger/log.h"
#include NV_IPC_CONFIG_H  // for nv::vrot::ApList
using namespace nv;
namespace nv::spdm::crypto {
namespace {
constexpr bool HasVrotAp = !nv::vrot::ApList.empty();

// Centralized AP auth request id allocator state. Callers never provide this
// value; the helper fills it before queueing an AP auth request.
NV_SHARED_BSS static uint8_t
    s_ap_auth_request_counter{};  // NOLINT(*-non-const-global-variables)

uint8_t allocate_ap_auth_request_id()
{
    // 0 is reserved as the "uninitialized" sentinel, so the returned ID cycles
    // through 1..255.
    ++s_ap_auth_request_counter;
    if (s_ap_auth_request_counter == 0U) {
        ++s_ap_auth_request_counter;
    }
    return s_ap_auth_request_counter;
}

void release_auth_worker(nv::ipc::Event& event, nv::ipc::CoreId core_id)
{
    if (event.set(nv::spdm::Task::EventBits::AuthenticateTaskIdle, core_id)
        != nv::ipc::Event::Status::Ok) {
        nv::info("spdm release AuthenticateTaskIdle fail for core %d\n", core_id);
    }
}
}  // namespace

void send_response_back(const CryptoReqResParameters& result)
{
    // get the current task id
    nv::ipc::TaskId current_task_id = nv::ipc::TaskId::Begin;
    current_task_id                 = ipc::Supervisor::inst().current_task_id();
    // return authenticate result directly if spdm task is on current core
    if (current_task_id != ipc::TaskId::Spdm) {
        NV_ASSERT(false,
                  "Only spdm could send back response, current task id: %d\n",
                  current_task_id);
    }

    // find the response according to the result type
    if (const auto*
            VarPtr = std::get_if<nv::spdm::crypto::AuthenticateMcuFirmwareResponseParameter>(
                &result)) {
        if (VarPtr->request_task_id == nv::ipc::TaskId::Pldm) {
            nv::spdm::crypto::send_authenticate_mcu_firmware_result<nv::ipc::TaskId::Pldm>(
                *VarPtr);
            return;
        }
    }
    if constexpr (HasVrotAp) {
        if (const auto*
                VarPtr = std::get_if<nv::spdm::crypto::AuthenticateApFirmwareResponseParameter>(
                    &result)) {
            if (VarPtr->request_task_id == nv::ipc::TaskId::Pldm) {
                nv::spdm::crypto::send_authenticate_ap_firmware_result<nv::ipc::TaskId::Pldm>(
                    *VarPtr);
                return;
            }
            // Callback-style AP requests already handled the result.
            return;
        }
    }

    NV_ASSERT(false, "Unexpected response type: %d\n", result.index());
}

CryptoStatus _authenticate_firmware_result(const nv::vrot::ApInfo&                  ap,
                                           const nv::fw_parser::ap::ParsingApFwType auth_slot,
                                           nv::ipc::TaskId request_task_id,
                                           uint8_t         auth_request_id)
{
    if constexpr (!HasVrotAp) {
        return CryptoStatus::FailUnknown;
    }

    auto auth_result = CryptoStatus::Success;
    auto ap_metadata = nv::fw_parser::ap::ApFwMetadata{};
    do {  // NOLINT(cppcoreguidelines-avoid-do-while)
        auto& meta_data_array_view = *std::bit_cast<std::array<uint8_t, sizeof(ap_metadata)>*>(
            &ap_metadata);
        const std::span<uint8_t> ap_metadata_buffer(meta_data_array_view.data(),
                                                    meta_data_array_view.size());
        if (nv::vrot::read_metadata(
                ap, nv::vrot::slot_from_parsing(auth_slot), 0, ap_metadata_buffer)
            != nv::vrot::ApOpErrCode::Success) {
            auth_result = CryptoStatus::FailApMetadataRead;
            break;
        }
        // NOLINTBEGIN(cppcoreguidelines-pro-bounds-constant-array-index)
        if (ap_metadata.tbs_data.verif_pub_key != ApFwPublicKeys[0]
            && ap_metadata.tbs_data.verif_pub_key != ApFwPublicKeys[1]) {
            // NOLINTEND(cppcoreguidelines-pro-bounds-constant-array-index)
            auth_result = CryptoStatus::FailApPublicKeyMismatch;
            break;
        }

        const std::span<uint8_t> ap_metadata_tbs_data(
            std::bit_cast<uint8_t*>(&ap_metadata.tbs_data), sizeof(ap_metadata.tbs_data));

        std::array<uint8_t, Sha384HashSize> ap_metadata_hash{};
        // hash the metadata
        auth_result = spdm_hash_data(
            ap_metadata_hash.data(), ap_metadata_tbs_data.data(), ap_metadata_tbs_data.size());
        if (auth_result != nv::spdm::crypto::CryptoStatus::Success) {
            break;
        }
        // metadata signature verify
        auto r_signature_span = std::span<const uint8_t>(ap_metadata.nv_signature.r);
        auto s_signature_span = std::span<const uint8_t>(ap_metadata.nv_signature.s);
        auth_result = nv::spdm::crypto::spdm_ecdsa_verify(ap_metadata.tbs_data.verif_pub_key,
                                                          r_signature_span,
                                                          s_signature_span,
                                                          ap_metadata_hash);
        if (auth_result != nv::spdm::crypto::CryptoStatus::Success) {
            break;
        }

        // check the rollback protection
        {
            uint32_t secure_fw_version_on_device = 0;
            if (nv::flash::Flash::read_secure_fw_version(secure_fw_version_on_device,
                                                         nv::flash::KeyRollbackSelect::Ap0)
                != nv::flash::Status::Ok) {
                auth_result = CryptoStatus::FailCfpaAccess;
                break;
            }
            if (secure_fw_version_on_device > ap_metadata.tbs_data.sec_version) {
                auth_result = CryptoStatus::FailApRollbackProtection;
                nv::logger::info(nv::logger::Event::SpdmCryptoApRollbackProtectionActive,
                                 nv::logger::EventData{
                                     std::to_underlying(auth_slot),
                                     ap_metadata.tbs_data.sec_version,
                                     static_cast<uint8_t>(secure_fw_version_on_device),
                                 });
                break;
            }
        }
        // check key revoke
        {
            uint32_t key_revocation_value_on_device = 0;
            auto     status = nv::flash::Flash::read_key_revoke(key_revocation_value_on_device,
                                                            nv::flash::KeyRollbackSelect::Ap0);
            static_assert(
                std::tuple_size_v<decltype(ApFwPublicKeys)>
                    == std::to_underlying(nv::fw_parser::ap::PublicKeyIndex::KeyIndexCount),
                "ApFwPublicKeys size should be the same as KeyIndexCount");
            static_assert(
                sizeof(nv::fw_parser::ap::PublicKeyIndex)
                    == sizeof(key_revocation_value_on_device),
                "PublicKeyIndex size should be the same as key_revocation_value_on_device");
            if (status != nv::flash::Status::Ok
                || static_cast<size_t>(key_revocation_value_on_device) >= std::to_underlying(
                       nv::fw_parser::ap::PublicKeyIndex::KeyIndexCount)) {
                auth_result = CryptoStatus::FailCfpaAccess;
                break;
            }
            // If revocation level requires prod key (debug key revoked),
            // but firmware is not using prod key - check if debug token allows bypass
            if (key_revocation_value_on_device
                    == std::to_underlying(nv::fw_parser::ap::PublicKeyIndex::ProdKeyIndex)
                && ap_metadata.tbs_data.verif_pub_key
                       != ApFwPublicKeys[std::to_underlying(
                           nv::fw_parser::ap::PublicKeyIndex::ProdKeyIndex)]) {
                const auto dt_status = nv::debugtoken::check_flash_debug_fw_token_for_ap(
                    ap.type);
                if (dt_status != nv::debugtoken::TokenErrorCode::NoErrorCode) {
                    auth_result = CryptoStatus::FailApImageSigningKeyRevoke;
                    break;
                }
            }
        }

        // store the ap metadata when metadata auth is done
        nv::secure_boot::SecureBoot::persist_ap_fw_authenticate_data_in_progress(
            static_cast<uint8_t>(ap.id), auth_slot, ap_metadata.tbs_data, request_task_id);

        const auto image_count = static_cast<size_t>(ap_metadata.tbs_data.ap_fw_images_count);
        if (image_count > ap_metadata.tbs_data.hash_table.size()) {
            auth_result = CryptoStatus::FailParsingFirmware;
            break;
        }

        // check image hash
        constexpr size_t MetadataSize = sizeof(ap_metadata);
        for (size_t i = 0; i < image_count; i++) {
            auto ctx = Sha384Context{};
            if (!ctx.init()) {
                auth_result = CryptoStatus::FailHashStart;
                break;
            }
            // NOLINTBEGIN(cppcoreguidelines-pro-bounds-constant-array-index)
            size_t need_to_read_size = ap_metadata.tbs_data.hash_table[i].length;

            const size_t image_blob_offset = ap_metadata.tbs_data.hash_table[i].offset
                                           + ap_metadata.tbs_data.image_offset;
            // NOLINTEND(cppcoreguidelines-pro-bounds-constant-array-index)
            if (image_blob_offset < MetadataSize) {
                auth_result = CryptoStatus::FailParsingFirmware;
                break;
            }
            size_t           fw_data_offset  = image_blob_offset - MetadataSize;
            constexpr size_t HashStreamChunk = 1024;
            std::array<uint8_t, HashStreamChunk> read_buffer{};
            while (need_to_read_size != 0) {
                const size_t read_size = std::min(need_to_read_size, HashStreamChunk);
                auto read_buffer_view  = std::span<uint8_t>(read_buffer).subspan(0, read_size);
                if (nv::vrot::read_fw_data(ap,
                                           nv::vrot::slot_from_parsing(auth_slot),
                                           static_cast<uint32_t>(fw_data_offset),
                                           read_buffer_view)
                    != nv::vrot::ApOpErrCode::Success) {
                    auth_result = CryptoStatus::FailApImageRead;
                    break;
                }
                if (!ctx.update(read_buffer.data(), read_size)) {
                    auth_result = CryptoStatus::FailHashUpdate;
                    break;
                }
                fw_data_offset    += read_size;
                need_to_read_size -= read_size;
            }
            if (auth_result != nv::spdm::crypto::CryptoStatus::Success) {
                break;
            }
            std::array<uint8_t, nv::spdm::crypto::Sha384HashSize> ap_image_hash{};
            if (!ctx.finish(ap_image_hash.data())) {
                auth_result = CryptoStatus::FailHashCalc;
                break;
            }
            // NOLINTBEGIN(cppcoreguidelines-pro-bounds-constant-array-index)
            if (ap_metadata.tbs_data.hash_table[i].hash != ap_image_hash) {
                // NOLINTEND(cppcoreguidelines-pro-bounds-constant-array-index)
                auth_result = CryptoStatus::FailApImageHashMismatch;
                break;
            }
        }
        if (auth_result != CryptoStatus::Success) {
            break;
        }
    } while (false);

    /*
    log the auth result
    1. auth_slot
    2. auth_result
    3. ap_metadata.tbs_data.ap_cfg_key_idx
    4. ap_metadata.tbs_data.sec_version
    5. ap_metadata.tbs_data.fw_version
    */
    nv::logger::info(nv::logger::Event::SpdmCryptoApAuthResult,
                     nv::logger::EventData{
                         std::to_underlying(auth_result),
                         std::to_underlying(auth_slot),
                         ap_metadata.tbs_data.ap_cfg_key_idx,
                         ap_metadata.tbs_data.sec_version,
                         ap_metadata.tbs_data.fw_version.major,
                         ap_metadata.tbs_data.fw_version.minor,
                         ap_metadata.tbs_data.fw_version.patch,
                         ap_metadata.tbs_data.fw_version.build,
                     });
    // Keep this constexpr-guarded so no-AP builds do not reference secure_boot.
    if constexpr (HasVrotAp) {
        return nv::secure_boot::SecureBoot::secure_boot_auth_callback(
            static_cast<uint8_t>(ap.id),
            auth_slot,
            auth_result,
            ap_metadata.tbs_data,
            request_task_id,
            auth_request_id);
    }
    return auth_result;
}

CryptoStatus _send_request_to_spdm(CryptoReqResParameters request_parameters,
                                   uint8_t*               auth_request_id = nullptr)
{
    // send the request to queue
    auto& crypto_queue = nv::ipc::Queue::make(nv::ipc::QueueId::SpdmCryptoHelper);

    auto       spdm_task_envnt = ipc::Event::make(ipc::EventId::SpdmTask);
    const auto request_core_id = nv::ipc::get_current_core();

    // check if there is another task want spdm to authenticate fw
    auto spdm_idle_check = spdm_task_envnt.wait(
        nv::spdm::Task::AuthenticateTaskIdle, true, false, std::chrono::seconds{1});
    if (!spdm_idle_check) {
        return CryptoStatus::FailSendToSpdm;
    }

    if (auth_request_id != nullptr) {
        auto* request = std::get_if<AuthenticateApFirmwareRequestParameter>(
            &request_parameters);
        if (request == nullptr) {
            release_auth_worker(spdm_task_envnt, request_core_id);
            return CryptoStatus::FailSendToSpdm;
        }
        request->auth_request_id = allocate_ap_auth_request_id();
    }

    auto& request_parameters_arr_view = *std::bit_cast<
        std::array<uint8_t, sizeof(decltype(request_parameters))>*>(&request_parameters);
    const nv::ipc::Queue::Item ReqItem(request_parameters_arr_view.data(),
                                       request_parameters_arr_view.size());

    // send to the queue.
    auto queue_status = crypto_queue.send(ReqItem, std::chrono::seconds{1});
    if (queue_status != nv::ipc::Queue::Status::Ok) {
        release_auth_worker(spdm_task_envnt, request_core_id);
        return CryptoStatus::FailSendToSpdm;
    }

    if (auth_request_id != nullptr) {
        const auto* request = std::get_if<AuthenticateApFirmwareRequestParameter>(
            &request_parameters);
        *auth_request_id = request->auth_request_id;
    }

    // spdm task is on Core0
    if (spdm_task_envnt.set(nv::spdm::Task::EventBits::AuthenticateRequestBit,
                            nv::ipc::CoreId::Core0)
        != nv::ipc::Event::Status::Ok) {
        nv::info("spdm set AuthenticateRequestBit fail\n");
    }

    // send queue to spdm success, will callback once auth done.
    return CryptoStatus::Success;
}

CryptoStatus authenticate_ap_firmware_with_request_id(
    const nv::vrot::ApInfo&            ap,
    nv::fw_parser::ap::ParsingApFwType InputParseingApFwType,
    nv::ipc::TaskId                    request_task_id,
    uint8_t                            auth_request_id)
{
    if constexpr (!HasVrotAp) {
        return CryptoStatus::FailUnknown;
    }

    if (InputParseingApFwType != nv::fw_parser::ap::ParsingApFwType::UpdateSlot
        && InputParseingApFwType != nv::fw_parser::ap::ParsingApFwType::ActiveSlot) {
        return CryptoStatus::FailUnknown;
    }
    if (auth_request_id == 0U) {
        return CryptoStatus::FailUnknown;
    }
    const auto current_task_id  = ipc::Supervisor::inst().current_task_id();
    const auto callback_task_id = request_task_id == nv::ipc::TaskId::Begin ? current_task_id
                                                                            : request_task_id;
    if (current_task_id != ipc::TaskId::Spdm) {
        return CryptoStatus::FailSendToSpdm;
    }

    return _authenticate_firmware_result(
        ap, InputParseingApFwType, callback_task_id, auth_request_id);
}

CryptoStatus
send_authenticate_firmware_request(const nv::vrot::ApInfo&            ap,
                                   nv::fw_parser::ap::ParsingApFwType InputParseingApFwType,
                                   uint8_t&                           auth_request_id,
                                   nv::ipc::TaskId                    request_task_id)
{
    auth_request_id = 0;
    if constexpr (!HasVrotAp) {
        return CryptoStatus::FailUnknown;
    }

    if (InputParseingApFwType != nv::fw_parser::ap::ParsingApFwType::UpdateSlot
        && InputParseingApFwType != nv::fw_parser::ap::ParsingApFwType::ActiveSlot) {
        return CryptoStatus::FailUnknown;
    }
    const auto current_task_id  = ipc::Supervisor::inst().current_task_id();
    const auto callback_task_id = request_task_id == nv::ipc::TaskId::Begin ? current_task_id
                                                                            : request_task_id;
    if (current_task_id == ipc::TaskId::Spdm) {
        return CryptoStatus::FailSendToSpdm;
    }

    CryptoReqResParameters request_parameters{};
    request_parameters = AuthenticateApFirmwareRequestParameter{
        .request_core_id       = nv::ipc::get_current_core(),
        .input_parsing_fw_type = InputParseingApFwType,
        .ap_id                 = ap.id,
        .auth_request_id       = 0,
        .request_task_id       = callback_task_id};
    return _send_request_to_spdm(request_parameters, &auth_request_id);
}

CryptoStatus authenticate_ap_firmware(const nv::vrot::ApInfo&            ap,
                                      nv::fw_parser::ap::ParsingApFwType InputParseingApFwType,
                                      uint8_t&                           auth_request_id,
                                      nv::ipc::TaskId                    request_task_id)
{
    auth_request_id = 0;
    if constexpr (!HasVrotAp) {
        return CryptoStatus::FailUnknown;
    }

    if (InputParseingApFwType != nv::fw_parser::ap::ParsingApFwType::UpdateSlot
        && InputParseingApFwType != nv::fw_parser::ap::ParsingApFwType::ActiveSlot) {
        return CryptoStatus::FailUnknown;
    }
    const auto current_task_id  = ipc::Supervisor::inst().current_task_id();
    const auto callback_task_id = request_task_id == nv::ipc::TaskId::Begin ? current_task_id
                                                                            : request_task_id;
    // Run directly on SPDM task; otherwise send IPC request.
    if (current_task_id == ipc::TaskId::Spdm) {
        const auto assigned_request_id = allocate_ap_auth_request_id();
        auth_request_id                = assigned_request_id;
        return _authenticate_firmware_result(
            ap, InputParseingApFwType, callback_task_id, assigned_request_id);
    }

    CryptoReqResParameters request_parameters{};
    request_parameters = AuthenticateApFirmwareRequestParameter{
        .request_core_id       = nv::ipc::get_current_core(),
        .input_parsing_fw_type = InputParseingApFwType,
        .ap_id                 = ap.id,
        .auth_request_id       = 0,
        .request_task_id       = callback_task_id};
    return _send_request_to_spdm(request_parameters, &auth_request_id);
}

// this verification will take lot of memory, we will let spdm task to do.
CryptoStatus
authenticate_mcu_firmware(const nv::fw_parser::mcu::ParsingFwType InputParseingFwType)
{
    // get the current task id
    nv::ipc::TaskId current_task_id = nv::ipc::TaskId::Begin;
    current_task_id                 = ipc::Supervisor::inst().current_task_id();
    // send command to spdm.
    if (current_task_id != ipc::TaskId::Spdm) {
        // fill the request parameters of mcu authenticate
        const CryptoReqResParameters
            RequestParameters = AuthenticateMcuFirmwareRequestParameter{
                .request_core_id       = nv::ipc::get_current_core(),
                .input_parsing_fw_type = InputParseingFwType,
                .request_task_id       = current_task_id};
        return _send_request_to_spdm(RequestParameters);
    }
    else {
        return sys::crypto::authenticate_firmware(InputParseingFwType);
    }

    // should not reach.
    return CryptoStatus::FailUnknown;
}

CryptoStatus trng_generate(std::span<uint8_t> output)
{
    if (output.size() != UdsKeyBytes) {
        return CryptoStatus::FailUnknown;
    }

    if (ipc::Supervisor::inst().current_task_id() == ipc::TaskId::Spdm) {
        return sys::crypto::trng_generate(output);
    }

    CryptoPrimitiveRequestParameters  req = TrngGenerateRequestParameter{};
    CryptoPrimitiveResponseParameters res{
        .status = CryptoStatus::FailUnknown,
        .output = {},
    };
    const nv::crypto::KeyClearGuard response_guard{std::span<uint8_t>(res.output)};
    if (submit_primitive_request(req, res) != nv::spdm::Status::Ok) {
        return CryptoStatus::FailRandomGen;
    }
    if (res.status != CryptoStatus::Success) {
        return res.status;
    }
    std::memcpy(output.data(), res.output.data(), UdsKeyBytes);
    return CryptoStatus::Success;
}

CryptoStatus puf_wrap(std::span<const uint8_t> key, std::span<uint8_t> wrapped)
{
    if (key.size() != UdsKeyBytes || wrapped.size() != PufWrappedBytes) {
        return CryptoStatus::FailUnknown;
    }

    if (ipc::Supervisor::inst().current_task_id() == ipc::TaskId::Spdm) {
        return sys::crypto::puf_wrap(key, wrapped);
    }

    CryptoPrimitiveRequestParameters req = PufWrapRequestParameter{
        .key = {},
    };
    auto& request = std::get<PufWrapRequestParameter>(req);
    std::memcpy(request.key.data(), key.data(), UdsKeyBytes);
    const nv::crypto::KeyClearGuard request_guard{std::span<uint8_t>(request.key)};

    CryptoPrimitiveResponseParameters res{
        .status = CryptoStatus::FailUnknown,
        .output = {},
    };
    const nv::crypto::KeyClearGuard response_guard{std::span<uint8_t>(res.output)};
    if (submit_primitive_request(req, res) != nv::spdm::Status::Ok) {
        return CryptoStatus::FailPufFail;
    }
    if (res.status != CryptoStatus::Success) {
        return res.status;
    }
    std::memcpy(wrapped.data(), res.output.data(), PufWrappedBytes);
    return CryptoStatus::Success;
}

CryptoStatus puf_unwrap(std::span<const uint8_t> wrapped, std::span<uint8_t> key)
{
    if (wrapped.size() != PufWrappedBytes || key.size() != UdsKeyBytes) {
        return CryptoStatus::FailUnknown;
    }

    if (ipc::Supervisor::inst().current_task_id() == ipc::TaskId::Spdm) {
        return sys::crypto::puf_unwrap(wrapped, key);
    }

    CryptoPrimitiveRequestParameters req = PufUnwrapRequestParameter{
        .wrapped = {},
    };
    auto& request = std::get<PufUnwrapRequestParameter>(req);
    std::memcpy(request.wrapped.data(), wrapped.data(), PufWrappedBytes);
    const nv::crypto::KeyClearGuard request_guard{std::span<uint8_t>(request.wrapped)};

    CryptoPrimitiveResponseParameters res{
        .status = CryptoStatus::FailUnknown,
        .output = {},
    };
    const nv::crypto::KeyClearGuard response_guard{std::span<uint8_t>(res.output)};
    if (submit_primitive_request(req, res) != nv::spdm::Status::Ok) {
        return CryptoStatus::FailPufFail;
    }
    if (res.status != CryptoStatus::Success) {
        return res.status;
    }
    std::memcpy(key.data(), res.output.data(), UdsKeyBytes);
    return CryptoStatus::Success;
}

CryptoStatus aes_256_gcm_encrypt(const nv::crypto::Aes256Key&                        key,
                                 std::span<const uint8_t, nv::crypto::AesGcmIvBytes> iv,
                                 std::span<const uint8_t>                            aad,
                                 std::span<const uint8_t>                            plaintext,
                                 std::span<uint8_t>                                  ciphertext,
                                 std::span<uint8_t, nv::crypto::AesGcmTagBytes>      tag)
{
    if (ciphertext.size() != plaintext.size()) {
        return CryptoStatus::FailAesGcmInvalidInput;
    }

    if (ipc::Supervisor::inst().current_task_id() == ipc::TaskId::Spdm) {
        return sys::crypto::aes_256_gcm_encrypt(key, iv, aad, plaintext, ciphertext, tag);
    }

    if (aad.size() > AesGcmPrimitiveMaxAadBytes
        || plaintext.size() > AesGcmPrimitiveMaxPlaintextBytes) {
        return CryptoStatus::FailAesGcmInvalidInput;
    }

    CryptoPrimitiveRequestParameters req = Aes256GcmEncryptRequestParameter{
        .key            = key,
        .iv             = {},
        .aad_size       = aad.size(),
        .plaintext_size = plaintext.size(),
        .aad            = {},
        .plaintext      = {},
    };
    auto& request = std::get<Aes256GcmEncryptRequestParameter>(req);
    std::memcpy(request.iv.data(), iv.data(), request.iv.size());
    if (!aad.empty()) {
        std::memcpy(request.aad.data(), aad.data(), aad.size());
    }
    if (!plaintext.empty()) {
        std::memcpy(request.plaintext.data(), plaintext.data(), plaintext.size());
    }
    const nv::crypto::KeyClearGuard request_key_guard{std::span<uint8_t>(request.key)};
    const nv::crypto::KeyClearGuard plaintext_guard{std::span<uint8_t>(request.plaintext)};

    CryptoPrimitiveResponseParameters res{
        .status = CryptoStatus::FailAesGcmEncrypt,
        .output = {},
    };
    const nv::crypto::KeyClearGuard response_guard{std::span<uint8_t>(res.output)};
    if (submit_primitive_request(req, res) != nv::spdm::Status::Ok) {
        return CryptoStatus::FailAesGcmEncrypt;
    }
    if (res.status != CryptoStatus::Success) {
        return res.status;
    }
    if (!ciphertext.empty()) {
        std::memcpy(ciphertext.data(), res.output.data(), ciphertext.size());
    }
    std::memcpy(tag.data(), res.output.data() + plaintext.size(), tag.size());
    return res.status;
}

// only support sha-384
CryptoStatus spdm_hash_data(uint8_t* ret, const uint8_t* data, uint32_t data_len)
{
    constexpr int          UsingSha384 = 1;
    int                    rc          = 0;
    mbedtls_sha512_context ctx;

    rc = mbedtls_sha512_starts_ret(&ctx, UsingSha384);
    if (rc != 0) {
        return nv::spdm::crypto::CryptoStatus::FailHashStart;
    }

    rc = mbedtls_sha512_update_ret(&ctx, data, data_len);
    if (rc != 0) {
        return nv::spdm::crypto::CryptoStatus::FailHashUpdate;
    }

    rc = mbedtls_sha512_finish_ret(&ctx, ret);
    if (rc != 0) {
        return nv::spdm::crypto::CryptoStatus::FailHashCalc;
    }

    return nv::spdm::crypto::CryptoStatus::Success;
}

CryptoStatus spdm_hash_sha1(uint8_t* ret, const uint8_t* data, uint32_t data_len)
{
    if (mbedtls_sha1_ret(data, data_len, ret) != 0) {
        return nv::spdm::crypto::CryptoStatus::FailHashCalc;
    }
    return nv::spdm::crypto::CryptoStatus::Success;
}

CryptoStatus spdm_random_data(uint8_t* data, size_t data_len)
{
    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_ctr_drbg_init(&ctr_drbg);

    int ret = 0;
    ret     = mbedtls_ctr_drbg_random(&ctr_drbg, data, data_len);
    if (ret != 0) {
        return nv::spdm::crypto::CryptoStatus::FailRandomGen;
    }
    mbedtls_ctr_drbg_free(&ctr_drbg);

    return nv::spdm::crypto::CryptoStatus::Success;
}

CryptoStatus
spdm_ecdsa_sign(uint8_t* signature, size_t sig_size, const uint8_t* hash, size_t hash_size)
{
    std::array<uint8_t, CryptoEngineBufferSize> buffer_for_mbedtls{};
    // check the input singnature buffer size is enough
    if (sig_size < Ecdsa384SignatureSize) {
        return nv::spdm::crypto::CryptoStatus::FailSignatureBufferLength;
    }

    // use the local buffer as stack to calloc
    mbedtls_memory_buffer_alloc_init(buffer_for_mbedtls.data(), buffer_for_mbedtls.size());

    int ret = 0;

    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_ctr_drbg_init(&ctr_drbg);

    mbedtls_ecdsa_context ecdsa;
    mbedtls_ecdsa_init(&ecdsa);

    mbedtls_mpi r, s;
    mbedtls_mpi_init(&s);
    mbedtls_mpi_init(&r);

    // get private key
    mbedtls_ecp_keypair kp;
    mbedtls_ecp_keypair_init(&kp);
    spdm::certlib::Ecdsa384PrivateKeyArray L5PrivateKey{};
    nv::spdm::cert::get_l5_private_key(L5PrivateKey);
    const auto*    priv_key_addr = static_cast<const uint8_t*>(L5PrivateKey.data());
    const uint32_t PrivKeyLen    = nv::spdm::cert::PrivateKeyLength;
    ret = mbedtls_ecp_read_key(MBEDTLS_ECP_DP_SECP384R1, &kp, priv_key_addr, PrivKeyLen);
    if (ret != 0) {
        return nv::spdm::crypto::CryptoStatus::FailGetPriKey;
    }

    // put the private key into context
    ret = mbedtls_ecp_group_load(&ecdsa.grp, MBEDTLS_ECP_DP_SECP384R1);
    if (ret != 0) {
        return nv::spdm::crypto::CryptoStatus::FailLoadEcdsaContext;
    }
    ret = mbedtls_ecdsa_from_keypair(&ecdsa, &kp);
    if (ret != 0) {
        return nv::spdm::crypto::CryptoStatus::FailLoadEcdsaContext;
    }

    // sign the signature
    ret = mbedtls_ecdsa_sign(
        &ecdsa.grp, &r, &s, &ecdsa.d, hash, hash_size, mbedtls_ctr_drbg_random, &ctr_drbg);
    if (ret != 0) {
        return nv::spdm::crypto::CryptoStatus::FailEcdsaSign;
    }

    // copy signature r and s.
    const int LengthOfRPartOfSignature = 48;
    const int LengthOfSPartOfSignature = 48;
    mbedtls_mpi_write_binary(&r, signature, LengthOfRPartOfSignature);
    mbedtls_mpi_write_binary(
        &s, &signature[LengthOfRPartOfSignature], LengthOfSPartOfSignature);

    // free all of the variable on stack
    mbedtls_ecp_keypair_free(&kp);
    mbedtls_ctr_drbg_free(&ctr_drbg);
    mbedtls_mpi_free(&s);
    mbedtls_mpi_free(&r);
    mbedtls_ecdsa_free(&ecdsa);
    mbedtls_memory_buffer_alloc_free();

    return nv::spdm::crypto::CryptoStatus::Success;
}
// this only verify the pass
CryptoStatus spdm_ecdsa_verify(const std::array<uint8_t, Ecdsa384PublicKeySize>& pub_key,
                               std::span<const uint8_t>&                         r_signature,
                               std::span<const uint8_t>&                         s_signature,
                               std::array<uint8_t, Sha384HashSize>&              hash)
{
    std::array<uint8_t, 2048> buffer_for_mbedtls{};
    // use the local buffer as stack to calloc
    mbedtls_memory_buffer_alloc_init(buffer_for_mbedtls.data(), buffer_for_mbedtls.size());
    mbedtls_ecp_group grp;
    mbedtls_ecp_group_init(&grp);
    mbedtls_ecp_group_load(&grp, MBEDTLS_ECP_DP_SECP384R1);

    mbedtls_mpi r, s;
    mbedtls_mpi_init(&r);
    mbedtls_mpi_init(&s);
    mbedtls_mpi_read_binary(&r, r_signature.data(), r_signature.size());
    mbedtls_mpi_read_binary(&s, s_signature.data(), s_signature.size());

    mbedtls_ecp_point q;
    mbedtls_ecp_point_init(&q);
    // the mbedtls library is expected the public with uncompress_token
    constexpr uint8_t UncompressedToken = 0x04;
    std::array<uint8_t, Ecdsa384PublicKeySize + sizeof(UncompressedToken)>
        pub_key_with_uncompress_token{};
    std::copy(pub_key.begin(), pub_key.end(), pub_key_with_uncompress_token.begin() + 1);
    pub_key_with_uncompress_token.at(0) = UncompressedToken;
    mbedtls_ecp_point_read_binary(
        &grp, &q, pub_key_with_uncompress_token.data(), pub_key_with_uncompress_token.size());

    int ret = 0;
    ret     = mbedtls_ecdsa_verify(&grp, hash.data(), hash.size(), &q, &r, &s);

    // free all of the variable on stack
    mbedtls_ecp_group_free(&grp);
    mbedtls_mpi_free(&r);
    mbedtls_mpi_free(&s);
    mbedtls_ecp_point_free(&q);
    mbedtls_memory_buffer_alloc_free();
    if (ret != 0) {
        nv::info("sign verify fail -%x\n", -ret);
        return nv::spdm::crypto::CryptoStatus::FailSignatureVerify;
    }
    return nv::spdm::crypto::CryptoStatus::Success;
}

}  // namespace nv::spdm::crypto
