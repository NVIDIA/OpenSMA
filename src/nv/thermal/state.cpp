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
#include "nv/thermal/state.h"

namespace nv::thermal {

// Plain .bss, not MPU shared RAM: every accessor today is privileged (thermal
// task writes it; the E368 target ISR reads it) and privileged code reaches
// normal .bss without faulting. Adding an UNPRIVILEGED reader (e.g. a non-priv
// fan / ZPI task) means moving this back to NV_SHARED_BSS, else it
// MemManage-faults at boot.
// NOLINTNEXTLINE(*-non-const-global-variables)
ThermalState ThermalState::s_instance;

ThermalState& ThermalState::inst()
{
    return s_instance;
}

}  // namespace nv::thermal
