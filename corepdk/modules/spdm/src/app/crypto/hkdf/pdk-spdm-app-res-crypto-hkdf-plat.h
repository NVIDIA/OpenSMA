#pragma once
#include <cstddef>
#include <cstdint>
#include <span>

namespace pdk::spdm::platforms::res::crypto::hkdf {

bool hkdf_sha256_extract(const uint8_t* key,
                         size_t         key_size,
                         const uint8_t* salt,
                         size_t         salt_size,
                         uint8_t*       prk_out,
                         size_t         prk_out_size);
bool hkdf_sha256_expand(const uint8_t* prk,
                        size_t         prk_size,
                        const uint8_t* info,
                        size_t         info_size,
                        uint8_t*       out,
                        size_t         out_size);

bool hkdf_sha384_extract(const uint8_t* key,
                         size_t         key_size,
                         const uint8_t* salt,
                         size_t         salt_size,
                         uint8_t*       prk_out,
                         size_t         prk_out_size);
bool hkdf_sha384_expand(const uint8_t* prk,
                        size_t         prk_size,
                        const uint8_t* info,
                        size_t         info_size,
                        uint8_t*       out,
                        size_t         out_size);

bool hkdf_sha512_extract(const uint8_t* key,
                         size_t         key_size,
                         const uint8_t* salt,
                         size_t         salt_size,
                         uint8_t*       prk_out,
                         size_t         prk_out_size);
bool hkdf_sha512_expand(const uint8_t* prk,
                        size_t         prk_size,
                        const uint8_t* info,
                        size_t         info_size,
                        uint8_t*       out,
                        size_t         out_size);

bool hkdf_sha3_256_extract(const uint8_t* key,
                           size_t         key_size,
                           const uint8_t* salt,
                           size_t         salt_size,
                           uint8_t*       prk_out,
                           size_t         prk_out_size);
bool hkdf_sha3_256_expand(const uint8_t* prk,
                          size_t         prk_size,
                          const uint8_t* info,
                          size_t         info_size,
                          uint8_t*       out,
                          size_t         out_size);

bool hkdf_sha3_384_extract(const uint8_t* key,
                           size_t         key_size,
                           const uint8_t* salt,
                           size_t         salt_size,
                           uint8_t*       prk_out,
                           size_t         prk_out_size);
bool hkdf_sha3_384_expand(const uint8_t* prk,
                          size_t         prk_size,
                          const uint8_t* info,
                          size_t         info_size,
                          uint8_t*       out,
                          size_t         out_size);

bool hkdf_sha3_512_extract(const uint8_t* key,
                           size_t         key_size,
                           const uint8_t* salt,
                           size_t         salt_size,
                           uint8_t*       prk_out,
                           size_t         prk_out_size);
bool hkdf_sha3_512_expand(const uint8_t* prk,
                          size_t         prk_size,
                          const uint8_t* info,
                          size_t         info_size,
                          uint8_t*       out,
                          size_t         out_size);

bool hkdf_sm3_256_extract(const uint8_t* key,
                          size_t         key_size,
                          const uint8_t* salt,
                          size_t         salt_size,
                          uint8_t*       prk_out,
                          size_t         prk_out_size);
bool hkdf_sm3_256_expand(const uint8_t* prk,
                         size_t         prk_size,
                         const uint8_t* info,
                         size_t         info_size,
                         uint8_t*       out,
                         size_t         out_size);

}  // namespace pdk::spdm::platforms::res::crypto::hkdf