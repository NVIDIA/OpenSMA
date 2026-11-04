/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES.
 * All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "nv/spdm/crypto_status.h"

namespace nv::crypto {

inline constexpr size_t EcdhP384CoordinateBytes   = 48U;
inline constexpr size_t EcdhP384PrivateKeyBytes   = EcdhP384CoordinateBytes;
inline constexpr size_t EcdhP384SharedSecretBytes = EcdhP384CoordinateBytes;

using EcdhP384PrivateKey   = std::array<uint8_t, EcdhP384PrivateKeyBytes>;
using EcdhP384SharedSecret = std::array<uint8_t, EcdhP384SharedSecretBytes>;

struct EcdhP384PublicKey
{
    std::array<uint8_t, EcdhP384CoordinateBytes> x{};
    std::array<uint8_t, EcdhP384CoordinateBytes> y{};
};

struct EcdhP384KeyPair
{
    EcdhP384PrivateKey private_key{};
    EcdhP384PublicKey  public_key{};
};

nv::spdm::crypto::CryptoStatus ecdh_p384_generate_key_pair(EcdhP384KeyPair& key_pair);
nv::spdm::crypto::CryptoStatus
ecdh_p384_compute_shared_secret(const EcdhP384PrivateKey& private_key,
                                const EcdhP384PublicKey&  peer_public_key,
                                EcdhP384SharedSecret&     shared_secret);

}  // namespace nv::crypto
