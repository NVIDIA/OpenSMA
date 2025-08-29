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
#include <array>
#include <cstdint>

#include "nv/reg_table/dsinterface.h"
#include NV_REG_CONFIG_H

namespace nv::reg_ds {

// Downstream Info
class Info : public reg_table::DsInterface
{
public:
    Info(){};
    bool read(Handle handle, reg_table::DsData& data);
    // writes are not supported
    bool write(Handle handle, reg_table::DsData& data) { return false; };

private:
    bool call_handler(Handle, reg_table::DsData& data);

protected:
    // handlers
    void get_table_id(reg_table::DsData& data);
};

}  // namespace nv::reg_ds
