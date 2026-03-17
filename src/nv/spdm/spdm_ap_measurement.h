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
#include "nv/spdm/spdm_crypto_helper.h"
#include <array>
#include <stdint.h>
#include "nv/fw_parser/fw_parser_ap.h"

namespace nv::spdm::ap_measurement {

void get_ap_firmware_hash(std::array<uint8_t, nv::spdm::crypto::Sha384HashSize>& hash);

void get_ap_rollback_fuses(uint32_t& rollback_fuse_value);

void get_ap_key_revocation_fuses(uint32_t& key_revocation_fuse_value);

void get_ap_metadata_hash(std::array<uint8_t, nv::spdm::crypto::Sha384HashSize>& hash);

void get_ap_firmware_version(nv::fw_parser::ap::ApFwVersion& firmware_version);

void get_ap_authenticated_status(uint8_t& authenticated_status);

void get_ap_firmware_security_version(uint64_t& security_version);

void get_ap_type(std::array<uint8_t, 4>& ap_type);
}  // namespace nv::spdm::ap_measurement
