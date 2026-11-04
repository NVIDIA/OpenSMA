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
#include <atomic>
#include <cstdint>

namespace nv::thermal {

/**
 * One consistent snapshot of every measured temperature ("the whiteboard").
 *
 * Temperatures are stored in 1/16 C ticks (1 LSB = 0.0625 C) - the TMP468
 * native step - so no precision is lost. Keep everything in this one unit and
 * convert to whole degrees / float only at the point of use (display, etc.).
 */
struct ThermalBoard
{
    static constexpr uint8_t RemoteCount = 8;  // TMP468 remote channels (1..8)
    static constexpr uint8_t GpuDieCount = 2;  // GA, GB

    // Sentinel stored in a slot whose sensor did not read this cycle. INT16_MIN
    // (= -2048 C) is never a real temperature, so a reader can tell "no reading"
    // from a genuine value - same idea as telemetry Cache::InvalidItem.
    static constexpr int16_t InvalidTemp = INT16_MIN;

    int16_t remote[RemoteCount]     = {};  // TMP468 remote diode temps, 1/16 C
    int16_t local                   = 0;   // TMP468 local (board) temp, 1/16 C
    int16_t gpu_tsense[GpuDieCount] = {};  // GPU-reported temps [0]=GA [1]=GB, 1/16 C

    /// Mark every slot "no reading". The producer calls this at the start of a
    /// cycle, then overwrites only the sensors that read OK; the rest stay
    /// InvalidTemp so readers skip them.
    void reset()
    {
        local = InvalidTemp;
        for (auto& value : remote) {
            value = InvalidTemp;
        }
        for (auto& value : gpu_tsense) {
            value = InvalidTemp;
        }
    }
};

/**
 * Running extremes (min/max) of one temperature, in 1/16 C ticks.
 *
 * Defaults make "no data yet" detectable: min starts above max, so valid() is
 * false until the first reading is observed.
 */
struct MinMax
{
    int16_t min = INT16_MAX;
    int16_t max = INT16_MIN;

    bool valid() const { return min <= max; }

    void observe(int16_t ticks)
    {
        if (ticks < min) {
            min = ticks;
        }
        if (ticks > max) {
            max = ticks;
        }
    }
};

/**
 * Per-channel min/max since boot (RAM-backed, so a reset/power cycle clears it),
 * mirroring ThermalBoard's layout.
 *
 * Cumulative across cycles, so this is NOT double-buffered. The thermal task is
 * the sole writer (observe()); readers take a copy via ThermalState::minmax().
 */
struct ThermalMinMax
{
    MinMax remote[ThermalBoard::RemoteCount];
    MinMax local;
    MinMax gpu[ThermalBoard::GpuDieCount];

    /// Fold one cycle's fresh readings into the running extremes, skipping any
    /// channel that did not read this cycle (InvalidTemp).
    void observe(const ThermalBoard& board)
    {
        if (board.local != ThermalBoard::InvalidTemp) {
            local.observe(board.local);
        }
        for (uint8_t i = 0; i < ThermalBoard::RemoteCount; ++i) {
            if (board.remote[i] != ThermalBoard::InvalidTemp) {
                remote[i].observe(board.remote[i]);
            }
        }
        for (uint8_t i = 0; i < ThermalBoard::GpuDieCount; ++i) {
            if (board.gpu_tsense[i] != ThermalBoard::InvalidTemp) {
                gpu[i].observe(board.gpu_tsense[i]);
            }
        }
    }
};

/**
 * Shared thermal data, accessed from both task and ISR context.
 *
 * The I2C target (E368-facing) callback runs in ISR context and must never
 * block, so this is lock-free (no mutex):
 *
 *   - Measured temps -> double-buffered ThermalBoard. The thermal task is the
 *     SOLE writer: it fills writer_buffer() then publish()es it. Readers (the
 *     I2C ISR, fan loop, ZPI) call snapshot() and always get a complete,
 *     internally consistent board. publish()/snapshot() use release/acquire so
 *     the board writes are guaranteed visible before the buffer flip is seen.
 *   - Min/max: running per-channel extremes, folded in by the thermal task
 *     (observe_minmax) and copied out by readers (minmax()). Cumulative, so
 *     it is kept separate from the double-buffered board.
 *
 * Usage:
 *   producer (thermal task, ~50 ms):
 *       auto& b = ThermalState::inst().writer_buffer();
 *       b.reset(); ... fill the sensors that read OK; the rest stay InvalidTemp;
 *       ThermalState::inst().publish();
 *   reader (anywhere, incl. ISR):
 *       ThermalBoard b = ThermalState::inst().snapshot();
 *       if (b.remote[ch] != ThermalBoard::InvalidTemp) use(b.remote[ch]);
 *
 * Init: call inst() once during startup (before interrupts are enabled) so the
 * instance exists before the I2C ISR can touch it.
 */
class ThermalState
{
public:
    static ThermalState& inst();

    // ---- measured temps: producer (thermal task = sole writer) ----

    /// Back buffer for the sole writer to fill (the one readers are NOT on).
    ThermalBoard& writer_buffer() { return board_[back_index()]; }

    /// Make the just-filled back buffer the live one readers see.
    void publish() { active_.store(back_index(), std::memory_order_release); }

    // ---- measured temps: readers (I2C ISR / fan / ZPI) ----

    /// A consistent copy of the live board.
    ThermalBoard snapshot() const { return board_[active_.load(std::memory_order_acquire)]; }

    // ---- min/max: running extremes, written by the thermal task ----

    /// Fold the latest readings into the running min/max. Call once per cycle;
    /// sole writer = thermal task. Only valid (non-InvalidTemp) entries count.
    void observe_minmax(const ThermalBoard& board) { minmax_.observe(board); }

    /// A copy of the running min/max. Single writer, so each field is current
    /// but the copy is not captured at one instant (fine for min/max).
    ThermalMinMax minmax() const { return minmax_; }

    ThermalState(const ThermalState&)            = delete;
    ThermalState& operator=(const ThermalState&) = delete;

private:
    ThermalState() noexcept
    {
        board_[0].reset();
        board_[1].reset();
    }

    /// Index of the buffer the writer may touch (the one that is not live).
    uint8_t back_index() const
    {
        return active_.load(std::memory_order_relaxed) == 0 ? uint8_t{1} : uint8_t{0};
    }

    ThermalBoard         board_[2]{};
    std::atomic<uint8_t> active_{0};  // index (0/1) of the live board
    ThermalMinMax        minmax_{};   // running per-channel extremes

    /// Single instance; defined in state.cpp in plain .bss (not shared RAM) -
    /// safe only while every accessor is privileged (see the note there).
    static ThermalState s_instance;
};

}  // namespace nv::thermal
