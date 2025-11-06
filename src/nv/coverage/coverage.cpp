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
#include <cstring>
#include "nv/coverage/coverage.h"
#include "nv/coverage/common.h"
#include "nv/mctp/vendor.h"

using namespace nv::coverage;
using namespace nv::mctp;

#ifdef NV_COVERAGE
// NOLINTBEGIN
extern const GcovInfo* const __gcov_info_start[];
extern const GcovInfo* const __gcov_info_end[];
// NOLINTEND

uint8_t Coverage::download(const Packet& rx, Packet& tx)
{
    auto& vrx = VendorPktReq::from(rx);
    auto& vtx = VendorPktRes::from(tx);

    constexpr uint8_t HeaderSizeResponse         = 10;
    constexpr uint8_t DownloadCoverageHeaderSize = 8;

    auto counter_active = [](const GcovInfo* const info, auto type) {
        return info->merge[type] != nullptr;
    };

    // Use DownloadCoverageSession to represent the state
    DownloadCoverageSession session(static_cast<const uint8_t*>(vrx.data));
    auto&                   coverage_phase    = session.coverage_phase;
    auto&                   gcovinfo_index    = session.gcovinfo_index;
    auto&                   file_name_index   = session.file_name_index;
    auto&                   gcovfninfo_index  = session.gcovfninfo_index;
    auto&                   gcovcounter_index = session.gcovcounter_index;
    auto&                   gcovctrinfo_index = session.gcovctrinfo_index;

    if (coverage_phase == DownloadCoverageSession::CoveragePhase::Invalid) {
        vtx.completion_code = Ccode::ErrorInvalidData;
        return 0;
    }

    auto       info_start = &__gcov_info_start[0];
    const auto info_end   = &__gcov_info_end[0];

    // when all files are done, go to finish
    if (info_start + gcovinfo_index == info_end) {
        session.coverage_phase = DownloadCoverageSession::CoveragePhase::Finish;
        session.to_data(static_cast<uint8_t*>(vtx.data));
        vtx.completion_code = Ccode::Success;
        return DownloadCoverageHeaderSize;
    }
    // check gcovinfo_index is valid and get gcovinfo
    if (info_start + gcovinfo_index > info_end) {
        vtx.completion_code = Ccode::ErrorInvalidData;
        return 0;
    }
    const GcovInfo* info = *(info_start + gcovinfo_index);
    if (info == nullptr) {
        vtx.completion_code = Ccode::ErrorInvalidData;
        return 0;
    }

    if (coverage_phase == DownloadCoverageSession::CoveragePhase::FileName) {
        const uint8_t filename_length = strlen(info->filename) + 1;  // include '\0'
        // if remaining file name exceed the transmit unit
        const uint8_t remaining_length = filename_length - file_name_index;
        const uint8_t available_length = pdk::mctp::platforms::TransmitUnit - HeaderSizeResponse
                                       - DownloadCoverageHeaderSize;
        if (remaining_length > available_length) {
            // copy filename start from file name index to file_name_index + available_length
            memcpy(&vtx.data[DownloadCoverageHeaderSize],
                   info->filename + file_name_index,
                   available_length);
            session.coverage_phase  = DownloadCoverageSession::CoveragePhase::FileName;
            session.file_name_index = file_name_index + available_length;
            session.to_data(static_cast<uint8_t*>(vtx.data));
            vtx.completion_code = Ccode::Success;
            return DownloadCoverageHeaderSize + available_length;
        }
        else {
            memcpy(&vtx.data[DownloadCoverageHeaderSize],
                   info->filename + file_name_index,
                   remaining_length);
            session.coverage_phase  = DownloadCoverageSession::CoveragePhase::GcovInfo;
            session.file_name_index = 0;
            session.to_data(static_cast<uint8_t*>(vtx.data));
            vtx.completion_code = Ccode::Success;
            return DownloadCoverageHeaderSize + remaining_length;
        }
    }
    if (coverage_phase == DownloadCoverageSession::CoveragePhase::GcovInfo) {
        memcpy(&vtx.data[DownloadCoverageHeaderSize], &info->version, sizeof(uint32_t));
        memcpy(&vtx.data[DownloadCoverageHeaderSize + sizeof(uint32_t)],
               &info->stamp,
               sizeof(uint32_t));
        memcpy(&vtx.data[DownloadCoverageHeaderSize + sizeof(uint32_t) * 2],
               &info->checksum,
               sizeof(uint32_t));
        memcpy(&vtx.data[DownloadCoverageHeaderSize + sizeof(uint32_t) * 3],
               &info->n_functions,
               sizeof(uint32_t));
        session.coverage_phase = DownloadCoverageSession::CoveragePhase::GcovFnInfo;
        session.to_data(static_cast<uint8_t*>(vtx.data));
        vtx.completion_code = Ccode::Success;
        return DownloadCoverageHeaderSize + sizeof(uint32_t) * 4;
    }
    auto fptr = info->functions[gcovfninfo_index];
    if (coverage_phase == DownloadCoverageSession::CoveragePhase::GcovFnInfo) {
        if (gcovfninfo_index > info->n_functions) {
            vtx.completion_code = Ccode::ErrorInvalidData;
            return 0;
        }
        // when in GcovFnInfo stage, gcovfninfo_index == info->n_functions means all functions
        // are done, go to next file, don't send any data except the session info
        if (gcovfninfo_index == info->n_functions) {
            session.coverage_phase   = DownloadCoverageSession::CoveragePhase::FileName;
            session.gcovinfo_index   = gcovinfo_index + 1;
            session.gcovfninfo_index = 0;
            tx.priv.packet_length    = +DownloadCoverageHeaderSize;
            session.to_data(static_cast<uint8_t*>(vtx.data));
            vtx.completion_code = Ccode::Success;
            return DownloadCoverageHeaderSize;
        }
        else {
            session.coverage_phase   = DownloadCoverageSession::CoveragePhase::GcovCounterInfo;
            session.gcovfninfo_index = gcovfninfo_index;
            memcpy(&vtx.data[DownloadCoverageHeaderSize], &fptr->ident, sizeof(uint32_t));
            memcpy(&vtx.data[DownloadCoverageHeaderSize + sizeof(uint32_t)],
                   &fptr->lineno_checksum,
                   sizeof(uint32_t));
            memcpy(&vtx.data[DownloadCoverageHeaderSize + sizeof(uint32_t) * 2],
                   &fptr->cfg_checksum,
                   sizeof(uint32_t));
            tx.priv.packet_length = +DownloadCoverageHeaderSize + sizeof(uint32_t) * 3;
            session.to_data(static_cast<uint8_t*>(vtx.data));
            vtx.completion_code = Ccode::Success;
            return DownloadCoverageHeaderSize + sizeof(uint32_t) * 3;
        }
    }
    auto cptr = static_cast<const gcov_ctr_info*>(fptr->ctrs);
    if (coverage_phase == DownloadCoverageSession::CoveragePhase::GcovCounterInfo) {
        // check gcovcounter_index is valid
        if (gcovcounter_index > GcovCounters) {
            vtx.completion_code = Ccode::ErrorInvalidData;
            return 0;
        }
        // when in GcovCounterInfo stage, gcovcounter_index == GcovCounters means all counters
        // are done, go to next gcovfninfo and don't send any data except the session info
        if (gcovcounter_index == GcovCounters) {
            session.coverage_phase    = DownloadCoverageSession::CoveragePhase::GcovFnInfo;
            session.gcovfninfo_index  = gcovfninfo_index + 1;
            session.gcovcounter_index = 0;
            session.to_data(static_cast<uint8_t*>(vtx.data));
            vtx.completion_code = Ccode::Success;
            return DownloadCoverageHeaderSize;
        }
        else {
            if (!counter_active(info, gcovcounter_index)) {
                // if counter is not active, don't send any data except the session info
                // and go to the next gcovcounter
                session
                    .coverage_phase = DownloadCoverageSession::CoveragePhase::GcovCounterInfo;
                session.gcovcounter_index = gcovcounter_index + 1;
                session.to_data(static_cast<uint8_t*>(vtx.data));
                vtx.completion_code = Ccode::Success;
                return DownloadCoverageHeaderSize;
            }
            else {
                session.coverage_phase    = DownloadCoverageSession::CoveragePhase::GcovCtrInfo;
                session.gcovcounter_index = gcovcounter_index;
                uint32_t v = GcovTagCounterBase + (gcovcounter_index << GcovTagForCounterShift);
                memcpy(&vtx.data[DownloadCoverageHeaderSize], &v, sizeof(uint32_t));
                v = cptr->num * 2 * GcovUnitSize;
                memcpy(&vtx.data[DownloadCoverageHeaderSize + sizeof(uint32_t)],
                       &v,
                       sizeof(uint32_t));
                memcpy(&vtx.data[DownloadCoverageHeaderSize + sizeof(uint32_t) * 2],
                       &cptr->num,
                       sizeof(uint32_t));
                tx.priv.packet_length = +DownloadCoverageHeaderSize + sizeof(uint32_t) * 3;
                session.to_data(static_cast<uint8_t*>(vtx.data));
                vtx.completion_code = Ccode::Success;
                return DownloadCoverageHeaderSize + sizeof(uint32_t) * 3;
            }
        };
    }
    if (coverage_phase == DownloadCoverageSession::CoveragePhase::GcovCtrInfo) {
        if (gcovctrinfo_index >= cptr->num) {
            vtx.completion_code = Ccode::ErrorInvalidData;
            return 0;
        }
        memcpy(&vtx.data[DownloadCoverageHeaderSize],
               &cptr->values[gcovctrinfo_index],
               sizeof(gcov_type));
        // when in GcovCtrInfo stage, gcovctrinfo_index == cptr->num -1 means this is the
        // last counter, go to next gcovcounter after this
        if (gcovctrinfo_index == cptr->num - 1) {
            session.coverage_phase    = DownloadCoverageSession::CoveragePhase::GcovCounterInfo;
            session.gcovcounter_index = gcovcounter_index + 1;
            session.gcovctrinfo_index = 0;
        }
        else {
            session.coverage_phase    = DownloadCoverageSession::CoveragePhase::GcovCtrInfo;
            session.gcovctrinfo_index = gcovctrinfo_index + 1;
        }
        session.to_data(static_cast<uint8_t*>(vtx.data));
        vtx.completion_code = Ccode::Success;
        return DownloadCoverageHeaderSize + sizeof(gcov_type);
    }
    return 0;
}
#endif