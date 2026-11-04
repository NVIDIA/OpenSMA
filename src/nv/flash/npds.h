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
#include "nv/secure_boot/authenticate_data.h"
namespace nv::flash {
struct AuthenticateData
{
    bool                              is_on_set_data = false;
    nv::secure_boot::AuthenticateData active_ap_fw_authenticate_data{};
};
class Npds
{
public:
    Npds() = default;
    Status get_data(Key key, Data& data);
    Status set_data(Key key, const Data data);

    AuthenticateData& get_ap_fw_authenticate_data_index(Key key);

    static constexpr size_t AuthDataSlots = nv::vrot::ApList.empty()
                                              ? 0U
                                              : static_cast<size_t>(
                                                    nv::vrot::ApList[0].slot_count + 1U);

    static constexpr bool has_ap_auth_data_record(Key key)
    {
        const auto ap0 = nv::vrot::find_ap(nv::vrot::ApId::AP0, nv::vrot::ApList);
        const auto ap1 = nv::vrot::find_ap(nv::vrot::ApId::AP1, nv::vrot::ApList);
        switch (key) {
            case Key::NpdsActiveApFwAuthenticateData:
            case Key::NpdsUpdateApFwAuthenticateData   : return ap0.has_value();
            case Key::NpdsInactiveApFwAuthenticateData : return ap0 && ap0->slot_count > 1U;
            case Key::NpdsActiveAp1FwAuthenticateData  :
            case Key::NpdsUpdateAp1FwAuthenticateData  : return ap1.has_value();
            case Key::NpdsInactiveAp1FwAuthenticateData: return ap1 && ap1->slot_count > 1U;
            default                                    : return false;
        }
    }

private:
    NpdsDataArray                               _npds_buffer{};
    std::array<AuthenticateData, AuthDataSlots> _ap_fw_authenticate_data{};
    std::array<AuthenticateData, AuthDataSlots> _ap1_fw_authenticate_data{};
};

}  // namespace nv::flash
