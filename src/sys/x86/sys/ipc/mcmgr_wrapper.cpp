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
#include "mcmgr_wrapper.h"
using namespace sys::ipc::task;

nv::ipc::task::Status
Mcmgr::trigger_event_force([[maybe_unused]] nv::ipc::CoreId          core_id,
                           [[maybe_unused]] nv::ipc::task::EventType event_type,
                           [[maybe_unused]] uint16_t                 event_data)
{
    return nv::ipc::task::Status::Ok;
}