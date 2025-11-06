#include "crypto/hkdf/pdk-spdm-app-res-crypto-hkdf.h"

namespace pdk::spdm::app::res::crypto::hkdf {

#ifdef __cplusplus
extern "C" {
#endif

/*=====================================================================================
 *    Key Derivation Function Primitives
 *=====================================================================================*/

/**
 * Derive SHA-256 HMAC-based Extract key Derivation Function (HKDF).
 *
 * @param[in]   key           Pointer to the user-supplied key.
 * @param[in]   key_size      Key size in bytes.
 * @param[in]   salt          Pointer to the salt value.
 * @param[in]   salt_size     Salt size in bytes.
 * @param[out]  prk_out       Pointer to buffer to receive prk value.
 * @param[in]   prk_out_size  Size of prk bytes to generate.
 *
 * @retval true   Hkdf generated successfully.
 * @retval false  Hkdf generation failed.
 **/
bool libspdm_hkdf_sha256_extract(const uint8_t* key,
                                 size_t         key_size,
                                 const uint8_t* salt,
                                 size_t         salt_size,
                                 uint8_t*       prk_out,
                                 size_t         prk_out_size)
{
    if constexpr (hash::config::SHA256_SUPPORT) {
        return pdk::spdm::platforms::res::crypto::hkdf::hkdf_sha256_extract(
            key, key_size, salt, salt_size, prk_out, prk_out_size);
    }
    return false;
}

/**
 * Derive SHA256 HMAC-based Expand key Derivation Function (HKDF).
 *
 * @param[in]   prk        Pointer to the user-supplied key.
 * @param[in]   prk_size   Key size in bytes.
 * @param[in]   info       Pointer to the application specific info.
 * @param[in]   info_size  Info size in bytes.
 * @param[out]  out        Pointer to buffer to receive hkdf value.
 * @param[in]   out_size   Size of hkdf bytes to generate.
 *
 * @retval true   Hkdf generated successfully.
 * @retval false  Hkdf generation failed.
 **/
bool libspdm_hkdf_sha256_expand(const uint8_t* prk,
                                size_t         prk_size,
                                const uint8_t* info,
                                size_t         info_size,
                                uint8_t*       out,
                                size_t         out_size)
{
    if constexpr (hash::config::SHA256_SUPPORT) {
        return pdk::spdm::platforms::res::crypto::hkdf::hkdf_sha256_expand(
            prk, prk_size, info, info_size, out, out_size);
    }
    return false;
}

/**
 * Derive SHA384 HMAC-based Extract key Derivation Function (HKDF).
 *
 * @param[in]   key           Pointer to the user-supplied key.
 * @param[in]   key_size      Key size in bytes.
 * @param[in]   salt          Pointer to the salt value.
 * @param[in]   salt_size     Salt size in bytes.
 * @param[out]  prk_out       Pointer to buffer to receive hkdf value.
 * @param[in]   prk_out_size  Size of hkdf bytes to generate.
 *
 * @retval true   Hkdf generated successfully.
 * @retval false  Hkdf generation failed.
 **/
bool libspdm_hkdf_sha384_extract(const uint8_t* key,
                                 size_t         key_size,
                                 const uint8_t* salt,
                                 size_t         salt_size,
                                 uint8_t*       prk_out,
                                 size_t         prk_out_size)
{
    if constexpr (hash::config::SHA384_SUPPORT) {
        return pdk::spdm::platforms::res::crypto::hkdf::hkdf_sha384_extract(
            key, key_size, salt, salt_size, prk_out, prk_out_size);
    }
    return false;
}

/**
 * Derive SHA384 HMAC-based Expand key Derivation Function (HKDF).
 *
 * @param[in]   prk        Pointer to the user-supplied key.
 * @param[in]   prk_size   Key size in bytes.
 * @param[in]   info       Pointer to the application specific info.
 * @param[in]   info_size  Info size in bytes.
 * @param[out]  out        Pointer to buffer to receive hkdf value.
 * @param[in]   out_size   Size of hkdf bytes to generate.
 *
 * @retval true   Hkdf generated successfully.
 * @retval false  Hkdf generation failed.
 **/
bool libspdm_hkdf_sha384_expand(const uint8_t* prk,
                                size_t         prk_size,
                                const uint8_t* info,
                                size_t         info_size,
                                uint8_t*       out,
                                size_t         out_size)
{
    if constexpr (hash::config::SHA384_SUPPORT) {
        return pdk::spdm::platforms::res::crypto::hkdf::hkdf_sha384_expand(
            prk, prk_size, info, info_size, out, out_size);
    }
    return false;
}

/**
 * Derive SHA512 HMAC-based Extract key Derivation Function (HKDF).
 *
 * @param[in]   key           Pointer to the user-supplied key.
 * @param[in]   key_size      Key size in bytes.
 * @param[in]   salt          Pointer to the salt value.
 * @param[in]   salt_size     Salt size in bytes.
 * @param[out]  prk_out       Pointer to buffer to receive hkdf value.
 * @param[in]   prk_out_size  Size of hkdf bytes to generate.
 *
 * @retval true   Hkdf generated successfully.
 * @retval false  Hkdf generation failed.
 **/
bool libspdm_hkdf_sha512_extract(const uint8_t* key,
                                 size_t         key_size,
                                 const uint8_t* salt,
                                 size_t         salt_size,
                                 uint8_t*       prk_out,
                                 size_t         prk_out_size)
{
    if constexpr (hash::config::SHA512_SUPPORT) {
        return pdk::spdm::platforms::res::crypto::hkdf::hkdf_sha512_extract(
            key, key_size, salt, salt_size, prk_out, prk_out_size);
    }
    return false;
}

/**
 * Derive SHA512 HMAC-based Expand key Derivation Function (HKDF).
 *
 * @param[in]   prk        Pointer to the user-supplied key.
 * @param[in]   prk_size   Key size in bytes.
 * @param[in]   info       Pointer to the application specific info.
 * @param[in]   info_size  Info size in bytes.
 * @param[out]  out        Pointer to buffer to receive hkdf value.
 * @param[in]   out_size   Size of hkdf bytes to generate.
 *
 * @retval true   Hkdf generated successfully.
 * @retval false  Hkdf generation failed.
 **/
bool libspdm_hkdf_sha512_expand(const uint8_t* prk,
                                size_t         prk_size,
                                const uint8_t* info,
                                size_t         info_size,
                                uint8_t*       out,
                                size_t         out_size)
{
    if constexpr (hash::config::SHA512_SUPPORT) {
        return pdk::spdm::platforms::res::crypto::hkdf::hkdf_sha512_expand(
            prk, prk_size, info, info_size, out, out_size);
    }
    return false;
}

/**
 * Derive SHA3_256 HMAC-based Extract key Derivation Function (HKDF).
 *
 * @param[in]   key           Pointer to the user-supplied key.
 * @param[in]   key_size      Key size in bytes.
 * @param[in]   salt          Pointer to the salt value.
 * @param[in]   salt_size     Salt size in bytes.
 * @param[out]  prk_out       Pointer to buffer to receive hkdf value.
 * @param[in]   prk_out_size  Size of hkdf bytes to generate.
 *
 * @retval true   Hkdf generated successfully.
 * @retval false  Hkdf generation failed.
 **/
bool libspdm_hkdf_sha3_256_extract(const uint8_t* key,
                                   size_t         key_size,
                                   const uint8_t* salt,
                                   size_t         salt_size,
                                   uint8_t*       prk_out,
                                   size_t         prk_out_size)
{
    if constexpr (hash::config::SHA3_256_SUPPORT) {
        return pdk::spdm::platforms::res::crypto::hkdf::hkdf_sha3_256_extract(
            key, key_size, salt, salt_size, prk_out, prk_out_size);
    }
    return false;
}

/**
 * Derive SHA3_256 HMAC-based Expand key Derivation Function (HKDF).
 *
 * @param[in]   prk        Pointer to the user-supplied key.
 * @param[in]   prk_size   Key size in bytes.
 * @param[in]   info       Pointer to the application specific info.
 * @param[in]   info_size  Info size in bytes.
 * @param[out]  out        Pointer to buffer to receive hkdf value.
 * @param[in]   out_size   Size of hkdf bytes to generate.
 *
 * @retval true   Hkdf generated successfully.
 * @retval false  Hkdf generation failed.
 **/
bool libspdm_hkdf_sha3_256_expand(const uint8_t* prk,
                                  size_t         prk_size,
                                  const uint8_t* info,
                                  size_t         info_size,
                                  uint8_t*       out,
                                  size_t         out_size)
{
    if constexpr (hash::config::SHA3_256_SUPPORT) {
        return pdk::spdm::platforms::res::crypto::hkdf::hkdf_sha3_256_expand(
            prk, prk_size, info, info_size, out, out_size);
    }
    return false;
}

/**
 * Derive SHA3_384 HMAC-based Extract key Derivation Function (HKDF).
 *
 * @param[in]   key           Pointer to the user-supplied key.
 * @param[in]   key_size      Key size in bytes.
 * @param[in]   salt          Pointer to the salt value.
 * @param[in]   salt_size     Salt size in bytes.
 * @param[out]  prk_out       Pointer to buffer to receive hkdf value.
 * @param[in]   prk_out_size  Size of hkdf bytes to generate.
 *
 * @retval true   Hkdf generated successfully.
 * @retval false  Hkdf generation failed.
 **/
bool libspdm_hkdf_sha3_384_extract(const uint8_t* key,
                                   size_t         key_size,
                                   const uint8_t* salt,
                                   size_t         salt_size,
                                   uint8_t*       prk_out,
                                   size_t         prk_out_size)
{
    if constexpr (hash::config::SHA3_384_SUPPORT) {
        return pdk::spdm::platforms::res::crypto::hkdf::hkdf_sha3_384_extract(
            key, key_size, salt, salt_size, prk_out, prk_out_size);
    }
    return false;
}

/**
 * Derive SHA3_384 HMAC-based Expand key Derivation Function (HKDF).
 *
 * @param[in]   prk        Pointer to the user-supplied key.
 * @param[in]   prk_size   Key size in bytes.
 * @param[in]   info       Pointer to the application specific info.
 * @param[in]   info_size  Info size in bytes.
 * @param[out]  out        Pointer to buffer to receive hkdf value.
 * @param[in]   out_size   Size of hkdf bytes to generate.
 *
 * @retval true   Hkdf generated successfully.
 * @retval false  Hkdf generation failed.
 **/
bool libspdm_hkdf_sha3_384_expand(const uint8_t* prk,
                                  size_t         prk_size,
                                  const uint8_t* info,
                                  size_t         info_size,
                                  uint8_t*       out,
                                  size_t         out_size)
{
    if constexpr (hash::config::SHA3_384_SUPPORT) {
        return pdk::spdm::platforms::res::crypto::hkdf::hkdf_sha3_384_expand(
            prk, prk_size, info, info_size, out, out_size);
    }
    return false;
}

/**
 * Derive SHA3_512 HMAC-based Extract key Derivation Function (HKDF).
 *
 * @param[in]   key           Pointer to the user-supplied key.
 * @param[in]   key_size      Key size in bytes.
 * @param[in]   salt          Pointer to the salt value.
 * @param[in]   salt_size     Salt size in bytes.
 * @param[out]  prk_out       Pointer to buffer to receive hkdf value.
 * @param[in]   prk_out_size  Size of hkdf bytes to generate.
 *
 * @retval true   Hkdf generated successfully.
 * @retval false  Hkdf generation failed.
 **/
bool libspdm_hkdf_sha3_512_extract(const uint8_t* key,
                                   size_t         key_size,
                                   const uint8_t* salt,
                                   size_t         salt_size,
                                   uint8_t*       prk_out,
                                   size_t         prk_out_size)
{
    if constexpr (hash::config::SHA3_512_SUPPORT) {
        return pdk::spdm::platforms::res::crypto::hkdf::hkdf_sha3_512_extract(
            key, key_size, salt, salt_size, prk_out, prk_out_size);
    }
    return false;
}

/**
 * Derive SHA3_512 HMAC-based Expand key Derivation Function (HKDF).
 *
 * @param[in]   prk        Pointer to the user-supplied key.
 * @param[in]   prk_size   Key size in bytes.
 * @param[in]   info       Pointer to the application specific info.
 * @param[in]   info_size  Info size in bytes.
 * @param[out]  out        Pointer to buffer to receive hkdf value.
 * @param[in]   out_size   Size of hkdf bytes to generate.
 *
 * @retval true   Hkdf generated successfully.
 * @retval false  Hkdf generation failed.
 **/
bool libspdm_hkdf_sha3_512_expand(const uint8_t* prk,
                                  size_t         prk_size,
                                  const uint8_t* info,
                                  size_t         info_size,
                                  uint8_t*       out,
                                  size_t         out_size)
{
    if constexpr (hash::config::SHA3_512_SUPPORT) {
        return pdk::spdm::platforms::res::crypto::hkdf::hkdf_sha3_512_expand(
            prk, prk_size, info, info_size, out, out_size);
    }
    return false;
}

/**
 * Derive SM3_256 HMAC-based Extract key Derivation Function (HKDF).
 *
 * @param[in]   key           Pointer to the user-supplied key.
 * @param[in]   key_size      Key size in bytes.
 * @param[in]   salt          Pointer to the salt value.
 * @param[in]   salt_size     Salt size in bytes.
 * @param[out]  prk_out       Pointer to buffer to receive hkdf value.
 * @param[in]   prk_out_size  Size of hkdf bytes to generate.
 *
 * @retval true   Hkdf generated successfully.
 * @retval false  Hkdf generation failed.
 **/
bool libspdm_hkdf_sm3_256_extract(const uint8_t* key,
                                  size_t         key_size,
                                  const uint8_t* salt,
                                  size_t         salt_size,
                                  uint8_t*       prk_out,
                                  size_t         prk_out_size)
{
    if constexpr (hash::config::SM3_256_SUPPORT) {
        return pdk::spdm::platforms::res::crypto::hkdf::hkdf_sm3_256_extract(
            key, key_size, salt, salt_size, prk_out, prk_out_size);
    }
    return false;
}

/**
 * Derive SM3_256 HMAC-based Expand key Derivation Function (HKDF).
 *
 * @param[in]   prk        Pointer to the user-supplied key.
 * @param[in]   prk_size   Key size in bytes.
 * @param[in]   info       Pointer to the application specific info.
 * @param[in]   info_size  Info size in bytes.
 * @param[out]  out        Pointer to buffer to receive hkdf value.
 * @param[in]   out_size   Size of hkdf bytes to generate.
 *
 * @retval true   Hkdf generated successfully.
 * @retval false  Hkdf generation failed.
 **/
bool libspdm_hkdf_sm3_256_expand(const uint8_t* prk,
                                 size_t         prk_size,
                                 const uint8_t* info,
                                 size_t         info_size,
                                 uint8_t*       out,
                                 size_t         out_size)
{
    if constexpr (hash::config::SM3_256_SUPPORT) {
        return pdk::spdm::platforms::res::crypto::hkdf::hkdf_sm3_256_expand(
            prk, prk_size, info, info_size, out, out_size);
    }
    return false;
}

#ifdef __cplusplus
}
#endif

}  // namespace pdk::spdm::app::res::crypto::hkdf
