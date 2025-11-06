#include "crypto/hash/pdk-spdm-app-res-crypto-hash.h"

namespace pdk::spdm::app::res::crypto::hash {

size_t find_hash_size_from_measurement_hash_algo(
    const pdk::spdm::app::res::algorithms::MeasurementHashAlgo measurement_hash_algo)
{
    using namespace pdk::spdm::app::res::algorithms;
    size_t ret_len = 0;
    switch (measurement_hash_algo) {
        case MeasurementHashAlgo::NotSupport      : ret_len = 0; break;
        case MeasurementHashAlgo::RawBitStreamOnly: ret_len = 0; break;
        case MeasurementHashAlgo::TpmAlgSha256:
            ret_len = HashAlgoDigestSize::TpmAlgSha256Size;
            break;
        case MeasurementHashAlgo::TpmAlgSha384:
            ret_len = HashAlgoDigestSize::TpmAlgSha384Size;
            break;
        case MeasurementHashAlgo::TpmAlgSha512:
            ret_len = HashAlgoDigestSize::TpmAlgSha512Size;
            break;
        case MeasurementHashAlgo::TpmAlgSha3_256:
            ret_len = HashAlgoDigestSize::TpmAlgSha3_256Size;
            break;
        case MeasurementHashAlgo::TpmAlgSha3_384:
            ret_len = HashAlgoDigestSize::TpmAlgSha3_384Size;
            break;
        case MeasurementHashAlgo::TpmAlgSha3_512:
            ret_len = HashAlgoDigestSize::TpmAlgSha3_512Size;
            break;
        default: ret_len = 0; break;
    }
    return ret_len;
}
size_t find_hash_size_from_base_hash_algo(
    const pdk::spdm::app::res::algorithms::BaseHashSel base_hash_algo)
{
    using namespace pdk::spdm::app::res::algorithms;
    switch (base_hash_algo) {
        case BaseHashSel::NotSupport    : return 0;
        case BaseHashSel::TpmAlgSha256  : return HashAlgoDigestSize::TpmAlgSha256Size;
        case BaseHashSel::TpmAlgSha384  : return HashAlgoDigestSize::TpmAlgSha384Size;
        case BaseHashSel::TpmAlgSha512  : return HashAlgoDigestSize::TpmAlgSha512Size;
        case BaseHashSel::TpmAlgSha3_256: return HashAlgoDigestSize::TpmAlgSha3_256Size;
        case BaseHashSel::TpmAlgSha3_384: return HashAlgoDigestSize::TpmAlgSha3_384Size;
        case BaseHashSel::TpmAlgSha3_512: return HashAlgoDigestSize::TpmAlgSha3_512Size;
        case BaseHashSel::TpmAlgSm3_256 : return HashAlgoDigestSize::TpmAlgSm3_256Size;
        default                         : return 0;
    }
}

bool hash_all_from_base_hash_algo(const uint32_t           base_hash_algo,
                                  std::span<const uint8_t> message,
                                  std::span<uint8_t>       hash_data)
{
    using namespace pdk::spdm::app::res::algorithms;
    if (hash_data.size()
        < find_hash_size_from_base_hash_algo(static_cast<BaseHashSel>(base_hash_algo))) {
        return false;
    }
    if (base_hash_algo == std::to_underlying(BaseHashSel::NotSupport)) {
        return false;
    }
    else if (base_hash_algo == std::to_underlying(BaseHashSel::TpmAlgSha256)) {
        return libspdm_sha256_hash_all(message.data(), message.size(), hash_data.data());
    }
    else if (base_hash_algo == std::to_underlying(BaseHashSel::TpmAlgSha384)) {
        return libspdm_sha384_hash_all(message.data(), message.size(), hash_data.data());
    }
    else if (base_hash_algo == std::to_underlying(BaseHashSel::TpmAlgSha512)) {
        return libspdm_sha512_hash_all(message.data(), message.size(), hash_data.data());
    }
    else if (base_hash_algo == std::to_underlying(BaseHashSel::TpmAlgSha3_256)) {
        return libspdm_sha3_256_hash_all(message.data(), message.size(), hash_data.data());
    }
    else if (base_hash_algo == std::to_underlying(BaseHashSel::TpmAlgSha3_384)) {
        return libspdm_sha3_384_hash_all(message.data(), message.size(), hash_data.data());
    }
    else if (base_hash_algo == std::to_underlying(BaseHashSel::TpmAlgSha3_512)) {
        return libspdm_sha3_512_hash_all(message.data(), message.size(), hash_data.data());
    }
    else if (base_hash_algo == std::to_underlying(BaseHashSel::TpmAlgSm3_256)) {
        return libspdm_sm3_256_hash_all(message.data(), message.size(), hash_data.data());
    }
    else {
        return false;
    }
}

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Allocates and initializes one HASH_CTX context for subsequent SHA-256 use.
 *
 * @return  Pointer to the HASH_CTX context that has been initialized.
 *          If the allocations fails, sha256_new() returns NULL. *
 **/
void* libspdm_sha256_new(void)
{
    if constexpr (hash::config::SHA256_SUPPORT) {
        return pdk::spdm::platforms::res::crypto::hash::sha256_new();
    }
    return nullptr;
}

/**
 * Release the specified HASH_CTX context.
 *
 * @param[in]  sha256_context  Pointer to the HASH_CTX context to be released.
 **/
void libspdm_sha256_free(void* sha256_context)
{
    if constexpr (hash::config::SHA256_SUPPORT) {
        pdk::spdm::platforms::res::crypto::hash::sha256_free(sha256_context);
    }
}

/**
 * Initializes user-supplied memory pointed to by sha256_context as SHA-256 hash context for
 * subsequent use.
 *
 * If sha256_context is NULL, then return false.
 *
 * @param[out]  sha256_context  Pointer to SHA-256 context being initialized.
 *
 * @retval true   SHA-256 context initialization succeeded.
 * @retval false  SHA-256 context initialization failed.
 **/
bool libspdm_sha256_init(void* sha256_context)
{
    if constexpr (hash::config::SHA256_SUPPORT) {
        return pdk::spdm::platforms::res::crypto::hash::sha256_init(sha256_context);
    }
    return false;
}

/**
 * Makes a copy of an existing SHA-256 context.
 *
 * If sha256_context is NULL, then return false.
 * If new_sha256_context is NULL, then return false.
 * If this interface is not supported, then return false.
 *
 * @param[in]  sha256_context      Pointer to SHA-256 context being copied.
 * @param[out] new_sha256_context  Pointer to new SHA-256 context.
 *
 * @retval true   SHA-256 context copy succeeded.
 * @retval false  SHA-256 context copy failed.
 * @retval false  This interface is not supported.
 **/
bool libspdm_sha256_duplicate(const void* sha256_context, void* new_sha256_context)
{
    if constexpr (hash::config::SHA256_SUPPORT) {
        return pdk::spdm::platforms::res::crypto::hash::sha256_duplicate(sha256_context,
                                                                         new_sha256_context);
    }
    return false;
}

/**
 * Digests the input data and updates SHA-256 context.
 *
 * This function performs SHA-256 digest on a data buffer of the specified size.
 * It can be called multiple times to compute the digest of long or discontinuous data streams.
 * SHA-256 context should be already correctly initialized by libspdm_sha256_init(), and must
 *not have been finalized by libspdm_sha256_final(). Behavior with invalid context is undefined.
 *
 * If sha256_context is NULL, then return false.
 *
 * @param[in, out]  sha256_context  Pointer to the SHA-256 context.
 * @param[in]       data            Pointer to the buffer containing the data to be hashed.
 * @param[in]       data_size       Size of data buffer in bytes.
 *
 * @retval true   SHA-256 data digest succeeded.
 * @retval false  SHA-256 data digest failed.
 **/
bool libspdm_sha256_update(void* sha256_context, const void* data, size_t data_size)
{
    if constexpr (hash::config::SHA256_SUPPORT) {
        return pdk::spdm::platforms::res::crypto::hash::sha256_update(
            sha256_context, data, data_size);
    }
    return false;
}

/**
 * Completes computation of the SHA-256 digest value.
 *
 * This function completes SHA-256 hash computation and populates the digest value into
 * the specified memory. After this function has been called, the SHA-256 context cannot
 * be used again. SHA-256 context should be already correctly initialized by
 *libspdm_sha256_init(), and must not have been finalized by libspdm_sha256_final(). Behavior
 *with invalid SHA-256 context is undefined.
 *
 * If sha256_context is NULL, then return false.
 * If hash_value is NULL, then return false.
 *
 * @param[in, out]  sha256_context  Pointer to the SHA-256 context.
 * @param[out]      hash_value      Pointer to a buffer that receives the SHA-256 digest
 *                                  value (32 bytes).
 *
 * @retval true   SHA-256 digest computation succeeded.
 * @retval false  SHA-256 digest computation failed.
 **/
bool libspdm_sha256_final(void* sha256_context, uint8_t* hash_value)
{
    if constexpr (hash::config::SHA256_SUPPORT) {
        return pdk::spdm::platforms::res::crypto::hash::sha256_final(sha256_context,
                                                                     hash_value);
    }
    return false;
}

/**
 * Computes the SHA-256 message digest of an input data buffer.
 *
 * This function performs the SHA-256 message digest of a given data buffer, and places
 * the digest value into the specified memory.
 *
 * If this interface is not supported, then return false.
 *
 * @param[in]   data        Pointer to the buffer containing the data to be hashed.
 * @param[in]   data_size   Size of data buffer in bytes.
 * @param[out]  hash_value  Pointer to a buffer that receives the SHA-256 digest value (32
 *bytes).
 *
 * @retval true   SHA-256 digest computation succeeded.
 * @retval false  SHA-256 digest computation failed.
 * @retval false  This interface is not supported.
 **/
bool libspdm_sha256_hash_all(const void* data, size_t data_size, uint8_t* hash_value)
{
    if constexpr (hash::config::SHA256_SUPPORT) {
        return pdk::spdm::platforms::res::crypto::hash::sha256_hash_all(
            data, data_size, hash_value);
    }
    return false;
}

/**
 * Allocates and initializes one HASH_CTX context for subsequent SHA-384 use.
 *
 * @return  Pointer to the HASH_CTX context that has been initialized.
 *          If the allocations fails, libspdm_sha384_new() returns NULL.
 **/
void* libspdm_sha384_new(void)
{
    if constexpr (hash::config::SHA384_SUPPORT) {
        return pdk::spdm::platforms::res::crypto::hash::sha384_new();
    }
    return nullptr;
}

/**
 * Release the specified HASH_CTX context.
 *
 * @param[in]  sha384_context  Pointer to the HASH_CTX context to be released.
 **/
void libspdm_sha384_free(void* sha384_context)
{
    if constexpr (hash::config::SHA384_SUPPORT) {
        pdk::spdm::platforms::res::crypto::hash::sha384_free(sha384_context);
    }
}

/**
 * Initializes user-supplied memory pointed to by sha384_context as SHA-384 hash context for
 * subsequent use.
 *
 * If sha384_context is NULL, then return false.
 *
 * @param[out]  sha384_context  Pointer to SHA-384 context being initialized.
 *
 * @retval true   SHA-384 context initialization succeeded.
 * @retval false  SHA-384 context initialization failed.
 **/
bool libspdm_sha384_init(void* sha384_context)
{
    if constexpr (hash::config::SHA384_SUPPORT) {
        return pdk::spdm::platforms::res::crypto::hash::sha384_init(sha384_context);
    }
    return false;
}

/**
 * Makes a copy of an existing SHA-384 context.
 *
 * If sha384_context is NULL, then return false.
 * If new_sha384_context is NULL, then return false.
 * If this interface is not supported, then return false.
 *
 * @param[in]  sha384_context      Pointer to SHA-384 context being copied.
 * @param[out] new_sha384_context  Pointer to new SHA-384 context.
 *
 * @retval true   SHA-384 context copy succeeded.
 * @retval false  SHA-384 context copy failed.
 * @retval false  This interface is not supported.
 **/
bool libspdm_sha384_duplicate(const void* sha384_context, void* new_sha384_context)
{
    if constexpr (hash::config::SHA384_SUPPORT) {
        return pdk::spdm::platforms::res::crypto::hash::sha384_duplicate(sha384_context,
                                                                         new_sha384_context);
    }
    return false;
}

/**
 * Digests the input data and updates SHA-384 context.
 *
 * This function performs SHA-384 digest on a data buffer of the specified size.
 * It can be called multiple times to compute the digest of long or discontinuous data streams.
 * SHA-384 context should be already correctly initialized by libspdm_sha384_init(), and must
 *not have been finalized by libspdm_sha384_final(). Behavior with invalid context is undefined.
 *
 * If sha384_context is NULL, then return false.
 *
 * @param[in, out]  sha384_context  Pointer to the SHA-384 context.
 * @param[in]       data            Pointer to the buffer containing the data to be hashed.
 * @param[in]       data_size       Size of data buffer in bytes.
 *
 * @retval true   SHA-384 data digest succeeded.
 * @retval false  SHA-384 data digest failed.
 **/
bool libspdm_sha384_update(void* sha384_context, const void* data, size_t data_size)
{
    if constexpr (hash::config::SHA384_SUPPORT) {
        return pdk::spdm::platforms::res::crypto::hash::sha384_update(
            sha384_context, data, data_size);
    }
    return false;
}

/**
 * Completes computation of the SHA-384 digest value.
 *
 * This function completes SHA-384 hash computation and populates the digest value into
 * the specified memory. After this function has been called, the SHA-384 context cannot
 * be used again. SHA-384 context should be already correctly initialized by
 *libspdm_sha384_init(), and must not have been finalized by libspdm_sha384_final(). Behavior
 *with invalid SHA-384 context is undefined.
 *
 * If sha384_context is NULL, then return false.
 * If hash_value is NULL, then return false.
 *
 * @param[in, out]  sha384_context  Pointer to the SHA-384 context.
 * @param[out]      hash_value      Pointer to a buffer that receives the SHA-384 digest
 *                                  value (48 bytes).
 *
 * @retval true   SHA-384 digest computation succeeded.
 * @retval false  SHA-384 digest computation failed.
 **/
bool libspdm_sha384_final(void* sha384_context, uint8_t* hash_value)
{
    if constexpr (hash::config::SHA384_SUPPORT) {
        return pdk::spdm::platforms::res::crypto::hash::sha384_final(sha384_context,
                                                                     hash_value);
    }
    return false;
}

/**
 * Computes the SHA-384 message digest of an input data buffer.
 *
 * This function performs the SHA-384 message digest of a given data buffer, and places
 * the digest value into the specified memory.
 *
 * If this interface is not supported, then return false.
 *
 * @param[in]   data        Pointer to the buffer containing the data to be hashed.
 * @param[in]   data_size   Size of data buffer in bytes.
 * @param[out]  hash_value  Pointer to a buffer that receives the SHA-384 digest value (48
 *bytes).
 *
 * @retval true   SHA-384 digest computation succeeded.
 * @retval false  SHA-384 digest computation failed.
 * @retval false  This interface is not supported.
 **/
bool libspdm_sha384_hash_all(const void* data, size_t data_size, uint8_t* hash_value)
{
    if constexpr (hash::config::SHA384_SUPPORT) {
        return pdk::spdm::platforms::res::crypto::hash::sha384_hash_all(
            data, data_size, hash_value);
    }
    return false;
}

/**
 * Allocates and initializes one HASH_CTX context for subsequent SHA-512 use.
 *
 * @return  Pointer to the HASH_CTX context that has been initialized.
 *          If the allocations fails, libspdm_sha512_new() returns NULL.
 **/
void* libspdm_sha512_new(void)
{
    if constexpr (hash::config::SHA512_SUPPORT) {
        return pdk::spdm::platforms::res::crypto::hash::sha512_new();
    }
    return nullptr;
}

/**
 * Release the specified HASH_CTX context.
 *
 * @param[in]  sha512_context  Pointer to the HASH_CTX context to be released.
 **/
void libspdm_sha512_free(void* sha512_context)
{
    if constexpr (hash::config::SHA512_SUPPORT) {
        pdk::spdm::platforms::res::crypto::hash::sha512_free(sha512_context);
    }
}

/**
 * Initializes user-supplied memory pointed by sha512_context as SHA-512 hash context for
 * subsequent use.
 *
 * If sha512_context is NULL, then return false.
 *
 * @param[out]  sha512_context  Pointer to SHA-512 context being initialized.
 *
 * @retval true   SHA-512 context initialization succeeded.
 * @retval false  SHA-512 context initialization failed.
 **/
bool libspdm_sha512_init(void* sha512_context)
{
    if constexpr (hash::config::SHA512_SUPPORT) {
        return pdk::spdm::platforms::res::crypto::hash::sha512_init(sha512_context);
    }
    return false;
}

/**
 * Makes a copy of an existing SHA-512 context.
 *
 * If sha512_context is NULL, then return false.
 * If new_sha512_context is NULL, then return false.
 * If this interface is not supported, then return false.
 *
 * @param[in]  sha512_context      Pointer to SHA-512 context being copied.
 * @param[out] new_sha512_context  Pointer to new SHA-512 context.
 *
 * @retval true   SHA-512 context copy succeeded.
 * @retval false  SHA-512 context copy failed.
 * @retval false  This interface is not supported.
 **/
bool libspdm_sha512_duplicate(const void* sha512_context, void* new_sha512_context)
{
    if constexpr (hash::config::SHA512_SUPPORT) {
        return pdk::spdm::platforms::res::crypto::hash::sha512_duplicate(sha512_context,
                                                                         new_sha512_context);
    }
    return false;
}

/**
 * Digests the input data and updates SHA-512 context.
 *
 * This function performs SHA-512 digest on a data buffer of the specified size.
 * It can be called multiple times to compute the digest of long or discontinuous data streams.
 * SHA-512 context should be already correctly initialized by libspdm_sha512_init(), and must
 *not have been finalized by libspdm_sha512_final(). Behavior with invalid context is undefined.
 *
 * If sha512_context is NULL, then return false.
 *
 * @param[in, out]  sha512_context  Pointer to the SHA-512 context.
 * @param[in]       data            Pointer to the buffer containing the data to be hashed.
 * @param[in]       data_size       Size of data buffer in bytes.
 *
 * @retval true   SHA-512 data digest succeeded.
 * @retval false  SHA-512 data digest failed.
 **/
bool libspdm_sha512_update(void* sha512_context, const void* data, size_t data_size)
{
    if constexpr (hash::config::SHA512_SUPPORT) {
        return pdk::spdm::platforms::res::crypto::hash::sha512_update(
            sha512_context, data, data_size);
    }
    return false;
}

/**
 * Completes computation of the SHA-512 digest value.
 *
 * This function completes SHA-512 hash computation and populates the digest value into
 * the specified memory. After this function has been called, the SHA-512 context cannot
 * be used again. SHA-512 context should be already correctly initialized by
 *libspdm_sha512_init(), and must not have been finalized by libspdm_sha512_final(). Behavior
 *with invalid SHA-512 context is undefined.
 *
 * If sha512_context is NULL, then return false.
 * If hash_value is NULL, then return false.
 *
 * @param[in, out]  sha512_context  Pointer to the SHA-512 context.
 * @param[out]      hash_value      Pointer to a buffer that receives the SHA-512 digest
 *                                value (64 bytes).
 *
 * @retval true   SHA-512 digest computation succeeded.
 * @retval false  SHA-512 digest computation failed.
 **/
bool libspdm_sha512_final(void* sha512_context, uint8_t* hash_value)
{
    if constexpr (hash::config::SHA512_SUPPORT) {
        return pdk::spdm::platforms::res::crypto::hash::sha512_final(sha512_context,
                                                                     hash_value);
    }
    return false;
}

/**
 * Computes the SHA-512 message digest of an input data buffer.
 *
 * This function performs the SHA-512 message digest of a given data buffer, and places
 * the digest value into the specified memory.
 *
 * If this interface is not supported, then return false.
 *
 * @param[in]   data        Pointer to the buffer containing the data to be hashed.
 * @param[in]   data_size   Size of data buffer in bytes.
 * @param[out]  hash_value  Pointer to a buffer that receives the SHA-512 digest value (64
 *bytes).
 *
 * @retval true   SHA-512 digest computation succeeded.
 * @retval false  SHA-512 digest computation failed.
 * @retval false  This interface is not supported.
 **/
bool libspdm_sha512_hash_all(const void* data, size_t data_size, uint8_t* hash_value)
{
    if constexpr (hash::config::SHA512_SUPPORT) {
        return pdk::spdm::platforms::res::crypto::hash::sha512_hash_all(
            data, data_size, hash_value);
    }
    return false;
}

/*=====================================================================================
 *    One-way cryptographic hash SHA3 primitives.
 *=====================================================================================
 */
/**
 * Allocates and initializes one HASH_CTX context for subsequent SHA3-256 use.
 *
 * @return  Pointer to the HASH_CTX context that has been initialized.
 *          If the allocations fails, libspdm_sha3_256_new() returns NULL.
 **/
void* libspdm_sha3_256_new(void)
{
    if constexpr (hash::config::SHA3_256_SUPPORT) {
        return pdk::spdm::platforms::res::crypto::hash::sha3_256_new();
    }
    return nullptr;
}

/**
 * Release the specified HASH_CTX context.
 *
 * @param[in]  sha3_256_context  Pointer to the HASH_CTX context to be released.
 **/
void libspdm_sha3_256_free(void* sha3_256_context)
{
    if constexpr (hash::config::SHA3_256_SUPPORT) {
        pdk::spdm::platforms::res::crypto::hash::sha3_256_free(sha3_256_context);
    }
}

/**
 * Initializes user-supplied memory pointed by sha3_256_context as SHA3-256 hash context for
 * subsequent use.
 *
 * If sha3_256_context is NULL, then return false.
 *
 * @param[out]  sha3_256_context  Pointer to SHA3-256 context being initialized.
 *
 * @retval true   SHA3-256 context initialization succeeded.
 * @retval false  SHA3-256 context initialization failed.
 **/
bool libspdm_sha3_256_init(void* sha3_256_context)
{
    if constexpr (hash::config::SHA3_256_SUPPORT) {
        return pdk::spdm::platforms::res::crypto::hash::sha3_256_init(sha3_256_context);
    }
    return false;
}

/**
 * Makes a copy of an existing SHA3-256 context.
 *
 * If sha3_256_context is NULL, then return false.
 * If new_sha3_256_context is NULL, then return false.
 * If this interface is not supported, then return false.
 *
 * @param[in]  sha3_256_context      Pointer to SHA3-256 context being copied.
 * @param[out] new_sha3_256_context  Pointer to new SHA3-256 context.
 *
 * @retval true   SHA3-256 context copy succeeded.
 * @retval false  SHA3-256 context copy failed.
 * @retval false  This interface is not supported.
 **/
bool libspdm_sha3_256_duplicate(const void* sha3_256_context, void* new_sha3_256_context)
{
    if constexpr (hash::config::SHA3_256_SUPPORT) {
        return pdk::spdm::platforms::res::crypto::hash::sha3_256_duplicate(
            sha3_256_context, new_sha3_256_context);
    }
    return false;
}

/**
 * Digests the input data and updates SHA3-256 context.
 *
 * This function performs SHA3-256 digest on a data buffer of the specified size.
 * It can be called multiple times to compute the digest of long or discontinuous data streams.
 * SHA3-256 context should be already correctly initialized by libspdm_sha3_256_init(), and must
 *not have been finalized by libspdm_sha3_256_final(). Behavior with invalid context is
 *undefined.
 *
 * If sha3_256_context is NULL, then return false.
 *
 * @param[in, out]  sha3_256_context  Pointer to the SHA3-256 context.
 * @param[in]       data              Pointer to the buffer containing the data to be hashed.
 * @param[in]       data_size       size of data buffer in bytes.
 *
 * @retval true   SHA3-256 data digest succeeded.
 * @retval false  SHA3-256 data digest failed.
 **/
bool libspdm_sha3_256_update(void* sha3_256_context, const void* data, size_t data_size)
{
    if constexpr (hash::config::SHA3_256_SUPPORT) {
        return pdk::spdm::platforms::res::crypto::hash::sha3_256_update(
            sha3_256_context, data, data_size);
    }
    return false;
}

/**
 * Completes computation of the SHA3-256 digest value.
 *
 * This function completes SHA3-256 hash computation and populates the digest value into
 * the specified memory. After this function has been called, the SHA3-512 context cannot
 * be used again. SHA3-256 context should be already correctly initialized by
 * libspdm_sha3_256_init(), and must not have been finalized by libspdm_sha3_256_final().
 * Behavior with invalid SHA3-256 context is undefined.
 *
 * If sha3_256_context is NULL, then return false.
 * If hash_value is NULL, then return false.
 *
 * @param[in, out]  sha3_256_context  Pointer to the SHA3-256 context.
 * @param[out]      hash_value        Pointer to a buffer that receives the SHA3-256 digest
 *                                    value (32 bytes).
 *
 * @retval true   SHA3-256 digest computation succeeded.
 * @retval false  SHA3-256 digest computation failed.
 **/
bool libspdm_sha3_256_final(void* sha3_256_context, uint8_t* hash_value)
{
    if constexpr (hash::config::SHA3_256_SUPPORT) {
        return pdk::spdm::platforms::res::crypto::hash::sha3_256_final(sha3_256_context,
                                                                       hash_value);
    }
    return false;
}

/**
 * Computes the SHA3-256 message digest of an input data buffer.
 *
 * This function performs the SHA3-256 message digest of a given data buffer, and places
 * the digest value into the specified memory.
 *
 * If this interface is not supported, then return false.
 *
 * @param[in]   data        Pointer to the buffer containing the data to be hashed.
 * @param[in]   data_size   Size of data buffer in bytes.
 * @param[out]  hash_value  Pointer to a buffer that receives the SHA3-256 digest value (32
 *bytes).
 *
 * @retval true   SHA3-256 digest computation succeeded.
 * @retval false  SHA3-256 digest computation failed.
 * @retval false  This interface is not supported.
 **/
bool libspdm_sha3_256_hash_all(const void* data, size_t data_size, uint8_t* hash_value)
{
    if constexpr (hash::config::SHA3_256_SUPPORT) {
        return pdk::spdm::platforms::res::crypto::hash::sha3_256_hash_all(
            data, data_size, hash_value);
    }
    return false;
}

/**
 * Allocates and initializes one HASH_CTX context for subsequent SHA3-384 use.
 *
 * @return  Pointer to the HASH_CTX context that has been initialized.
 *          If the allocations fails, libspdm_sha3_384_new() returns NULL.
 **/
void* libspdm_sha3_384_new(void)
{
    if constexpr (hash::config::SHA3_384_SUPPORT) {
        return pdk::spdm::platforms::res::crypto::hash::sha3_384_new();
    }
    return nullptr;
}

/**
 * Release the specified HASH_CTX context.
 *
 * @param[in]  sha3_384_context  Pointer to the HASH_CTX context to be released.
 **/
void libspdm_sha3_384_free(void* sha3_384_context)
{
    if constexpr (hash::config::SHA3_384_SUPPORT) {
        pdk::spdm::platforms::res::crypto::hash::sha3_384_free(sha3_384_context);
    }
}

/**
 * Initializes user-supplied memory pointed by sha3_384_context as SHA3-384 hash context for
 * subsequent use.
 *
 * If sha3_384_context is NULL, then return false.
 *
 * @param[out]  sha3_384_context  Pointer to SHA3-384 context being initialized.
 *
 * @retval true   SHA3-384 context initialization succeeded.
 * @retval false  SHA3-384 context initialization failed.
 **/
bool libspdm_sha3_384_init(void* sha3_384_context)
{
    if constexpr (hash::config::SHA3_384_SUPPORT) {
        return pdk::spdm::platforms::res::crypto::hash::sha3_384_init(sha3_384_context);
    }
    return false;
}

/**
 * Makes a copy of an existing SHA3-384 context.
 *
 * If sha3_384_context is NULL, then return false.
 * If new_sha3_384_context is NULL, then return false.
 * If this interface is not supported, then return false.
 *
 * @param[in]  sha3_384_context      Pointer to SHA3-384 context being copied.
 * @param[out] new_sha3_384_context  Pointer to new SHA3-384 context.
 *
 * @retval true   SHA3-384 context copy succeeded.
 * @retval false  SHA3-384 context copy failed.
 * @retval false  This interface is not supported.
 **/
bool libspdm_sha3_384_duplicate(const void* sha3_384_context, void* new_sha3_384_context)
{
    if constexpr (hash::config::SHA3_384_SUPPORT) {
        return pdk::spdm::platforms::res::crypto::hash::sha3_384_duplicate(
            sha3_384_context, new_sha3_384_context);
    }
    return false;
}

/**
 * Digests the input data and updates SHA3-384 context.
 *
 * This function performs SHA3-384 digest on a data buffer of the specified size.
 * It can be called multiple times to compute the digest of long or discontinuous data streams.
 * SHA3-384 context should be already correctly initialized by libspdm_sha3_384_init(), and must
 *not have been finalized by libspdm_sha3_384_final(). Behavior with invalid context is
 *undefined.
 *
 * If sha3_384_context is NULL, then return false.
 *
 * @param[in, out]  sha3_384_context  Pointer to the SHA3-384 context.
 * @param[in]       data              Pointer to the buffer containing the data to be hashed.
 * @param[in]       data_size         Size of data buffer in bytes.
 *
 * @retval true   SHA3-384 data digest succeeded.
 * @retval false  SHA3-384 data digest failed.
 **/
bool libspdm_sha3_384_update(void* sha3_384_context, const void* data, size_t data_size)
{
    if constexpr (hash::config::SHA3_384_SUPPORT) {
        return pdk::spdm::platforms::res::crypto::hash::sha3_384_update(
            sha3_384_context, data, data_size);
    }
    return false;
}

/**
 * Completes computation of the SHA3-384 digest value.
 *
 * This function completes SHA3-384 hash computation and populates the digest value into
 * the specified memory. After this function has been called, the SHA3-384 context cannot
 * be used again. SHA3-384 context should be already correctly initialized by
 * libspdm_sha3_384_init(), and must not have been finalized by libspdm_sha3_384_final().
 * Behavior with invalid SHA3-384 context is undefined.
 *
 * If sha3_384_context is NULL, then return false.
 * If hash_value is NULL, then return false.
 *
 * @param[in, out]  sha3_384_context  Pointer to the SHA3-384 context.
 * @param[out]      hash_value        Pointer to a buffer that receives the SHA3-384 digest
 *                                    value (48 bytes).
 *
 * @retval true   SHA3-384 digest computation succeeded.
 * @retval false  SHA3-384 digest computation failed.
 *
 **/
bool libspdm_sha3_384_final(void* sha3_384_context, uint8_t* hash_value)
{
    if constexpr (hash::config::SHA3_384_SUPPORT) {
        return pdk::spdm::platforms::res::crypto::hash::sha3_384_final(sha3_384_context,
                                                                       hash_value);
    }
    return false;
}

/**
 * Computes the SHA3-384 message digest of an input data buffer.
 *
 * This function performs the SHA3-384 message digest of a given data buffer, and places
 * the digest value into the specified memory.
 *
 * If this interface is not supported, then return false.
 *
 * @param[in]   data        Pointer to the buffer containing the data to be hashed.
 * @param[in]   data_size   Size of data buffer in bytes.
 * @param[out]  hash_value  Pointer to a buffer that receives the SHA3-384 digest value (48
 *bytes).
 *
 * @retval true   SHA3-384 digest computation succeeded.
 * @retval false  SHA3-384 digest computation failed.
 * @retval false  This interface is not supported.
 **/
bool libspdm_sha3_384_hash_all(const void* data, size_t data_size, uint8_t* hash_value)
{
    if constexpr (hash::config::SHA3_384_SUPPORT) {
        return pdk::spdm::platforms::res::crypto::hash::sha3_384_hash_all(
            data, data_size, hash_value);
    }
    return false;
}

/**
 * Allocates and initializes one HASH_CTX context for subsequent SHA3-512 use.
 *
 * @return  Pointer to the HASH_CTX context that has been initialized.
 *          If the allocations fails, libspdm_sha3_512_new() returns NULL.
 **/
void* libspdm_sha3_512_new(void)
{
    if constexpr (hash::config::SHA3_512_SUPPORT) {
        return pdk::spdm::platforms::res::crypto::hash::sha3_512_new();
    }
    return nullptr;
}

/**
 * Release the specified HASH_CTX context.
 *
 * @param[in]  sha3_512_context  Pointer to the HASH_CTX context to be released.
 **/
void libspdm_sha3_512_free(void* sha3_512_context)
{
    if constexpr (hash::config::SHA3_512_SUPPORT) {
        pdk::spdm::platforms::res::crypto::hash::sha3_512_free(sha3_512_context);
    }
}

/**
 * Initializes user-supplied memory pointed by sha3_512_context as SHA3-512 hash context for
 * subsequent use.
 *
 * If sha3_512_context is NULL, then return false.
 *
 * @param[out]  sha3_512_context  Pointer to SHA3-512 context being initialized.
 *
 * @retval true   SHA3-512 context initialization succeeded.
 * @retval false  SHA3-512 context initialization failed.
 **/
bool libspdm_sha3_512_init(void* sha3_512_context)
{
    if constexpr (hash::config::SHA3_512_SUPPORT) {
        return pdk::spdm::platforms::res::crypto::hash::sha3_512_init(sha3_512_context);
    }
    return false;
}

/**
 * Makes a copy of an existing SHA3-512 context.
 *
 * If sha3_512_context is NULL, then return false.
 * If new_sha3_512_context is NULL, then return false.
 * If this interface is not supported, then return false.
 *
 * @param[in]  sha3_512_context      Pointer to SHA3-512 context being copied.
 * @param[out] new_sha3_512_context  Pointer to new SHA3-512 context.
 *
 * @retval true   SHA3-512 context copy succeeded.
 * @retval false  SHA3-512 context copy failed.
 * @retval false  This interface is not supported.
 *
 **/
bool libspdm_sha3_512_duplicate(const void* sha3_512_context, void* new_sha3_512_context)
{
    if constexpr (hash::config::SHA3_512_SUPPORT) {
        return pdk::spdm::platforms::res::crypto::hash::sha3_512_duplicate(
            sha3_512_context, new_sha3_512_context);
    }
    return false;
}

/**
 * Digests the input data and updates SHA3-512 context.
 *
 * This function performs SHA3-512 digest on a data buffer of the specified size.
 * It can be called multiple times to compute the digest of long or discontinuous data streams.
 * SHA3-512 context should be already correctly initialized by libspdm_sha3_512_init(), and must
 *not have been finalized by libspdm_sha3_512_final(). Behavior with invalid context is
 *undefined.
 *
 * If sha3_512_context is NULL, then return false.
 *
 * @param[in, out]  sha3_512_context  Pointer to the SHA3-512 context.
 * @param[in]       data              Pointer to the buffer containing the data to be hashed.
 * @param[in]       data_size         Size of data buffer in bytes.
 *
 * @retval true   SHA3-512 data digest succeeded.
 * @retval false  SHA3-512 data digest failed.
 **/
bool libspdm_sha3_512_update(void* sha3_512_context, const void* data, size_t data_size)
{
    if constexpr (hash::config::SHA3_512_SUPPORT) {
        return pdk::spdm::platforms::res::crypto::hash::sha3_512_update(
            sha3_512_context, data, data_size);
    }
    return false;
}

/**
 * Completes computation of the SHA3-512 digest value.
 *
 * This function completes SHA3-512 hash computation and populates the digest value into
 * the specified memory. After this function has been called, the SHA3-512 context cannot
 * be used again. SHA3-512 context should be already correctly initialized by
 * libspdm_sha3_512_init(), and must not have been finalized by libspdm_sha3_512_final().
 * Behavior with invalid SHA3-512 context is undefined.
 *
 * If sha3_512_context is NULL, then return false.
 * If hash_value is NULL, then return false.
 *
 * @param[in, out]  sha3_512_context  Pointer to the SHA3-512 context.
 * @param[out]      hash_value        Pointer to a buffer that receives the SHA3-512 digest
 *                                    value (64 bytes).
 *
 * @retval true   SHA3-512 digest computation succeeded.
 * @retval false  SHA3-512 digest computation failed.
 **/
bool libspdm_sha3_512_final(void* sha3_512_context, uint8_t* hash_value)
{
    if constexpr (hash::config::SHA3_512_SUPPORT) {
        return pdk::spdm::platforms::res::crypto::hash::sha3_512_final(sha3_512_context,
                                                                       hash_value);
    }
    return false;
}

/**
 * Computes the SHA3-512 message digest of an input data buffer.
 *
 * This function performs the SHA3-512 message digest of a given data buffer, and places
 * the digest value into the specified memory.
 *
 * If this interface is not supported, then return false.
 *
 * @param[in]   data        Pointer to the buffer containing the data to be hashed.
 * @param[in]   data_size   Size of data buffer in bytes.
 * @param[out]  hash_value  Pointer to a buffer that receives the SHA3-512 digest value (64
 *bytes).
 *
 * @retval true   SHA3-512 digest computation succeeded.
 * @retval false  SHA3-512 digest computation failed.
 * @retval false  This interface is not supported.
 **/
bool libspdm_sha3_512_hash_all(const void* data, size_t data_size, uint8_t* hash_value)
{
    if constexpr (hash::config::SHA3_512_SUPPORT) {
        return pdk::spdm::platforms::res::crypto::hash::sha3_512_hash_all(
            data, data_size, hash_value);
    }
    return false;
}

/*=====================================================================================
 *    One-Way Cryptographic hash SM3 Primitives
 *=====================================================================================
 */

/**
 * Allocates and initializes one HASH_CTX context for subsequent SM3-256 use.
 *
 * @return  Pointer to the HASH_CTX context that has been initialized.
 *          If the allocations fails, libspdm_sm3_256_new() returns NULL.
 **/
void* libspdm_sm3_256_new(void)
{
    if constexpr (hash::config::SM3_256_SUPPORT) {
        return pdk::spdm::platforms::res::crypto::hash::sm3_256_new();
    }
    return nullptr;
}

/**
 * Release the specified HASH_CTX context.
 *
 * @param[in]  sm3_context  Pointer to the HASH_CTX context to be released.
 **/
void libspdm_sm3_256_free(void* sm3_context)
{
    if constexpr (hash::config::SM3_256_SUPPORT) {
        pdk::spdm::platforms::res::crypto::hash::sm3_256_free(sm3_context);
    }
}

/**
 * Initializes user-supplied memory pointed by sm3_context as SM3 hash context for
 * subsequent use.
 *
 * If sm3_context is NULL, then return false.
 *
 * @param[out]  sm3_context  Pointer to SM3 context being initialized.
 *
 * @retval true   SM3 context initialization succeeded.
 * @retval false  SM3 context initialization failed.
 **/
bool libspdm_sm3_256_init(void* sm3_context)
{
    if constexpr (hash::config::SM3_256_SUPPORT) {
        return pdk::spdm::platforms::res::crypto::hash::sm3_256_init(sm3_context);
    }
    return false;
}

/**
 * Makes a copy of an existing SM3 context.
 *
 * If sm3_context is NULL, then return false.
 * If new_sm3_context is NULL, then return false.
 * If this interface is not supported, then return false.
 *
 * @param[in]  sm3_context      Pointer to SM3 context being copied.
 * @param[out] new_sm3_context  Pointer to new SM3 context.
 *
 * @retval true   SM3 context copy succeeded.
 * @retval false  SM3 context copy failed.
 * @retval false  This interface is not supported.
 **/
bool libspdm_sm3_256_duplicate(const void* sm3_context, void* new_sm3_context)
{
    if constexpr (hash::config::SM3_256_SUPPORT) {
        return pdk::spdm::platforms::res::crypto::hash::sm3_256_duplicate(sm3_context,
                                                                          new_sm3_context);
    }
    return false;
}

/**
 * Digests the input data and updates SM3 context.
 *
 * This function performs SM3 digest on a data buffer of the specified size.
 * It can be called multiple times to compute the digest of long or discontinuous data streams.
 * SM3 context should be already correctly initialized by sm3_init(), and should not be
 *finalized by sm3_final(). Behavior with invalid context is undefined.
 *
 * If sm3_context is NULL, then return false.
 *
 * @param[in, out]  sm3_context  Pointer to the SM3 context.
 * @param[in]       data         Pointer to the buffer containing the data to be hashed.
 * @param[in]       data_size    Size of data buffer in bytes.
 *
 * @retval true   SM3 data digest succeeded.
 * @retval false  SM3 data digest failed.
 **/
bool libspdm_sm3_256_update(void* sm3_context, const void* data, size_t data_size)
{
    if constexpr (hash::config::SM3_256_SUPPORT) {
        return pdk::spdm::platforms::res::crypto::hash::sm3_256_update(
            sm3_context, data, data_size);
    }
    return false;
}

/**
 * Completes computation of the SM3 digest value.
 *
 * This function completes SM3 hash computation and retrieves the digest value into
 * the specified memory. After this function has been called, the SM3 context cannot
 * be used again. SM3 context should be already correctly initialized by sm3_init(), and should
 *not be finalized by sm3_final(). Behavior with invalid SM3 context is undefined.
 *
 * If sm3_context is NULL, then return false.
 * If hash_value is NULL, then return false.
 *
 * @param[in, out]  sm3_context  Pointer to the SM3 context.
 * @param[out]      hash_value   Pointer to a buffer that receives the SM3 digest value (32
 *bytes).
 *
 * @retval true   SM3 digest computation succeeded.
 * @retval false  SM3 digest computation failed.
 **/
bool libspdm_sm3_256_final(void* sm3_context, uint8_t* hash_value)
{
    if constexpr (hash::config::SM3_256_SUPPORT) {
        return pdk::spdm::platforms::res::crypto::hash::sm3_256_final(sm3_context, hash_value);
    }
    return false;
}

/**
 * Computes the SM3 message digest of an input data buffer.
 *
 * This function performs the SM3 message digest of a given data buffer, and places
 * the digest value into the specified memory.
 *
 * If this interface is not supported, then return false.
 *
 * @param[in]   data        Pointer to the buffer containing the data to be hashed.
 * @param[in]   data_size   Size of data buffer in bytes.
 * @param[out]  hash_value  Pointer to a buffer that receives the SM3 digest value (32 bytes).
 *
 * @retval true   SM3 digest computation succeeded.
 * @retval false  SM3 digest computation failed.
 * @retval false  This interface is not supported.
 **/
bool libspdm_sm3_256_hash_all(const void* data, size_t data_size, uint8_t* hash_value)
{
    if constexpr (hash::config::SM3_256_SUPPORT) {
        return pdk::spdm::platforms::res::crypto::hash::sm3_256_hash_all(
            data, data_size, hash_value);
    }
    return false;
}

#ifdef __cplusplus
}
#endif

}  // namespace pdk::spdm::app::res::crypto::hash
