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
#include "api_driver.h"
#include "cfpa_driver.h"
#include "fccob.h"
#include "iap_driver.h"
#include "otp.h"
namespace sys::flash {

constexpr uint32_t BromApiAlignLength = 0x80;

class Driver
{
public:
    enum class ApiSelect
    {
        Begin,
        Flash = Begin,
        Iap,
        End,
    };

protected:
    ApiSelect  api_select{};
    ApiDriver  flash_api_driver{};
    CfpaDriver cfpa_driver{};
    IapDriver  iap_driver{};
    OtpDriver  otp_driver{};
};

}  // namespace sys::flash
