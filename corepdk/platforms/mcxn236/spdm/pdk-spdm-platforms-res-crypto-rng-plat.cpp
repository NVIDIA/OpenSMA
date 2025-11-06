#include "corepdk/modules/spdm/src/app/crypto/rng/pdk-spdm-app-res-crypto-rng-plat.h"
#include "mbedtls/ctr_drbg.h"

namespace pdk::spdm::platforms::res::crypto::rng {

bool random_bytes(uint8_t* output, size_t size)
{
    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_ctr_drbg_init(&ctr_drbg);

    int ret = 0;
    ret     = mbedtls_ctr_drbg_random(&ctr_drbg, output, size);
    if (ret != 0) {
        return false;
    }
    mbedtls_ctr_drbg_free(&ctr_drbg);

    return true;
}

}  // namespace pdk::spdm::platforms::res::crypto::rng