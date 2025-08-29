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

namespace nv::flash {

using Data = uint32_t;

enum class Key : uint32_t
{
    NpdsStart      = 0x0,
    NpdsBootReason = NpdsStart,
    NpdsFmcStatus,
    NpdsFmcAuthResult,
    NpdsExtTimestampLSB,
    NpdsExtTimestampMSB,
    NpdsExtTimestampTick,
    NpdsAllowInitBackgroundCopy,
    NpdsBootTimeFromFmcEndToMctpReady,
    NpdsBootReasonOriginal,
    NpdsI2cDynamicAddrForOcpDevice0,
    NpdsI2cDynamicAddrForOcpDevice1,
    NpdsApplicationFaultMagic,
    NpdsCfsr,
    NpdsHfsr,
    NpdsWdtEventBits,
    NpdsEnd,
    NpdsInvalid,
    PdsStart = 0x1000,
    // TBD: Replace it with real data field
    PdsBootableSlot0 = PdsStart,
    PdsBootableSlot1,
    PdsUpdateState,
    PdsUpdateSlot,
    PdscfpaCustomerLastUpdated,
    PdsL3CertificateProgramed,
    PdsDbgTokenNonceValid,      ///< indicates nonce in PDS is valid
    PdsDbgTokenNonce1,          ///< first chunk of 16 byte nonce
    PdsDbgTokenNonce2,          ///< second chunk of 16 byte nonce
    PdsDbgTokenNonce3,          ///< third chunk of 16 byte nonce
    PdsDbgTokenNonce4,          ///< last chunk of 16 byte nonce
    PdsBackgroundSetup,         ///< Background update setup 0: Default 1: enable, 2: disable
    PdsBackgroundSetupOneTime,  ///< Background update setup for one time boot only 0: not setup
                                ///< 2: disable 3: enable
    PdsEnd,
    PdsInvalid,
};

struct [[gnu::packed]] NpdsEntry
{
    bool valid;
    Data data;
};

struct [[gnu::packed]] PdsEntry
{
    Data data;
};

constexpr uint32_t pds_index(Key key)
{
    return static_cast<uint32_t>(key) - static_cast<uint32_t>(Key::PdsStart);
};

constexpr uint32_t npds_index(Key key)
{
    return static_cast<uint32_t>(key) - static_cast<uint32_t>(Key::NpdsStart);
};

constexpr auto NpdsSize = npds_index(Key::NpdsEnd);

constexpr auto PdsSize = pds_index(Key::PdsEnd);

constexpr uint32_t PdsDigestSize = 32;  // TBD: Determine digest size after crypto lib ready
using NpdsDataArray              = std::array<NpdsEntry, NpdsSize>;
using PdsDataArray               = std::array<PdsEntry, PdsSize>;
using PdsDigest                  = std::array<uint8_t, PdsDigestSize>;

}  // namespace nv::flash