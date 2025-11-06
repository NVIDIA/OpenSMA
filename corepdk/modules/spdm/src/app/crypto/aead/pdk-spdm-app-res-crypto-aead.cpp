#include "pdk-spdm-app-res-crypto-aead.h"

namespace pdk::spdm::app::res::crypto::aead {

#ifdef __cplusplus
extern "C" {
#endif

/*=====================================================================================
 *    Authenticated Encryption with Associated data (AEAD) Cryptography Primitives
 *=====================================================================================
 */

/**
 * Performs AEAD AES-GCM authenticated encryption on a data buffer and additional authenticated
 * data.
 *
 * iv_size must be 12, otherwise false is returned.
 * key_size must be 16 or 32, otherwise false is returned.
 * tag_size must be 12, 13, 14, 15, 16, otherwise false is returned.
 *
 * @param[in]   key            Pointer to the encryption key.
 * @param[in]   key_size       Size of the encryption key in bytes.
 * @param[in]   iv             Pointer to the IV value.
 * @param[in]   iv_size        Size of the IV value in bytes.
 * @param[in]   a_data         Pointer to the additional authenticated data.
 * @param[in]   a_data_size    Size of the additional authenticated data in bytes.
 * @param[in]   data_in        Pointer to the input data buffer to be encrypted.
 * @param[in]   data_in_size   Size of the input data buffer in bytes.
 * @param[out]  tag_out        Pointer to a buffer that receives the authentication tag output.
 * @param[in]   tag_size       Size of the authentication tag in bytes.
 * @param[out]  data_out       Pointer to a buffer that receives the encryption output.
 * @param[out]  data_out_size  Size of the output data buffer in bytes.
 *
 * @retval true   AEAD AES-GCM authenticated encryption succeeded.
 * @retval false  AEAD AES-GCM authenticated encryption failed.
 **/
bool libspdm_aead_aes_gcm_encrypt(const uint8_t* key,
                                  size_t         key_size,
                                  const uint8_t* iv,
                                  size_t         iv_size,
                                  const uint8_t* a_data,
                                  size_t         a_data_size,
                                  const uint8_t* data_in,
                                  size_t         data_in_size,
                                  uint8_t*       tag_out,
                                  size_t         tag_size,
                                  uint8_t*       data_out,
                                  size_t*        data_out_size)
{
    if constexpr (aead::config::AEAD_AES_GCM_SUPPORT) {
        return pdk::spdm::platforms::res::crypto::aead::aead_aes_gcm_encrypt(key,
                                                                             key_size,
                                                                             iv,
                                                                             iv_size,
                                                                             a_data,
                                                                             a_data_size,
                                                                             data_in,
                                                                             data_in_size,
                                                                             tag_out,
                                                                             tag_size,
                                                                             data_out,
                                                                             data_out_size);
    }
    return false;
}

/**
 * Performs AEAD AES-GCM authenticated decryption on a data buffer and additional authenticated
 * data.
 *
 * iv_size must be 12, otherwise false is returned.
 * key_size must be 16 or 32, otherwise false is returned.
 * tag_size must be 12, 13, 14, 15, 16, otherwise false is returned.
 *
 * If data verification fails, false is returned.
 *
 * @param[in]   key            Pointer to the encryption key.
 * @param[in]   key_size       Size of the encryption key in bytes.
 * @param[in]   iv             Pointer to the IV value.
 * @param[in]   iv_size        Size of the IV value in bytes.
 * @param[in]   a_data         Pointer to the additional authenticated data.
 * @param[in]   a_data_size    Size of the additional authenticated data in bytes.
 * @param[in]   data_in        Pointer to the input data buffer to be decrypted.
 * @param[in]   data_in_size   Size of the input data buffer in bytes.
 * @param[in]   tag            Pointer to a buffer that contains the authentication tag.
 * @param[in]   tag_size       Size of the authentication tag in bytes.
 * @param[out]  data_out       Pointer to a buffer that receives the decryption output.
 * @param[out]  data_out_size  Size of the output data buffer in bytes.
 *
 * @retval true   AEAD AES-GCM authenticated decryption succeeded.
 * @retval false  AEAD AES-GCM authenticated decryption failed.
 **/
bool libspdm_aead_aes_gcm_decrypt(const uint8_t* key,
                                  size_t         key_size,
                                  const uint8_t* iv,
                                  size_t         iv_size,
                                  const uint8_t* a_data,
                                  size_t         a_data_size,
                                  const uint8_t* data_in,
                                  size_t         data_in_size,
                                  const uint8_t* tag,
                                  size_t         tag_size,
                                  uint8_t*       data_out,
                                  size_t*        data_out_size)
{
    if constexpr (aead::config::AEAD_AES_GCM_SUPPORT) {
        return pdk::spdm::platforms::res::crypto::aead::aead_aes_gcm_decrypt(key,
                                                                             key_size,
                                                                             iv,
                                                                             iv_size,
                                                                             a_data,
                                                                             a_data_size,
                                                                             data_in,
                                                                             data_in_size,
                                                                             tag,
                                                                             tag_size,
                                                                             data_out,
                                                                             data_out_size);
    }
    return false;
}

/**
 * Performs AEAD ChaCha20Poly1305 authenticated encryption on a data buffer and additional
 * authenticated data.
 *
 * iv_size must be 12, otherwise false is returned.
 * key_size must be 32, otherwise false is returned.
 * tag_size must be 16, otherwise false is returned.
 *
 * @param[in]   key            Pointer to the encryption key.
 * @param[in]   key_size       Size of the encryption key in bytes.
 * @param[in]   iv             Pointer to the IV value.
 * @param[in]   iv_size        Size of the IV value in bytes.
 * @param[in]   a_data         Pointer to the additional authenticated data.
 * @param[in]   a_data_size    Size of the additional authenticated data in bytes.
 * @param[in]   data_in        Pointer to the input data buffer to be encrypted.
 * @param[in]   data_in_size   Size of the input data buffer in bytes.
 * @param[out]  tag_out        Pointer to a buffer that receives the authentication tag output.
 * @param[in]   tag_size       Size of the authentication tag in bytes.
 * @param[out]  data_out       Pointer to a buffer that receives the encryption output.
 * @param[out]  data_out_size  Size of the output data buffer in bytes.
 *
 * @retval true   AEAD ChaCha20Poly1305 authenticated encryption succeeded.
 * @retval false  AEAD ChaCha20Poly1305 authenticated encryption failed.
 **/
bool libspdm_aead_chacha20_poly1305_encrypt(const uint8_t* key,
                                            size_t         key_size,
                                            const uint8_t* iv,
                                            size_t         iv_size,
                                            const uint8_t* a_data,
                                            size_t         a_data_size,
                                            const uint8_t* data_in,
                                            size_t         data_in_size,
                                            uint8_t*       tag_out,
                                            size_t         tag_size,
                                            uint8_t*       data_out,
                                            size_t*        data_out_size)
{
    if constexpr (aead::config::AEAD_CHACHA20_POLY1305_SUPPORT) {
        return pdk::spdm::platforms::res::crypto::aead::aead_chacha20_poly1305_encrypt(
            key,
            key_size,
            iv,
            iv_size,
            a_data,
            a_data_size,
            data_in,
            data_in_size,
            tag_out,
            tag_size,
            data_out,
            data_out_size);
    }
    return false;
}

/**
 * Performs AEAD ChaCha20Poly1305 authenticated decryption on a data buffer and additional
 *authenticated data (AAD).
 *
 * iv_size must be 12, otherwise false is returned.
 * key_size must be 32, otherwise false is returned.
 * tag_size must be 16, otherwise false is returned.
 *
 * If data verification fails, false is returned.
 *
 * @param[in]   key            Pointer to the encryption key.
 * @param[in]   key_size       Size of the encryption key in bytes.
 * @param[in]   iv             Pointer to the IV value.
 * @param[in]   iv_size        Size of the IV value in bytes.
 * @param[in]   a_data         Pointer to the additional authenticated data.
 * @param[in]   a_data_size    Size of the additional authenticated data in bytes.
 * @param[in]   data_in        Pointer to the input data buffer to be decrypted.
 * @param[in]   data_in_size   Size of the input data buffer in bytes.
 * @param[in]   tag            Pointer to a buffer that contains the authentication tag.
 * @param[in]   tag_size       Size of the authentication tag in bytes.
 * @param[out]  data_out       Pointer to a buffer that receives the decryption output.
 * @param[out]  data_out_size  Size of the output data buffer in bytes.
 *
 * @retval true   AEAD ChaCha20Poly1305 authenticated decryption succeeded.
 * @retval false  AEAD ChaCha20Poly1305 authenticated decryption failed.
 *
 **/
bool libspdm_aead_chacha20_poly1305_decrypt(const uint8_t* key,
                                            size_t         key_size,
                                            const uint8_t* iv,
                                            size_t         iv_size,
                                            const uint8_t* a_data,
                                            size_t         a_data_size,
                                            const uint8_t* data_in,
                                            size_t         data_in_size,
                                            const uint8_t* tag,
                                            size_t         tag_size,
                                            uint8_t*       data_out,
                                            size_t*        data_out_size)
{
    if constexpr (aead::config::AEAD_CHACHA20_POLY1305_SUPPORT) {
        return pdk::spdm::platforms::res::crypto::aead::aead_chacha20_poly1305_decrypt(
            key,
            key_size,
            iv,
            iv_size,
            a_data,
            a_data_size,
            data_in,
            data_in_size,
            tag,
            tag_size,
            data_out,
            data_out_size);
    }
    return false;
}

/**
 * Performs AEAD SM4-GCM authenticated encryption on a data buffer and additional authenticated
 * data.
 *
 * iv_size must be 12, otherwise false is returned.
 * key_size must be 16, otherwise false is returned.
 * tag_size must be 16, otherwise false is returned.
 *
 * @param[in]   key            Pointer to the encryption key.
 * @param[in]   key_size       Size of the encryption key in bytes.
 * @param[in]   iv             Pointer to the IV value.
 * @param[in]   iv_size        Size of the IV value in bytes.
 * @param[in]   a_data         Pointer to the additional authenticated data.
 * @param[in]   a_data_size    Size of the additional authenticated data in bytes.
 * @param[in]   data_in        Pointer to the input data buffer to be encrypted.
 * @param[in]   data_in_size   Size of the input data buffer in bytes.
 * @param[out]  tag_out        Pointer to a buffer that receives the authentication tag output.
 * @param[in]   tag_size       Size of the authentication tag in bytes.
 * @param[out]  data_out       Pointer to a buffer that receives the encryption output.
 * @param[out]  data_out_size  Size of the output data buffer in bytes.
 *
 * @retval true   AEAD SM4-GCM authenticated encryption succeeded.
 * @retval false  AEAD SM4-GCM authenticated encryption failed.
 **/
bool libspdm_aead_sm4_gcm_encrypt(const uint8_t* key,
                                  size_t         key_size,
                                  const uint8_t* iv,
                                  size_t         iv_size,
                                  const uint8_t* a_data,
                                  size_t         a_data_size,
                                  const uint8_t* data_in,
                                  size_t         data_in_size,
                                  uint8_t*       tag_out,
                                  size_t         tag_size,
                                  uint8_t*       data_out,
                                  size_t*        data_out_size)
{
    if constexpr (aead::config::AEAD_SM4_GCM_SUPPORT) {
        return pdk::spdm::platforms::res::crypto::aead::aead_sm4_gcm_encrypt(key,
                                                                             key_size,
                                                                             iv,
                                                                             iv_size,
                                                                             a_data,
                                                                             a_data_size,
                                                                             data_in,
                                                                             data_in_size,
                                                                             tag_out,
                                                                             tag_size,
                                                                             data_out,
                                                                             data_out_size);
    }
    return false;
}

/**
 * Performs AEAD SM4-GCM authenticated decryption on a data buffer and additional authenticated
 * data.
 *
 * iv_size must be 12, otherwise false is returned.
 * key_size must be 16, otherwise false is returned.
 * tag_size must be 16, otherwise false is returned.
 *
 * If data verification fails, false is returned.
 *
 * @param[in]   key            Pointer to the encryption key.
 * @param[in]   key_size       Size of the encryption key in bytes.
 * @param[in]   iv             Pointer to the IV value.
 * @param[in]   iv_size        Size of the IV value in bytes.
 * @param[in]   a_data         Pointer to the additional authenticated data.
 * @param[in]   a_data_size    Size of the additional authenticated data in bytes.
 * @param[in]   data_in        Pointer to the input data buffer to be decrypted.
 * @param[in]   data_in_size   Size of the input data buffer in bytes.
 * @param[in]   tag            Pointer to a buffer that contains the authentication tag.
 * @param[in]   tag_size       Size of the authentication tag in bytes.
 * @param[out]  data_out       Pointer to a buffer that receives the decryption output.
 * @param[out]  data_out_size  Size of the output data buffer in bytes.
 *
 * @retval true   AEAD SM4-GCM authenticated decryption succeeded.
 * @retval false  AEAD SM4-GCM authenticated decryption failed.
 **/
bool libspdm_aead_sm4_gcm_decrypt(const uint8_t* key,
                                  size_t         key_size,
                                  const uint8_t* iv,
                                  size_t         iv_size,
                                  const uint8_t* a_data,
                                  size_t         a_data_size,
                                  const uint8_t* data_in,
                                  size_t         data_in_size,
                                  const uint8_t* tag,
                                  size_t         tag_size,
                                  uint8_t*       data_out,
                                  size_t*        data_out_size)
{
    if constexpr (aead::config::AEAD_SM4_GCM_SUPPORT) {
        return pdk::spdm::platforms::res::crypto::aead::aead_sm4_gcm_decrypt(key,
                                                                             key_size,
                                                                             iv,
                                                                             iv_size,
                                                                             a_data,
                                                                             a_data_size,
                                                                             data_in,
                                                                             data_in_size,
                                                                             tag,
                                                                             tag_size,
                                                                             data_out,
                                                                             data_out_size);
    }
    return false;
}

#ifdef __cplusplus
}
#endif

}  // namespace pdk::spdm::app::res::crypto::aead
