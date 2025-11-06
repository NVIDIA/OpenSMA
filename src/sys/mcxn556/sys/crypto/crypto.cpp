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
#include "sys/crypto/crypto.h"
#include "sys/common/address_map.h"
#include "fsl_nboot.h"
#include "fsl_flash_ffr.h"
#include "nv/flash/flash.h"
#include "sys/common/utils.h"
#include "sys/crypto/crypto.h"
#include "mpu_syscall_numbers.h"

namespace sys::crypto {

namespace {  // some helper function
template<typename InputT>
std::array<uint8_t, sizeof(InputT)>& to_array_view(InputT& input_data)
{
    return *std::bit_cast<std::array<uint8_t, sizeof(input_data)>*>(&input_data);
}
template<typename InputT>
static std::span<uint8_t> to_span_view(InputT& input_data)
{
    return std::span(*std::bit_cast<std::array<uint8_t, sizeof(InputT)>*>(&input_data));
}
}  // namespace

#if defined(__cplusplus)
extern "C" {
#endif

NV_PRIVILEGED_FUNCTION nv::spdm::crypto::CryptoStatus
                       Crypto_Image_Auth_Ecdsa_Priv(nboot_context_t& nbootCtx,
                                                    uint8_t*         imageAddress,
                                                    nboot_bool_t&    is_signature_verified,
                                                    auth_params_t&   parms)
{
    return perform_image_auth_ecdsa_impl(nbootCtx, imageAddress, is_signature_verified, parms);
}

#if defined(__cplusplus)
}
#endif

nv::spdm::crypto::CryptoStatus perform_image_auth_ecdsa(nboot_context_t& nbootCtx,
                                                        uint8_t*         imageAddress,
                                                        nboot_bool_t&    is_signature_verified,
                                                        auth_params_t&   parms)
{
    return perform_image_auth_ecdsa_svc(nbootCtx, imageAddress, is_signature_verified, parms);
}

NV_SYS_CALL nv::spdm::crypto::CryptoStatus
            perform_image_auth_ecdsa_svc(nboot_context_t& nbootCtx,
                                         uint8_t*         imageAddress,
                                         nboot_bool_t&    is_signature_verified,
                                         auth_params_t&   parms)
{
#if ((configENABLE_MPU == 1) && (configUSE_MPU_WRAPPERS_V1 == 0))
    __asm volatile(  // NOLINT
        " .syntax unified                                       \n"
        " .extern Crypto_Image_Auth_Ecdsa_Priv                  \n"
        "                                                       \n"
        " push {r0}                                             \n"
        " mrs r0, ipsr                                          \n"
        " cmp r0, #0                                            \n"
        " bne Privileged_Crypto_Image_Auth_Ecdsa_Priv           \n"
        " mrs r0, control                                       \n"
        " tst r0, #1                                            \n"
        " bne Unprivileged_Crypto_Image_Auth_Ecdsa_Priv         \n"
        " Privileged_Crypto_Image_Auth_Ecdsa_Priv:              \n"
        "     pop {r0}                                          \n"
        "     b Crypto_Image_Auth_Ecdsa_Priv                    \n"
        " Unprivileged_Crypto_Image_Auth_Ecdsa_Priv:            \n"
        "     pop {r0}                                          \n"
        "     svc %0                                            \n"
        "                                                       \n"
        :
        : "i"(NV_SYSTEM_CALL_Crypto_Image_Auth_Ecdsa)
        : "memory");
#else
    return perform_image_auth_ecdsa_impl(nbootCtx, imageAddress, is_signature_verified, parms);
#endif
    // coverity[cert_msc52_cpp_violation] - Expect no return
}

NV_PRIVILEGED_FUNCTION nv::spdm::crypto::CryptoStatus
                       perform_image_auth_ecdsa_impl(nboot_context_t& nbootCtx,
                                                     uint8_t*         imageAddress,
                                                     nboot_bool_t&    is_signature_verified,
                                                     auth_params_t&   parms)
{
    using namespace nv::spdm::crypto;
    if (NBOOT_ContextInit(&nbootCtx) != kStatus_NBOOT_Success) {
        return CryptoStatus::FailNbootContextInit;
    }
#ifdef CPU_MCXN547VDF
    const auto AuthStatus = NBOOT_ImgAuthenticateEcdsa(
        &nbootCtx, imageAddress, &is_signature_verified, &parms);
#else
    const auto AuthStatus = NBOOT_ImgAuthenticate(
        &nbootCtx, imageAddress + sys::common::SecureAddr, &is_signature_verified, &parms);
#endif

    constexpr uint32_t AuthStatusMask = 0xFFFFFFFF;
    const auto         authStatus     = (uint32_t)(AuthStatus & AuthStatusMask);

    if (authStatus == kStatus_NBOOT_Success && is_signature_verified == kNBOOT_TRUE) {
        return CryptoStatus::Success;
    }
    else {
        return CryptoStatus::FailNbootImageAuthenticate;
    }
}

nv::spdm::crypto::CryptoStatus
authenticate_firmware(const nv::fw_parser::mcu::ParsingFwType InputParseingFwType)
{
    using namespace nv::spdm::crypto;
    // prepare cmpa parameters
    std::array<uint8_t, Sha384HashSize> rotkh{};                          //!< [0x060-0x08f]
    uint32_t                            rokth_usage                 = 0;  //!< [0x054-0x057]
    constexpr uint32_t                  RotKeyHashOffsetInCmpa      = 0x060;
    constexpr uint32_t                  RotKeyHashUsageOffsetInCmpa = 0x054;

    if (nv::flash::Status::Ok
            != nv::flash::Flash::read_cmpa(std::span<uint8_t>(rotkh), RotKeyHashOffsetInCmpa)
        || nv::flash::Status::Ok
               != nv::flash::Flash::read_cmpa(to_span_view(rokth_usage),
                                              RotKeyHashUsageOffsetInCmpa)) {
        return CryptoStatus::FailCmpaAccess;
    }
    // prepare cfpa parameters
    uint32_t secure_fw_version = 0;  //!< [0x008-0x00b]
    uint32_t image_key_revoke  = 0;  //!< [0x018-0x01b]
    if (nv::flash::Status::Ok
            != nv::flash::Flash::read_secure_fw_version(secure_fw_version,
                                                        nv::flash::KeyRollbackSelect::Mcu)
        || nv::flash::Status::Ok
               != nv::flash::Flash::read_key_revoke(image_key_revoke,
                                                    nv::flash::KeyRollbackSelect::Mcu)) {
        return CryptoStatus::FailCfpaAccess;
    }

    // [TODO] Currently disable the security fw version check
    // check security fw version between fw and device
#ifdef CPU_MCXN547VDF
    auto svn_on_firmware = nv::fw_parser::mcu::get_security_version(InputParseingFwType);
    if (!svn_on_firmware) {
        return CryptoStatus::FailParsingFirmware;
    }
    if ((*svn_on_firmware) < secure_fw_version) {
        return CryptoStatus::FailSecurityVersionRollBack;
    }
#endif

    // [TODO] Currently disable the image key revoke check
    // check image key revoke between fw and device
#ifdef CPU_MCXN547VDF
    auto image_key_version_on_firmware = nv::fw_parser::mcu::get_image_signing_key_version(
        InputParseingFwType);
    if (!image_key_version_on_firmware) {
        return CryptoStatus::FailParsingFirmware;
    }
    if ((*image_key_version_on_firmware) < image_key_revoke) {
        return CryptoStatus::FailImageSigningKeyRevoke;
    }
#endif

    // start prepare NBOOT_ImgAuthenticateEcdsa parameter.
    constexpr static uint32_t SocLiefcycleCfgMask    = 0xFF;
    constexpr static uint32_t SocLiefcycleUpperMask  = 0xFFFF0000;
    constexpr static uint32_t SocLiefcycleLowerMask  = 0x0000FFFF;
    constexpr static uint32_t SocLiefcycleUpperShift = 16;

    nboot_bool_t       is_signature_verified = kNBOOT_FALSE;
    auth_params_t      parms                 = {};
    constexpr uint32_t NumRokthUsage         = 4;
    using RokthUsageMaskShift                = std::array<uint32_t, 2>;
    constexpr std::array<RokthUsageMaskShift, NumRokthUsage> RokthUsageMaskShifts{
        RokthUsageMaskShift{  0x7, 0x0},
        RokthUsageMaskShift{ 0x38,   3},
        RokthUsageMaskShift{0x1c0,   6},
        RokthUsageMaskShift{0xE00,   9}
    };
    parms.soc_trustedFirmwareVersion          = secure_fw_version;
    parms.soc_RoTNVM.soc_rootKeyRevocation[0] = kNBOOT_RootKey_Enabled;
    parms.soc_RoTNVM.soc_rootKeyRevocation[1] = kNBOOT_RootKey_Enabled;
    parms.soc_RoTNVM.soc_rootKeyRevocation[2] = kNBOOT_RootKey_Enabled;
    parms.soc_RoTNVM.soc_rootKeyRevocation[3] = kNBOOT_RootKey_Enabled;
    parms.soc_RoTNVM.soc_imageKeyRevocation   = image_key_revoke;

    to_array_view(parms.soc_RoTNVM.soc_rkh).fill(0x0);

    to_array_view(parms.soc_RoTNVM.soc_rkh) = rotkh;

    parms.soc_RoTNVM.soc_rootKeyUsage[0] = (rokth_usage & RokthUsageMaskShifts[0][0])
                                        >> RokthUsageMaskShifts[0][1];
    parms.soc_RoTNVM.soc_rootKeyUsage[1] = (rokth_usage & RokthUsageMaskShifts[1][0])
                                        >> RokthUsageMaskShifts[1][1];
    parms.soc_RoTNVM.soc_rootKeyUsage[2] = (rokth_usage & RokthUsageMaskShifts[2][0])
                                        >> RokthUsageMaskShifts[2][1];
    parms.soc_RoTNVM.soc_rootKeyUsage[3] = (rokth_usage & RokthUsageMaskShifts[3][0])
                                        >> RokthUsageMaskShifts[3][1];

    parms.soc_RoTNVM.soc_numberOfRootKeys     = 4;
    parms.soc_RoTNVM.soc_rootKeyTypeAndLength = kNBOOT_RootKey_Ecdsa_P384;
    parms.soc_RoTNVM.soc_lifecycle            = SYSCON->ELS_AS_CFG0
                                   & SocLiefcycleCfgMask;  // nboot_lc_oemLocked;
    parms.soc_RoTNVM.soc_lifecycle = (((~parms.soc_RoTNVM.soc_lifecycle)
                                       << SocLiefcycleUpperShift)
                                      & SocLiefcycleUpperMask)
                                   | (parms.soc_RoTNVM.soc_lifecycle & SocLiefcycleLowerMask);
    nboot_context_t nbootCtx     = {0u};
    auto            imageAddress = std::bit_cast<uint8_t*>(
        nv::fw_parser::mcu::get_fw_image_address(InputParseingFwType));
    return perform_image_auth_ecdsa(nbootCtx, imageAddress, is_signature_verified, parms);
}

}  // namespace sys::crypto
