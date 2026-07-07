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

#include <type_traits>

#include "nv/common/preproc.h"
#include "nv/vcom/vruart/cdc_bridge.h"
#include "nv/vcom/vruart/common.h"
#include "nv/vcom/vruart/lstp_bridge.h"

#include NV_IPC_CONFIG_H

namespace nv::vruart {

#ifdef USB_CONFIG_UART_BRIDGE
using Bridge = std::
    conditional_t<nv::ipc::UartOverUsbProtocol == Protocol::Lstp, LstpBridge, CdcBridge>;
#else
// Set a default to avoid forcing each config.h to define UartOverUsbProtocol
using Bridge = LstpBridge;
#endif

}  // namespace nv::vruart
