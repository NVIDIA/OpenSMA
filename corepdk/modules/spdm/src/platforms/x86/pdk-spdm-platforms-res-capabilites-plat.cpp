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
#include "app/pdk-spdm-app-res-capabilities-plat.h"

namespace pdk::spdm::platforms::res::capabilities {
uint8_t get_ct_exponent()
{
    constexpr uint8_t CtExponentValue = 0x1a;
    return CtExponentValue;
}

pdk::spdm::app::res::capabilities::CacheCap get_flag_cache()
{
    return pdk::spdm::app::res::capabilities::CacheCap::NoSupportCache;
}
pdk::spdm::app::res::capabilities::CertCap get_flag_cert()
{
    return pdk::spdm::app::res::capabilities::CertCap::SupportDigestAndCertificate;
}
pdk::spdm::app::res::capabilities::ChalCap get_flag_chal()
{
    return pdk::spdm::app::res::capabilities::ChalCap::NoSupportChallengeAuth;
}
pdk::spdm::app::res::capabilities::MeasCap get_flag_meas()
{
    return pdk::spdm::app::res::capabilities::MeasCap::SupportMeasurementWithSignature;
}

pdk::spdm::app::res::capabilities::MeasFreshCap get_flag_meas_fresh()
{
    return pdk::spdm::app::res::capabilities::MeasFreshCap::SupportFreshMeasurement;
}

pdk::spdm::app::res::capabilities::EncryptCap get_flag_encrypt()
{
    return pdk::spdm::app::res::capabilities::EncryptCap::NoSupportMessageEncryption;
}

pdk::spdm::app::res::capabilities::MacCap get_flag_mac()
{
    return pdk::spdm::app::res::capabilities::MacCap::NoSupportMessageAuthentication;
}

pdk::spdm::app::res::capabilities::MutAuthCap get_flag_mut_auth()
{
    return pdk::spdm::app::res::capabilities::MutAuthCap::NoSupportMutualAuthentication;
}

pdk::spdm::app::res::capabilities::KeyExCap get_flag_key_ex()
{
    return pdk::spdm::app::res::capabilities::KeyExCap::NoSupportKeyExchange;
}
pdk::spdm::app::res::capabilities::PskCap get_flag_psk()
{
    return pdk::spdm::app::res::capabilities::PskCap::NoSupportPreSharedKey;
}
pdk::spdm::app::res::capabilities::EncapCap get_flag_encap()
{
    return pdk::spdm::app::res::capabilities::EncapCap::NoSupport;
}
pdk::spdm::app::res::capabilities::HbeatCap get_flag_hbeat()
{
    return pdk::spdm::app::res::capabilities::HbeatCap::NoSupportHeartbeat;
}
pdk::spdm::app::res::capabilities::KeyUpdCap get_flag_key_upd()
{
    return pdk::spdm::app::res::capabilities::KeyUpdCap::NoSupportKeyUpdate;
}
pdk::spdm::app::res::capabilities::HandshakeInTheClearCap get_flag_handshake_in_the_clear()
{
    return pdk::spdm::app::res::capabilities::HandshakeInTheClearCap::NoSupport;
}
pdk::spdm::app::res::capabilities::PubKeyIdCap get_flag_pub_key_id()
{
    return pdk::spdm::app::res::capabilities::PubKeyIdCap::NoPublicKeyProvisioned;
}

}  // namespace pdk::spdm::platforms::res::capabilities