#pragma once

namespace pdk::spdm::platforms::res::crypto {

// AEAD Support
namespace aead::config {
constexpr bool AEAD_AES_GCM_SUPPORT           = false;
constexpr bool AEAD_CHACHA20_POLY1305_SUPPORT = false;
constexpr bool AEAD_SM4_GCM_SUPPORT           = false;
}  // namespace aead::config
// RSA Support
namespace rsa::config {
constexpr bool RSA_SSA_SUPPORT   = false;
constexpr bool RSA_PSS_SUPPORT   = false;
constexpr bool RSA_OAEP_SUPPORT  = false;
constexpr bool RSA_PKCS1_SUPPORT = false;
}  // namespace rsa::config

// Elliptic Curve Support
namespace ec::config {
constexpr bool ECDSA_SUPPORT = true;
constexpr bool ECDHE_SUPPORT = false;
constexpr bool FIPS_MODE     = false;
}  // namespace ec::config

// Hash Support
namespace hash::config {
constexpr bool SHA256_SUPPORT   = false;
constexpr bool SHA384_SUPPORT   = true;
constexpr bool SHA512_SUPPORT   = false;
constexpr bool SHA3_256_SUPPORT = false;
constexpr bool SHA3_384_SUPPORT = false;
constexpr bool SHA3_512_SUPPORT = false;
constexpr bool SM3_256_SUPPORT  = false;
}  // namespace hash::config

// Certificate Support
namespace certificate::config {
constexpr bool CERT_PARSE_SUPPORT  = false;
constexpr bool X509_VERIFY_SUPPORT = false;
}  // namespace certificate::config

// Diffie-Hellman Support
namespace dh::config {
constexpr bool DH_SUPPORT = false;
}

}  // namespace pdk::spdm::platforms::res::crypto