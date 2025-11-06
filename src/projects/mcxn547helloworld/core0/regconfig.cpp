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

// TODO: Auto generate this file with compiler script from <platform>.yaml - GFWLYNT1-544

#include "nv/reg_table/table.h"

namespace nv::reg_table {

// Register Table definitions
Table::Table()
: _tbl({
      {
       COMMON_TABLE_ENTRIES{TableEntry::ThermOvertN,
                               0,
                               reg_ds::Handle::GpioThermOvertN,
                               ControlBitsRas,
                               DsApiType::Gpio,
                               0},
       }
})
{}

}  // namespace nv::reg_table

namespace nv::reg_ds {

// maps the GpioSetup offset in config.h to register table handles for GPIO
Gpio::Gpio() : _gpio({{0}}) {}

}  // namespace nv::reg_ds
