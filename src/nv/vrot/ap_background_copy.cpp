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
#include "nv/vrot/ap_background_copy.h"

#include <algorithm>
#include <array>
#include <span>

#include "nv/ctimer/ctimer.h"
#include "nv/common/preproc.h"
#include "nv/common/utils.h"
#include "nv/nv.h"
#include "nv/vrot/interface/interface.h"
#include NV_IPC_CONFIG_H

namespace nv::vrot::ap_background_copy {
namespace {

constexpr uint32_t ChunkSize         = 256;
constexpr uint32_t PercentMultiplier = 100;
constexpr uint32_t ReleasedWindowUs  = 1000;

enum class State : uint8_t
{
    Idle,
    Pending,
    InProgress,
    Done,
    Failed,
};

struct Context
{
    State                                                          state = State::Idle;
    ApInfo                                                         ap{};
    bool                                                           has_ap = false;
    std::array<ApBackgroundCopyRegion, ApBackgroundCopyMaxRegions> regions{};
    uint32_t                                                       current_offset = 0;
    uint32_t                                                       region_offset  = 0;
    uint32_t                                                       copy_size      = 0;
    std::size_t                                                    region_index   = 0;
    std::size_t                                                    region_count   = 0;
    uint8_t                                                        source_slot    = Slot0;
    uint8_t                                                        dest_slot      = Slot1;
    bool                                                           resource_held  = false;
};

Context& ctx()
{
    static NV_SHARED_BSS Context instance{};
    return instance;
}

nv::flash::Status state_to_status(State state)
{
    switch (state) {
        case State::Idle      : return nv::flash::Status::BackgroundCopyIdle;
        case State::Done      : return nv::flash::Status::BackgroundCopyDone;
        case State::Pending   : return nv::flash::Status::BackgroundCopyInprogress;
        case State::InProgress: return nv::flash::Status::BackgroundCopyInprogress;
        case State::Failed    : return nv::flash::Status::BackgroundCopyFailed;
    }
    return nv::flash::Status::Error;
}

bool is_valid_slot_pair(uint8_t source_slot, uint8_t dest_slot)
{
    return source_slot <= Slot1 && dest_slot <= Slot1 && source_slot != dest_slot;
}

void release_resource_if_held(Context& context)
{
    if (context.resource_held) {
        if (context.has_ap) {
            (void)background_copy_end(context.ap);
        }
        context.resource_held = false;
    }
}

void mark_failed(Context& context)
{
    release_resource_if_held(context);
    context.state = State::Failed;
}

void mark_done(Context& context)
{
    release_resource_if_held(context);
    context.state = State::Done;
    nv::info("AP bg copy done\n");
}

bool prepare_chunk(Context& context)
{
    if (!context.has_ap) {
        context.state = State::Failed;
        return false;
    }

    if (context.state != State::Pending && context.state != State::InProgress) {
        return false;
    }

    if (!context.resource_held) {
        if (background_copy_begin(context.ap) != ApOpErrCode::Success) {
            context.state = State::Failed;
            return false;
        }
        context.resource_held = true;
    }

    if (context.region_count == 0U) {
        if (background_copy_regions(context.ap,
                                    context.source_slot,
                                    context.regions,
                                    context.region_count,
                                    context.copy_size)
                != ApOpErrCode::Success
            || context.region_count == 0U || context.copy_size == 0U) {
            nv::info("AP bg copy layout failed\n");
            mark_failed(context);
            return false;
        }
    }

    if (context.state == State::Pending) {
        context.state = State::InProgress;
        nv::info("AP bg copy start ap=%u src=%u dst=%u size=%u regions=%u\n",
                 static_cast<unsigned>(context.ap.id),
                 static_cast<unsigned>(context.source_slot),
                 static_cast<unsigned>(context.dest_slot),
                 static_cast<unsigned>(context.copy_size),
                 static_cast<unsigned>(context.region_count));
    }

    return true;
}

ApOpErrCode read_partition(const ApInfo&      ap,
                           uint8_t            slot,
                           uint32_t           partition_offset,
                           std::span<uint8_t> data)
{
    if (data.empty()) {
        return ApOpErrCode::Success;
    }

    const uint32_t end = nv::common::add(partition_offset, static_cast<uint32_t>(data.size()));
    if (end <= ap.metadata_offset) {
        return read_fw_data(ap, slot, partition_offset, data);
    }
    if (partition_offset >= ap.metadata_offset) {
        return read_metadata(ap, slot, partition_offset - ap.metadata_offset, data);
    }

    const uint32_t fw_bytes = ap.metadata_offset - partition_offset;
    const auto     status   = read_fw_data(ap, slot, partition_offset, data.first(fw_bytes));
    if (status != ApOpErrCode::Success) {
        return status;
    }
    return read_metadata(ap, slot, 0, data.subspan(fw_bytes));
}

ApOpErrCode write_partition(const ApInfo&      ap,
                            uint8_t            slot,
                            uint32_t           partition_offset,
                            std::span<uint8_t> data)
{
    if (data.empty()) {
        return ApOpErrCode::Success;
    }

    const uint32_t end = nv::common::add(partition_offset, static_cast<uint32_t>(data.size()));
    if (end <= ap.metadata_offset) {
        return write_fw_data(ap, slot, partition_offset, data, /*background_copy=*/true);
    }
    if (partition_offset >= ap.metadata_offset) {
        return write_metadata(ap, slot, partition_offset - ap.metadata_offset, data);
    }

    const uint32_t fw_bytes = ap.metadata_offset - partition_offset;
    const auto     status   = write_fw_data(
        ap, slot, partition_offset, data.first(fw_bytes), /*background_copy=*/true);
    if (status != ApOpErrCode::Success) {
        return status;
    }
    return write_metadata(ap, slot, 0, data.subspan(fw_bytes));
}

[[maybe_unused]] void next_chunk(std::span<uint8_t, ChunkSize> buffer)
{
    auto& context = ctx();
    if (!prepare_chunk(context) || context.state != State::InProgress) {
        return;
    }

    if (context.region_index >= context.region_count) {
        mark_done(context);
        return;
    }

    const auto& region = context.regions.at(context.region_index);
    if (context.region_offset >= region.size) {
        ++context.region_index;
        context.region_offset = 0;
        if (context.region_index >= context.region_count) {
            mark_done(context);
            return;
        }
    }

    const auto&    current_region = context.regions.at(context.region_index);
    const uint32_t remaining      = current_region.size - context.region_offset;
    if (remaining == 0) {
        mark_done(context);
        return;
    }

    const uint32_t chunk            = std::min(remaining, ChunkSize);
    const uint32_t partition_offset = nv::common::add(current_region.offset,
                                                      context.region_offset);

    if (read_partition(context.ap, context.source_slot, partition_offset, buffer.first(chunk))
        != ApOpErrCode::Success) {
        nv::info("AP bg copy read failed\n");
        mark_failed(context);
        return;
    }

    if (write_partition(context.ap, context.dest_slot, partition_offset, buffer.first(chunk))
        != ApOpErrCode::Success) {
        nv::info("AP bg copy write failed\n");
        mark_failed(context);
        return;
    }

    context.current_offset = nv::common::add(context.current_offset, chunk);
    context.region_offset  = nv::common::add(context.region_offset, chunk);

    if (context.current_offset >= context.copy_size) {
        mark_done(context);
        return;
    }

    release_resource_if_held(context);
    nv::ctimer::Driver::delay_for_us(ReleasedWindowUs);
}

}  // namespace

bool supports([[maybe_unused]] const ApInfo& ap)
{
    if constexpr (ApList.empty()) {
        return false;
    }
    return supports_ap_background_copy(ap);
}

nv::flash::Status start([[maybe_unused]] const ApInfo& ap,
                        [[maybe_unused]] uint8_t       source_slot,
                        [[maybe_unused]] uint8_t       dest_slot)
{
    if constexpr (ApList.empty()) {
        return nv::flash::Status::InvalidParam;
    }

    if (!supports(ap) || !is_valid_slot_pair(source_slot, dest_slot)) {
        return nv::flash::Status::InvalidParam;
    }

    auto& context = ctx();
    if (context.state == State::Pending || context.state == State::InProgress) {
        return nv::flash::Status::Busy;
    }

    context.ap             = ap;
    context.has_ap         = true;
    context.regions        = {};
    context.copy_size      = 0;
    context.region_count   = 0;
    context.region_index   = 0;
    context.source_slot    = source_slot;
    context.dest_slot      = dest_slot;
    context.current_offset = 0;
    context.region_offset  = 0;
    context.state          = State::Pending;
    context.resource_held  = false;

    nv::info("AP bg copy request ap=%u src=%u dst=%u\n",
             static_cast<unsigned>(ap.id),
             static_cast<unsigned>(source_slot),
             static_cast<unsigned>(dest_slot));
    return nv::flash::Status::Ok;
}

nv::flash::Status service()
{
    if constexpr (ApList.empty()) {
        return nv::flash::Status::BackgroundCopyIdle;
    }
    else {
        if (!is_in_progress()) {
            return state_to_status(ctx().state);
        }

        std::array<uint8_t, ChunkSize> buffer{};
        next_chunk(buffer);

        const auto status = state_to_status(ctx().state);
        if (status == nv::flash::Status::BackgroundCopyDone
            || status == nv::flash::Status::BackgroundCopyFailed) {
            nv::info("ap bg end 0x%x\n", status);
        }

        return status;
    }
}

nv::flash::Status query([[maybe_unused]] const ApInfo& ap, nv::flash::ProgressPercent& progress)
{
    if constexpr (ApList.empty()) {
        progress = 0;
        return nv::flash::Status::BackgroundCopyIdle;
    }

    auto& context = ctx();
    progress      = 0;
    if (!context.has_ap || context.ap.id != ap.id
        || context.ap.component_id != ap.component_id) {
        return nv::flash::Status::Error;
    }

    const auto status = state_to_status(context.state);
    if (context.copy_size != 0) {
        progress = static_cast<nv::flash::ProgressPercent>(
            ((nv::common::mul(context.current_offset, PercentMultiplier)) / context.copy_size)
            & UINT8_MAX);
    }
    return status;
}

bool is_in_progress()
{
    if constexpr (ApList.empty()) {
        return false;
    }
    return ctx().state == State::Pending || ctx().state == State::InProgress;
}

void cancel(bool report_failure)
{
    if constexpr (ApList.empty()) {
        return;
    }

    auto& context = ctx();
    release_resource_if_held(context);
    context.current_offset = 0;
    context.region_offset  = 0;
    context.copy_size      = 0;
    context.region_index   = 0;
    context.region_count   = 0;
    context.regions        = {};
    context.state          = report_failure ? State::Failed : State::Idle;
}

}  // namespace nv::vrot::ap_background_copy
