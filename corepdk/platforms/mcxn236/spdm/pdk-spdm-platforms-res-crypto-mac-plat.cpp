#include "corepdk/modules/spdm/src/app/crypto/mac/pdk-spdm-app-res-crypto-mac-plat.h"

namespace pdk::spdm::platforms::res::crypto::mac {

void* hmac_sha384_new()
{
    return nullptr;
}
void hmac_sha384_free([[maybe_unused]] void* hmac_sha384_ctx)
{
    return;
}
bool hmac_sha384_set_key([[maybe_unused]] void*          hmac_sha384_ctx,
                         [[maybe_unused]] const uint8_t* key,
                         [[maybe_unused]] size_t         key_size)
{
    return false;
}
bool hmac_sha384_duplicate([[maybe_unused]] const void* hmac_sha384_ctx,
                           [[maybe_unused]] void*       new_hmac_sha384_ctx)
{
    return false;
}
bool hmac_sha384_update([[maybe_unused]] void*       hmac_sha384_ctx,
                        [[maybe_unused]] const void* data,
                        [[maybe_unused]] size_t      data_size)
{
    return false;
}
bool hmac_sha384_final([[maybe_unused]] void*    hmac_sha384_ctx,
                       [[maybe_unused]] uint8_t* hmac_value)
{
    return false;
}
bool hmac_sha384_all([[maybe_unused]] const void*    data,
                     [[maybe_unused]] size_t         data_size,
                     [[maybe_unused]] const uint8_t* key,
                     [[maybe_unused]] size_t         key_size,
                     [[maybe_unused]] uint8_t*       hmac_value)
{
    return false;
}
}  // namespace pdk::spdm::platforms::res::crypto::mac