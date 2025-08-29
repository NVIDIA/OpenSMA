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
#include "background_copy.h"

#include <algorithm>

#include "nv/common/utils.h"
#include "nv/nv.h"

using namespace nv::flash;

Status BackgroundCopy::start()
{
    if (_state != State::InProgress) {
        current_offset = 0x0;
        _state         = State::InProgress;
        nv::info("Background copy start\n");
    }
    else {
        // Handling TBD, initiate BG during it is on-going
        return Status::Busy;
    }
    return Status::Ok;
}

BackgroundCopy::State BackgroundCopy::state()
{
    return _state;
}

Status BackgroundCopy::state_to_status()
{
    switch (_state) {
        case State::Idle: {
            return Status::BackgroundCopyIdle;
        } break;
        case State::Done: {
            return Status::BackgroundCopyDone;
        } break;
        case State::InProgress: {
            return Status::BackgroundCopyInprogress;
        } break;
        case State::Failed: {
            return Status::BackgroundCopyFailed;
        } break;
        default: break;
    }
    return Status::Error;
}

Status BackgroundCopy::status_progress(Response& response)
{
    const Status  Status   = state_to_status();
    const uint8_t Progress = ((nv::common::mul(current_offset, PercentMultiplier)) / CopySize)
                           & UINT8_MAX;
    response.buffer[0] = Progress;

    return Status;
}

void BackgroundCopy::stop()
{
    _state = State::Idle;
}

void BackgroundCopy::next(Driver& driver)
{
    if (_state != State::InProgress) {
        return;
    }

    const Address ReadAddress  = current_offset;
    const Address WriteAddress = nv::common::add(current_offset, RemapSize);
    if (WriteAddress % SectorSize == 0) {
        auto erase_status = driver.erase(WriteAddress);
        nv::info("Bg erase at 0x%x\n", WriteAddress);
        if (erase_status != Status::Ok) {
            stop();
            nv::info("BG copy erase failed\n");
            _state = State::Failed;
            return;
        }
    }

    const uint32_t CurSize = std::min(static_cast<uint32_t>(CopySize) - current_offset,
                                      static_cast<uint32_t>(BufferSize));

    auto read_status = driver.read(ReadAddress, CurSize, read_buffer);
    if (read_status != Status::Ok) {
        stop();
        nv::info("BG copy read failed\n");
        _state = State::Failed;
        return;
    }

    auto write_status = driver.write(WriteAddress, CurSize, read_buffer);
    if (write_status != Status::Ok) {
        stop();
        nv::info("BG copy write failed\n");
        _state = State::Failed;
        return;
    }

    current_offset = nv::common::add(current_offset, CurSize);

    if (current_offset >= CopySize) {
        stop();
        _state = State::Done;
        nv::info("BG copy done\n");
    }
}
