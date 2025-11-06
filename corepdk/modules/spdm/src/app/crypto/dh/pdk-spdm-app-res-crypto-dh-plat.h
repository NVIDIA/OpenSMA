#pragma once
#include <cstddef>
#include <cstdint>
#include <span>

namespace pdk::spdm::platforms::res::crypto::dh {

void* dh_new_by_nid(size_t nid);
void  dh_free(void* dh_context);
bool  dh_generate_key(void* dh_context, uint8_t* public_key, size_t* public_key_size);
bool  dh_compute_key(void*          dh_context,
                     const uint8_t* peer_public_key,
                     size_t         peer_public_key_size,
                     uint8_t*       key,
                     size_t*        key_size);

}  // namespace pdk::spdm::platforms::res::crypto::dh