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
#include "nv/flash/npds.h"

#include <algorithm>

#include "nv/bootloader.h"
#include "nv/nv.h"
using namespace nv::flash;

Status Npds::get_data(Key key, Data& data)
{
    const uint32_t Index = npds_index(key);
    if (!_npds_buffer.at(Index).valid) {
        return Status::Error;
    }
    data = _npds_buffer.at(Index).data;
    // nv::info("Npds::get_data data:%d index:%d\n", data, index);
    return Status::Ok;
}

Status Npds::set_data(Key key, const Data data)
{
    const uint32_t Index         = npds_index(key);
    _npds_buffer.at(Index).valid = true;
    _npds_buffer.at(Index).data  = data;
    // nv::info("Npds::set_data data:%d index:%d\n", data, index);
    return Status::Ok;
}

AuthenticateData& Npds::get_ap_fw_authenticate_data_index(Key key)
{
    if (key == Key::NpdsActiveApFwAuthenticateData) {
        return _ap_fw_authenticate_data[0];
    }
    else {
        return _ap_fw_authenticate_data[1];
    }
}