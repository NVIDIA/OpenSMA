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
#include "pdk-spdm-app-res-measurements.h"

#include <bitset>
#include <cstring>
#include <span>

#include "libspdm/include/internal/libspdm_common_lib.h"

namespace pdk::spdm::app::res::measurements {
#ifdef __cplusplus
extern "C" {
#endif

constexpr uint8_t AllMeasurementsIndex = 0xff;

/**
 * Sign an SPDM message data.
 *
 * @param  spdm_context    A pointer to the SPDM context.
 * @param  spdm_version    Indicates the negotiated s version.
 * @param  base_asym_algo  Indicates the signing algorithm.
 * @param  base_hash_algo  Indicates the hash algorithm.
 * @param  is_data_hash    Indicate the message type.
 *                         If true, raw message before hash.
 *                         If false, message hash.
 * @param  message         A pointer to a message to be signed.
 * @param  message_size    The size, in bytes, of the message to be signed.
 * @param  signature       A pointer to a destination buffer to store the signature.
 * @param  sig_size        On input, indicates the size, in bytes, of the destination buffer to
 *                         store the signature.
 *                         On output, indicates the size, in bytes, of the signature in the
 *buffer.
 *
 * @retval true  Signing success.
 * @retval false Signing fail.
 **/
bool libspdm_responder_data_sign(
#if LIBSPDM_HAL_PASS_SPDM_CONTEXT
    void* spdm_context,
#endif
    [[maybe_unused]] spdm_version_number_t spdm_version,
    [[maybe_unused]] uint8_t               op_code,
    uint32_t                               base_asym_algo,
    uint32_t                               base_hash_algo,
    bool                                   is_data_hash,
    const uint8_t*                         message,
    size_t                                 message_size,
    uint8_t*                               signature,
    size_t*                                sig_size)
{
    using namespace pdk::spdm::app::res::algorithms;

    constexpr size_t                 MaxHashSize = 128;
    std::array<uint8_t, MaxHashSize> hash_data   = {0};
    const size_t
        hash_size = pdk::spdm::app::res::crypto::hash::find_hash_size_from_base_hash_algo(
            static_cast<BaseHashSel>(base_hash_algo));

    // need hashing the message
    if (!is_data_hash) {
        if (hash_size == 0) {
            // no support the hash algorithm to hash the message
            return false;
        }
        if (!pdk::spdm::app::res::crypto::hash::hash_all_from_base_hash_algo(
                base_hash_algo,
                std::span<const uint8_t>(message, message_size),
                std::span<uint8_t>(hash_data.data(), hash_size))) {
            return false;
        }
    }
    else {
        // the message size if not match or not enough as the hash size
        if (message_size < hash_size) {
            return false;
        }
        memcpy(hash_data.data(), message, hash_size);
    }
    // [TODO] need to support more algorithms
    size_t nid = LIBSPDM_CRYPTO_NID_NULL;
    switch (base_asym_algo) {
        case SPDM_ALGORITHMS_BASE_ASYM_ALGO_TPM_ALG_ECDSA_ECC_NIST_P256:
            nid = LIBSPDM_CRYPTO_NID_ECDSA_NIST_P256;
            break;
        case SPDM_ALGORITHMS_BASE_ASYM_ALGO_TPM_ALG_ECDSA_ECC_NIST_P384:
            nid = LIBSPDM_CRYPTO_NID_ECDSA_NIST_P384;
            break;
        case SPDM_ALGORITHMS_BASE_ASYM_ALGO_TPM_ALG_ECDSA_ECC_NIST_P521:
            nid = LIBSPDM_CRYPTO_NID_ECDSA_NIST_P521;
            break;
        default: nid = LIBSPDM_CRYPTO_NID_NULL; break;
    }
    void* ec_context = pdk::spdm::app::res::crypto::ec::libspdm_ec_new_by_nid(nid);
    if (ec_context == nullptr) {
        return false;
    }
    const bool Result = libspdm_asym_sign_hash(spdm_version,
                                               op_code,
                                               base_asym_algo,
                                               base_hash_algo,
                                               ec_context,
                                               hash_data.data(),
                                               hash_size,
                                               signature,
                                               sig_size);
    pdk::spdm::app::res::crypto::ec::libspdm_ec_free(ec_context);
    return Result;
}

#if LIBSPDM_ENABLE_CAPABILITY_MEAS_CAP
/**
 * Collect the device measurement.
 *
 * libspdm will call this function to retrieve the measurements for a device.
 * The "measurement_index" parameter indicates the measurement requested.
 *
 * @param  spdm_context  A pointer to the SPDM context.
 * @param  spdm_version  Indicates the negotiated SPDM version.
 *
 * @param  measurement_specification  Indicates the measurement specification.
 * Must be a SPDM_MEASUREMENT_BLOCK_HEADER_SPECIFICATION_* value in spdm.h.
 *
 * @param  measurement_hash_algo  Indicates the measurement hash algorithm.
 * Must be SPDM_ALGORITHMS_MEASUREMENT_HASH_ALGO_* value in spdm.h.
 *
 * @param  measurement_index  The index of the measurement to collect.
 * A value of 0x00 requests only the total number of measurements to be returned in
 * "measurements_count". The parameters "measurements" and "measurements_size" will be left
 * unmodified.
 *
 * A value of [0x01 - 0xFE] requests a single measurement for that measurement index
 * be returned. On success, "measurements_count" will be set to 1 and the
 * "measurements" and "measurements_size" fields will be set based
 * on the single measurement. An invalid measurement index will cause
 * "measurements_count" to return 0.
 *
 * A value of 0xFF requests all measurements be returned.
 * On success, "measurements_count", "measurements", and "measurements_size"
 * fields will be set with data from all measurements.
 *
 * @param request_attribute A bitmask who fields are SPDM_GET_MEASUREMENTS_REQUEST_ATTRIBUTES_*.
 *
 * @param  measurements_count
 * When "measurement_index" is zero, returns the total count of
 * measurements available for the device. None of the actual measurements are
 * returned however, and "measurements" and "measurements_size" are unmodified.
 *
 * When "measurement_index" is non-zero, returns the number of measurements
 * returned in "measurements" and "measurements_size". If "measurements_index"
 * is an invalid index not supported by the device, "measurements_count" will
 * return 0 and the function will return LIBSPDM_STATUS_MEAS_INVALID_INDEX.
 *
 * @param  measurements
 * A pointer to a destination buffer to store the concatenation of all device
 * measurement blocks. This buffer will only be modified if "measurement_index" is non-zero.
 *
 * @param  measurements_size
 * On input, indicates the size in bytes of the destination buffer.
 * On output, indicates the total size in bytes of all device measurement
 * blocks in the buffer. This field should only be modified if "measurement_index" is non-zero.
 * The maximum size is SPDM_MAX_MEASUREMENT_RECORD_LENGTH (2^24 - 1 bytes).
 **/
libspdm_return_t libspdm_measurement_collection(
#if LIBSPDM_HAL_PASS_SPDM_CONTEXT
    void* spdm_context,
#endif
    [[maybe_unused]] spdm_version_number_t spdm_version,
    uint8_t                                measurement_specification,
    [[maybe_unused]] uint32_t              measurement_hash_algo,
    uint8_t                                measurement_index,
    [[maybe_unused]] uint8_t               request_attribute,
    [[maybe_unused]] uint8_t*              content_changed,
    uint8_t*                               measurements_count,
    void*                                  measurements,
    size_t*                                measurements_size)
{
    const size_t MeasurementsBufferSize = *measurements_size;
    // get the number of measurements
    const uint8_t num_meas = pdk::spdm::platforms::res::measurements::get_measurement_number();
    // check if the measurement index is valid and is not query all of measurements
    if (measurement_index > num_meas && measurement_index != AllMeasurementsIndex) {
        *measurements_count = 0;
        return LIBSPDM_STATUS_MEAS_INVALID_INDEX;  // NOLINT(hicpp-signed-bitwise)
    }
    // return the number of measurements
    if (measurement_index == 0x00) {
        *measurements_count = pdk::spdm::platforms::res::measurements::get_measurement_number();
        return LIBSPDM_STATUS_SUCCESS;  // NOLINT(hicpp-signed-bitwise)
    }

    // helper function to get the measurement block
    auto get_measurement_block = [&](std::span<uint8_t> measurement_buffer, uint8_t m_index) {
        spdm_measurement_block_dmtf_t meas_block{};

        // get the measurement type
        meas_block.measurement_block_dmtf_header.dmtf_spec_measurement_value_type = pdk::spdm::
            platforms::res::measurements::get_measurement_type(m_index);
        // get the measurement size
        meas_block.measurement_block_dmtf_header.dmtf_spec_measurement_value_size = pdk::spdm::
            platforms::res::measurements::get_measurement_size(m_index);
        meas_block.measurement_block_common_header.index = m_index;
        meas_block.measurement_block_common_header
            .measurement_specification = measurement_specification;
        meas_block.measurement_block_common_header
            .measurement_size = sizeof(meas_block.measurement_block_dmtf_header)
                              + meas_block.measurement_block_dmtf_header
                                    .dmtf_spec_measurement_value_size;

        // copy the measurement to the buffer
        std::array<uint8_t, 1024> meas_buffer{};

        if (!pdk::spdm::platforms::res::measurements::get_measurement(
                m_index, std::span<uint8_t>(meas_buffer.data(), meas_buffer.size()))) {
            return LIBSPDM_STATUS_MEAS_INVALID_INDEX;  // NOLINT(hicpp-signed-bitwise)
        }

        // generate the fully measurement block padyload.
        memcpy(measurement_buffer.data(), &meas_block, sizeof(spdm_measurement_block_dmtf_t));
        memcpy(measurement_buffer.data() + sizeof(spdm_measurement_block_dmtf_t),
               meas_buffer.data(),
               meas_block.measurement_block_dmtf_header.dmtf_spec_measurement_value_size);

        return LIBSPDM_STATUS_SUCCESS;  // NOLINT(hicpp-signed-bitwise)
    };

    // start get the measurement block
    auto measurement_span = std::span<uint8_t>(static_cast<uint8_t*>(measurements),
                                               MeasurementsBufferSize);
    // return single measurement
    if (measurement_index != AllMeasurementsIndex) {
        // check the buffer is enough to save the measurement block
        if (MeasurementsBufferSize
            < sizeof(spdm_measurement_block_dmtf_t)
                  + pdk::spdm::platforms::res::measurements::get_measurement_size(
                      measurement_index)) {
            *measurements_count = 0;
            return LIBSPDM_STATUS_MEAS_INVALID_INDEX;  // NOLINT(hicpp-signed-bitwise)
        }

        // get the measurement block failed
        if (auto ret = get_measurement_block(measurement_span, measurement_index);
            ret != LIBSPDM_STATUS_SUCCESS) {  // NOLINT(hicpp-signed-bitwise)
            *measurements_count = 0;
            return ret;
        }
        *measurements_count = 1;
        *measurements_size  = sizeof(spdm_measurement_block_dmtf_t)
                           + pdk::spdm::platforms::res::measurements::get_measurement_size(
                                 measurement_index);

        return LIBSPDM_STATUS_SUCCESS;  // NOLINT(hicpp-signed-bitwise)
    }

    // return all measurements
    if (measurement_index == AllMeasurementsIndex) {
        // get the size of all measurements
        size_t all_meas_size = 0;
        for (uint8_t i = 1; i <= num_meas; i++) {
            all_meas_size += sizeof(spdm_measurement_block_dmtf_t)
                           + pdk::spdm::platforms::res::measurements::get_measurement_size(i);
        }
        // check the buffer is enough to save the measurement block
        if (MeasurementsBufferSize < all_meas_size) {
            *measurements_count = 0;
            return LIBSPDM_STATUS_MEAS_INVALID_INDEX;  // NOLINT(hicpp-signed-bitwise)
        }

        // get the measurement block
        for (uint8_t i = 1; i <= num_meas; i++) {
            if (auto ret = get_measurement_block(measurement_span, i);
                ret != LIBSPDM_STATUS_SUCCESS) {  // NOLINT(hicpp-signed-bitwise)
                *measurements_count = 0;
                return ret;
            }
            // move the span to the next measurement block start address
            measurement_span = measurement_span.subspan(
                sizeof(spdm_measurement_block_dmtf_t)
                + pdk::spdm::platforms::res::measurements::get_measurement_size(i));
        }
        *measurements_count = num_meas;
        *measurements_size  = all_meas_size;
        return LIBSPDM_STATUS_SUCCESS;  // NOLINT(hicpp-signed-bitwise)
    }
    // will not reach here
    return LIBSPDM_STATUS_SUCCESS;  // NOLINT(hicpp-signed-bitwise)
}

/**
 * This functions returns the opaque data in a MEASUREMENTS response.
 *
 * It is called immediately after libspdm_measurement_collection() is called and allows the
 *opaque data field to vary based on the GET_MEASUREMENTS request.
 *
 * @param  spdm_context  A pointer to the SPDM context.
 * @param  spdm_version  Indicates the negotiated SPDM version.
 *
 * @param  measurement_specification  Indicates the measurement specification.
 * Must be a SPDM_MEASUREMENT_BLOCK_HEADER_SPECIFICATION_* value in spdm.h.
 *
 * @param  measurement_hash_algo  Indicates the measurement hash algorithm.
 * Must be SPDM_ALGORITHMS_MEASUREMENT_HASH_ALGO_* value in spdm.h.
 *
 * @param  measurement_index  The index of the measurement to collect.
 *
 * @param request_attribute A bitmask who fields are SPDM_GET_MEASUREMENTS_REQUEST_ATTRIBUTES_*.
 *
 * @param opaque_data
 * A pointer to a destination buffer whose size, in bytes, is opaque_data_size. The opaque data
 *is copied to this buffer.
 *
 * @param opaque_data_size
 * On input, indicates the size, in bytes, of the destination buffer.
 * On output, indicates the size of the opaque data.
 **/
bool libspdm_measurement_opaque_data(
#if LIBSPDM_HAL_PASS_SPDM_CONTEXT
    void* spdm_context,
#endif
    [[maybe_unused]] spdm_version_number_t spdm_version,
    [[maybe_unused]] uint8_t               measurement_specification,
    [[maybe_unused]] uint32_t              measurement_hash_algo,
    [[maybe_unused]] uint8_t               measurement_index,
    [[maybe_unused]] uint8_t               request_attribute,
    [[maybe_unused]] void*                 opaque_data,
    size_t*                                opaque_data_size)
{
    // to do: need to exprort this function to platform layer
    // current not support the opaque data
    *opaque_data_size = 0;
    return true;
}

/**
 * This function calculates the measurement summary hash.
 *
 * @param  spdm_context               A pointer to the SPDM context.
 * @param  spdm_version               The SPDM version.
 * @param  base_hash_algo             The hash algo to use on summary.
 * @param  measurement_specification  Indicates the measurement specification.
 *                                    It must align with measurement_specification.
 *                                    (SPDM_MEASUREMENT_BLOCK_HEADER_SPECIFICATION_*)
 * @param  measurement_hash_algo      Indicates the measurement hash algorithm.
 *                                    (SPDM_ALGORITHMS_MEASUREMENT_HASH_ALGO_*)
 *
 * @param  measurement_summary_hash_type  The type of the measurement summary hash. Either
 *                                        SPDM_REQUEST_TCB_COMPONENT_MEASUREMENT_HASH or
 *                                        SPDM_REQUEST_ALL_MEASUREMENTS_HASH.
 * @param  measurement_summary_hash       The buffer to store the measurement summary hash.
 * @param  measurement_summary_hash_size  The size in bytes of the buffer.
 *
 * @retval true   Measurement summary hash is successfully generated.
 * @retval false  Error when generating the measurement summary hash.
 **/
bool libspdm_generate_measurement_summary_hash(
#if LIBSPDM_HAL_PASS_SPDM_CONTEXT
    void* spdm_context,
#endif
    [[maybe_unused]] spdm_version_number_t spdm_version,
    [[maybe_unused]] uint32_t              base_hash_algo,
    [[maybe_unused]] uint8_t               measurement_specification,
    [[maybe_unused]] uint32_t              measurement_hash_algo,
    [[maybe_unused]] uint8_t               measurement_summary_hash_type,
    [[maybe_unused]] uint8_t*              measurement_summary_hash,
    [[maybe_unused]] uint32_t              measurement_summary_hash_size)
{
    // to do: need to exprort this function to platform layer
    // current not support the measurement summary hash
    return false;
}
#endif /* LIBSPDM_ENABLE_CAPABILITY_MEAS_CAP */

#ifdef __cplusplus
}
#endif

}  // namespace pdk::spdm::app::res::measurements
