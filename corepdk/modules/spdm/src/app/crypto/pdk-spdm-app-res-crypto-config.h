#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>

#include "pdk-spdm-platforms-res-crypto-config-plat.h"

namespace pdk::spdm::app::res::crypto {

// AEAD Support
namespace aead::config {
constexpr bool AEAD_AES_GCM_SUPPORT = pdk::spdm::platforms::res::crypto::aead::config::
    AEAD_AES_GCM_SUPPORT;
constexpr bool AEAD_CHACHA20_POLY1305_SUPPORT = pdk::spdm::platforms::res::crypto::aead::
    config::AEAD_CHACHA20_POLY1305_SUPPORT;
constexpr bool AEAD_SM4_GCM_SUPPORT = pdk::spdm::platforms::res::crypto::aead::config::
    AEAD_SM4_GCM_SUPPORT;
}  // namespace aead::config

// RSA Support
namespace rsa::config {
constexpr bool
    RSA_SSA_SUPPORT = pdk::spdm::platforms::res::crypto::rsa::config::RSA_SSA_SUPPORT;
constexpr bool
    RSA_PSS_SUPPORT = pdk::spdm::platforms::res::crypto::rsa::config::RSA_PSS_SUPPORT;
constexpr bool
    RSA_OAEP_SUPPORT = pdk::spdm::platforms::res::crypto::rsa::config::RSA_OAEP_SUPPORT;
constexpr bool
    RSA_PKCS1_SUPPORT = pdk::spdm::platforms::res::crypto::rsa::config::RSA_PKCS1_SUPPORT;
}  // namespace rsa::config

// Elliptic Curve Primitives Support
namespace ec::config {
constexpr bool ECDSA_SUPPORT = pdk::spdm::platforms::res::crypto::ec::config::ECDSA_SUPPORT;
constexpr bool ECDHE_SUPPORT = pdk::spdm::platforms::res::crypto::ec::config::ECDHE_SUPPORT;
constexpr bool FIPS_MODE     = pdk::spdm::platforms::res::crypto::ec::config::FIPS_MODE;
}  // namespace ec::config

// HASH Support (will take effect with folder hash, hkdf)
namespace hash::config {
constexpr bool SHA256_SUPPORT = pdk::spdm::platforms::res::crypto::hash::config::SHA256_SUPPORT;
constexpr bool SHA384_SUPPORT = pdk::spdm::platforms::res::crypto::hash::config::SHA384_SUPPORT;
constexpr bool SHA512_SUPPORT = pdk::spdm::platforms::res::crypto::hash::config::SHA512_SUPPORT;
constexpr bool
    SHA3_256_SUPPORT = pdk::spdm::platforms::res::crypto::hash::config::SHA3_256_SUPPORT;
constexpr bool
    SHA3_384_SUPPORT = pdk::spdm::platforms::res::crypto::hash::config::SHA3_384_SUPPORT;
constexpr bool
    SHA3_512_SUPPORT = pdk::spdm::platforms::res::crypto::hash::config::SHA3_512_SUPPORT;
constexpr bool
    SM3_256_SUPPORT = pdk::spdm::platforms::res::crypto::hash::config::SM3_256_SUPPORT;
}  // namespace hash::config

// Certificate Support
namespace certificate::config {
constexpr bool CERT_PARSE_SUPPORT = pdk::spdm::platforms::res::crypto::certificate::config::
    CERT_PARSE_SUPPORT;
constexpr bool X509_VERIFY_SUPPORT = pdk::spdm::platforms::res::crypto::certificate::config::
    X509_VERIFY_SUPPORT;
}  // namespace certificate::config

// Diffie-Hellman Support
namespace dh::config {
constexpr bool DH_SUPPORT = pdk::spdm::platforms::res::crypto::dh::config::DH_SUPPORT;
}

}  // namespace pdk::spdm::app::res::crypto