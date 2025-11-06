#include "corepdk/modules/spdm/src/app/pdk-spdm-app-res-memory-allocate-plat.h"
#include "nv/spdm/task.h"
#include "nv/logger/log.h"
#include <utility>

namespace pdk::spdm::platforms::res::memory_allocate {

void* allocate(size_t size)
{
    auto memory_pool_items = std::tie(
        nv::spdm::Task::get_task().crypto_context_allocate_item,
        nv::spdm::Task::get_task().spdm_context_allocate_item,
        nv::spdm::Task::get_task().spdm_scratch_buffer_allocate_item);
    auto allcate_from_memory_pool = [&](auto& memory_pool) -> void* {
        for (auto& memory_unit : memory_pool) {
            if (memory_unit.used_size == 0 && memory_unit.data.size() >= size) {
                memory_unit.used_size = size;
                return memory_unit.data.data();
            }
        }
        return nullptr;
    };
    auto allocate_address = std::apply(
        [&](auto&&... memory_pool_items) -> void* {
            void* result       = nullptr;
            auto  try_allocate = [&](auto& pool) {
                if (result == nullptr) {
                    result = allcate_from_memory_pool(pool);
                }
                return false;
            };
            (try_allocate(memory_pool_items) || ...);
            return result;
        },
        memory_pool_items);

    return allocate_address;
}

void deallocate(void* ptr)
{
    auto memory_pool_items = std::tie(
        nv::spdm::Task::get_task().crypto_context_allocate_item,
        nv::spdm::Task::get_task().spdm_context_allocate_item,
        nv::spdm::Task::get_task().spdm_scratch_buffer_allocate_item);

    auto deallocate_from_memory_pool = [&](auto& memory_pool) -> bool {
        for (auto& memory_unit : memory_pool) {
            if (memory_unit.data.data() == ptr) {
                memory_unit.data.fill(0);
                memory_unit.used_size = 0;
                return true;
            }
        }
        return false;
    };

    std::apply(
        [&](auto&&... memory_pool_items) {
            ((deallocate_from_memory_pool(memory_pool_items)) || ...);
            return;
        },
        memory_pool_items);

    return;
}

}  // namespace pdk::spdm::platforms::res::memory_allocate