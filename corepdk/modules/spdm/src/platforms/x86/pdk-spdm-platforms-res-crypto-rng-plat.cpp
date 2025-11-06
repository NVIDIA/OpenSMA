#include <cstdlib>
#include <limits>
#include <random>

#include "crypto/rng/pdk-spdm-app-res-crypto-rng-plat.h"
namespace pdk::spdm::platforms::res::crypto::rng {

bool random_bytes(uint8_t* output, size_t size)
{
    std::random_device rd;
    std::mt19937       gen(rd());

    std::uniform_int_distribution<> dis(0, std::numeric_limits<uint8_t>::max());
    for (size_t i = 0; i < size; i++) {
        output[i] = dis(gen);
    }
    return true;
}

}  // namespace pdk::spdm::platforms::res::crypto::rng