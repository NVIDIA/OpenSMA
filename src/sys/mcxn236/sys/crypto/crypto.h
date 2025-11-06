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
#include "fsl_nboot.h"
#include "nv/spdm/spdm_crypto_helper.h"
#include "nv/fw_parser/fw_parser_mcu.h"

namespace sys::crypto {

nv::spdm::crypto::CryptoStatus
authenticate_firmware(const nv::fw_parser::mcu::ParsingFwType InputParseingFwType);

nv::spdm::crypto::CryptoStatus perform_image_auth_ecdsa(nboot_context_t& nbootCtx,
                                                        uint8_t*         imageAddress,
                                                        nboot_bool_t&    is_signature_verified,
                                                        nboot_img_auth_ecdsa_parms_t& parms);
nv::spdm::crypto::CryptoStatus
perform_image_auth_ecdsa_svc(nboot_context_t&              nbootCtx,
                             uint8_t*                      imageAddress,
                             nboot_bool_t&                 is_signature_verified,
                             nboot_img_auth_ecdsa_parms_t& parms);
nv::spdm::crypto::CryptoStatus
perform_image_auth_ecdsa_impl(nboot_context_t&              nbootCtx,
                              uint8_t*                      imageAddress,
                              nboot_bool_t&                 is_signature_verified,
                              nboot_img_auth_ecdsa_parms_t& parms);

}  // namespace sys::crypto
