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
    FailApMetadataRead,
    FailApImageRead,
    FailApImageHashMismatch,
    FailApPublicKeyMismatch,
    FailApRollbackProtection,
    FailApImageSigningKeyRevoke,
    FailUnknown,       // this FailUnknown should be the last terminal-failure value.
    ApAuthInProgress,  // full authentication is in progress
    FailApNotFound,
    FailPufFail,
    FailAesGcmInvalidInput,
    FailAesGcmSetKey,
    FailAesGcmEncrypt,
    End,
};

}  // namespace nv::spdm::crypto
