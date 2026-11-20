/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES.
 * All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <cstdint>

namespace nv::spdm::crypto {

enum class CryptoStatus : uint8_t
{
    Success = 0,
    FailEcdsaSign,
    FailGetPriKey,
    FailLoadEcdsaContext,
    FailHashStart,
    FailHashUpdate,
    FailHashCalc,
    FailRandomGen,
    FailSignatureBufferLength,
    FailSignatureVerify,
    FailCfpaAccess,
    FailCmpaAccess,
    FailNbootContextInit,
    FailNbootImageAuthenticate,
    FailSecurityVersionRollBack,
    FailImageSigningKeyRevoke,
    FailEfuseAccess,
    FailSendToSpdm,
    FailParsingFirmware,
    // 19-24 are reserved.
    FailUnknown = 25,
    // 26-36 are reserved.
    End = 37,
};

}  // namespace nv::spdm::crypto
