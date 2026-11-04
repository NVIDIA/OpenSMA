/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES.
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
#include "nv/i2c/i2c_forward.h"

#include <array>

#include "nv/i2c/task.h"
#include "nv/ipc/queue.h"
#include "nv/logger/log.h"
#include "nv/nv.h"

namespace nv::i2c::forward {

namespace {

bool in_window(const State& state, uint8_t reg)
{
    return reg >= state.base && (reg - state.base) < state.len;
}

}  // namespace

void set_window(State& state, uint8_t base, uint8_t len)
{
    state.base = base;
    state.len  = static_cast<uint8_t>(len > ShadowCapacity ? ShadowCapacity : len);
}

uint8_t shadow_read(const State& state, uint8_t reg)
{
    if (!in_window(state, reg)) {
        return DefaultRegValue;
    }
    return state.shadow.at(static_cast<uint8_t>(reg - state.base));
}

void select_reg(State& state, uint8_t reg)
{
    state.read_reg = reg;
}

uint8_t selected_reg(const State& state)
{
    return state.read_reg;
}

bool on_target_write(State&                   state,
                     nv::ipchandler::Id       dst,
                     uint8_t                  target_address,
                     std::span<const uint8_t> data)
{
    if (dst == nv::ipchandler::Id::Unuse || data.empty()) {
        return false;
    }

    select_reg(state, data[0]);
    if (data.size() == 1) {
        /// register select for a following read: nothing to forward
        return true;
    }

    // NOLINTNEXTLINE(*-const-cast) item is only read by to_i2c_from_isr
    auto item = nv::ipc::Queue::Item(const_cast<uint8_t*>(data.data()), data.size());
    return Task::to_i2c_from_isr(nv::ipchandler::Id::I2cForward,
                                 target_address,
                                 dst,
                                 static_cast<uint8_t>(data.size()),
                                 0,
                                 item);
}

void on_forward_result(State&                     state,
                       nv::i2c::I2cStatus         status,
                       const nv::i2c::I2cRequest& request)
{
    if (status != nv::i2c::I2cStatus::Ok) {
        /// real device did not take the write: keep the shadow untouched so a
        /// read-back exposes the mismatch instead of hiding it. Log only the
        /// first failure after a success so a dead device cannot flood the log.
        if (!state.failing) {
            state.failing = true;
            nv::logger::info(
                nv::logger::Event::I2cForwardWriteFail,
                {request.address, request.write_buffer.at(0), static_cast<uint8_t>(status)});
        }
        return;
    }
    state.failing = false;

    if (request.read_length != 0 || request.write_length < 2) {
        return;
    }

    /// write_buffer[0] is the first register; the supported device class is
    /// single-data-byte writes, extra bytes assume register auto-increment
    for (uint16_t i = 1; i < request.write_length; i++) {
        const auto reg = static_cast<uint8_t>(request.write_buffer[0] + (i - 1U));
        if (in_window(state, reg)) {
            state.shadow.at(static_cast<uint8_t>(reg - state.base)) = request.write_buffer.at(
                i);
        }
    }
}

}  // namespace nv::i2c::forward
