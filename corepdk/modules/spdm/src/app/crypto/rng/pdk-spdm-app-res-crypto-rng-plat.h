#pragma once
#include <cstddef>
#include <cstdint>
#include <span>

namespace pdk::spdm::platforms::res::crypto::rng {

bool random_bytes(uint8_t* output, size_t size);

}