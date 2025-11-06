#include "crypto/rng/pdk-spdm-app-res-crypto-rng.h"

namespace pdk::spdm::app::res::crypto::rng {

#ifdef __cplusplus
extern "C" {
#endif

/*=====================================================================================
 *    Random Number Generation Primitive
 *=====================================================================================*/

/**
 * Generates a random byte stream of the specified size. If initialization, testing, or seeding
 *of the (pseudo)random number generator is required it should be done before this function is
 *called.
 *
 * If output is NULL, then return false.
 * If this interface is not supported, then return false.
 *
 * @param[out]  output  Pointer to buffer to receive random value.
 * @param[in]   size    Size of random bytes to generate.
 *
 * @retval true   Random byte stream generated successfully.
 * @retval false  Generation of random byte stream failed.
 * @retval false  This interface is not supported.
 **/
extern bool libspdm_random_bytes(uint8_t* output, size_t size)
{
    return pdk::spdm::platforms::res::crypto::rng::random_bytes(output, size);
}

#ifdef __cplusplus
}
#endif

}  // namespace pdk::spdm::app::res::crypto::rng
