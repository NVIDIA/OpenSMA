#pragma once
#include <cstddef>

namespace pdk::spdm::app::res::memory_allocate {

void* allocate(size_t size);
void  deallocate(void* ptr);

}  // namespace pdk::spdm::app::res::memory_allocate