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
#include "nv/flash/common.h"
#include "nv/spdm/secure_boot.h"
namespace nv::flash {
struct AuthenticateData
{
    bool                                                is_on_set_data = false;
    nv::spdm::secure_boot::SecureBoot::AuthenticateData active_ap_fw_authenticate_data{};
};
class Npds
{
public:
    Npds() = default;
    Status            get_data(Key key, Data& data);
    Status            set_data(Key key, const Data data);
    AuthenticateData& get_ap_fw_authenticate_data_index(Key key);

private:
    NpdsDataArray _npds_buffer{};
    // 0 is for active ap fw authenticate data
    // 1 is for update ap fw authenticate data
    std::array<AuthenticateData, 2> _ap_fw_authenticate_data{};
};

}  // namespace nv::flash
