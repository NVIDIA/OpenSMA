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
#include "nv/flash/flash.h"

#include <algorithm>
#include <cstring>
#include <limits>

#include "nv/flash/task.h"
#include "nv/ipc/supervisor.h"
#include "nv/nv.h"
#include "nv/logger/log.h"

using namespace nv::flash;
using namespace std::chrono_literals;
namespace {
bool log_status_error(Status status)
{
    return (nv::common::bit(status) & nv::common::to_underlying<Status>(Status::StatusLogMask))
         > 0;
}

// Translate a flash offset for an image booted from Slot 1. The flash remap swaps the two
// windows [0, window) and [slot1, slot1 + window); every other offset in the flash array is
// bank-fixed. Offsets at or beyond the end of the flash array (e.g. the FMC in IFR) are not
// remapped and are returned unchanged, so read/write/erase still reject them.
constexpr Address remap_address(Address address, Address slot1, Address window, Address max)
{
    if (address >= max) {
        return address;
    }
    if (address < window) {
        return address + slot1;
    }
    if ((address >= slot1) && ((address - slot1) < window)) {
        return address - slot1;
    }
    return address;
}

// Both windows must lie inside the flash array so a translated offset never leaves it.
static_assert(RemapSize <= Slot1Address, "remap windows must not overlap");
static_assert(Slot1Address + RemapSize <= MaxAddress, "Slot 1 window must fit in flash");

// The cases below use literal flash offsets so each can be checked against its layout.
// NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers)
// 1 MB layout: 512 KB windows, the whole array is swapped.
static_assert(remap_address(0x00000, 0x80000, 0x80000, 0x100000) == 0x80000);
static_assert(remap_address(0x70000, 0x80000, 0x80000, 0x100000) == 0xF0000);
static_assert(remap_address(0xF0000, 0x80000, 0x80000, 0x100000) == 0x70000);
static_assert(remap_address(0x100000, 0x80000, 0x80000, 0x100000) == 0x100000);
// 2 MB layout: 768 KB windows, Slot 1 at the bank 1 base, the rest is bank-fixed.
static_assert(remap_address(0x00000, 0x100000, 0xC0000, 0x200000) == 0x100000);
static_assert(remap_address(0xBF000, 0x100000, 0xC0000, 0x200000) == 0x1BF000);
static_assert(remap_address(0xC0000, 0x100000, 0xC0000, 0x200000) == 0xC0000);
static_assert(remap_address(0xFE000, 0x100000, 0xC0000, 0x200000) == 0xFE000);
static_assert(remap_address(0x100000, 0x100000, 0xC0000, 0x200000) == 0x00000);
static_assert(remap_address(0x1BF000, 0x100000, 0xC0000, 0x200000) == 0xBF000);
static_assert(remap_address(0x1C0000, 0x100000, 0xC0000, 0x200000) == 0x1C0000);
static_assert(remap_address(0x200000, 0x100000, 0xC0000, 0x200000) == 0x200000);
static_assert(remap_address(0x1008000, 0x100000, 0xC0000, 0x200000) == 0x1008000);
// NOLINTEND(cppcoreguidelines-avoid-magic-numbers)

}  // namespace

Status
Flash::read(Address address, const std::span<uint8_t>& buffer, nv::ipc::Queue::Usecs timeout)
{
    // check if the flash address and buffer size are valid
    if (buffer.size() > BufferSize
        or !((address >= FmcFwAddress && address + buffer.size() <= (FmcFwAddress + FmcFwSize))
             or (address + buffer.size() <= MaxAddress))) {
        return Status::InvalidParam;
    }
    const uintptr_t address_ptr{address};

    if (sys::ipc::is_in_isr() || ipc::Supervisor::inst().current_task_id() == ipc::TaskId::Flash
        || !sys::ipc::is_scheduler_run()) {
        memcpy(buffer.data(), std::bit_cast<uint8_t*>(address_ptr), buffer.size());
        return Status::Ok;
    }

    uint32_t buffer_length = 0;
    if (buffer.size() <= BufferSize) {
        buffer_length = static_cast<uint32_t>(buffer.size());
    }
    const Request       Req = {.header  = RequestHeader(RequestType::Read),
                               .address = address,
                               .length  = buffer_length};
    nv::flash::Response response{};
    auto                status = Task::request(Req, response, timeout);
    if (status == Status::Ok) {
        std::copy_n(response.buffer.begin(), Req.length, buffer.begin());
    }

    if (status != Status::Ok && log_status_error(status)) {
        nv::logger::error_no_wait(
            nv::logger::Event::FlashReqError,
            nv::logger::data_from_two_u32(static_cast<uint32_t>(Req.header.type),
                                          static_cast<uint32_t>(status)),
            nv::logger::OutputDirection::Both,
            0s);
    }

    return status;
}

Status Flash::write(Address                         address,
                    const std::span<const uint8_t>& buffer,
                    nv::ipc::Queue::Usecs           timeout)
{
    // check if the flash address and buffer size are valid
    if (buffer.size() > BufferSize or address >= MaxAddress
        or address + buffer.size() > MaxAddress) {
        return Status::InvalidParam;
    }

    uint32_t buffer_length = 0;
    if (buffer.size() <= BufferSize) {
        buffer_length = static_cast<uint32_t>(buffer.size());
    }

    Request request = {.header  = RequestHeader(RequestType::Write),
                       .address = address,
                       .length  = buffer_length};
    std::copy(buffer.begin(), buffer.end(), request.buffer.begin());
    nv::flash::Response response{};
    auto                status = Task::request(request, response, timeout);

    if (status != Status::Ok && log_status_error(status)) {
        nv::logger::error_no_wait(
            nv::logger::Event::FlashReqError,
            nv::logger::data_from_two_u32(static_cast<uint32_t>(request.header.type),
                                          static_cast<uint32_t>(status)),
            nv::logger::OutputDirection::Both,
            0s);
    }

    return status;
}

Status Flash::erase(Address address, nv::ipc::Queue::Usecs timeout)
{
    // check if the flash address is valid
    if (address > MaxAddress - SectorSize) {
        return Status::InvalidParam;
    }
    const Request       Req = {.header = RequestHeader(RequestType::Erase), .address = address};
    nv::flash::Response response{};
    auto                status = Task::request(Req, response, timeout);

    if (status != Status::Ok && log_status_error(status)) {
        nv::logger::error_no_wait(
            nv::logger::Event::FlashReqError,
            nv::logger::data_from_two_u32(static_cast<uint32_t>(Req.header.type),
                                          static_cast<uint32_t>(status)),
            nv::logger::OutputDirection::Both,
            0s);
    }

    return status;
}

Status Flash::background_copy_start()
{
    auto event  = ipc::Event::make(ipc::EventId::FlashEvent);
    auto status = event.set(EventBits::BackgroundCopyStartEvent);
    if (status != ipc::Event::Status::Ok) {
        return Status::Error;
    }
    return Status::Ok;
}

Status Flash::background_copy_query(ProgressPercent& progress, nv::ipc::Queue::Usecs timeout)
{
    const Request       Req = {.header = RequestHeader(RequestType::BgStatusQuery)};
    nv::flash::Response response{};
    auto                status = Task::request(Req, response, timeout);
    progress                   = response.buffer[0];

    return status;
}

Status Flash::get_data(Key key, Data& data, nv::ipc::Queue::Usecs timeout)
{
    const Request       Req = {.header = RequestHeader(RequestType::DataStoreGet), .key = key};
    nv::flash::Response response{};
    auto                status = Task::request(Req, response, timeout);
    if (status == Status::Ok) {
        static_assert(std::tuple_size_v<decltype(Response::buffer)>
                              * sizeof(decltype(Response::buffer)::value_type)
                          >= sizeof(Data),
                      "invalid Response/Data type sizes");
        std::memcpy(&data, response.buffer.data(), sizeof(Data));
    }

    if (status != Status::Ok && log_status_error(status)) {
        nv::logger::error_no_wait(
            nv::logger::Event::FlashReqError,
            nv::logger::data_from_two_u32(static_cast<uint32_t>(Req.header.type),
                                          static_cast<uint32_t>(status)),
            nv::logger::OutputDirection::Both,
            0s);
    }

    return status;
}

Status Flash::set_data(Key key, const Data& data, nv::ipc::Queue::Usecs timeout)
{
    const Request       Req = {.header = RequestHeader(RequestType::DataStoreSet), .key = key};
    nv::flash::Response response{};
    std::memcpy(std::bit_cast<void*>(&Req.buffer[0]), &data, sizeof(Data));
    auto status = Task::request(Req, response, timeout);

    if (status != Status::Ok && log_status_error(status)) {
        nv::logger::error_no_wait(
            nv::logger::Event::FlashReqError,
            nv::logger::data_from_two_u32(static_cast<uint32_t>(Req.header.type),
                                          static_cast<uint32_t>(status)),
            nv::logger::OutputDirection::Both,
            0s);
    }

    return status;
}

Status Flash::get_data_from_kernel(Key key, Data& data)
{
    // NOLINTBEGIN(cppcoreguidelines*)
    auto& task       = ipc::Supervisor::inst().task(ipc::TaskId::Flash);
    auto& flash_task = static_cast<Task&>(task);
    return flash_task.get_data_from_kernel(key, data);
    // NOLINTEND(cppcoreguidelines*)
}

Status Flash::set_data_from_kernel(Key key, const Data& data)
{
    // NOLINTBEGIN(cppcoreguidelines*)
    auto& task       = ipc::Supervisor::inst().task(ipc::TaskId::Flash);
    auto& flash_task = static_cast<Task&>(task);
    return flash_task.set_data_from_kernel(key, data);
    // NOLINTEND(cppcoreguidelines*)
}

Address Flash::get_flash_address(Address address, nv::bootloader::Driver::ImageIndex boot_index)
{
    if (boot_index == nv::bootloader::Driver::ImageIndex::Image0) {
        return address;
    }
    return remap_address(address, Slot1Address, RemapSize, MaxAddress);
};

Status Flash::write_phrase(Address address, const std::span<uint8_t>& buffer)
{
    Buffer flash_buffer{};
    std::copy(buffer.begin(), buffer.end(), flash_buffer.begin());
    if (buffer.size_bytes() < std::numeric_limits<uint32_t>::max()) {
        const auto Length = static_cast<uint32_t>(buffer.size_bytes());
        return Driver::write_phrase(address, Length, flash_buffer);
    }

    return Status::InvalidParam;
}

Status Flash::check_all_erased(uint32_t address, uint32_t length)
{
    return Driver::check_all_erased(address, length);
}

Status Flash::static_erase(uint32_t address)
{
    return Driver::static_erase(address);
}

Status Flash::read_cfpa(const std::span<uint8_t>& buffer,
                        uint32_t                  offset,
                        nv::ipc::Queue::Usecs     timeout)
{
    if (buffer.size() > BufferSize) {
        return Status::InvalidParam;
    }

    uint32_t buffer_length = 0;
    if (buffer.size() <= BufferSize) {
        buffer_length = static_cast<uint32_t>(buffer.size());
    }

    const Request       Req = {.header = RequestHeader(RequestType::CfpaRead),
                               .offset = offset,
                               .length = buffer_length};
    nv::flash::Response response{};
    auto                status = Task::request(Req, response, timeout);
    if (status == Status::Ok) {
        std::copy_n(response.buffer.begin(), Req.length, buffer.begin());
    }

    if (status != Status::Ok && log_status_error(status)) {
        nv::logger::error_no_wait(
            nv::logger::Event::FlashReqError,
            nv::logger::data_from_two_u32(static_cast<uint32_t>(Req.header.type),
                                          static_cast<uint32_t>(status)),
            nv::logger::OutputDirection::Both,
            0s);
    }

    return status;
}

Status Flash::read_cmpa(const std::span<uint8_t>& buffer,
                        uint32_t                  offset,
                        nv::ipc::Queue::Usecs     timeout)
{
    if (buffer.size() > BufferSize) {
        return Status::InvalidParam;
    }

    uint32_t buffer_length = 0;
    if (buffer.size() <= BufferSize) {
        buffer_length = static_cast<uint32_t>(buffer.size());
    }

    const uint32_t CmpaBaseAddress = 0x1004000;
    // Check for overflow before addition to pass coverity
    if (offset > std::numeric_limits<uint32_t>::max() - CmpaBaseAddress) {
        return Status::InvalidParam;
    }
    const uintptr_t address_ptr{CmpaBaseAddress + offset};

    if (!nv::ipc::Supervisor::is_scheduler_run() || sys::ipc::is_in_isr()
        || ipc::Supervisor::inst().current_task_id() == ipc::TaskId::Flash
        || ipc::Supervisor::inst().current_task_id() == ipc::TaskId::Timer) {
        memcpy(buffer.data(), std::bit_cast<uint8_t*>(address_ptr), buffer.size());
        return Status::Ok;
    }

    const Request       Req = {.header = RequestHeader(RequestType::CmpaRead),
                               .offset = offset,
                               .length = buffer_length};
    nv::flash::Response response{};
    auto                status = Task::request(Req, response, timeout);
    if (status == Status::Ok) {
        std::copy_n(response.buffer.begin(), Req.length, buffer.begin());
    }

    if (status != Status::Ok && log_status_error(status)) {
        nv::logger::error_no_wait(
            nv::logger::Event::FlashReqError,
            nv::logger::data_from_two_u32(static_cast<uint32_t>(Req.header.type),
                                          static_cast<uint32_t>(status)),
            nv::logger::OutputDirection::Both,
            0s);
    }

    return status;
}

Status Flash::write_secure_fw_version(uint32_t              sec_version,
                                      KeyRollbackSelect     key_rollback_select,
                                      nv::ipc::Queue::Usecs timeout)
{
    const Request       Req = {.header = RequestHeader(RequestType::CfpaWriteSecVersion),
                               .sec_key_version     = sec_version,
                               .key_rollback_select = key_rollback_select};
    nv::flash::Response response{};
    auto                status = Task::request(Req, response, timeout);

    if (status != Status::Ok && log_status_error(status)) {
        nv::logger::error_no_wait(
            nv::logger::Event::FlashReqError,
            nv::logger::data_from_two_u32(static_cast<uint32_t>(Req.header.type),
                                          static_cast<uint32_t>(status)),
            nv::logger::OutputDirection::Both,
            0s);
    }

    return status;
}

Status Flash::read_secure_fw_version(uint32_t&             sec_version,
                                     KeyRollbackSelect     key_rollback_select,
                                     nv::ipc::Queue::Usecs timeout)
{
    uint32_t           CfpaSecurityVersionOffset    = 0;
    constexpr uint32_t McuCfpaSecurityVersionOffset = 0x008;
    constexpr uint32_t ReservedCfpaCounterOffset    = 0x090;
    switch (key_rollback_select) {
        case KeyRollbackSelect::Mcu:
            CfpaSecurityVersionOffset = McuCfpaSecurityVersionOffset;
            break;
        case KeyRollbackSelect::Reserved:
            CfpaSecurityVersionOffset = ReservedCfpaCounterOffset;
            break;
        default: return Status::InvalidParam;
    }

    const Request       Req = {.header = RequestHeader(RequestType::CfpaRead),
                               .offset = CfpaSecurityVersionOffset,
                               .length = sizeof(sec_version)};
    nv::flash::Response response{};
    auto                status = Task::request(Req, response, timeout);

    if (status == Status::Ok) {
        sec_version = *std::bit_cast<uint32_t*>(response.buffer.data());
    }

    if (status != Status::Ok && log_status_error(status)) {
        nv::logger::error_no_wait(
            nv::logger::Event::FlashReqError,
            nv::logger::data_from_two_u32(static_cast<uint32_t>(Req.header.type),
                                          static_cast<uint32_t>(status)),
            nv::logger::OutputDirection::Both,
            0s);
    }

    return status;
}

Status Flash::read_key_revoke(uint32_t&             key_revoke,
                              KeyRollbackSelect     key_rollback_select,
                              nv::ipc::Queue::Usecs timeout)
{
    constexpr uint32_t McuCfpaKeyRevokeOffset    = 0x018;
    constexpr uint32_t ReservedCfpaCounterOffset = 0x080;
    // read from cmpa
    uint32_t CfpaKeyRevokeOffset = 0;
    switch (key_rollback_select) {
        case KeyRollbackSelect::Mcu: CfpaKeyRevokeOffset = McuCfpaKeyRevokeOffset; break;
        case KeyRollbackSelect::Reserved:
            CfpaKeyRevokeOffset = ReservedCfpaCounterOffset;
            break;
        default: return Status::InvalidParam;
    }
    const Request       ReqCmpa = {.header = RequestHeader(RequestType::CfpaRead),
                                   .offset = CfpaKeyRevokeOffset,
                                   .length = sizeof(key_revoke)};
    nv::flash::Response response_cmpa{};
    auto                status          = Task::request(ReqCmpa, response_cmpa, timeout);
    uint32_t            key_revoke_cfpa = 0;
    if (status == Status::Ok) {
        key_revoke_cfpa = *std::bit_cast<uint32_t*>(response_cmpa.buffer.data());
    }

    if (status != Status::Ok && log_status_error(status)) {
        nv::logger::error_no_wait(
            nv::logger::Event::FlashReqError,
            nv::logger::data_from_two_u32(static_cast<uint32_t>(ReqCmpa.header.type),
                                          static_cast<uint32_t>(status)),
            nv::logger::OutputDirection::Both,
            0s);
        return status;
    }

    if (key_rollback_select == KeyRollbackSelect::Mcu) {
        // read from efuse
        uint32_t            key_revoke_efues             = 0;
        constexpr uint32_t  ImageKeyRovocationEfuseIndex = 2;
        constexpr uint32_t  ImageKeyRovocationEfuseMask  = 0x0000ff00u;
        const Request       ReqFuse = {.header  = RequestHeader(RequestType::EfuseRead),
                                       .address = ImageKeyRovocationEfuseIndex,
                                       .length  = sizeof(key_revoke)};
        nv::flash::Response response_efuse{};
        status = Task::request(ReqFuse, response_efuse, timeout);
        if (status == Status::Ok) {
            key_revoke_efues = *std::bit_cast<uint32_t*>(response_efuse.buffer.data());
        }

        if (status != Status::Ok && log_status_error(status)) {
            nv::logger::error_no_wait(
                nv::logger::Event::FlashReqError,
                nv::logger::data_from_two_u32(static_cast<uint32_t>(ReqFuse.header.type),
                                              static_cast<uint32_t>(status)),
                nv::logger::OutputDirection::Both,
                0s);
            return status;
        }

        constexpr uint8_t OneByteBits   = 8u;
        constexpr uint8_t ThreeByteBits = 24u;
        // combine value from efuse and cmpa
        key_revoke = (((key_revoke_efues & ImageKeyRovocationEfuseMask) >> OneByteBits)
                      << ThreeByteBits)
                   | key_revoke_cfpa;
    }
    else {
        key_revoke = key_revoke_cfpa;
    }
    return status;
}

Status Flash::write_key_revoke(uint32_t              key_version,
                               KeyRollbackSelect     key_rollback_select,
                               nv::ipc::Queue::Usecs timeout)
{
    const Request       Req = {.header = RequestHeader(RequestType::CfpaWriteKeyRevoke),
                               .sec_key_version     = key_version,
                               .key_rollback_select = key_rollback_select};
    nv::flash::Response response{};
    auto                status = Task::request(Req, response, timeout);

    if (status != Status::Ok && log_status_error(status)) {
        nv::logger::error_no_wait(
            nv::logger::Event::FlashReqError,
            nv::logger::data_from_two_u32(static_cast<uint32_t>(Req.header.type),
                                          static_cast<uint32_t>(status)),
            nv::logger::OutputDirection::Both,
            0s);
    }

    return status;
}

Status Flash::read_cfpa_customer(const std::span<uint8_t>& buffer,
                                 uint32_t                  offset,
                                 nv::ipc::Queue::Usecs     timeout)
{
    if (buffer.size() > BufferSize) {
        return Status::InvalidParam;
    }
    uint32_t buffer_length = 0;
    if (buffer.size() <= BufferSize) {
        buffer_length = static_cast<uint32_t>(buffer.size());
    }
    const Request       Req = {.header = RequestHeader(RequestType::CfpaCustomerRead),
                               .offset = offset,
                               .length = buffer_length};
    nv::flash::Response response{};
    auto                status = Task::request(Req, response, timeout);
    if (status == Status::Ok) {
        std::copy_n(response.buffer.begin(), Req.length, buffer.begin());
    }

    if (status != Status::Ok && log_status_error(status)) {
        nv::logger::error_no_wait(
            nv::logger::Event::FlashReqError,
            nv::logger::data_from_two_u32(static_cast<uint32_t>(Req.header.type),
                                          static_cast<uint32_t>(status)),
            nv::logger::OutputDirection::Both,
            0s);
    }

    return status;
}

Status Flash::read_efuse(Address address, uint32_t& data, nv::ipc::Queue::Usecs timeout)
{
    const Request       Req = {.header  = RequestHeader(RequestType::EfuseRead),
                               .address = address,
                               .length  = sizeof(decltype(data))};
    nv::flash::Response response{};
    auto                status = Task::request(Req, response, timeout);
    if (status == Status::Ok) {
        data = *std::bit_cast<uint32_t*>(response.buffer.data());
    }

    if (status != Status::Ok && log_status_error(status)) {
        nv::logger::error_no_wait(
            nv::logger::Event::FlashReqError,
            nv::logger::data_from_two_u32(static_cast<uint32_t>(Req.header.type),
                                          static_cast<uint32_t>(status)),
            nv::logger::OutputDirection::Both,
            0s);
    }

    return status;
}

Status Flash::program_efuse(Address address, const uint32_t data, nv::ipc::Queue::Usecs timeout)
{
    Request request = {.header  = RequestHeader(RequestType::EfuseProgram),
                       .address = address,
                       .length  = sizeof(decltype(data))};

    *std::bit_cast<uint32_t*>(request.buffer.data()) = data;
    nv::flash::Response response{};
    auto                status = Task::request(request, response, timeout);

    if (status != Status::Ok && log_status_error(status)) {
        nv::logger::error_no_wait(
            nv::logger::Event::FlashReqError,
            nv::logger::data_from_two_u32(static_cast<uint32_t>(request.header.type),
                                          static_cast<uint32_t>(status)),
            nv::logger::OutputDirection::Both,
            0s);
    }

    return status;
}

Status Flash::read_crc(uint32_t& data, nv::ipc::Queue::Usecs timeout)
{
    const Request       request = {.header = RequestHeader(RequestType::CrcRead),
                                   .length = sizeof(decltype(data))};
    nv::flash::Response response{};
    auto                status = Task::request(request, response, timeout);
    if (status == Status::Ok) {
        data = *std::bit_cast<uint32_t*>(response.buffer.data());
    }

    if (status != Status::Ok && log_status_error(status)) {
        nv::logger::error_no_wait(
            nv::logger::Event::FlashReqError,
            nv::logger::data_from_two_u32(static_cast<uint32_t>(request.header.type),
                                          static_cast<uint32_t>(status)),
            nv::logger::OutputDirection::Both,
            0s);
    }

    return status;
}

Status Flash::get_life_cycle(sys::flash::LifeCycleStatus& data, nv::ipc::Queue::Usecs timeout)
{
    const Request       Req = {.header = RequestHeader(RequestType::EfuseLifeCycleRead),
                               .length = sizeof(decltype(data))};
    nv::flash::Response response{};
    auto                status = Task::request(Req, response, timeout);
    if (status == Status::Ok) {
        data = *std::bit_cast<sys::flash::LifeCycleStatus*>(response.buffer.data());
    }

    if (status != Status::Ok && log_status_error(status)) {
        nv::logger::error_no_wait(
            nv::logger::Event::FlashReqError,
            nv::logger::data_from_two_u32(static_cast<uint32_t>(Req.header.type),
                                          static_cast<uint32_t>(status)),
            nv::logger::OutputDirection::Both,
            0s);
    }
    return status;
}

Status Flash::init_on_fault()
{
    return Driver::init_on_fault();
}

Status Flash::read_enf_cnsa(uint32_t& enf_cnsa, nv::ipc::Queue::Usecs timeout)
{
    constexpr uint32_t FuseSecureBootCfgIdx    = 6u;
    constexpr uint32_t EnfCnsaShift            = 8u;
    constexpr uint32_t EnfCnsaMask             = 0x3u << EnfCnsaShift;
    constexpr uint32_t SecureBootCfgCmpaOffset = 0x50u;

    uint32_t            sec_boot_cfg = 0;
    const Request       EfuseReq     = {.header  = RequestHeader(RequestType::EfuseRead),
                                        .address = FuseSecureBootCfgIdx,
                                        .length  = sizeof(sec_boot_cfg)};
    nv::flash::Response response{};
    auto                status = Task::request(EfuseReq, response, timeout);
    if (status == Status::Ok) {
        sec_boot_cfg = *std::bit_cast<uint32_t*>(response.buffer.data());
    }

    if (status != Status::Ok && log_status_error(status)) {
        nv::logger::error_no_wait(
            nv::logger::Event::FlashReqError,
            nv::logger::data_from_two_u32(static_cast<uint32_t>(EfuseReq.header.type),
                                          static_cast<uint32_t>(status)),
            nv::logger::OutputDirection::Both,
            0s);
    }

    if (status != Status::Ok) {
        return status;
    }

    enf_cnsa = (sec_boot_cfg & EnfCnsaMask) >> EnfCnsaShift;
    if (enf_cnsa != 0) {
        return Status::Ok;
    }

    const Request CmpaReq = {.header = RequestHeader(RequestType::CmpaRead),
                             .offset = SecureBootCfgCmpaOffset,
                             .length = sizeof(sec_boot_cfg)};
    response              = {};
    status                = Task::request(CmpaReq, response, timeout);
    if (status == Status::Ok) {
        sec_boot_cfg = *std::bit_cast<uint32_t*>(response.buffer.data());
    }

    if (status != Status::Ok && log_status_error(status)) {
        nv::logger::error_no_wait(
            nv::logger::Event::FlashReqError,
            nv::logger::data_from_two_u32(static_cast<uint32_t>(CmpaReq.header.type),
                                          static_cast<uint32_t>(status)),
            nv::logger::OutputDirection::Both,
            0s);
    }

    if (status != Status::Ok) {
        return status;
    }

    enf_cnsa = (sec_boot_cfg & EnfCnsaMask) >> EnfCnsaShift;
    return Status::Ok;
}

Status Flash::write_cfpa_customer(const std::span<uint8_t>& buffer,
                                  uint32_t                  offset,
                                  nv::ipc::Queue::Usecs     timeout)
{
    if (buffer.size() > BufferSize) {
        return Status::InvalidParam;
    }
    Request request = {.header = RequestHeader(RequestType::CfpaCustomerWrite),
                       .offset = offset,
                       .length = static_cast<uint32_t>(buffer.size())};
    std::copy(buffer.begin(), buffer.end(), request.buffer.begin());
    nv::flash::Response response{};
    auto                status = Task::request(request, response, timeout);
    return status;
}
