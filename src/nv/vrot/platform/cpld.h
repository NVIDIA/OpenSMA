/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES.
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

#include <cstdint>
#include <span>
#include "nv/fw_parser/fw_parser_ap.h"
#include "nv/vrot/interface/types.h"
#include "nv/spdm/crypto_status.h"

namespace nv::vrot {

struct CpldOps
{
    static ApOpErrCode hold_reset(const ApInfo& ap);
    static ApOpErrCode pre_authenticate(const ApInfo& ap);
    static ApOpErrCode post_authenticate(const ApInfo&                  ap,
                                         nv::spdm::crypto::CryptoStatus result);
    static ApOpErrCode release_reset(const ApInfo& ap);
    static ApOpErrCode check_booted(const ApInfo& ap);
    static ApOpErrCode read_metadata(const ApInfo&      ap,
                                     uint8_t            slot,
                                     uint32_t           metadata_offset,
                                     std::span<uint8_t> data);
    static ApOpErrCode read_fw_data(const ApInfo&      ap,
                                    uint8_t            slot,
                                    uint32_t           fw_data_offset,
                                    std::span<uint8_t> data);
    static ApOpErrCode write_metadata(const ApInfo&            ap,
                                      uint8_t                  slot,
                                      uint32_t                 metadata_offset,
                                      std::span<const uint8_t> data);
    static ApOpErrCode write_fw_data(const ApInfo&            ap,
                                     uint8_t                  slot,
                                     uint32_t                 fw_data_offset,
                                     std::span<const uint8_t> data,
                                     bool                     background_copy);

    static ApOpErrCode fw_update_prepare(const ApInfo& ap);
    static ApOpErrCode fw_update_callback(const ApInfo&                  ap,
                                          nv::spdm::crypto::CryptoStatus result);

    static ApOpErrCode
    set_debug_token_feature(const ApInfo& ap, DebugTokenFeature feature, bool enable);

    // PLDM-facing platform behavior. fw_update_pds / get_update_state /
    // metadata parsing are AP-generic and live in pldm_wrap.cpp.
    // request_authentication is kept as a platform hook so future APs can
    // override or refuse authentication (e.g. an AP that uses a different
    // attestation mechanism, or a project that wants to gate auth on
    // additional preconditions).
    static ApOpErrCode request_authentication(const ApInfo& ap, uint8_t& auth_request_id);
    static uint8_t     get_write_fail_retry(const ApInfo& ap);
};

}  // namespace nv::vrot
