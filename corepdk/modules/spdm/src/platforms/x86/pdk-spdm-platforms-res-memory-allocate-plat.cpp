#include <cstdlib>

#include "pdk-spdm-app-res-memory-allocate-plat.h"

namespace pdk::spdm::platforms::res::memory_allocate {
// NOLINTBEGIN
void* allocate(size_t size)
{
    return malloc(size);
}

void deallocate(void* ptr)
{
    free(ptr);
}
// NOLINTEND
}  // namespace pdk::spdm::platforms::res::memory_allocate