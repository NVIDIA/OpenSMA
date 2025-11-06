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
#include "pdk-spdm-app-res-capabilities-plat.h"

namespace pdk::spdm::platforms::res::capabilities {
namespace app_res_capabilities = pdk::spdm::app::res::capabilities;

uint8_t get_ct_exponent()
{
    constexpr uint8_t CtExponentValue = 0x1a;
    return CtExponentValue;
}

app_res_capabilities::CacheCap get_flag_cache()
{
    return app_res_capabilities::CacheCap::NoSupportCache;
}
app_res_capabilities::CertCap get_flag_cert()
{
    return app_res_capabilities::CertCap::SupportDigestAndCertificate;
}
app_res_capabilities::ChalCap get_flag_chal()
{
    return app_res_capabilities::ChalCap::NoSupportChallengeAuth;
}
app_res_capabilities::MeasCap get_flag_meas()
{
    return app_res_capabilities::MeasCap::SupportMeasurementWithSignature;
}

app_res_capabilities::MeasFreshCap get_flag_meas_fresh()
{
    return app_res_capabilities::MeasFreshCap::SupportFreshMeasurement;
}

app_res_capabilities::EncryptCap get_flag_encrypt()
{
    return app_res_capabilities::EncryptCap::NoSupportMessageEncryption;
}

app_res_capabilities::MacCap get_flag_mac()
{
    return app_res_capabilities::MacCap::NoSupportMessageAuthentication;
}

app_res_capabilities::MutAuthCap get_flag_mut_auth()
{
    return app_res_capabilities::MutAuthCap::NoSupportMutualAuthentication;
}

app_res_capabilities::KeyExCap get_flag_key_ex()
{
    return app_res_capabilities::KeyExCap::NoSupportKeyExchange;
}
app_res_capabilities::PskCap get_flag_psk()
{
    return app_res_capabilities::PskCap::NoSupportPreSharedKey;
}
app_res_capabilities::EncapCap get_flag_encap()
{
    return app_res_capabilities::EncapCap::NoSupport;
}
app_res_capabilities::HbeatCap get_flag_hbeat()
{
    return app_res_capabilities::HbeatCap::NoSupportHeartbeat;
}
app_res_capabilities::KeyUpdCap get_flag_key_upd()
{
    return app_res_capabilities::KeyUpdCap::NoSupportKeyUpdate;
}
app_res_capabilities::HandshakeInTheClearCap get_flag_handshake_in_the_clear()
{
    return app_res_capabilities::HandshakeInTheClearCap::NoSupport;
}
app_res_capabilities::PubKeyIdCap get_flag_pub_key_id()
{
    return app_res_capabilities::PubKeyIdCap::NoPublicKeyProvisioned;
}

// for spdm v1.2
app_res_capabilities::ChunkCap get_flag_chunk()
{
    return app_res_capabilities::ChunkCap::NoSupport;
}
// for spdm v1.2
app_res_capabilities::AliasCertCap get_flag_alias_cert()
{
    return app_res_capabilities::AliasCertCap::NoSupport;
}
// for spdm v1.2
app_res_capabilities::SetCertCap get_flag_set_cert()
{
    return app_res_capabilities::SetCertCap::NoSupport;
}
// for spdm v1.2
app_res_capabilities::CsrCap get_flag_csr()
{
    return app_res_capabilities::CsrCap::NoSupport;
}
// for spdm v1.2
app_res_capabilities::CertInstallResetCap get_flag_cert_install_reset()
{
    return app_res_capabilities::CertInstallResetCap::NoSupport;
}

}  // namespace pdk::spdm::platforms::res::capabilities