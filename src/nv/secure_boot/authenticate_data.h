/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES.
 * All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "nv/fw_parser/fw_parser_ap.h"
#include "nv/spdm/crypto_status.h"

namespace nv::secure_boot {

struct AuthenticateData
{
    nv::fw_parser::ap::ApFwMetadata::TbsData ap_metadata_tbs_data{};
    // Default to FailUnknown so value-initialized slots (e.g. NPDS RAM
    // before the SecureBoot FSM has ever produced a result) don't look like
    // a successful auth to consumers such as fw_parser_ap.
    nv::spdm::crypto::CryptoStatus ap_auth_result{nv::spdm::crypto::CryptoStatus::FailUnknown};
};

}  // namespace nv::secure_boot
