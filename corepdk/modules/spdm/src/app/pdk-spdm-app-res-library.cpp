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
#include "pdk-spdm-app-res-library.h"

#include <bitset>
#include <cstring>
#include <span>

#include "pdk-spdm-app-res-algorithms.h"
#include "pdk-spdm-app-res-capabilities.h"
#include "pdk-spdm-app-res-certificate.h"
#include "pdk-spdm-app-res-digests.h"
#include "pdk-spdm-app-res-library-plat.h"
#include "pdk-spdm-app-res-memory-allocate.h"
#include "pdk-spdm-app-res-version.h"

namespace pdk::spdm::app::res::library {

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Send an SPDM transport layer message to an endpoint.
 *
 * The message is an SPDM message with transport layer wrapper, or a secured SPDM message with
 * transport layer wrapper.
 *
 * For Requester, the message is a transport layer SPDM request.
 * For Responder, the message is a transport layer SPDM response.
 *
 * @param  spdm_context  A pointer to the SPDM context.
 * @param  message_size  Size, in bytes, of the message data buffer.
 * @param  message       A pointer to a destination buffer to store the message.
 *                       The caller is responsible for having either implicit or explicit
 *ownership of the buffer. The message pointer shall be inside of [msg_buf_ptr, msg_buf_ptr +
 *max_msg_size] from acquired sender_buffer.
 * @param  timeout       The timeout, in microsends, to use for the execution of the message.
 *                       If called in a Requester context then timeout is equal to RTT.
 *                       If called in a Responder context then timeout is equal to 0 and
 *Responder should not timeout when sending the message.
 *
 * @retval LIBSPDM_STATUS_SUCCESS   The message was successfully sent to the receiving endpoint.
 * @retval LIBSPDM_STATUS_SEND_FAIL Unable to send message to the receiving endpoint.
 **/
libspdm_return_t send_data_from_spdm_responsder([[maybe_unused]] void*    spdm_context,
                                                size_t                    message_size,
                                                const void*               message,
                                                [[maybe_unused]] uint64_t timeout)
{
    const std::span<const uint8_t> data_from_spdm_responsder(
        static_cast<const uint8_t*>(message), message_size);
    pdk::spdm::platforms::res::library::get_data_from_spdm_responsder(
        data_from_spdm_responsder);

    return LIBSPDM_STATUS_SUCCESS;  // NOLINT(hicpp-signed-bitwise)
}

/**
 * Receive an SPDM transport layer message from an endpoint.
 *
 * The message is an SPDM message with transport layer wrapper or a secured SPDM message with
 * transport layer wrapper.
 *
 * For Requester, the message is a transport layer SPDM response.
 * For Responder, the message is a transport layer SPDM request.
 *
 * @param  spdm_context  A pointer to the SPDM context.
 * @param  message_size  Size, in bytes, of the message data buffer.
 * @param  message       A pointer to a destination buffer to store the message.
 *                       The caller is responsible for having either implicit or explicit
 *ownership of the buffer. On input, the message pointer shall be msg_buf_ptr from acquired
 *receiver_buffer. On output, the message pointer shall be inside of [msg_buf_ptr, msg_buf_ptr +
 *max_msg_size] from acquired receiver_buffer.
 * @param  timeout       The timeout, in microsends, to use for the execution of the message.
 *                       If called in a Requester context then timeout is equal to RTT plus
 *either CT or ST1. If called in a Responder context then timeout is equal to 0 and Responder
 *                       should not timeout when receiving the message.
 *
 * @retval LIBSPDM_STATUS_SUCCESS      The message was successfully received from the sending
 *                                     endpoint.
 * @retval LIBSPDM_STATUS_RECEIVE_FAIL Unable to receive message from the sending endpoint.
 **/
libspdm_return_t receive_data_to_spdm_responsder([[maybe_unused]] void*    spdm_context,
                                                 size_t*                   message_size,
                                                 void**                    message,
                                                 [[maybe_unused]] uint64_t timeout)
{
    pdk::spdm::platforms::res::library::send_data_to_spdm_responsder(
        std::span<uint8_t>(static_cast<uint8_t*>(*message), *message_size));
    return LIBSPDM_STATUS_SUCCESS;  // NOLINT(hicpp-signed-bitwise)
}

/**
 * Acquire a device sender buffer for transport layer message.
 *
 * @param  spdm_context  A pointer to the SPDM context.
 * @param  msg_buf_ptr   A pointer to a sender buffer.
 *
 * @retval LIBSPDM_STATUS_SUCCESS       The sender buffer has been acquired.
 * @retval LIBSPDM_STATUS_ACQUIRE_FAIL  Unable to acquire sender buffer.
 **/
libspdm_return_t spdm_library_device_acquire_sender_buffer([[maybe_unused]] void* spdm_context,
                                                           void**                 msg_buf_ptr)
{
    *msg_buf_ptr = pdk::spdm::platforms::res::library::acquire_sender_buffer();
    return LIBSPDM_STATUS_SUCCESS;  // NOLINT(hicpp-signed-bitwise)
}

/**
 * Release a device sender buffer for transport layer message.
 *
 * @param  spdm_context  A pointer to the SPDM context.
 * @param  msg_buf_ptr   A pointer to a sender buffer.
 **/
void spdm_library_device_release_sender_buffer([[maybe_unused]] void* spdm_context,
                                               const void*            msg_buf_ptr)
{
    pdk::spdm::platforms::res::library::release_sender_buffer(msg_buf_ptr);
}

/**
 * Acquire a device receiver buffer for transport layer message.
 *
 * @param  spdm_context  A pointer to the SPDM context.
 * @param  msg_buf_ptr   A pointer to a receiver buffer.
 *
 * @retval LIBSPDM_STATUS_SUCCESS       The receiver buffer has been acquired.
 * @retval LIBSPDM_STATUS_ACQUIRE_FAIL  Unable to acquire receiver buffer.
 **/
libspdm_return_t
spdm_library_device_acquire_receiver_buffer([[maybe_unused]] void* spdm_context,
                                            void**                 msg_buf_ptr)
{
    *msg_buf_ptr = pdk::spdm::platforms::res::library::acquire_receiver_buffer();
    return LIBSPDM_STATUS_SUCCESS;  // NOLINT(hicpp-signed-bitwise)
}

/**
 * Release a device receiver buffer for transport layer message.
 *
 * @param  spdm_context  A pointer to the SPDM context.
 * @param  msg_buf_ptr   A pointer to a receiver buffer.
 **/
void spdm_library_device_release_receiver_buffer([[maybe_unused]] void* spdm_context,
                                                 const void*            msg_buf_ptr)
{
    pdk::spdm::platforms::res::library::release_receiver_buffer(msg_buf_ptr);
}
#ifdef __cplusplus
}
#endif

libspdm_return_t SpdmEngineContext::process_message()
{
    return libspdm_responder_dispatch_message(this->spdm_context_ptr);
}

SpdmEngineSettingError SpdmEngineContext::spdm_engine_initialize()
{
    // set spdm context
    const auto spdm_context_need_size = libspdm_get_context_size();
    auto       spdm_context           = static_cast<uint8_t*>(
        pdk::spdm::app::res::memory_allocate::allocate(spdm_context_need_size));
    this->spdm_context_size = spdm_context_need_size;
    this->spdm_context_ptr  = spdm_context;
    if (spdm_context == nullptr) {
        return SpdmEngineSettingError::SPDM_CONTEXT_ALLOCATE_FAILED;
    }

    // set spdm context
    if (LIBSPDM_STATUS_SUCCESS  // NOLINT(hicpp-signed-bitwise)
        != libspdm_init_context(spdm_context)) {
        return SpdmEngineSettingError::SPDM_CONTEXT_INIT_FAILED;
    }

    libspdm_register_device_io_func(
        spdm_context, send_data_from_spdm_responsder, receive_data_to_spdm_responsder);
    libspdm_register_transport_layer_func(
        spdm_context,
        pdk::spdm::platforms::res::library::get_sender_buffer_size(),
        LIBSPDM_MCTP_TRANSPORT_HEADER_SIZE,
        LIBSPDM_MCTP_TRANSPORT_TAIL_SIZE,
        libspdm_transport_mctp_encode_message,
        libspdm_transport_mctp_decode_message);

    libspdm_register_device_buffer_func(
        spdm_context,
        pdk::spdm::platforms::res::library::get_sender_buffer_size(),    // sender buffer size
        pdk::spdm::platforms::res::library::get_receiver_buffer_size(),  // receiver buffer size
        spdm_library_device_acquire_sender_buffer,
        spdm_library_device_release_sender_buffer,
        spdm_library_device_acquire_receiver_buffer,
        spdm_library_device_release_receiver_buffer);

    // set scratch buffer
    const auto scratch_buffer_size = libspdm_get_sizeof_required_scratch_buffer(spdm_context);
    auto       scratch_buffer      = static_cast<uint8_t*>(
        pdk::spdm::app::res::memory_allocate::allocate(scratch_buffer_size));
    this->spdm_scratch_buffer_ptr  = scratch_buffer;
    this->spdm_scratch_buffer_size = scratch_buffer_size;
    if (scratch_buffer == nullptr) {
        return SpdmEngineSettingError::SPDM_SCRATCH_BUFFER_ALLOCATE_FAILED;
    }

    libspdm_set_scratch_buffer(spdm_context, scratch_buffer, scratch_buffer_size);

    // start to set the spdm engine
    libspdm_data_parameter_t parameter{};
    uint8_t                  data8{};
    uint16_t                 data16{};
    uint32_t                 data32{};

    // set spdm version setting
    {
        bool is_spdm_v1_1 = false;
        bool is_spdm_v1_2 = false;
        bool is_spdm_v1_3 = false;
        pdk::spdm::app::res::version::spdm_library_get_versions(
            is_spdm_v1_1, is_spdm_v1_2, is_spdm_v1_3);
        std::array<spdm_version_number_t, 3> spdm_version_array = {};
        size_t                               spdm_version_count = 0;

        // if none of the spdm version is supported, return error
        if (false == (is_spdm_v1_1 or is_spdm_v1_2 or is_spdm_v1_3)) {
            return SpdmEngineSettingError::NO_SPDM_VERSION_SUPPORTED;
        }
        if (is_spdm_v1_1) {
            spdm_version_array.at(spdm_version_count++) = static_cast<spdm_version_number_t>(
                static_cast<uint16_t>(SPDM_MESSAGE_VERSION_11) << 8U);
        }
        if (is_spdm_v1_2) {
            spdm_version_array.at(spdm_version_count++) = static_cast<spdm_version_number_t>(
                static_cast<uint16_t>(SPDM_MESSAGE_VERSION_12) << 8U);
        }
        if (is_spdm_v1_3) {
            spdm_version_array.at(spdm_version_count++) = static_cast<spdm_version_number_t>(
                static_cast<uint16_t>(SPDM_MESSAGE_VERSION_13) << 8U);
        }

        memset(&parameter, 0, sizeof(parameter));
        parameter.location = LIBSPDM_DATA_LOCATION_LOCAL;
        auto status        = libspdm_set_data(spdm_context,
                                       LIBSPDM_DATA_SPDM_VERSION,
                                       &parameter,
                                       spdm_version_array.data(),
                                       spdm_version_count * sizeof(spdm_version_number_t));
        if (status != LIBSPDM_STATUS_SUCCESS) {  // NOLINT(hicpp-signed-bitwise)
            return SpdmEngineSettingError::SPDM_VERSION_SETTING_FAILED;
        }
    }
    // set spdm capabilities setting
    {
        /* Configurable parameters
         *   LIBSPDM_DATA_CAPABILITY_FLAGS
         *   LIBSPDM_DATA_CAPABILITY_CT_EXPONENT
         *   LIBSPDM_DATA_CAPABILITY_RTT_US
         *   LIBSPDM_DATA_CAPABILITY_DATA_TRANSFER_SIZE
         *   LIBSPDM_DATA_CAPABILITY_MAX_SPDM_MSG_SIZE
         *   LIBSPDM_DATA_CAPABILITY_SENDER_DATA_TRANSFER_SIZE
         */

        memset(&parameter, 0, sizeof(parameter));
        parameter.location = LIBSPDM_DATA_LOCATION_LOCAL;
        data8 = pdk::spdm::app::res::capabilities::spdm_library_get_capabilites_ct_exponent();
        auto status = libspdm_set_data(spdm_context,
                                       LIBSPDM_DATA_CAPABILITY_CT_EXPONENT,
                                       &parameter,
                                       &data8,
                                       sizeof(data8));
        if (status != LIBSPDM_STATUS_SUCCESS) {  // NOLINT(hicpp-signed-bitwise)
            return SpdmEngineSettingError::CAPABILITY_CT_EXPONENT_SETTING_FAILED;
        }
        // start collect the flags
        const auto
            flags = pdk::spdm::app::res::capabilities::spdm_library_get_capabilites_flags();
        status    = libspdm_set_data(
            spdm_context, LIBSPDM_DATA_CAPABILITY_FLAGS, &parameter, &flags, sizeof(flags));
        if (status != LIBSPDM_STATUS_SUCCESS) {  // NOLINT(hicpp-signed-bitwise)
            return SpdmEngineSettingError::CAPABILITY_FLAGS_SETTING_FAILED;
        }
    }
    // set spdm algorithm setting
    {
        /* Configurable parameters
         *   LIBSPDM_DATA_MEASUREMENT_SPEC
         *   LIBSPDM_DATA_MEASUREMENT_HASH_ALGO
         *   LIBSPDM_DATA_BASE_ASYM_ALGO
         *   LIBSPDM_DATA_BASE_HASH_ALGO
         *   LIBSPDM_DATA_DHE_NAME_GROUP
         *   LIBSPDM_DATA_AEAD_CIPHER_SUITE
         *   LIBSPDM_DATA_REQ_BASE_ASYM_ALG
         *   LIBSPDM_DATA_KEY_SCHEDULE
         *   LIBSPDM_DATA_OTHER_PARAMS_SUPPORT
         *   LIBSPDM_DATA_MEL_SPEC
         */
        bool support_dmtf_meas_spec              = false;
        bool support_tpm_alg_sha_256             = false;
        bool support_tpm_alg_sha_384             = false;
        bool support_tpm_alg_sha_512             = false;
        bool support_tpm_alg_sha3_256            = false;
        bool support_tpm_alg_sha3_384            = false;
        bool support_tpm_alg_sha3_512            = false;
        bool support_raw_bit_streams_only        = false;
        bool support_tpm_alg_sm3_256             = false;
        bool support_tpm_alg_ecdsa_ecc_nist_p384 = false;
        bool support_tpm_alg_rsapss_4096         = false;
        bool support_tpm_alg_rsassa_4096         = false;
        bool support_tpm_alg_ecdsa_ecc_nist_p256 = false;
        bool support_tpm_alg_rsapss_3072         = false;
        bool support_tpm_alg_rsassa_3072         = false;
        bool support_tpm_alg_rsapss_2048         = false;
        bool support_tpm_alg_rsassa_2048         = false;
        bool support_tpm_alg_ecdsa_ecc_nist_p521 = false;
        bool support_tpm_alg_sm2_ecc_sm2_p256    = false;
        bool support_tpm_alg_eddsa_ed25519       = false;
        bool support_tpm_alg_eddsa_ed448         = false;
        bool support_secp521r1                   = false;
        bool support_secp384r1                   = false;
        bool support_secp256r1                   = false;
        bool support_ffdhe4096                   = false;
        bool support_ffdhe3072                   = false;
        bool support_ffdhe2048                   = false;
        bool support_sm2_p256                    = false;
        bool support_chacha20_poly1305           = false;
        bool support_aes_256_gcm                 = false;
        bool support_aes_128_gcm                 = false;
        bool support_aead_sm4_gcm                = false;
        // TODO: add support for these algorithms
        bool support_ra_tpm_alg_ecdsa_ecc_nist_p384 = false;
        bool support_ra_tpm_alg_rsapss_4096         = false;
        bool support_ra_tpm_alg_rsassa_4096         = false;
        bool support_ra_tpm_alg_ecdsa_ecc_nist_p256 = false;
        bool support_ra_tpm_alg_rsapss_3072         = false;
        bool support_ra_tpm_alg_rsassa_3072         = false;
        bool support_ra_tpm_alg_rsapss_2048         = false;
        bool support_ra_tpm_alg_rsassa_2048         = false;
        bool support_ra_tpm_alg_ecdsa_ecc_nist_p521 = false;
        bool support_ra_tpm_alg_sm2_ecc_sm2_p256    = false;
        bool support_ra_tpm_alg_eddsa_ed25519       = false;
        bool support_ra_tpm_alg_eddsa_ed448         = false;
        bool support_spdm_key_schedule              = false;

        // set measurement specification
        pdk::spdm::app::res::algorithms::spdm_library_measurement_specification_sel(
            support_dmtf_meas_spec);
        if (support_dmtf_meas_spec) {
            memset(&parameter, 0, sizeof(parameter));
            parameter.location = LIBSPDM_DATA_LOCATION_LOCAL;
            data8              = SPDM_MEASUREMENT_SPECIFICATION_DMTF;
            auto status        = libspdm_set_data(
                spdm_context, LIBSPDM_DATA_MEASUREMENT_SPEC, &parameter, &data8, sizeof(data8));
            if (status != LIBSPDM_STATUS_SUCCESS) {  // NOLINT(hicpp-signed-bitwise)
                return SpdmEngineSettingError::MEASUREMENT_SPEC_SETTING_FAILED;
            }
        }
        // set measurement hash algorithm
        pdk::spdm::app::res::algorithms::spdm_library_negotiate_algo_measurement_hash_algo(
            support_raw_bit_streams_only,
            support_tpm_alg_sha_256,
            support_tpm_alg_sha_384,
            support_tpm_alg_sha_512,
            support_tpm_alg_sha3_256,
            support_tpm_alg_sha3_384,
            support_tpm_alg_sha3_512,
            support_tpm_alg_sm3_256);
        data32 = 0;
        memset(&parameter, 0, sizeof(parameter));
        parameter.location = LIBSPDM_DATA_LOCATION_LOCAL;
        if (support_raw_bit_streams_only) {
            data32 |= static_cast<uint32_t>(
                SPDM_ALGORITHMS_MEASUREMENT_HASH_ALGO_RAW_BIT_STREAM_ONLY);
        }
        if (support_tpm_alg_sha_256) {
            data32 |= static_cast<uint32_t>(
                SPDM_ALGORITHMS_MEASUREMENT_HASH_ALGO_TPM_ALG_SHA_256);
        }
        if (support_tpm_alg_sha_384) {
            data32 |= static_cast<uint32_t>(
                SPDM_ALGORITHMS_MEASUREMENT_HASH_ALGO_TPM_ALG_SHA_384);
        }
        if (support_tpm_alg_sha_512) {
            data32 |= static_cast<uint32_t>(
                SPDM_ALGORITHMS_MEASUREMENT_HASH_ALGO_TPM_ALG_SHA_512);
        }
        if (support_tpm_alg_sha3_256) {
            data32 |= static_cast<uint32_t>(
                SPDM_ALGORITHMS_MEASUREMENT_HASH_ALGO_TPM_ALG_SHA3_256);
        }
        if (support_tpm_alg_sha3_384) {
            data32 |= static_cast<uint32_t>(
                SPDM_ALGORITHMS_MEASUREMENT_HASH_ALGO_TPM_ALG_SHA3_384);
        }
        if (support_tpm_alg_sha3_512) {
            data32 |= static_cast<uint32_t>(
                SPDM_ALGORITHMS_MEASUREMENT_HASH_ALGO_TPM_ALG_SHA3_512);
        }
        if (support_tpm_alg_sm3_256) {
            data32 |= static_cast<uint32_t>(
                SPDM_ALGORITHMS_MEASUREMENT_HASH_ALGO_TPM_ALG_SM3_256);
        }
        auto status = libspdm_set_data(spdm_context,
                                       LIBSPDM_DATA_MEASUREMENT_HASH_ALGO,
                                       &parameter,
                                       &data32,
                                       sizeof(data32));
        if (status != LIBSPDM_STATUS_SUCCESS) {  // NOLINT(hicpp-signed-bitwise)
            return SpdmEngineSettingError::MEASUREMENT_HASH_ALGO_SETTING_FAILED;
        }
        // set base asymmetric algorithm
        pdk::spdm::app::res::algorithms::spdm_library_negotiate_algo_base_asym_algo(
            support_tpm_alg_rsassa_2048,
            support_tpm_alg_rsapss_2048,
            support_tpm_alg_rsassa_3072,
            support_tpm_alg_rsapss_3072,
            support_tpm_alg_ecdsa_ecc_nist_p256,
            support_tpm_alg_rsassa_4096,
            support_tpm_alg_rsapss_4096,
            support_tpm_alg_ecdsa_ecc_nist_p384,
            support_tpm_alg_ecdsa_ecc_nist_p521,
            support_tpm_alg_sm2_ecc_sm2_p256,
            support_tpm_alg_eddsa_ed25519,
            support_tpm_alg_eddsa_ed448);

        data32 = 0;
        if (support_tpm_alg_ecdsa_ecc_nist_p384) {
            data32 |= static_cast<uint32_t>(
                SPDM_ALGORITHMS_BASE_ASYM_ALGO_TPM_ALG_ECDSA_ECC_NIST_P384);
        }
        if (support_tpm_alg_rsapss_4096) {
            data32 |= static_cast<uint32_t>(SPDM_ALGORITHMS_BASE_ASYM_ALGO_TPM_ALG_RSAPSS_4096);
        }
        if (support_tpm_alg_rsassa_4096) {
            data32 |= static_cast<uint32_t>(SPDM_ALGORITHMS_BASE_ASYM_ALGO_TPM_ALG_RSASSA_4096);
        }
        if (support_tpm_alg_ecdsa_ecc_nist_p256) {
            data32 |= static_cast<uint32_t>(
                SPDM_ALGORITHMS_BASE_ASYM_ALGO_TPM_ALG_ECDSA_ECC_NIST_P256);
        }
        if (support_tpm_alg_rsapss_3072) {
            data32 |= static_cast<uint32_t>(SPDM_ALGORITHMS_BASE_ASYM_ALGO_TPM_ALG_RSAPSS_3072);
        }
        if (support_tpm_alg_rsassa_3072) {
            data32 |= static_cast<uint32_t>(SPDM_ALGORITHMS_BASE_ASYM_ALGO_TPM_ALG_RSASSA_3072);
        }
        if (support_tpm_alg_rsapss_2048) {
            data32 |= static_cast<uint32_t>(SPDM_ALGORITHMS_BASE_ASYM_ALGO_TPM_ALG_RSAPSS_2048);
        }
        if (support_tpm_alg_rsassa_2048) {
            data32 |= static_cast<uint32_t>(SPDM_ALGORITHMS_BASE_ASYM_ALGO_TPM_ALG_RSASSA_2048);
        }
        if (support_tpm_alg_ecdsa_ecc_nist_p521) {
            data32 |= static_cast<uint32_t>(
                SPDM_ALGORITHMS_BASE_ASYM_ALGO_TPM_ALG_ECDSA_ECC_NIST_P521);
        }
        if (support_tpm_alg_sm2_ecc_sm2_p256) {
            data32 |= static_cast<uint32_t>(
                SPDM_ALGORITHMS_BASE_ASYM_ALGO_TPM_ALG_SM2_ECC_SM2_P256);
        }
        if (support_tpm_alg_eddsa_ed25519) {
            data32 |= static_cast<uint32_t>(SPDM_ALGORITHMS_BASE_ASYM_ALGO_EDDSA_ED25519);
        }
        if (support_tpm_alg_eddsa_ed448) {
            data32 |= static_cast<uint32_t>(SPDM_ALGORITHMS_BASE_ASYM_ALGO_EDDSA_ED448);
        }
        memset(&parameter, 0, sizeof(parameter));
        parameter.location = LIBSPDM_DATA_LOCATION_LOCAL;
        status             = libspdm_set_data(
            spdm_context, LIBSPDM_DATA_BASE_ASYM_ALGO, &parameter, &data32, sizeof(data32));
        if (status != LIBSPDM_STATUS_SUCCESS) {  // NOLINT(hicpp-signed-bitwise)
            return SpdmEngineSettingError::BASE_ASYM_ALGO_SETTING_FAILED;
        }

        pdk::spdm::app::res::algorithms::spdm_library_negotiate_algo_base_hash_algo(
            support_tpm_alg_sha_256,
            support_tpm_alg_sha_384,
            support_tpm_alg_sha_512,
            support_tpm_alg_sha3_256,
            support_tpm_alg_sha3_384,
            support_tpm_alg_sha3_512,
            support_tpm_alg_sm3_256);
        data32 = 0;
        if (support_tpm_alg_sha_256) {
            data32 |= static_cast<uint32_t>(SPDM_ALGORITHMS_BASE_HASH_ALGO_TPM_ALG_SHA_256);
        }
        if (support_tpm_alg_sha_384) {
            data32 |= static_cast<uint32_t>(SPDM_ALGORITHMS_BASE_HASH_ALGO_TPM_ALG_SHA_384);
        }
        if (support_tpm_alg_sha_512) {
            data32 |= static_cast<uint32_t>(SPDM_ALGORITHMS_BASE_HASH_ALGO_TPM_ALG_SHA_512);
        }
        if (support_tpm_alg_sha3_256) {
            data32 |= static_cast<uint32_t>(SPDM_ALGORITHMS_BASE_HASH_ALGO_TPM_ALG_SHA3_256);
        }
        if (support_tpm_alg_sha3_384) {
            data32 |= static_cast<uint32_t>(SPDM_ALGORITHMS_BASE_HASH_ALGO_TPM_ALG_SHA3_384);
        }
        if (support_tpm_alg_sha3_512) {
            data32 |= static_cast<uint32_t>(SPDM_ALGORITHMS_BASE_HASH_ALGO_TPM_ALG_SHA3_512);
        }
        if (support_tpm_alg_sm3_256) {
            data32 |= static_cast<uint32_t>(SPDM_ALGORITHMS_BASE_HASH_ALGO_TPM_ALG_SM3_256);
        }
        memset(&parameter, 0, sizeof(parameter));
        parameter.location = LIBSPDM_DATA_LOCATION_LOCAL;
        status             = libspdm_set_data(
            spdm_context, LIBSPDM_DATA_BASE_HASH_ALGO, &parameter, &data32, sizeof(data32));
        if (status != LIBSPDM_STATUS_SUCCESS) {  // NOLINT(hicpp-signed-bitwise)
            return SpdmEngineSettingError::BASE_HASH_ALGO_SETTING_FAILED;
        }

        pdk::spdm::app::res::algorithms::spdm_library_negotiate_algo_dhe(support_secp521r1,
                                                                         support_secp384r1,
                                                                         support_secp256r1,
                                                                         support_ffdhe4096,
                                                                         support_ffdhe3072,
                                                                         support_ffdhe2048,
                                                                         support_sm2_p256);

        data16 = 0;
        if (support_secp521r1) {
            data16 |= static_cast<uint16_t>(SPDM_ALGORITHMS_DHE_NAMED_GROUP_SECP_521_R1);
        }
        if (support_secp384r1) {
            data16 |= static_cast<uint16_t>(SPDM_ALGORITHMS_DHE_NAMED_GROUP_SECP_384_R1);
        }
        if (support_secp256r1) {
            data16 |= static_cast<uint16_t>(SPDM_ALGORITHMS_DHE_NAMED_GROUP_SECP_256_R1);
        }
        if (support_ffdhe4096) {
            data16 |= static_cast<uint16_t>(SPDM_ALGORITHMS_DHE_NAMED_GROUP_FFDHE_4096);
        }
        if (support_ffdhe3072) {
            data16 |= static_cast<uint16_t>(SPDM_ALGORITHMS_DHE_NAMED_GROUP_FFDHE_3072);
        }
        if (support_ffdhe2048) {
            data16 |= static_cast<uint16_t>(SPDM_ALGORITHMS_DHE_NAMED_GROUP_FFDHE_2048);
        }
        if (support_sm2_p256) {
            data16 |= static_cast<uint16_t>(SPDM_ALGORITHMS_DHE_NAMED_GROUP_SM2_P256);
        }
        memset(&parameter, 0, sizeof(parameter));
        parameter.location = LIBSPDM_DATA_LOCATION_LOCAL;
        status             = libspdm_set_data(
            spdm_context, LIBSPDM_DATA_DHE_NAME_GROUP, &parameter, &data16, sizeof(data16));
        if (status != LIBSPDM_STATUS_SUCCESS) {  // NOLINT(hicpp-signed-bitwise)
            return SpdmEngineSettingError::DHE_NAME_GROUP_SETTING_FAILED;
        }

        pdk::spdm::app::res::algorithms::spdm_library_negotiate_algo_aead(
            support_aes_128_gcm,
            support_aes_256_gcm,
            support_chacha20_poly1305,
            support_aead_sm4_gcm);
        data16 = 0;
        if (support_chacha20_poly1305) {
            data16 |= static_cast<uint16_t>(
                SPDM_ALGORITHMS_AEAD_CIPHER_SUITE_CHACHA20_POLY1305);
        }
        if (support_aes_256_gcm) {
            data16 |= static_cast<uint16_t>(SPDM_ALGORITHMS_AEAD_CIPHER_SUITE_AES_256_GCM);
        }
        if (support_aes_128_gcm) {
            data16 |= static_cast<uint16_t>(SPDM_ALGORITHMS_AEAD_CIPHER_SUITE_AES_128_GCM);
        }
        if (support_aead_sm4_gcm) {
            data16 |= static_cast<uint16_t>(SPDM_ALGORITHMS_AEAD_CIPHER_SUITE_AEAD_SM4_GCM);
        }
        memset(&parameter, 0, sizeof(parameter));
        parameter.location = LIBSPDM_DATA_LOCATION_LOCAL;
        status             = libspdm_set_data(
            spdm_context, LIBSPDM_DATA_AEAD_CIPHER_SUITE, &parameter, &data16, sizeof(data16));
        if (status != LIBSPDM_STATUS_SUCCESS) {  // NOLINT(hicpp-signed-bitwise)
            return SpdmEngineSettingError::AEAD_CIPHER_SUITE_SETTING_FAILED;
        }

        // set base asymmetric algorithm
        pdk::spdm::app::res::algorithms::spdm_library_negotiate_algo_rbaa(
            support_ra_tpm_alg_rsassa_2048,
            support_ra_tpm_alg_rsapss_2048,
            support_ra_tpm_alg_rsassa_3072,
            support_ra_tpm_alg_rsapss_3072,
            support_ra_tpm_alg_ecdsa_ecc_nist_p256,
            support_ra_tpm_alg_rsassa_4096,
            support_ra_tpm_alg_rsapss_4096,
            support_ra_tpm_alg_ecdsa_ecc_nist_p384,
            support_ra_tpm_alg_ecdsa_ecc_nist_p521,
            support_ra_tpm_alg_sm2_ecc_sm2_p256,
            support_ra_tpm_alg_eddsa_ed25519,
            support_ra_tpm_alg_eddsa_ed448);

        data16 = 0;
        if (support_ra_tpm_alg_ecdsa_ecc_nist_p384) {
            data16 |= static_cast<uint16_t>(
                SPDM_ALGORITHMS_BASE_ASYM_ALGO_TPM_ALG_ECDSA_ECC_NIST_P384);
        }
        if (support_ra_tpm_alg_rsapss_4096) {
            data16 |= static_cast<uint16_t>(SPDM_ALGORITHMS_BASE_ASYM_ALGO_TPM_ALG_RSAPSS_4096);
        }
        if (support_ra_tpm_alg_rsassa_4096) {
            data16 |= static_cast<uint16_t>(SPDM_ALGORITHMS_BASE_ASYM_ALGO_TPM_ALG_RSASSA_4096);
        }
        if (support_ra_tpm_alg_ecdsa_ecc_nist_p256) {
            data16 |= static_cast<uint16_t>(
                SPDM_ALGORITHMS_BASE_ASYM_ALGO_TPM_ALG_ECDSA_ECC_NIST_P256);
        }
        if (support_ra_tpm_alg_rsapss_3072) {
            data16 |= static_cast<uint16_t>(SPDM_ALGORITHMS_BASE_ASYM_ALGO_TPM_ALG_RSAPSS_3072);
        }
        if (support_ra_tpm_alg_rsassa_3072) {
            data16 |= static_cast<uint16_t>(SPDM_ALGORITHMS_BASE_ASYM_ALGO_TPM_ALG_RSASSA_3072);
        }
        if (support_ra_tpm_alg_rsapss_2048) {
            data16 |= static_cast<uint16_t>(SPDM_ALGORITHMS_BASE_ASYM_ALGO_TPM_ALG_RSAPSS_2048);
        }
        if (support_ra_tpm_alg_rsassa_2048) {
            data16 |= static_cast<uint16_t>(SPDM_ALGORITHMS_BASE_ASYM_ALGO_TPM_ALG_RSASSA_2048);
        }
        if (support_ra_tpm_alg_ecdsa_ecc_nist_p521) {
            data16 |= static_cast<uint16_t>(
                SPDM_ALGORITHMS_BASE_ASYM_ALGO_TPM_ALG_ECDSA_ECC_NIST_P521);
        }
        if (support_ra_tpm_alg_sm2_ecc_sm2_p256) {
            data16 |= static_cast<uint16_t>(
                SPDM_ALGORITHMS_BASE_ASYM_ALGO_TPM_ALG_SM2_ECC_SM2_P256);
        }
        if (support_ra_tpm_alg_eddsa_ed25519) {
            data16 |= static_cast<uint16_t>(SPDM_ALGORITHMS_BASE_ASYM_ALGO_EDDSA_ED25519);
        }
        if (support_ra_tpm_alg_eddsa_ed448) {
            data16 |= static_cast<uint16_t>(SPDM_ALGORITHMS_BASE_ASYM_ALGO_EDDSA_ED448);
        }
        memset(&parameter, 0, sizeof(parameter));
        parameter.location = LIBSPDM_DATA_LOCATION_LOCAL;
        status             = libspdm_set_data(
            spdm_context, LIBSPDM_DATA_REQ_BASE_ASYM_ALG, &parameter, &data16, sizeof(data16));
        if (status != LIBSPDM_STATUS_SUCCESS) {  // NOLINT(hicpp-signed-bitwise)
            return SpdmEngineSettingError::REQ_BASE_ASYM_ALG_SETTING_FAILED;
        }

        pdk::spdm::app::res::algorithms::spdm_library_negotiate_algo_key_schedule(
            support_spdm_key_schedule);
        data16 = 0;
        if (support_spdm_key_schedule) {
            data16 |= static_cast<uint16_t>(SPDM_ALGORITHMS_KEY_SCHEDULE_SPDM);
        }
        memset(&parameter, 0, sizeof(parameter));
        parameter.location = LIBSPDM_DATA_LOCATION_LOCAL;
        status             = libspdm_set_data(
            spdm_context, LIBSPDM_DATA_KEY_SCHEDULE, &parameter, &data16, sizeof(data16));
        if (status != LIBSPDM_STATUS_SUCCESS) {  // NOLINT(hicpp-signed-bitwise)
            return SpdmEngineSettingError::KEY_SCHEDULE_SETTING_FAILED;
        }
    }

    // set certificate chain
    {
        memset(&parameter, 0, sizeof(parameter));
        parameter.location = LIBSPDM_DATA_LOCATION_LOCAL;
        std::bitset<8> slot_mask(pdk::spdm::app::res::digests::spdm_library_get_slot_mask());
        for (size_t i = 0; i < 8; i++) {
            if (slot_mask[i]) {
                auto certificate_chain = pdk::spdm::app::res::certificate::
                    spdm_library_get_certificate_chain(i);
                parameter.additional_data[0] = i;
                auto status                  = libspdm_set_data(spdm_context,
                                               LIBSPDM_DATA_LOCAL_PUBLIC_CERT_CHAIN,
                                               &parameter,
                                               certificate_chain.data(),
                                               certificate_chain.size());
                if (status != LIBSPDM_STATUS_SUCCESS) {  // NOLINT(hicpp-signed-bitwise)
                    return SpdmEngineSettingError::CERTIFICATE_CHAIN_SETTING_FAILED;
                }
            }
        }
    }

    return SpdmEngineSettingError::SUCCESS;
}

}  // namespace pdk::spdm::app::res::library