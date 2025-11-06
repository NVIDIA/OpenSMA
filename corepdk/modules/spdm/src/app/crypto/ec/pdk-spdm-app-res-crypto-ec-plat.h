#pragma once
#include <cstddef>
#include <cstdint>
#include <span>

namespace pdk::spdm::platforms::res::crypto::ec {

void* ec_new_by_nid(size_t nid);
void  ec_free(void* ec_context);
bool  ec_set_priv_key(void* ec_context, const uint8_t* private_key, size_t private_key_size);
bool  ec_set_pub_key(void* ec_context, const uint8_t* public_key, size_t public_key_size);
bool  ec_generate_key(void* ec_context, uint8_t* public_key, size_t* public_key_size);
bool  ec_compute_key(void*          ec_context,
                     const uint8_t* peer_public,
                     size_t         peer_public_size,
                     uint8_t*       key,
                     size_t*        key_size);
bool  ec_get_public_key_from_der(const uint8_t* der_data, size_t der_size, void** ec_context);
bool  ecdsa_sign(void*          ec_context,
                 size_t         hash_nid,
                 const uint8_t* message_hash,
                 size_t         hash_size,
                 uint8_t*       signature,
                 size_t*        sig_size);
bool  ecdsa_sign_ex(void*          ec_context,
                    size_t         hash_nid,
                    const uint8_t* message_hash,
                    size_t         hash_size,
                    uint8_t*       signature,
                    size_t*        sig_size,
                    int (*random_func)(void*, unsigned char*, size_t));
bool  ecdsa_verify(void*          ec_context,
                   size_t         hash_nid,
                   const uint8_t* message_hash,
                   size_t         hash_size,
                   const uint8_t* signature,
                   size_t         sig_size);
}  // namespace pdk::spdm::platforms::res::crypto::ec