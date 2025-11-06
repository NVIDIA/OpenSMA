/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES.
 * All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "nv/ipc/queue.h"

#include <algorithm>

#include "nv/common/enum_ops.h"
#include "nv/ut/unittest.h"

using namespace nv::ut;
using namespace nv::ipc;
using namespace std::chrono_literals;
using namespace nv::common::enum_ops;

template<typename T>
T dist()
{
    constexpr auto a = 1103515245ull;
    constexpr auto c = 12345ull;
    constexpr auto m = 2147483648ull;

    static auto seed = 0x1234ull;
    seed             = (a * seed + c) % m;
    return seed;
}

TEST(Queue, Core)
{
    // fake package with random data;
    std::array<uint8_t, 1024> data{}, data2{};

    std::generate(data.begin(), data.end(), [] { return dist<uint8_t>(); });

    // check for all defined queue ids
    for (auto id = QueueId::Test1; id < QueueId::Test8; id++) {
        auto  idx            = nv::common::to_underlying(id);
        auto& q              = Queue::make(id);
        auto& q_another_inst = Queue::make(id);
        ensure::is_eq(&q, &q_another_inst);

        // check id
        ensure::is_eq(q.id(), id);

        // check sizes
        ensure::is_ge(q.max_items(), std::get<1>(QueueInfos[idx]));
        ensure::is_eq(q.item_size(), std::get<2>(QueueInfos[idx]));

        // create a send and recv item
        const Queue::Item item(data.begin(), data.begin() + q.item_size());
        Queue::Item       recv_item(data2.begin(), data2.begin() + q.item_size());

        // fill up the queue
        for (auto j = 0ul; j < q.max_items(); j++) {
            ensure::is_eq(q.size(), j);
            ensure::is_eq(q.available(), q.max_items() - j);
            if (j & 1) {
                ensure::is_eq(q.send(item, 100ms), Queue::Status::Ok);
            }
            else {
                ensure::is_eq(q.send_isr(item), Queue::Status::Ok);
            }
            ensure::is_eq(q.size(), j + 1);
            ensure::is_eq(q.available(), q.max_items() - j - 1);
        }
        // try sending to a full queue
        ensure::is_eq(q.send(item, 1ms), Queue::Status::Full);

        // drain queue
        for (auto j = q.max_items(); j > 0; j--) {
            ensure::is_eq(q.size(), j);
            ensure::is_eq(q.available(), q.max_items() - j);
            // make sure we get the correct data back.
            std::generate(data2.begin(), data2.end(), [&] { return dist<uint8_t>(); });
            if (j & 1) {
                ensure::is_eq(q.recv(recv_item, 100ms), Queue::Status::Ok);
            }
            else {
                ensure::is_eq(q.recv_isr(recv_item), Queue::Status::Ok);
            }
            auto eq = std::equal(data.begin(), data.begin() + recv_item.size(), data2.begin());
            ensure::is_true(eq);
        }
        // try receiving from an empty
        ensure::is_eq(q.recv(recv_item, 1ms), Queue::Status::Timeout);
    }
};

TEST(Queue, ItemSize)
{
    // large
    std::array<uint8_t, 8192> data{};
    auto&                     q = Queue::make(QueueId::Begin);

    Queue::Item item(data.begin(), data.end());
    ensure::is_eq(q.send(item, 1ms), Queue::Status::InvalidParam);
    ensure::is_eq(q.recv(item, 1ms), Queue::Status::InvalidParam);
    ensure::is_eq(q.send_isr(item), Queue::Status::InvalidParam);
    ensure::is_eq(q.recv_isr(item), Queue::Status::InvalidParam);

    // small
    Queue::Item item_small(data.begin(), data.begin() + 2);
    ensure::is_eq(q.send(item, 1ms), Queue::Status::InvalidParam);
    ensure::is_eq(q.recv(item, 1ms), Queue::Status::InvalidParam);
    ensure::is_eq(q.send_isr(item), Queue::Status::InvalidParam);
    ensure::is_eq(q.recv_isr(item), Queue::Status::InvalidParam);
};
