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
#include <stdint.h>

#include "pdk-spdm-app-res-ada-library.h"
#include "pdk-spdm-app-res-code.h"

namespace pdk::spdm::app::res::capabilities {

#ifdef __cplusplus
extern "C" {
#endif

uint8_t spdm_platform_config_ct_exponent(
    pdk::spdm::platforms::res::ada::library::PlatformContext* instance);
uint8_t spdm_platform_config_cap_mac(
    pdk::spdm::platforms::res::ada::library::PlatformContext* instance);
uint8_t spdm_platform_config_cap_encrypt(
    pdk::spdm::platforms::res::ada::library::PlatformContext* instance);
uint8_t spdm_platform_config_cap_meas_fresh(
    pdk::spdm::platforms::res::ada::library::PlatformContext* instance);
uint8_t spdm_platform_config_cap_meas(
    pdk::spdm::platforms::res::ada::library::PlatformContext* instance);
uint8_t spdm_platform_config_cap_chal(
    pdk::spdm::platforms::res::ada::library::PlatformContext* instance);
uint8_t spdm_platform_config_cap_cert(
    pdk::spdm::platforms::res::ada::library::PlatformContext* instance);
uint8_t spdm_platform_config_cap_cache(
    pdk::spdm::platforms::res::ada::library::PlatformContext* instance);
uint8_t spdm_platform_config_cap_handshake_in_the_clear(
    pdk::spdm::platforms::res::ada::library::PlatformContext* instance);
uint8_t spdm_platform_config_cap_key_upd(
    pdk::spdm::platforms::res::ada::library::PlatformContext* instance);
uint8_t spdm_platform_config_cap_hbeat(
    pdk::spdm::platforms::res::ada::library::PlatformContext* instance);
uint8_t spdm_platform_config_cap_encap(
    pdk::spdm::platforms::res::ada::library::PlatformContext* instance);
uint8_t spdm_platform_config_cap_psk(
    pdk::spdm::platforms::res::ada::library::PlatformContext* instance);
uint8_t spdm_platform_config_cap_key_ex(
    pdk::spdm::platforms::res::ada::library::PlatformContext* instance);
uint8_t spdm_platform_config_cap_mut_auth(
    pdk::spdm::platforms::res::ada::library::PlatformContext* instance);
uint8_t spdm_platform_config_cap_pub_key_id(
    pdk::spdm::platforms::res::ada::library::PlatformContext* instance);

#ifdef __cplusplus
}

using SPDMVersion    = uint8_t;
using Param1         = uint8_t;
using Param2         = uint8_t;
using Reserved       = uint8_t;
using CTExponent     = uint8_t;
using Reserved2Bytes = uint16_t;

enum class CacheCap : uint8_t
{
    NoSupportCache = 0b0u,
    SupportCache   = 0b1u
};
enum class CertCap : uint8_t
{
    NoSupportDigestAndCertificate = 0b0u,
    SupportDigestAndCertificate   = 0b1u
};
enum class ChalCap : uint8_t
{
    NoSupportChallengeAuth = 0b0u,
    SupportChallengeAuth   = 0b1u
};
enum class MeasCap : uint8_t
{
    NotSupportMeasurement              = 0b00u,
    SupportMeasurementWithoutSignature = 0b01,
    SupportMeasurementWithSignature    = 0b10,
    Reserved                           = 0b11u
};
enum class MeasFreshCap : uint8_t
{
    NotSupportFreshMeasurement = 0b0u,
    SupportFreshMeasurement    = 0b1u,
};
enum class EncryptCap : uint8_t
{
    NoSupportMessageEncryption = 0b0u,
    SupportMessageEncryption   = 0b1u
};
enum class MacCap : uint8_t
{
    NoSupportMessageAuthentication = 0b0u,
    SupportMessageAuthentication   = 0b1u
};
enum class MutAuthCap : uint8_t
{
    NoSupportMutualAuthentication = 0b0u,
    SupportMutualAuthentication   = 0b1u
};
enum class KeyExCap : uint8_t
{
    NoSupportKeyExchange = 0b0u,
    SupportKeyExchange   = 0b1u
};

enum class PskCap : uint8_t
{
    NoSupportPreSharedKey                          = 0b00u,
    SupportPreSharedKeyWithoutSessionKeyDerivation = 0b01u,
    SupportPreSharedKeyWithSessionKeyDerivation    = 0b10u,
    Reserved                                       = 0b11u
};
enum class EncapCap : uint8_t
{
    NoSupport = 0b0u,
    Support   = 0b1u,
};

enum class HbeatCap : uint8_t
{
    NoSupportHeartbeat = 0b0u,
    SupportHeartbeat   = 0b1u
};
enum class KeyUpdCap : uint8_t
{
    NoSupportKeyUpdate = 0b0u,
    SupportKeyUpdate   = 0b1u
};
enum class HandshakeInTheClearCap : uint8_t
{
    NoSupport = 0b0u,
    Support   = 0b1u
};
enum class PubKeyIdCap : uint8_t
{
    NoPublicKeyProvisioned = 0b0,
    PublicKeyProvisioned   = 0b1
};

struct [[gnu::packed]] Flags
{
    // Byte 0
    CacheCap cahae_cap                                : 1;
    CertCap  cert_cap                                 : 1;
    ChalCap  chal_cap                                 : 1;
    MeasCap  meas_cap                                 : 2;
    MeasFreshCap                                      : 1;
    EncryptCap encrypt_cap                            : 1;
    MacCap     mac_cap                                : 1;
    // Byte 1
    MutAuthCap             mut_auth_cap               : 1;
    KeyExCap               key_ex_cap                 : 1;
    PskCap                 psk_cap                    : 2;
    EncapCap               encap_cap                  : 1;
    HbeatCap               hbeat_cap                  : 1;
    KeyUpdCap              key_upd_cap                : 1;
    HandshakeInTheClearCap handshake_in_the_clear_cap : 1;
    // Byte 2
    PubKeyIdCap pub_key_id_cap                        : 1;
    uint8_t     reserved_2                            : 7;
    // Byte 3
    uint8_t reserved_3                                : 8;
};

static_assert(sizeof(Flags) == 4, "Flags structure should be 4 bytes");
using DataTransferSize = uint32_t;
using MaxSPDMMsgSize   = uint32_t;

struct [[gnu::packed]] SpdmResCapabilitiesPayload
{
    SPDMVersion            spdm_version;
    code::SpdmResponseCode RequestResponseCode;
    Param1                 param1;
    Param2                 param2;
    Reserved               reserved;
    CTExponent             ct_exponent;
    Reserved2Bytes         reserved_2_bytes;
    Flags                  flags;
    // for spdm v1.2
    // DataTransferSize data_transfer_size;
    // for spdm v1.2
    // MaxSPDMMsgSize max_spdm_msg_size;
    template<size_t N>
    static SpdmResCapabilitiesPayload& from_arr(std::array<uint8_t, N>& arr);
};

// check for spdm v1.1
static_assert(sizeof(SpdmResCapabilitiesPayload) == 12,
              "SpdmResCapabilitiesPayload structure should be 12 bytes");

#endif
}  // namespace pdk::spdm::app::res::capabilities