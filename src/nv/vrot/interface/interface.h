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
#include "nv/vrot/interface/types.h"
#include "nv/spdm/crypto_status.h"

namespace nv::vrot {

// ---------------------------------------------------------------------------
// Public API. Each function takes an ApInfo that identifies which AP to operate
// on. The dispatch layer routes supported APs to the matching platform Ops.
// ---------------------------------------------------------------------------

// SecureBoot -> platform operations.
ApOpErrCode hold_reset(const ApInfo& ap);
ApOpErrCode pre_authenticate(const ApInfo& ap);
ApOpErrCode post_authenticate(const ApInfo& ap, nv::spdm::crypto::CryptoStatus result);
ApOpErrCode release_reset(const ApInfo& ap);
ApOpErrCode check_booted(const ApInfo& ap);

ApOpErrCode fw_update_prepare(const ApInfo& ap);
ApOpErrCode fw_update_write(const ApInfo& ap, uint32_t addr, std::span<const uint8_t> data);
ApOpErrCode fw_update_callback(const ApInfo& ap, nv::spdm::crypto::CryptoStatus result);

ApOpErrCode read_flash(const ApInfo& ap, uint32_t start_address, std::span<uint8_t> data);

ApOpErrCode set_debug_token_feature(const ApInfo& ap, DebugTokenFeature feature, bool enable);

// Platform -> SecureBoot notifications.
void notify_ap_reset(const ApInfo& ap);

}  // namespace nv::vrot
