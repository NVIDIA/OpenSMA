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

#include "pdk-spdm-app-res-certificate.h"
#include "pdk-spdm-app-res-measurements-plat.h"
#include "pdk-spdm-app-res-signature-library-plat.h"

namespace pdk::spdm::app::res::measurements {
#ifdef __cplusplus
extern "C" {
#endif

/*
 *  spdm_platform_get_number_of_indices()
 *
 *  This function returns the number of measurements available.
 *
 *  [in] instance - platform instance pointer
 *
 *  Returns: number of measurements
 */
uint8_t spdm_platform_get_number_of_indices(
    pdk::spdm::platforms::res::ada::library::PlatformContext* instance)
{
    auto num_meas = static_cast<uint8_t>(
        platforms::res::measurements::get_measurement_number());
    uint8_t ret_val = 0;
    if (!instance) {
        return 0;
    }
    // make sure within limits
    if (num_meas < MaxNumMeas) {
        ret_val = num_meas;
    }
    else {
        ret_val = MaxNumMeas;
    }
    return ret_val;
}

/*
 *  spdm_platform_get_nonce()
 *
 *  This function returns a 32-byte nonce.  The caller is expected to have
 *  the buffer allocated for 32 bytes.
 *
 *  [in] instance - platform instance pointer
 *  [out] nonce - pointer to buffer for nonce
 *
 *  Returns: void
 */
void spdm_platform_get_nonce(pdk::spdm::platforms::res::ada::library::PlatformContext* instance,
                             void*                                                     nonce)
{
    // check the input pointer
    if (instance == nullptr || nonce == nullptr) {
        return;
    }

    platforms::res::measurements::get_nonce(
        std::span<uint8_t>(instance->nonce.data(), instance->nonce.size()));

    std::copy(instance->nonce.begin(), instance->nonce.end(), std::bit_cast<uint8_t*>(nonce));

    instance->nonce_good = true;

    return;
}

constexpr uint8_t MeasRepresentationMask = 0x80;
constexpr uint8_t MeasBaseTypeMask       = 0x7F;

/*
 *  spdm_platform_get_dmtf_measurement_field()
 *
 *  This function retrieves the measurement record corresponding to the index.
 *
 *  [in] instance - platform instance pointer
 *  [in] index - index of measurement to retrieve, 1-based
 *  [out] representation - indicates measurement format, we use DMTF
 *  [out] type - measurement type
 *  [out] size - size of measurement record
 *  [out] buffer - pointer to buffer for holding the measurement record
 *
 */
void spdm_platform_get_dmtf_measurement_field(
    pdk::spdm::platforms::res::ada::library::PlatformContext* instance,
    uint32_t                                                  index,
    uint32_t*                                                 representation,
    uint32_t*                                                 type,
    uint32_t*                                                 size,
    void*                                                     buffer)
{
    if (instance == nullptr || representation == nullptr || type == nullptr || size == nullptr
        || buffer == nullptr) {
        if (size != nullptr) {
            *size = 0;
        }
        return;
    }

    if (index == 0
        || index
               > static_cast<uint8_t>(platforms::res::measurements::get_measurement_number())) {
        // illegal index
        *size = 0;
        return;
    }
    auto meas_type = platforms::res::measurements::get_measurement_type(index);
    auto meas_size = platforms::res::measurements::get_measurement_size(index);

    // dmtf type of measurement
    if ((meas_type & MeasRepresentationMask) > 0) {
        *representation = 1;
    }
    else {
        *representation = 0;
    }

    // update the measurement type
    *type = meas_type & MeasBaseTypeMask;

    // check the size is enough to save the measurement.
    if (*size < meas_size) {
        *size = 0;
        return;
    }
    else {
        platforms::res::measurements::get_measurement(
            index, std::span<uint8_t>(static_cast<uint8_t*>(buffer), meas_size));
        *size = meas_size;
    }
    return;
}

/*
 *  with move to the v0.4.0 drop of the spdm responder from AdaCore, the
 *  platform APIs that were named with "hash" were changed to use
 *  "transcript".  This is because in supporting key exchange, the session
 *  transcript needed support.  Since we do not support key exchange, we
 *  will change the API names to match, but only handle the measurement
 *  transcript type and the underlying platform API implementation will use
 *  a hash context for the transcript.
 */

constexpr uint8_t MeasTranscript = 1;

/*
 *  spdm_platform_get_new_transcript()
 *
 *  This function provides a new hash context for storing the running hash
 *  of a measurement transcript.
 *
 *  [in] instance - ptr to responder instance data
 *  [in] trans_type - type of transcript: 0 is session transcript
 *                                        1 is measurement transcript
 *
 *  Return: 32 bit transcript ID of the hash context
 */
uint32_t spdm_platform_get_new_transcript(
    pdk::spdm::platforms::res::ada::library::PlatformContext* instance, uint8_t trans_type)
{
    if (instance == nullptr || trans_type != MeasTranscript) {
        // nv::info("bad param for get new transcript \n");
        // return the number of the spdm responder
        return pdk::spdm::platforms::res::ada::library::SpdmResponderNumber;
    }
    if (instance->hash_in_use) {
        // nv::info("hash in use %d\n", instance->inst_num);
    }
    else {
        instance->hash_in_use  = 1;
        instance->hash_started = 1;
        // clear the hash context
        instance->hash_context.init();
    }

    return (instance->inst_num);
}

/*
 *  spdm_platform_valid_transcript_id()
 *
 *  This function verifies if a transcript ID is valid.  The transcript
 *  corresponds to a hash context in the implementation.
 *
 *  [in] instance - ptr to responder instance data
 *  [in] transcript_id - transcript id to verify as valid
 *
 *  Returns: TRUE if the transcript_id is valid
 */
uint8_t spdm_platform_valid_transcript_id(
    pdk::spdm::platforms::res::ada::library::PlatformContext* instance, uint32_t transcript_id)
{
    if (instance == nullptr) {
        // nv::info("instance not avaliable: %d\n", transcript_id);
        return 0;
    }

    if (!instance->hash_in_use) {
        // nv::info("hash not in use: %d\n", instance->inst_num);
        return 0;
    }

    return (transcript_id == instance->inst_num);
}

/*
 *  spdm_platform_reset_transcript()
 *
 *  This function resets the hash context for a running hash.
 *
 *  [in] instance - ptr to responder instance data
 *  [in] transcript_id - hash context to reset
 *  [in] trans_type - transcript type for new transcript
 *
 *  Returns: transcript_id of reset context
 */
uint32_t spdm_platform_reset_transcript(
    pdk::spdm::platforms::res::ada::library::PlatformContext* instance,
    uint32_t                                                  transcript_id,
    uint8_t                                                   trans_type)
{
    // validate the hash id
    if (!spdm_platform_valid_transcript_id(instance, transcript_id)) {
        // do nothing, return the same hash id
        return transcript_id;
    }

    // validate the transcript type
    if (trans_type != MeasTranscript) {
        // nv::info("bad trans_type for reset transcript\n");
        // return the number of the spdm responder
        return pdk::spdm::platforms::res::ada::library::SpdmResponderNumber;
    }

    // clear the hash context
    instance->hash_context.init();
    instance->hash_started = 1;

    return transcript_id;
}

/*
 *  spdm_platform_update_transcript()
 *
 *  This function updates the running hash with new data to hash.
 *
 *  [in] instance - ptr to responder instance data
 *  [in] transcript_id - hash context to update
 *  [in] data - data to include in the running hash
 *  [in] offset - offset to start of data to hash
 *  [in] size - size of data to hash
 *
 *  Returns: TRUE if the hash update was successful
 */
uint8_t spdm_platform_update_transcript(
    pdk::spdm::platforms::res::ada::library::PlatformContext* instance,
    uint32_t                                                  transcript_id,
    void*                                                     data,
    uint32_t                                                  offset,
    uint32_t                                                  size)
{
    // validate the transcript id
    if (!spdm_platform_valid_transcript_id(instance, transcript_id)) {
        return 0;
    }

    // verify hash has started
    if (!instance->hash_started) {
        // nv::info("hash not started\n");
        return 0;
    }

    if (data == nullptr) {
        return 0;
    }

    instance->hash_context.update(
        std::span<uint8_t>(static_cast<uint8_t*>(data) + offset, size));
    return 1;
}

/*
 *  spdm_platform_update_transcript_nonce()
 *
 *  This function adds the nonce saved in the instance data to the running hash.
 *
 *  [in] instance - ptr to responder instance data
 *  [in] transcript_id - hash context to update with nonce
 *
 *  Returns: TRUE if the transcript was updated successfully
 */
uint8_t spdm_platform_update_transcript_nonce(
    pdk::spdm::platforms::res::ada::library::PlatformContext* instance, uint32_t transcript_id)
{
    if (instance == nullptr) {
        return 0;
    }

    // validate the transcript id
    if (!spdm_platform_valid_transcript_id(instance, transcript_id)) {
        return 0;
    }

    // make sure nonce is good
    if (!instance->nonce_good) {
        // nv::info("invalid nonce\n");
        return 0;
    }

    // verify hash has started
    if (!instance->hash_started) {
        // nv::info("nonce hash not started\n");
        return 0;
    }

    // mark the nonce invalid
    instance->nonce_good = 0;

    return (spdm_platform_update_transcript(
        instance, transcript_id, instance->nonce.data(), 0, instance->nonce.size()));
}

/*
 *  spdm_platform_get_signature()
 *
 *  This function gets the signature of the running hash data.
 *
 *  [in] instance - ptr to responder instance data
 *  [in] transcript_id - hash context to use for generating signature
 *  [in] slot - slot to use for private key
 *  [in] signature - ptr to buffer to hold the signature
 *  [in out] size - ptr to signature buffer size, returns signature size
 *
 *  Returns: void
 */
void spdm_platform_get_signature(
    pdk::spdm::platforms::res::ada::library::PlatformContext* instance,
    uint32_t                                                  transcript_id,
    uint8_t                                                   slot,
    void*                                                     signature,
    uint32_t*                                                 size)
{
    if (signature == nullptr || size == nullptr) {
        if (size != nullptr) {
            *size = 0;
        }
        return;
    }

    // validate the transcript id
    if (!spdm_platform_valid_transcript_id(instance, transcript_id)) {
        // do nothing, return zero length signature for error
        // nv::info("369spdm_platform_get_signature error\n", slot);
        *size = 0;
        return;
    }
    // validate the slot number
    if (!pdk::spdm::app::res::certificate::check_slot_existed(slot)) {
        // nv::info("get signature invalid slot %d\n", slot);
        *size = 0;
        return;
    }
    constexpr size_t Sha384Size = pdk::spdm::platforms::res::hash::type::Sha384HashSize;
    std::array<uint8_t, Sha384Size> hash = {0};

    // hash the signature data
    const bool CryptoRet = instance->hash_context.finish(hash);

    if (!CryptoRet) {
        *size = 0;
        return;
    }

    // check buffer is enough to save signature
    if (*size < pdk::spdm::platforms::res::signature::type::Ecdsa384SignatureSize) {
        *size = 0;
        return;
    }
    auto& signature_arr = *std::bit_cast<
        std::array<uint8_t,
                   pdk::spdm::platforms::res::signature::type::Ecdsa384SignatureSize>*>(
        signature);

    // sign with the digest data
    if (!pdk::spdm::platforms::res::signature::library::get_signature_from_hash(
            hash, signature_arr)) {
        *size = 0;
        return;
    }
    *size                  = pdk::spdm::platforms::res::signature::type::Ecdsa384SignatureSize;
    instance->hash_started = 0;
    return;
}

/*
 *  spdm_platform_get_meas_opaque_data()
 *
 *  This function allows the platform to return any opaque data in the
 * measurment response.  This is unused.
 *
 *  [in] instance - pointer to responder instance data
 *  [out] data - pointer to buffer to hold data
 *  [in|out] size - contains size of data buffer on input, holds size of data
 * returned
 *
 *  Returns: void
 */
void spdm_platform_get_meas_opaque_data(
    pdk::spdm::platforms::res::ada::library::PlatformContext* instance,
    void*                                                     data,
    uint32_t*                                                 size)
{
    // verify parameters
    if (instance == nullptr || data == nullptr) {
        return;
    }

    // return 0 size only if non-null, not support to have any opaque data
    if (size != nullptr) {
        *size = 0;
    }
    return;
}

#ifdef __cplusplus
}
#endif

}  // namespace pdk::spdm::app::res::measurements
