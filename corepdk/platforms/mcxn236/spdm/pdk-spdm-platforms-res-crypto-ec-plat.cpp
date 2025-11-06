#include <cstddef>
#include <cstdint>
#include <span>
#include "corepdk/modules/spdm/src/app/crypto/ec/pdk-spdm-app-res-crypto-ec.h"
#include "corepdk/modules/spdm/src/app/pdk-spdm-app-res-memory-allocate-plat.h"
#include "corepdk/modules/spdm/src/app/libspdm/include/hal/library/cryptlib.h"
#include "nv/spdm/spdm_crypto_helper.h"

namespace pdk::spdm::platforms::res::crypto::ec {

class EcdsaContext
{
    // current only support P-384
    // std::array<uint8_t, 384 / 8> private_key;
};

void* ec_new_by_nid(size_t nid)
{
    if (nid == LIBSPDM_CRYPTO_NID_ECDSA_NIST_P384) {
        return pdk::spdm::platforms::res::memory_allocate::allocate(sizeof(EcdsaContext));
    }
    else {
        return nullptr;
    }
}
void ec_free(void* ec_context)
{
    if (ec_context != nullptr) {
        pdk::spdm::platforms::res::memory_allocate::deallocate(ec_context);
    }
    return;
}
bool ec_get_public_key_from_der([[maybe_unused]] const uint8_t* der_data,
                                [[maybe_unused]] size_t         der_size,
                                [[maybe_unused]] void**         ec_context)
{
    return false;
}
bool ecdsa_sign(void*          ec_context,
                size_t         hash_nid,
                const uint8_t* message_hash,
                size_t         hash_size,
                uint8_t*       signature,
                size_t*        sig_size)
{
    auto ecdsa_context = static_cast<EcdsaContext*>(ec_context);
    if (ecdsa_context == nullptr) {
        return false;
    }
    // only support SHA384
    if (hash_nid != LIBSPDM_CRYPTO_NID_SHA384) {
        return false;
    }
    constexpr size_t expected_hash_size = 384 / 8;
    if (hash_size != expected_hash_size) {
        return false;
    }
    const size_t input_sig_size = *sig_size;
    if (input_sig_size < expected_hash_size * 2) {
        return false;
    }
    *sig_size = expected_hash_size * 2;
    return nv::spdm::crypto::CryptoStatus::Success
        == nv::spdm::crypto::spdm_ecdsa_sign(signature, *sig_size, message_hash, hash_size);
}

bool ecdsa_verify([[maybe_unused]] void*          ec_context,
                  [[maybe_unused]] size_t         hash_nid,
                  [[maybe_unused]] const uint8_t* message_hash,
                  [[maybe_unused]] size_t         hash_size,
                  [[maybe_unused]] const uint8_t* signature,
                  [[maybe_unused]] size_t         sig_size)
{
    return false;
}

}  // namespace pdk::spdm::platforms::res::crypto::ec