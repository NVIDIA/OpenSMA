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
#include <cstddef>
#include "nv/fw_parser/fw_parser_ap.h"
#include "nv/i2c/lattice_driver.h"
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

// First sizeof(ApFwMetadata) bytes live in CPLD UFM; anything beyond that is
// in the general offset region.
constexpr auto MetadataSize = static_cast<uint32_t>(sizeof(fw_parser::ap::ApFwMetadata));

ApOpErrCode read_cpld_flash(uint32_t start_address, std::span<uint8_t> data)
{
    if constexpr (!i2c::LatticeCpld::is_enabled()) {
        return ApOpErrCode::NotSupported;
    }
    if (data.empty()) {
        return ApOpErrCode::Success;
    }

    auto& cpld = i2c::LatticeCpld::inst();
    if (start_address < MetadataSize) {
        const auto bytes_to_boundary = MetadataSize - start_address;
        const auto metadata_bytes    = std::min(data.size(),
                                             static_cast<std::size_t>(bytes_to_boundary));
        auto       metadata          = data.first(metadata_bytes);
        auto       rc                = from_i2c(cpld.read_ufm(
            metadata.data(), static_cast<uint32_t>(metadata.size()), start_address));
        if (rc != ApOpErrCode::Success || metadata_bytes == data.size()) {
            return rc;
        }

        data          = data.subspan(metadata_bytes);
        start_address = MetadataSize;
    }

    return from_i2c(cpld.read_offset(
        data.data(), start_address - MetadataSize, static_cast<uint32_t>(data.size())));
}

ApOpErrCode write_cpld_flash(uint32_t start_address, std::span<const uint8_t> data)
{
    if constexpr (!i2c::LatticeCpld::is_enabled()) {
        return ApOpErrCode::NotSupported;
    }
    if (data.empty()) {
        return ApOpErrCode::Success;
    }

    auto& cpld = i2c::LatticeCpld::inst();
    if (start_address < MetadataSize) {
        const auto bytes_to_boundary = MetadataSize - start_address;
        const auto metadata_bytes    = std::min(data.size(),
                                             static_cast<std::size_t>(bytes_to_boundary));
        auto       metadata          = data.first(metadata_bytes);
        auto       rc                = from_i2c(cpld.write_ufm(
            metadata.data(), static_cast<uint32_t>(metadata.size()), start_address, false));
        if (rc != ApOpErrCode::Success || metadata_bytes == data.size()) {
            return rc;
        }

        data          = data.subspan(metadata_bytes);
        start_address = MetadataSize;
    }

    return from_i2c(cpld.write_offset(
        data.data(), start_address - MetadataSize, static_cast<uint32_t>(data.size())));
}

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
    if constexpr (!i2c::LatticeCpld::is_enabled()) {
        return ApOpErrCode::NotSupported;
    }
    return from_i2c(i2c::LatticeCpld::inst().isc_enable());
}

ApOpErrCode CpldOps::post_authenticate(const ApInfo& /*ap*/,
                                       nv::spdm::crypto::CryptoStatus result)
{
    if constexpr (!i2c::LatticeCpld::is_enabled()) {
        return ApOpErrCode::NotSupported;
    }
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

ApOpErrCode
CpldOps::read_flash(const ApInfo& /*ap*/, uint32_t start_address, std::span<uint8_t> data)
{
    return read_cpld_flash(start_address, data);
}

ApOpErrCode CpldOps::fw_update_prepare(const ApInfo& /*ap*/)
{
    if constexpr (!i2c::LatticeCpld::is_enabled()) {
        return ApOpErrCode::NotSupported;
    }
    auto& cpld = i2c::LatticeCpld::inst();
    if (auto rc = from_i2c(cpld.isc_enable()); rc != ApOpErrCode::Success) {
        return rc;
    }
    return from_i2c(cpld.erase());
}

ApOpErrCode CpldOps::fw_update_write(const ApInfo& /*ap*/,
                                     uint32_t                 start_address,
                                     std::span<const uint8_t> data)
{
    return write_cpld_flash(start_address, data);
}

ApOpErrCode CpldOps::fw_update_callback(const ApInfo& /*ap*/,
                                        nv::spdm::crypto::CryptoStatus result)
{
    if constexpr (!i2c::LatticeCpld::is_enabled()) {
        return ApOpErrCode::NotSupported;
    }
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

}  // namespace nv::vrot
