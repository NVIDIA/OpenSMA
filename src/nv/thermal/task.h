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

#include "nv/ipc/task.h"

namespace nv::thermal {

/**
 * Thermal task - the sole producer of ThermalState (the shared "whiteboard").
 *
 * It wakes periodically, reads the board temperature sensors, and publishes a
 * consistent snapshot for the consumers (the E368-facing I2C task, fan control,
 * ZPI) to read. It is the only writer of the measured-temperature buffers.
 *
 * Scope today: read the TMP468 (local + 8 remote channels). GPU Tsense is a
 * separate source (normal-mode I2CS read / IST-mode SMBPBI) and is left as a
 * TODO until its access method is confirmed.
 */
class Task : public ipc::Task
{
public:
    Task();

    [[noreturn]] void main();

    static void  make();
    static void  entrypoint(void* params);
    static Task& inst();

private:
    /// One measurement cycle: read every sensor, fill the back buffer, publish.
    void poll_once();
};

}  // namespace nv::thermal
