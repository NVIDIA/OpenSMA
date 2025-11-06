#include "pdk-spdm-app-res-crypto-dh.h"

namespace pdk::spdm::app::res::crypto::dh {

#ifdef __cplusplus
extern "C" {
#endif

/*=====================================================================================
 *    Diffie-Hellman Key Exchange Primitives
 *=====================================================================================
 */

/**
 * Allocates and initializes one Diffie-Hellman context for subsequent use with the NID.
 *
 * @param nid cipher NID
 *
 * @return  Pointer to the Diffie-Hellman context that has been initialized.
 *          If the allocations fails, libspdm_dh_new_by_nid() returns NULL.
 *          If the interface is not supported, libspdm_dh_new_by_nid() returns NULL.
 **/
void* libspdm_dh_new_by_nid(size_t nid)
{
    if constexpr (dh::config::DH_SUPPORT) {
        return pdk::spdm::platforms::res::crypto::dh::dh_new_by_nid(nid);
    }
    return nullptr;
}

/**
 * Release the specified DH context.
 *
 * @param[in]  dh_context  Pointer to the DH context to be released.
 **/
void libspdm_dh_free(void* dh_context)
{
    if constexpr (dh::config::DH_SUPPORT) {
        return pdk::spdm::platforms::res::crypto::dh::dh_free(dh_context);
    }
}

/**
 * Generates DH public key.
 *
 * This function generates random secret exponent, and computes the public key, which is
 * returned via parameter public_key and public_key_size. DH context is updated accordingly.
 * If the public_key buffer is too small to hold the public key, false is returned and
 * public_key_size is set to the required buffer size to obtain the public key.
 *
 * If dh_context is NULL, then return false.
 * If public_key_size is NULL, then return false.
 * If public_key_size is large enough but public_key is NULL, then return false.
 * If this interface is not supported, then return false.
 *
 * For FFDHE2048, the public_size is 256.
 * For FFDHE3072, the public_size is 384.
 * For FFDHE4096, the public_size is 512.
 *
 * @param[in, out]  dh_context       Pointer to the DH context.
 * @param[out]      public_key       Pointer to the buffer to receive generated public key.
 * @param[in, out]  public_key_size  On input, the size of public_key buffer in bytes.
 *                                   On output, the size of data returned in public_key buffer
 *in bytes.
 *
 * @retval true   DH public key generation succeeded.
 * @retval false  DH public key generation failed.
 * @retval false  public_key_size is not large enough.
 * @retval false  This interface is not supported.
 **/
bool libspdm_dh_generate_key(void* dh_context, uint8_t* public_key, size_t* public_key_size)
{
    if constexpr (dh::config::DH_SUPPORT) {
        return pdk::spdm::platforms::res::crypto::dh::dh_generate_key(
            dh_context, public_key, public_key_size);
    }
    return false;
}

/**
 * Computes exchanged common key.
 *
 * Given peer's public key, this function computes the exchanged common key, based on its own
 * context including value of prime modulus and random secret exponent.
 *
 * If dh_context is NULL, then return false.
 * If peer_public_key is NULL, then return false.
 * If key_size is NULL, then return false.
 * If key is NULL, then return false.
 * If key_size is not large enough, then return false.
 * If this interface is not supported, then return false.
 *
 * For FFDHE2048, the peer_public_size and key_size is 256.
 * For FFDHE3072, the peer_public_size and key_size is 384.
 * For FFDHE4096, the peer_public_size and key_size is 512.
 *
 * @param[in, out]  dh_context            Pointer to the DH context.
 * @param[in]       peer_public_key       Pointer to the peer's public key.
 * @param[in]       peer_public_key_size  size of peer's public key in bytes.
 * @param[out]      key                   Pointer to the buffer to receive generated key.
 * @param[in, out]  key_size              On input, the size of key buffer in bytes.
 *                                        On output, the size of data returned in key buffer in
 *                                        bytes.
 *
 * @retval true   DH exchanged key generation succeeded.
 * @retval false  DH exchanged key generation failed.
 * @retval false  key_size is not large enough.
 * @retval false  This interface is not supported.
 **/
bool libspdm_dh_compute_key(void*          dh_context,
                            const uint8_t* peer_public_key,
                            size_t         peer_public_key_size,
                            uint8_t*       key,
                            size_t*        key_size)
{
    if constexpr (dh::config::DH_SUPPORT) {
        return pdk::spdm::platforms::res::crypto::dh::dh_compute_key(
            dh_context, peer_public_key, peer_public_key_size, key, key_size);
    }
    return false;
}

#ifdef __cplusplus
}
#endif

}  // namespace pdk::spdm::app::res::crypto::dh
