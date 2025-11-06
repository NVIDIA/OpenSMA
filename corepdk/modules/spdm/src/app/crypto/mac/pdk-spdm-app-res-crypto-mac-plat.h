#pragma once
#include <cstddef>
#include <cstdint>
#include <span>

namespace pdk::spdm::platforms::res::crypto::mac {

void* hmac_sha256_new(void);
void  hmac_sha256_free(void* hmac_sha256_ctx);
bool  hmac_sha256_set_key(void* hmac_sha256_ctx, const uint8_t* key, size_t key_size);
bool  hmac_sha256_duplicate(const void* hmac_sha256_ctx, void* new_hmac_sha256_ctx);
bool  hmac_sha256_update(void* hmac_sha256_ctx, const void* data, size_t data_size);
bool  hmac_sha256_final(void* hmac_sha256_ctx, uint8_t* hmac_value);
bool  hmac_sha256_all(const void*    data,
                      size_t         data_size,
                      const uint8_t* key,
                      size_t         key_size,
                      uint8_t*       hmac_value);

void* hmac_sha384_new(void);
void  hmac_sha384_free(void* hmac_sha384_ctx);
bool  hmac_sha384_set_key(void* hmac_sha384_ctx, const uint8_t* key, size_t key_size);
bool  hmac_sha384_duplicate(const void* hmac_sha384_ctx, void* new_hmac_sha384_ctx);
bool  hmac_sha384_update(void* hmac_sha384_ctx, const void* data, size_t data_size);
bool  hmac_sha384_final(void* hmac_sha384_ctx, uint8_t* hmac_value);
bool  hmac_sha384_all(const void*    data,
                      size_t         data_size,
                      const uint8_t* key,
                      size_t         key_size,
                      uint8_t*       hmac_value);

void* hmac_sha512_new(void);
void  hmac_sha512_free(void* hmac_sha512_ctx);
bool  hmac_sha512_set_key(void* hmac_sha512_ctx, const uint8_t* key, size_t key_size);
bool  hmac_sha512_duplicate(const void* hmac_sha512_ctx, void* new_hmac_sha512_ctx);
bool  hmac_sha512_update(void* hmac_sha512_ctx, const void* data, size_t data_size);
bool  hmac_sha512_final(void* hmac_sha512_ctx, uint8_t* hmac_value);
bool  hmac_sha512_all(const void*    data,
                      size_t         data_size,
                      const uint8_t* key,
                      size_t         key_size,
                      uint8_t*       hmac_value);

void* hmac_sha3_256_new(void);
void  hmac_sha3_256_free(void* hmac_sha3_256_ctx);
bool  hmac_sha3_256_set_key(void* hmac_sha3_256_ctx, const uint8_t* key, size_t key_size);
bool  hmac_sha3_256_duplicate(const void* hmac_sha3_256_ctx, void* new_hmac_sha3_256_ctx);
bool  hmac_sha3_256_update(void* hmac_sha3_256_ctx, const void* data, size_t data_size);
bool  hmac_sha3_256_final(void* hmac_sha3_256_ctx, uint8_t* hmac_value);
bool  hmac_sha3_256_all(const void*    data,
                        size_t         data_size,
                        const uint8_t* key,
                        size_t         key_size,
                        uint8_t*       hmac_value);

void* hmac_sha3_384_new(void);
void  hmac_sha3_384_free(void* hmac_sha3_384_ctx);
bool  hmac_sha3_384_set_key(void* hmac_sha3_384_ctx, const uint8_t* key, size_t key_size);
bool  hmac_sha3_384_duplicate(const void* hmac_sha3_384_ctx, void* new_hmac_sha3_384_ctx);
bool  hmac_sha3_384_update(void* hmac_sha3_384_ctx, const void* data, size_t data_size);
bool  hmac_sha3_384_final(void* hmac_sha3_384_ctx, uint8_t* hmac_value);
bool  hmac_sha3_384_all(const void*    data,
                        size_t         data_size,
                        const uint8_t* key,
                        size_t         key_size,
                        uint8_t*       hmac_value);

void* hmac_sha3_512_new(void);
void  hmac_sha3_512_free(void* hmac_sha3_512_ctx);
bool  hmac_sha3_512_set_key(void* hmac_sha3_512_ctx, const uint8_t* key, size_t key_size);
bool  hmac_sha3_512_duplicate(const void* hmac_sha3_512_ctx, void* new_hmac_sha3_512_ctx);
bool  hmac_sha3_512_update(void* hmac_sha3_512_ctx, const void* data, size_t data_size);
bool  hmac_sha3_512_final(void* hmac_sha3_512_ctx, uint8_t* hmac_value);
bool  hmac_sha3_512_all(const void*    data,
                        size_t         data_size,
                        const uint8_t* key,
                        size_t         key_size,
                        uint8_t*       hmac_value);

void* hmac_sm3_256_new(void);
void  hmac_sm3_256_free(void* hmac_sm3_256_ctx);
bool  hmac_sm3_256_set_key(void* hmac_sm3_256_ctx, const uint8_t* key, size_t key_size);
bool  hmac_sm3_256_duplicate(const void* hmac_sm3_256_ctx, void* new_hmac_sm3_256_ctx);
bool  hmac_sm3_256_update(void* hmac_sm3_256_ctx, const void* data, size_t data_size);
bool  hmac_sm3_256_final(void* hmac_sm3_256_ctx, uint8_t* hmac_value);
bool  hmac_sm3_256_all(const void*    data,
                       size_t         data_size,
                       const uint8_t* key,
                       size_t         key_size,
                       uint8_t*       hmac_value);

}  // namespace pdk::spdm::platforms::res::crypto::mac