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
#include "nv/vrot/platform/cpld.h"

#include <algorithm>
#include <chrono>

#include "nv/flash/flash.h"
#include "nv/fw_parser/fw_parser_ap.h"
#include "nv/i2c/lattice_driver.h"
#include "nv/ipc/supervisor.h"
#include "nv/spdm/spdm_crypto_helper.h"
#include NV_IPC_CONFIG_H

// Defined in the project's GPIO glue (e.g. core0/main.cpp). Held via GPIO
// initial state (GpioState::High → inverted → CPLD ProgramN low), deasserted
// via release_reset() once authentication passes.
void clear_cpld_program_pin();

namespace nv::vrot {

namespace {

constexpr ApOpErrCode from_i2c(i2c::I2cStatus status)
{
    return status == i2c::I2cStatus::Ok ? ApOpErrCode::Success : ApOpErrCode::Fail;
}

// CPLD region layout.
//
// - Metadata region (UFM): the first sizeof(ApFwMetadata) bytes of the AP
//   partition. Size is the same across every AP type because the metadata
//   schema in fw_parser_ap.h is shared (statically asserted to 4096).
// - Firmware-data region (offset region): the remainder of the partition,
//   i.e. ap.fw_size - MetadataRegionSize. ap.fw_size is the total partition
//   size that PLDM advertises to BMC via get_fw_info.
constexpr uint32_t MetadataRegionSize = static_cast<uint32_t>(
    sizeof(nv::fw_parser::ap::ApFwMetadata));

// Overflow-safe range check: true iff [offset, offset + len) ⊂ [0, region_size).
// Never computes offset + len directly, so it works for adversarial inputs.
constexpr bool in_region(uint32_t offset, size_t len, uint32_t region_size)
{
    return offset <= region_size && len <= static_cast<size_t>(region_size - offset);
}

constexpr uint32_t fw_data_region_size(const ApInfo& ap)
{
    // Guard a misconfigured partition (fw_size < MetadataRegionSize) by
    // returning 0 — callers can't access any fw-data bytes in that case.
    return ap.fw_size >= MetadataRegionSize ? ap.fw_size - MetadataRegionSize : 0;
}

// Boot-watchdog-feed yield cadence for long CPLD reads.
//
// A single LatticeCpld::read_ufm / read_offset call is a synchronous,
// CPU-bound sequence of per-page I2C transactions. A multi-KB read (e.g.
// the full 4 KB ApFwMetadata, or the multi-100-KB CPLD firmware image
// streamed for SHA384 verification) holds the SPDM task on-CPU long
// enough to starve the lower-priority boot WDT-feed task and trip the
// boot watchdog. read_metadata / read_fw_data below break the request
// into WdtYieldIntervalBytes sub-calls and yield WdtYieldDelay between them.
//
// This concern is specifically a property of the CPLD-on-I2C transport,
// not of the SPDM auth layer above or the Lattice driver below: the auth
// layer doesn't know what AP transport it's using, and the driver doesn't
// know whether its caller is doing one short transaction or streaming a
// whole image. The CPLD platform Ops is the right place to encode it.
//
// Cadence (256 bytes between yields, 10 ms per yield = 2 ticks at the
// 5 ms RTOS tick rate) matches the legacy SPDM-side throttling so total
// auth latency is unchanged.
//
// The write paths (write_metadata / write_fw_data) deliberately do NOT
// use the same chunked-yield pattern: each page program already busy-
// polls the CPLD's flash-program completion through the RTOS-level
// delay inside cpld_write_wait / cpld_write_wait_ufm, which yields to
// the scheduler at least once per page. Reads don't have that natural
// yield because no slave-side wait is involved.
constexpr uint32_t WdtYieldIntervalBytes = 256;
constexpr auto     WdtYieldDelay         = std::chrono::milliseconds(10);

}  // anonymous namespace

ApOpErrCode CpldOps::hold_reset(const ApInfo& /*ap*/)
{
    // No-op: CPLD ProgramN is held low by GPIO initial state at boot, and
    // release_reset() deasserts it via clear_cpld_program_pin(). On recovery
    // re-auth, pre_authenticate enters transparent mode on a running CPLD
    // — accepted by the Lattice debug interface.
    return ApOpErrCode::Success;
}

ApOpErrCode CpldOps::pre_authenticate(const ApInfo& /*ap*/)
{
    return from_i2c(i2c::LatticeCpld::inst().isc_enable());
}

ApOpErrCode CpldOps::post_authenticate(const ApInfo& /*ap*/,
                                       nv::spdm::crypto::CryptoStatus result)
{
    const auto rc = from_i2c(i2c::LatticeCpld::inst().isc_disable());
    if (rc == ApOpErrCode::Success && result != nv::spdm::crypto::CryptoStatus::Success) {
        i2c::LatticeCpld::inst().trigger_vgpio_event();
    }
    return rc;
}

ApOpErrCode CpldOps::release_reset(const ApInfo& /*ap*/)
{
    if constexpr (CPLD_ProgramN_Pin_Enabled) {
        clear_cpld_program_pin();
    }
    return ApOpErrCode::Success;
}

ApOpErrCode CpldOps::check_booted(const ApInfo& /*ap*/)
{
    return ApOpErrCode::Success;
}

ApOpErrCode CpldOps::read_metadata(const ApInfo& /*ap*/,
                                   uint8_t /*slot*/,
                                   uint32_t           metadata_offset,
                                   std::span<uint8_t> data)
{
    // CPLD metadata lives in UFM; address is relative to the start of the
    // metadata region (0 .. sizeof(ApFwMetadata)).
    //
    // ap.fw_offset is intentionally unused for CPLD: each CPLD has its own
    // dedicated UFM and offset region addressed natively via I2C — there is
    // no shared resource that needs an AP-specific base. (For shared-flash
    // APs, e.g. LPU on SPI NOR, the platform Ops would add ap.fw_offset to
    // the caller's region-relative address.)
    if (!in_region(metadata_offset, data.size(), MetadataRegionSize)) {
        return ApOpErrCode::InvalidParam;
    }
    if (data.empty()) {
        return ApOpErrCode::Success;
    }
    // Chunk the request into WdtYieldIntervalBytes pieces and yield between
    // pieces so the boot WDT-feed task doesn't starve. See the comment on
    // WdtYieldIntervalBytes above for rationale.
    size_t remaining = data.size();
    size_t done      = 0;
    while (remaining != 0) {
        const size_t chunk = std::min<size_t>(remaining, WdtYieldIntervalBytes);
        if (i2c::LatticeCpld::inst().read_ufm(data.data() + done,
                                              static_cast<uint32_t>(chunk),
                                              metadata_offset + static_cast<uint32_t>(done))
            != i2c::I2cStatus::Ok) {
            return ApOpErrCode::Fail;
        }
        done      += chunk;
        remaining -= chunk;
        if (remaining != 0) {
            nv::ipc::Supervisor::inst().current_task().delay(WdtYieldDelay);
        }
    }
    return ApOpErrCode::Success;
}

ApOpErrCode CpldOps::read_fw_data(const ApInfo& ap,
                                  uint8_t /*slot*/,
                                  uint32_t           fw_data_offset,
                                  std::span<uint8_t> data)
{
    // CPLD firmware data lives in the offset region; address is relative to
    // the start of the firmware-data region. ap.fw_offset is unused (see
    // read_metadata above for rationale).
    if (!in_region(fw_data_offset, data.size(), fw_data_region_size(ap))) {
        return ApOpErrCode::InvalidParam;
    }
    if (data.empty()) {
        return ApOpErrCode::Success;
    }
    // Chunk the request into WdtYieldIntervalBytes pieces and yield between
    // pieces — same rationale as read_metadata. Note read_offset's argument
    // order is (buf, addr, len), unlike read_ufm's (buf, size, offset).
    size_t remaining = data.size();
    size_t done      = 0;
    while (remaining != 0) {
        const size_t chunk = std::min<size_t>(remaining, WdtYieldIntervalBytes);
        if (i2c::LatticeCpld::inst().read_offset(data.data() + done,
                                                 fw_data_offset + static_cast<uint32_t>(done),
                                                 static_cast<uint32_t>(chunk))
            != i2c::I2cStatus::Ok) {
            return ApOpErrCode::Fail;
        }
        done      += chunk;
        remaining -= chunk;
        if (remaining != 0) {
            nv::ipc::Supervisor::inst().current_task().delay(WdtYieldDelay);
        }
    }
    return ApOpErrCode::Success;
}

ApOpErrCode CpldOps::write_metadata(const ApInfo& /*ap*/,
                                    uint8_t /*slot*/,
                                    uint32_t                 metadata_offset,
                                    std::span<const uint8_t> data)
{
    // CPLD metadata lives in UFM; address is relative to the start of the
    // metadata region (0 .. sizeof(ApFwMetadata)). ap.fw_offset is unused
    // (see read_metadata for rationale).
    if (!in_region(metadata_offset, data.size(), MetadataRegionSize)) {
        return ApOpErrCode::InvalidParam;
    }
    if (data.empty()) {
        return ApOpErrCode::Success;
    }
    return from_i2c(i2c::LatticeCpld::inst().write_ufm(
        data.data(), static_cast<uint32_t>(data.size()), metadata_offset, false));
}

ApOpErrCode CpldOps::write_fw_data(const ApInfo& ap,
                                   uint8_t /*slot*/,
                                   uint32_t                 fw_data_offset,
                                   std::span<const uint8_t> data,
                                   bool /*background_copy*/)
{
    // CPLD firmware data lives in the offset region; address is relative to
    // the start of the firmware-data region. ap.fw_offset is unused (see
    // read_metadata for rationale).
    if (!in_region(fw_data_offset, data.size(), fw_data_region_size(ap))) {
        return ApOpErrCode::InvalidParam;
    }
    if (data.empty()) {
        return ApOpErrCode::Success;
    }
    return from_i2c(i2c::LatticeCpld::inst().write_offset(
        data.data(), fw_data_offset, static_cast<uint32_t>(data.size())));
}

ApOpErrCode CpldOps::fw_update_prepare(const ApInfo& /*ap*/)
{
    auto& cpld = i2c::LatticeCpld::inst();
    if (auto rc = from_i2c(cpld.isc_enable()); rc != ApOpErrCode::Success) {
        return rc;
    }
    return from_i2c(cpld.erase());
}

ApOpErrCode CpldOps::fw_update_callback(const ApInfo& /*ap*/,
                                        nv::spdm::crypto::CryptoStatus result)
{
    if (result != nv::spdm::crypto::CryptoStatus::Success) {
        i2c::LatticeCpld::inst().trigger_vgpio_event();
    }
    return from_i2c(i2c::LatticeCpld::inst().update_complete());
}

ApOpErrCode
CpldOps::set_debug_token_feature(const ApInfo& /*ap*/, DebugTokenFeature feature, bool enable)
{
    if constexpr (!i2c::LatticeCpld::is_enabled()) {
        return ApOpErrCode::NotSupported;
    }
    if (feature != DebugTokenFeature::CpldUnlock) {
        return ApOpErrCode::NotSupported;
    }
    const uint8_t value = enable ? Cpld_User_Reg::MCU_UNLOCK_EN_UNLOCK
                                 : Cpld_User_Reg::MCU_UNLOCK_EN_LOCK;
    return from_i2c(i2c::LatticeCpld::inst().write_debug_bit(value));
}

ApOpErrCode CpldOps::request_authentication(const ApInfo& ap, uint8_t& auth_request_id)
{
    auto status = nv::spdm::crypto::authenticate_ap_firmware(
        ap,
        nv::fw_parser::ap::ParsingApFwType::UpdateSlot,
        auth_request_id,
        nv::ipc::TaskId::Begin);
    return (status == nv::spdm::crypto::CryptoStatus::Success) ? ApOpErrCode::Success
                                                               : ApOpErrCode::Fail;
}

uint8_t CpldOps::get_write_fail_retry(const ApInfo& /*ap*/)
{
    // CPLD performs chip erase, so a single failed page cannot be retried in place.
    return 0;
}

}  // namespace nv::vrot
