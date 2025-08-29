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
#include "pdk-spdm-app-res-capabilities.h"

#include "pdk-spdm-app-res-capabilities-plat.h"
#include "pdk-spdm-app-res-certificate-plat.h"

namespace pdk::spdm::app::res::capabilities {
#ifdef __cplusplus
extern "C" {
#endif

uint8_t spdm_platform_config_ct_exponent(
    [[maybe_unused]] pdk::spdm::platforms::res::ada::library::PlatformContext* instance)
{
    return pdk::spdm::platforms::res::capabilities::get_ct_exponent();
}
uint8_t spdm_platform_config_cap_mac(
    [[maybe_unused]] pdk::spdm::platforms::res::ada::library::PlatformContext* instance)
{
    const auto MacCap = pdk::spdm::platforms::res::capabilities::get_flag_mac();
    return static_cast<std::underlying_type_t<decltype(MacCap)>>(MacCap);
}
uint8_t spdm_platform_config_cap_encrypt(
    [[maybe_unused]] pdk::spdm::platforms::res::ada::library::PlatformContext* instance)
{
    const auto EncryptCap = pdk::spdm::platforms::res::capabilities::get_flag_encrypt();
    return static_cast<std::underlying_type_t<decltype(EncryptCap)>>(EncryptCap);
}
uint8_t spdm_platform_config_cap_meas_fresh(
    [[maybe_unused]] pdk::spdm::platforms::res::ada::library::PlatformContext* instance)
{
    const auto MeasFreshCap = pdk::spdm::platforms::res::capabilities::get_flag_meas_fresh();
    return static_cast<std::underlying_type_t<decltype(MeasFreshCap)>>(MeasFreshCap);
}
uint8_t spdm_platform_config_cap_meas(
    [[maybe_unused]] pdk::spdm::platforms::res::ada::library::PlatformContext* instance)
{
    const auto MeasCap = pdk::spdm::platforms::res::capabilities::get_flag_meas();
    return static_cast<std::underlying_type_t<decltype(MeasCap)>>(MeasCap);
}
uint8_t spdm_platform_config_cap_chal(
    [[maybe_unused]] pdk::spdm::platforms::res::ada::library::PlatformContext* instance)
{
    const auto ChalCap = pdk::spdm::platforms::res::capabilities::get_flag_chal();
    return static_cast<std::underlying_type_t<decltype(ChalCap)>>(ChalCap);
}
uint8_t spdm_platform_config_cap_cert(
    [[maybe_unused]] pdk::spdm::platforms::res::ada::library::PlatformContext* instance)
{
    const auto CertCap = pdk::spdm::platforms::res::capabilities::get_flag_cert();
    return static_cast<std::underlying_type_t<decltype(CertCap)>>(CertCap);
}
uint8_t spdm_platform_config_cap_cache(
    [[maybe_unused]] pdk::spdm::platforms::res::ada::library::PlatformContext* instance)
{
    const auto CacheCap = pdk::spdm::platforms::res::capabilities::get_flag_cache();
    return static_cast<std::underlying_type_t<decltype(CacheCap)>>(CacheCap);
}
uint8_t spdm_platform_config_cap_handshake_in_the_clear(
    [[maybe_unused]] pdk::spdm::platforms::res::ada::library::PlatformContext* instance)
{
    const auto HandShakeCap = pdk::spdm::platforms::res::capabilities::
        get_flag_handshake_in_the_clear();
    return static_cast<std::underlying_type_t<decltype(HandShakeCap)>>(HandShakeCap);
}
uint8_t spdm_platform_config_cap_key_upd(
    [[maybe_unused]] pdk::spdm::platforms::res::ada::library::PlatformContext* instance)
{
    const auto KeyUpdCap = pdk::spdm::platforms::res::capabilities::get_flag_key_upd();
    return static_cast<std::underlying_type_t<decltype(KeyUpdCap)>>(KeyUpdCap);
}
uint8_t spdm_platform_config_cap_hbeat(
    [[maybe_unused]] pdk::spdm::platforms::res::ada::library::PlatformContext* instance)
{
    const auto HbeatCap = pdk::spdm::platforms::res::capabilities::get_flag_hbeat();
    return static_cast<std::underlying_type_t<decltype(HbeatCap)>>(HbeatCap);
}
uint8_t spdm_platform_config_cap_encap(
    [[maybe_unused]] pdk::spdm::platforms::res::ada::library::PlatformContext* instance)
{
    const auto EncapCap = pdk::spdm::platforms::res::capabilities::get_flag_encap();
    return static_cast<std::underlying_type_t<decltype(EncapCap)>>(EncapCap);
}
uint8_t spdm_platform_config_cap_psk(
    [[maybe_unused]] pdk::spdm::platforms::res::ada::library::PlatformContext* instance)
{
    const auto PskCap = pdk::spdm::platforms::res::capabilities::get_flag_psk();
    return static_cast<std::underlying_type_t<decltype(PskCap)>>(PskCap);
}
uint8_t spdm_platform_config_cap_key_ex(
    [[maybe_unused]] pdk::spdm::platforms::res::ada::library::PlatformContext* instance)
{
    const auto KeyExCap = pdk::spdm::platforms::res::capabilities::get_flag_key_ex();
    return static_cast<std::underlying_type_t<decltype(KeyExCap)>>(KeyExCap);
}
uint8_t spdm_platform_config_cap_mut_auth(
    [[maybe_unused]] pdk::spdm::platforms::res::ada::library::PlatformContext* instance)
{
    const auto MutAuthCap = pdk::spdm::platforms::res::capabilities::get_flag_mut_auth();
    return static_cast<std::underlying_type_t<decltype(MutAuthCap)>>(MutAuthCap);
}
uint8_t spdm_platform_config_cap_pub_key_id(
    [[maybe_unused]] pdk::spdm::platforms::res::ada::library::PlatformContext* instance)
{
    const auto PudKeyIdCap = pdk::spdm::platforms::res::capabilities::get_flag_pub_key_id();
    return static_cast<std::underlying_type_t<decltype(PudKeyIdCap)>>(PudKeyIdCap);
}
#ifdef __cplusplus
}
#endif

}  // namespace pdk::spdm::app::res::capabilities
