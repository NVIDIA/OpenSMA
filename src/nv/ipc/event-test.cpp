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
#include "nv/ipc/event.h"

#include "nv/common/enum_ops.h"
#include "nv/common/utils.h"
#include "nv/ipc/supervisor.h"
#include "nv/ut/unittest.h"

using namespace nv;
using namespace nv::ut;
using namespace nv::ipc;
using namespace std::chrono_literals;

namespace {
void test_event()
{
    using namespace common::enum_ops;
    auto isr = sys::ipc::is_in_isr();

    for (auto i = EventId::Test1; i < EventId::Test4; i++) {
        auto& ev              = Event::make(i);
        auto& ev_another_inst = Event::make(i);
        ensure::is_eq(&ev, &ev_another_inst);
        ensure::is_eq(ev.id(), i);

        using Status           = Event::Status;
        constexpr auto Pattern = 0b00110011;
        constexpr auto Mask    = 0b00110001;
        constexpr auto Result  = 0b00000010;
        constexpr auto All     = 0b11111111;
        constexpr auto None    = 0b00000000;
        ensure::is_eq(ev.bits(), None);
        ensure::is_eq(ev.set(Pattern), Status::Ok);
        if (isr) {
            ev.wait(Pattern, false, true, 1ms);
        }
        ensure::is_eq(ev.bits(), Pattern);
        ensure::is_eq(ev.clear(Mask), Status::Ok);
        if (isr) {
            ev.wait(Pattern, false, true, 1ms);
        }
        ensure::is_eq(ev.bits(), Result);
        ensure::is_eq(ev.clear(All), Status::Ok);
        if (isr) {
            ev.wait(Pattern, false, true, 1ms);
        }
        ensure::is_eq(ev.bits(), None);

        Event::Bits    val{};
        constexpr auto nbits = 24;
        for (uint32_t b = 0; b < nbits; b++) {
            ev.set(common::bit(b));
            if (isr) {
                ev.wait(Pattern, false, true, 1ms);
            }
            val |= common::bit(b);
            ensure::is_eq(ev.bits().value(), val);
        }
        for (uint32_t b = 0; b < nbits; b++) {
            ev.clear(common::bit(b));
            if (isr) {
                ev.wait(Pattern, false, true, 1ms);
            }
            val &= common::mask(b);
            ensure::is_eq(ev.bits().value(), val);
        }
    }
}
}  // namespace

TEST(Event, Core)
{
    test_event();
};

TEST(EventISR, Core)
{
    test_event();
};
