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

#include <stdint.h>

namespace pdk::spdm::app::res::code {
enum class SpdmResponseCode : uint8_t
{
    Digests                 = 0x01,
    Certificate             = 0x02,
    ChallengeAuth           = 0x03,
    Version                 = 0x04,
    ChunkSendAck            = 0x05,
    ChunkResponse           = 0x06,
    EndpointInfo            = 0x07,
    Measurements            = 0x60,
    Capabilities            = 0x61,
    SupportedEventTypes     = 0x62,
    Algorithms              = 0x63,
    KeyExchangeRsp          = 0x64,
    FinishRsp               = 0x65,
    PskExchangeRsp          = 0x66,
    PskFinishRsp            = 0x67,
    HeartbeatAck            = 0x68,
    KeyUpdateAck            = 0x69,
    EncapsulatedRequest     = 0x6a,
    EncapsulatedResponseAck = 0x6b,
    EndSessionAck           = 0x6c,
    Csr                     = 0x6d,
    SetCertificateRsp       = 0x6e,
    MeasurementExtensionLog = 0x6f,
    SubscribeEventTypesAck  = 0x70,
    EventAck                = 0x71,
    KeyPairInfo             = 0x7c,
    SetKeyPairInfoAck       = 0x7d,
    VendorDefinedResponse   = 0x7e,
    Error                   = 0x7f
};
}
