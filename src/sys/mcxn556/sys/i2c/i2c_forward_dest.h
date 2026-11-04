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
#include "nv/i2c/port.h"
#include NV_IPC_CONFIG_H  // per-project nv::ipc::TaskId

namespace sys::i2c {

/// Destination task that owns the I2cForward shadow for a forward whose
/// claimed target lives on \p source_port. The shared weak default (i2c.cpp)
/// returns nv::ipc::TaskId::End, meaning "no forward on this project"; a
/// project that configures a forward provides a strong override **in this
/// namespace** (sys::i2c) - a definition in another namespace will not
/// override the weak symbol. Routing on the source port lets a project map
/// several source buses to different destination tasks in the future.
nv::ipc::TaskId forward_task_id(nv::i2c::Port source_port);

}  // namespace sys::i2c
