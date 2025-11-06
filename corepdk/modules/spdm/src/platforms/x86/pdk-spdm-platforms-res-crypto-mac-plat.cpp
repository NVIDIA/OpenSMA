#include "crypto/mac/pdk-spdm-app-res-crypto-mac-plat.h"

namespace pdk::spdm::platforms::res::crypto::mac {

// NOLINTBEGIN
void* hmac_sha384_new(void)
{
    return nullptr;
}
void hmac_sha384_free(void* hmac_sha384_ctx)
{
    return;
}
bool hmac_sha384_set_key(void* hmac_sha384_ctx, const uint8_t* key, size_t key_size)
{
    return false;
}
bool hmac_sha384_duplicate(const void* hmac_sha384_ctx, void* new_hmac_sha384_ctx)
{
    return false;
}
bool hmac_sha384_update(void* hmac_sha384_ctx, const void* data, size_t data_size)
{
    return false;
}
bool hmac_sha384_final(void* hmac_sha384_ctx, uint8_t* hmac_value)
{
    return false;
}
bool hmac_sha384_all(const void*    data,
                     size_t         data_size,
                     const uint8_t* key,
                     size_t         key_size,
                     uint8_t*       hmac_value)
{
    return false;
}
// NOLINTEND
}  // namespace pdk::spdm::platforms::res::crypto::mac