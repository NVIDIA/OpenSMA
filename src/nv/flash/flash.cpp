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

    if (sys::ipc::is_in_isr()) {
        memcpy(buffer.data(),
               std::bit_cast<uint8_t*>(buffer.data() - buffer.data())
                   + static_cast<size_t>(address),
               buffer.size());
        return Status::Ok;
    }

    uint32_t buffer_length = 0;
    if (buffer.size() <= BufferSize) {
        buffer_length = static_cast<uint32_t>(buffer.size());
    }
    const Request Req = {
        .type = RequestType::Read, .address = address, .length = buffer_length};
    nv::flash::Response response{};
    auto                status = Task::request(Req, response, timeout);
    if (status == Status::Ok) {
        std::copy_n(response.buffer.begin(), Req.length, buffer.begin());
    }

    if (status != Status::Ok && log_status_error(status)) {
        nv::logger::error_no_wait(nv::logger::Event::FlashReqError,
                                  nv::logger::data_from_two_u32(static_cast<uint32_t>(Req.type),
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

    Request request = {.type = RequestType::Write, .address = address, .length = buffer_length};
    std::copy(buffer.begin(), buffer.end(), request.buffer.begin());
    nv::flash::Response response{};
    auto                status = Task::request(request, response, timeout);

    if (status != Status::Ok && log_status_error(status)) {
        nv::logger::error_no_wait(
            nv::logger::Event::FlashReqError,
            nv::logger::data_from_two_u32(static_cast<uint32_t>(request.type),
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
    const Request       Req = {.type = RequestType::Erase, .address = address};
    nv::flash::Response response{};
    auto                status = Task::request(Req, response, timeout);

    if (status != Status::Ok && log_status_error(status)) {
        nv::logger::error_no_wait(nv::logger::Event::FlashReqError,
                                  nv::logger::data_from_two_u32(static_cast<uint32_t>(Req.type),
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
    const Request       Req = {.type = RequestType::BgStatusQuery};
    nv::flash::Response response{};
    auto                status = Task::request(Req, response, timeout);
    progress                   = response.buffer[0];

    return status;
}

Status Flash::get_data(Key key, Data& data, nv::ipc::Queue::Usecs timeout)
{
    const Request       Req = {.type = RequestType::DataStoreGet, .key = key};
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
        nv::logger::error_no_wait(nv::logger::Event::FlashReqError,
                                  nv::logger::data_from_two_u32(static_cast<uint32_t>(Req.type),
                                                                static_cast<uint32_t>(status)),
                                  nv::logger::OutputDirection::Both,
                                  0s);
    }

    return status;
}

Status Flash::set_data(Key key, const Data& data, nv::ipc::Queue::Usecs timeout)
{
    const Request       Req = {.type = RequestType::DataStoreSet, .key = key};
    nv::flash::Response response{};
    std::memcpy(std::bit_cast<void*>(&Req.buffer[0]), &data, sizeof(Data));
    auto status = Task::request(Req, response, timeout);

    if (status != Status::Ok && log_status_error(status)) {
        nv::logger::error_no_wait(nv::logger::Event::FlashReqError,
                                  nv::logger::data_from_two_u32(static_cast<uint32_t>(Req.type),
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
    if (address < RemapSize) {
        return address + RemapSize;
    }
    else {
        return address - RemapSize;
    }
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

    const Request Req = {
        .type = RequestType::CfpaRead, .offset = offset, .length = buffer_length};
    nv::flash::Response response{};
    auto                status = Task::request(Req, response, timeout);
    if (status == Status::Ok) {
        std::copy_n(response.buffer.begin(), Req.length, buffer.begin());
    }

    if (status != Status::Ok && log_status_error(status)) {
        nv::logger::error_no_wait(nv::logger::Event::FlashReqError,
                                  nv::logger::data_from_two_u32(static_cast<uint32_t>(Req.type),
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

    const Request Req = {
        .type = RequestType::CmpaRead, .offset = offset, .length = buffer_length};
    nv::flash::Response response{};
    auto                status = Task::request(Req, response, timeout);
    if (status == Status::Ok) {
        std::copy_n(response.buffer.begin(), Req.length, buffer.begin());
    }

    if (status != Status::Ok && log_status_error(status)) {
        nv::logger::error_no_wait(nv::logger::Event::FlashReqError,
                                  nv::logger::data_from_two_u32(static_cast<uint32_t>(Req.type),
                                                                static_cast<uint32_t>(status)),
                                  nv::logger::OutputDirection::Both,
                                  0s);
    }

    return status;
}

Status Flash::write_secure_fw_version(uint32_t sec_version, nv::ipc::Queue::Usecs timeout)
{
    const Request       Req = {.type            = RequestType::CfpaWriteSecVersion,
                               .sec_key_version = sec_version};
    nv::flash::Response response{};
    auto                status = Task::request(Req, response, timeout);

    if (status != Status::Ok && log_status_error(status)) {
        nv::logger::error_no_wait(nv::logger::Event::FlashReqError,
                                  nv::logger::data_from_two_u32(static_cast<uint32_t>(Req.type),
                                                                static_cast<uint32_t>(status)),
                                  nv::logger::OutputDirection::Both,
                                  0s);
    }

    return status;
}

Status Flash::read_secure_fw_version(uint32_t& sec_version, nv::ipc::Queue::Usecs timeout)
{
    constexpr uint32_t  CfpaSecurityVersionOffset = 0x008;
    const Request       Req                       = {.type   = RequestType::CfpaRead,
                                                     .offset = CfpaSecurityVersionOffset,
                                                     .length = sizeof(sec_version)};
    nv::flash::Response response{};
    auto                status = Task::request(Req, response, timeout);

    if (status == Status::Ok) {
        sec_version = *std::bit_cast<uint32_t*>(response.buffer.data());
    }

    if (status != Status::Ok && log_status_error(status)) {
        nv::logger::error_no_wait(nv::logger::Event::FlashReqError,
                                  nv::logger::data_from_two_u32(static_cast<uint32_t>(Req.type),
                                                                static_cast<uint32_t>(status)),
                                  nv::logger::OutputDirection::Both,
                                  0s);
    }

    return status;
}

Status Flash::read_key_revoke(uint32_t& key_revoke, nv::ipc::Queue::Usecs timeout)
{
    // read from cmpa
    constexpr uint32_t  CfpaKeyRevokeOffset = 0x018;
    const Request       ReqCmpa             = {.type   = RequestType::CfpaRead,
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
            nv::logger::data_from_two_u32(static_cast<uint32_t>(ReqCmpa.type),
                                          static_cast<uint32_t>(status)),
            nv::logger::OutputDirection::Both,
            0s);
        return status;
    }

    // read from efuse
    uint32_t            key_revoke_efues             = 0;
    constexpr uint32_t  ImageKeyRovocationEfuseIndex = 2;
    constexpr uint32_t  ImageKeyRovocationEfuseMask  = 0x0000ff00u;
    const Request       ReqFuse                      = {.type    = RequestType::EfuseRead,
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
            nv::logger::data_from_two_u32(static_cast<uint32_t>(ReqFuse.type),
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
    return status;
}

Status Flash::write_key_revoke(uint32_t key_version, nv::ipc::Queue::Usecs timeout)
{
    const Request       Req = {.type            = RequestType::CfpaWriteKeyRevoke,
                               .sec_key_version = key_version};
    nv::flash::Response response{};
    auto                status = Task::request(Req, response, timeout);

    if (status != Status::Ok && log_status_error(status)) {
        nv::logger::error_no_wait(nv::logger::Event::FlashReqError,
                                  nv::logger::data_from_two_u32(static_cast<uint32_t>(Req.type),
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
    const Request Req = {
        .type = RequestType::CfpaCustomerRead, .offset = offset, .length = buffer_length};
    nv::flash::Response response{};
    auto                status = Task::request(Req, response, timeout);
    if (status == Status::Ok) {
        std::copy_n(response.buffer.begin(), Req.length, buffer.begin());
    }

    if (status != Status::Ok && log_status_error(status)) {
        nv::logger::error_no_wait(nv::logger::Event::FlashReqError,
                                  nv::logger::data_from_two_u32(static_cast<uint32_t>(Req.type),
                                                                static_cast<uint32_t>(status)),
                                  nv::logger::OutputDirection::Both,
                                  0s);
    }

    return status;
}

Status Flash::read_efuse(Address address, uint32_t& data, nv::ipc::Queue::Usecs timeout)
{
    const Request Req = {
        .type = RequestType::EfuseRead, .address = address, .length = sizeof(decltype(data))};
    nv::flash::Response response{};
    auto                status = Task::request(Req, response, timeout);
    if (status == Status::Ok) {
        data = *std::bit_cast<uint32_t*>(response.buffer.data());
    }

    if (status != Status::Ok && log_status_error(status)) {
        nv::logger::error_no_wait(nv::logger::Event::FlashReqError,
                                  nv::logger::data_from_two_u32(static_cast<uint32_t>(Req.type),
                                                                static_cast<uint32_t>(status)),
                                  nv::logger::OutputDirection::Both,
                                  0s);
    }

    return status;
}

Status Flash::program_efuse(Address address, const uint32_t data, nv::ipc::Queue::Usecs timeout)
{
    Request request = {.type    = RequestType::EfuseProgram,
                       .address = address,
                       .length  = sizeof(decltype(data))};

    *std::bit_cast<uint32_t*>(request.buffer.data()) = data;
    nv::flash::Response response{};
    auto                status = Task::request(request, response, timeout);

    if (status != Status::Ok && log_status_error(status)) {
        nv::logger::error_no_wait(
            nv::logger::Event::FlashReqError,
            nv::logger::data_from_two_u32(static_cast<uint32_t>(request.type),
                                          static_cast<uint32_t>(status)),
            nv::logger::OutputDirection::Both,
            0s);
    }

    return status;
}

Status Flash::read_crc(uint32_t& data, nv::ipc::Queue::Usecs timeout)
{
    const Request request = {.type = RequestType::CrcRead, .length = sizeof(decltype(data))};
    nv::flash::Response response{};
    auto                status = Task::request(request, response, timeout);
    if (status == Status::Ok) {
        data = *std::bit_cast<uint32_t*>(response.buffer.data());
    }

    if (status != Status::Ok && log_status_error(status)) {
        nv::logger::error_no_wait(
            nv::logger::Event::FlashReqError,
            nv::logger::data_from_two_u32(static_cast<uint32_t>(request.type),
                                          static_cast<uint32_t>(status)),
            nv::logger::OutputDirection::Both,
            0s);
    }

    return status;
}

Status Flash::get_life_cycle(sys::flash::LifeCycleStatus& data, nv::ipc::Queue::Usecs timeout)
{
    const Request       Req = {.type   = RequestType::EfuseLifeCycleRead,
                               .length = sizeof(decltype(data))};
    nv::flash::Response response{};
    auto                status = Task::request(Req, response, timeout);
    if (status == Status::Ok) {
        data = *std::bit_cast<sys::flash::LifeCycleStatus*>(response.buffer.data());
    }

    if (status != Status::Ok && log_status_error(status)) {
        nv::logger::error_no_wait(nv::logger::Event::FlashReqError,
                                  nv::logger::data_from_two_u32(static_cast<uint32_t>(Req.type),
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

// Customer data may be programmed before jump to application
// Disable this API to avoid unintentional use
#if 0
Status Flash::write_cfpa_customer(const std::span<uint8_t>& buffer,
                                  uint32_t                  offset,
                                  nv::ipc::Queue::Usecs     timeout)
{
    if (buffer.size() > BufferSize) {
        return Status::InvalidParam;
    }
    Request request = {.type   = RequestType::CfpaCustomerWrite,
                       .offset = offset,
                       .length = static_cast<uint32_t>(buffer.size())};
    std::copy(buffer.begin(), buffer.end(), request.buffer.begin());
    nv::flash::Response response{};
    auto                status = Task::request(request, response, timeout);
    return status;
}
#endif