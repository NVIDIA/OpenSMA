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

#include <cstring>

#include "pdk-spdm-app-res-capabilities-plat.h"
#include "pdk-spdm-app-res-certificate-plat.h"

namespace pdk::spdm::app::res::capabilities {
#ifdef __cplusplus
extern "C" {
#endif
uint8_t spdm_library_get_capabilites_ct_exponent()
{
    return pdk::spdm::platforms::res::capabilities::get_ct_exponent();
}

CacheCap spdm_library_get_capabilites_flags_cache()
{
    return pdk::spdm::platforms::res::capabilities::get_flag_cache();
}
CertCap spdm_library_get_capabilites_flags_cert()
{
    return pdk::spdm::platforms::res::capabilities::get_flag_cert();
}
ChalCap spdm_library_get_capabilites_flags_chal()
{
    return pdk::spdm::platforms::res::capabilities::get_flag_chal();
}
MeasCap spdm_library_get_capabilites_flags_meas()
{
    return pdk::spdm::platforms::res::capabilities::get_flag_meas();
}
MeasFreshCap spdm_library_get_capabilites_flags_meas_fresh()
{
    return pdk::spdm::platforms::res::capabilities::get_flag_meas_fresh();
}
EncryptCap spdm_library_get_capabilites_flags_encrypt()
{
    return pdk::spdm::platforms::res::capabilities::get_flag_encrypt();
}
MacCap spdm_library_get_capabilites_flags_mac()
{
    return pdk::spdm::platforms::res::capabilities::get_flag_mac();
}
MutAuthCap spdm_library_get_capabilites_flags_mut_auth()
{
    return pdk::spdm::platforms::res::capabilities::get_flag_mut_auth();
}
KeyExCap spdm_library_get_capabilites_flags_key_ex()
{
    return pdk::spdm::platforms::res::capabilities::get_flag_key_ex();
}
PskCap spdm_library_get_capabilites_flags_psk()
{
    return pdk::spdm::platforms::res::capabilities::get_flag_psk();
}
EncapCap spdm_library_get_capabilites_flags_encap()
{
    return pdk::spdm::platforms::res::capabilities::get_flag_encap();
}
HbeatCap spdm_library_get_capabilites_flags_hbeat()
{
    return pdk::spdm::platforms::res::capabilities::get_flag_hbeat();
}
KeyUpdCap spdm_library_get_capabilites_flags_key_upd()
{
    return pdk::spdm::platforms::res::capabilities::get_flag_key_upd();
}
HandshakeInTheClearCap spdm_library_get_capabilites_flags_handshake_in_the_clear()
{
    return pdk::spdm::platforms::res::capabilities::get_flag_handshake_in_the_clear();
}
PubKeyIdCap spdm_library_get_capabilites_flags_pub_key_id()
{
    return pdk::spdm::platforms::res::capabilities::get_flag_pub_key_id();
}
// for spdm v1.2
ChunkCap spdm_library_get_capabilites_flags_chunk()
{
    return pdk::spdm::platforms::res::capabilities::get_flag_chunk();
}
// for spdm v1.2
AliasCertCap spdm_library_get_capabilites_flags_alias_cert()
{
    return pdk::spdm::platforms::res::capabilities::get_flag_alias_cert();
}
// for spdm v1.2
SetCertCap spdm_library_get_capabilites_flags_set_cert()
{
    return pdk::spdm::platforms::res::capabilities::get_flag_set_cert();
}
// for spdm v1.2
CsrCap spdm_library_get_capabilites_flags_csr()
{
    return pdk::spdm::platforms::res::capabilities::get_flag_csr();
}
// for spdm v1.2
CertInstallResetCap spdm_library_get_capabilites_flags_cert_install_reset()
{
    return pdk::spdm::platforms::res::capabilities::get_flag_cert_install_reset();
}

Flags spdm_library_get_capabilites_flags()
{
    Flags flags{};
    memset(&flags, 0, sizeof(flags));
    flags.cache_cap      = pdk::spdm::platforms::res::capabilities::get_flag_cache();
    flags.cert_cap       = pdk::spdm::platforms::res::capabilities::get_flag_cert();
    flags.chal_cap       = pdk::spdm::platforms::res::capabilities::get_flag_chal();
    flags.meas_cap       = pdk::spdm::platforms::res::capabilities::get_flag_meas();
    flags.meas_fresh_cap = pdk::spdm::platforms::res::capabilities::get_flag_meas_fresh();
    flags.encrypt_cap    = pdk::spdm::platforms::res::capabilities::get_flag_encrypt();
    flags.mac_cap        = pdk::spdm::platforms::res::capabilities::get_flag_mac();
    flags.mut_auth_cap   = pdk::spdm::platforms::res::capabilities::get_flag_mut_auth();
    flags.key_ex_cap     = pdk::spdm::platforms::res::capabilities::get_flag_key_ex();
    flags.psk_cap        = pdk::spdm::platforms::res::capabilities::get_flag_psk();
    flags.encap_cap      = pdk::spdm::platforms::res::capabilities::get_flag_encap();
    flags.hbeat_cap      = pdk::spdm::platforms::res::capabilities::get_flag_hbeat();
    flags.key_upd_cap    = pdk::spdm::platforms::res::capabilities::get_flag_key_upd();
    flags.handshake_in_the_clear_cap = pdk::spdm::platforms::res::capabilities::
        get_flag_handshake_in_the_clear();
    flags.pub_key_id_cap = pdk::spdm::platforms::res::capabilities::get_flag_pub_key_id();
    flags.chunk_cap      = pdk::spdm::platforms::res::capabilities::get_flag_chunk();
    flags.alias_cert_cap = pdk::spdm::platforms::res::capabilities::get_flag_alias_cert();
    flags.set_cert_cap   = pdk::spdm::platforms::res::capabilities::get_flag_set_cert();
    flags.csr_cap        = pdk::spdm::platforms::res::capabilities::get_flag_csr();
    flags.cert_install_reset_cap = pdk::spdm::platforms::res::capabilities::
        get_flag_cert_install_reset();
    return flags;
}

#ifdef __cplusplus
}
#endif

}  // namespace pdk::spdm::app::res::capabilities
