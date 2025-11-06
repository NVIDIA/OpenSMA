

#include "pdk-spdm-app-res-memory-allocate.h"

#include "pdk-spdm-app-res-memory-allocate-plat.h"

namespace pdk::spdm::app::res::memory_allocate {

void* allocate(size_t size)
{
    return pdk::spdm::platforms::res::memory_allocate::allocate(size);
}

void deallocate(void* ptr)
{
    pdk::spdm::platforms::res::memory_allocate::deallocate(ptr);
}

}  // namespace pdk::spdm::app::res::memory_allocate