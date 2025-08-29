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
#include "nv/spdm/spdm_cert_chain.h"
#include "nv/spdm/spdm_crypto_helper.h"
#include "sys/common/utils.h"
#include "nv/ipc/supervisor.h"
#include "nv/spdm/task.h"
#include "nv/nv.h"
#include "nv/flash/flash.h"
#include "sys/crypto/crypto.h"
#include "nv/pldm/task.h"
using namespace nv;
namespace nv::spdm::crypto {

// this verification will take lot of memory, we will let spdm task to do.
CryptoStatus authenticate_firmware(const nv::fw_parser::ParsingFwType InputParseingFwType)
{
    nv::ipc::TaskId current_task_id = nv::ipc::TaskId::Begin;

    current_task_id = ipc::Supervisor::inst().current_task_id();

    // send command to spdm.
    if (current_task_id != ipc::TaskId::Spdm) {
        // send the request to queue
        auto& crypto_queue = nv::ipc::Queue::make(nv::ipc::QueueId::SpdmCryptoHelper);

        auto spdm_task_envnt = ipc::Event::make(ipc::EventId::SpdmTask);

        // check if there is another task want spdm to authenticate fw
        auto spdm_idle_check = spdm_task_envnt.wait(
            nv::spdm::Task::AuthenticateTaskIdle, true, false, std::chrono::seconds{1});
        if (!spdm_idle_check) {
            return CryptoStatus::FailSendToSpdm;
        }

        // fill the request parameters
        CryptoReqResParameters request_parameters{};
        request_parameters = AuthenticateFirmwareRequestParameter{
            .input_parsing_fw_type = InputParseingFwType, .request_task_id = current_task_id};

        auto& request_parameters_arr_view = *std::bit_cast<
            std::array<uint8_t, sizeof(decltype(request_parameters))>*>(&request_parameters);
        const nv::ipc::Queue::Item ReqItem(request_parameters_arr_view.data(),
                                           request_parameters_arr_view.size());

        // send to the queue.
        auto queue_status = crypto_queue.send(ReqItem, std::chrono::seconds{1});
        if (queue_status != nv::ipc::Queue::Status::Ok) {
            return CryptoStatus::FailSendToSpdm;
        }

        if (spdm_task_envnt.set(nv::spdm::Task::EventBits::AuthenticateRequestBit)
            != nv::ipc::Event::Status::Ok) {
            nv::info("spdm set AuthenticateRequestBit fail\n");
        }

        // send queue to spdm success, will callback once auth done.
        return CryptoStatus::Success;
    }
    else {
        return sys::crypto::authenticate_firmware(InputParseingFwType);
    }

    // should not reach.
    return CryptoStatus::FailUnknown;
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
spdm_ecdsa_sign(uint8_t* signature, uint16_t sig_size, const uint8_t* hash, uint16_t hash_size)
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