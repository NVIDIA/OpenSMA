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
#pragma once
#include <array>
#include <span>
#include <stddef.h>
#include <stdint.h>

#include "nv/i2c/common.h"
#include "nv/ipchandler/enums.h"

namespace nv::i2c::forward {

/// Generic I2C target forward: writes received on a claimed target address
/// are queued verbatim to the I2C task owning the bus where the real device
/// lives (SlaveFunctionEntry::forward_dst); reads are served from a local
/// shadow that is synced on each successfully forwarded write.
///
/// Supported device class: 8-bit register addressing, single-data-byte writes.
/// For anything fancier (16-bit registers, read auto-increment, unlock
/// sequences) add a dedicated SlaveFunction instead - see the EEPROM bridge
/// precedent in sys/i2c.
///
/// First user: E4188 GPU-to-DAC margining bridge, DS4424 on MCU_I2C5
/// (bug 6400342).

/// Upper bound of the shadowed register window; the per-project window
/// (SlaveFunctionEntry::shadow_base/shadow_len) is clamped to this.
constexpr size_t ShadowCapacity = 8;

/// Served for reads outside the shadowed window or before it is configured
constexpr uint8_t DefaultRegValue = 0x00;

/// Cross-context state of one forward instance: the shadow, its window and
/// the selected read register. Lives inside the destination I2C task's private MPU
/// block (nv::i2c::Task::forward_state()), following the EEPROM bridge
/// cache precedent: the owning task syncs it from task context and the
/// source-port target ISR reads it in handler (privileged) mode, so no
/// shared RAM is involved. The ISR locates the owning task on demand via
/// the per-project weak sys::i2c::forward_task_id() (EepromTaskId pattern).
struct State
{
    std::array<uint8_t, ShadowCapacity> shadow;
    uint8_t                             base;  ///< shadowed window start register
    uint8_t                             len;   ///< window length; 0 until set_window latches it
    uint8_t read_reg;  ///< register served on target reads, selected by the last write
    bool    failing;   ///< forward-failure latch for transition logging
};

/// Latch the shadowed window from the matched table entry. Called on every
/// address match of the forwarded target, so it must be idempotent and
/// ISR-safe (it is: two byte stores).
void set_window(State& state, uint8_t base, uint8_t len);

/// Shadow lookup for the target read path. ISR-safe, no side effects.
uint8_t shadow_read(const State& state, uint8_t reg);

/// Register selected by the last write; target reads are served from its
/// shadow (the device-side "register select" behaviour, a plain uint8_t
/// register number).
void    select_reg(State& state, uint8_t reg);
uint8_t selected_reg(const State& state);

/// Handle a completed target write: data[0] = register, data[1..] = payload.
/// A 1-byte write only selects the register for a following read. Longer
/// writes are queued verbatim to the I2C task selected by \p dst. ISR-safe.
/// Returns false when disabled (dst == Unuse, empty data) or the queue
/// rejects it.
bool on_target_write(State&                   state,
                     nv::ipchandler::Id       dst,
                     uint8_t                  target_address,
                     std::span<const uint8_t> data);

/// Consume the result of a forwarded request. Runs in the destination I2C
/// task (the owner of \p state) right after the bus transaction; syncs the
/// shadow on success.
void on_forward_result(State&                     state,
                       nv::i2c::I2cStatus         status,
                       const nv::i2c::I2cRequest& request);

}  // namespace nv::i2c::forward
