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
#include <assert.h>
#include <fsl_i3c.h>
#include <fsl_i3c_edma.h>

#include "nv/i2c/common.h"

namespace sys::i3c {

class Driver
{
protected:
    uint32_t                 _clock = 25000000UL;
    I3C_Type*                _base  = nullptr;
    i3c_master_config_t      _master_config;
    i3c_master_edma_handle_t _i3c_m_handle;
    edma_handle_t            _tx_edma_handle;
    edma_handle_t            _rx_edma_handle;
    void                     i2c_stop();
    nv::i2c::I2cStatus       to_status(status_t status);
};

}  // namespace sys::i3c
