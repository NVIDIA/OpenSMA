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
#include "nv/logger/log.h"

#include <array>
#include <tuple>
#include <cstring>
#include <cstdio>

#include "nv/common/console.h"
#include "nv/ipc/supervisor.h"
#include "nv/logger/common.h"
#include "nv/logger/log_fault.h"
#include "nv/logger/task.h"
#include "nv/nv.h"

#include "nv/ipc/supervisor.h"
#include <algorithm>

using namespace nv::logger;

namespace {
auto get_phrase(uint32_t index)
{
    return index / EntryNumInPharse;
}
auto get_offset(uint32_t index)
{
    return nv::common::mul(index, PhraseSize);
}
auto get_metadata_offset(uint32_t index)
{
    return nv::common::mul(index, LogPtrSize);
}
auto log_meta_get_next_index(uint32_t x)
{
    return nv::common::add(x, static_cast<uint32_t>(1)) % LogMetaMaxIndex;
}

auto get_pre_idx(uint32_t index)
{
    return index != 0 ? index - 1 : EntryNumInFlash - 1;
}

auto get_log_entry_address(uint32_t index)
{
    return nv::common::add(get_offset(index), LogEntryStart);
}

auto get_metadata_address(uint32_t index)
{
    return nv::common::add(get_metadata_offset(index), LogMetadataStart);
}

auto get_fault_offset(uint32_t page_index)
{
    return nv::common::mul(page_index, PageSize);
}

auto get_fault_entry_address(uint32_t page_index)
{
    return nv::common::add(get_fault_offset(page_index), FaultEntryStart);
}

auto get_fault_entry_write_address(uint32_t page_index)
{
    return nv::common::add(nv::common::mul(page_index, FaultEntrySize), FaultEntryStart);
}

auto get_fault_page(uint32_t offset)
{
    return offset / PageSize;
}

auto can_logger_block()
{
    const bool SchedulerRun = sys::ipc::is_scheduler_run();

    if (!SchedulerRun) {
        return false;
    }

    // Logging itself depends on flash
    constexpr std::array<nv::ipc::TaskId, 3> non_block_task = {
        nv::ipc::TaskId::Logger, nv::ipc::TaskId::Flash, nv::ipc::TaskId::Timer};

    auto cur_task_id = nv::ipc::Supervisor::inst().current_task_id();
    if (std::find(non_block_task.begin(), non_block_task.end(), cur_task_id)
        != non_block_task.end()) {
        return false;
    }

    return true;
}

}  // namespace

Status Logger::init()
{
    load_metadata();
    // If pointer is invalid, reset it
    if (_buffer.ptr.head >= EntryNumInFlash || _buffer.ptr.tail >= EntryNumInFlash) {
        nv::info("Reset log buffer since invalid index\n");
        _buffer.index    = 0;
        _buffer.ptr.head = 0;
        _buffer.ptr.tail = 0;
        update_metadata();
    }
    // Erase first sector if first time init log
    if (_buffer.ptr.head == 0 && _buffer.ptr.tail == 0) {
        erase_to_flash(0, MemorySource::LogEntry);
    }
    // store buffer
    auto status = read_from_flash(
        _buffer.ptr.tail, _buffer.entry_buffer.to_span(), MemorySource::LogEntry);
    if (status != Status::Ok) {
        clean();
    }
    // init log dl session
    _download_session.session = LogSessionMax;

    return Status::Ok;
}

namespace {
constexpr uint8_t kUpperBitsMask = 0x1Fu;  // 5-bit mask for upper bits
constexpr uint8_t kLowerBitsMask = 0x7Fu;  // 7-bit mask for lower bits

constexpr auto footprint_mask(uint16_t id)
{
    return std::make_tuple(static_cast<uint8_t>((id >> 7) & kUpperBitsMask),
                           static_cast<uint8_t>(id & kLowerBitsMask));
}
}  // namespace

// API
Status Logger::add(EventId               event,
                   Level                 level,
                   EventData             data,
                   OutputDirection       output,
                   bool                  wait,
                   nv::ipc::Queue::Usecs timeout,
                   const footprint::Id   fid)
{
    const auto [footprint_msb, footprint_lsb] = footprint_mask(fid);

    if (wait || (timeout != nv::ipc::Queue::Usecs(0))) {
        const bool CanBlock = can_logger_block();
        if (!CanBlock) {
            wait    = false;
            timeout = nv::ipc::Queue::Usecs(0);
        }
    }

    auto current_core = nv::ipc::get_current_core();
    // coverity[dead_error_line] - core1 is possible
    const uint8_t core_id = (current_core == nv::ipc::CoreId::Core1) ? 1 : 0;

    const Item ReqItem{.timestamp     = nv::ipc::Supervisor::get_os_ticks(),
                       .direction     = output,
                       .event         = event,
                       .level         = level,
                       .footprint_msb = footprint_msb,
                       .data          = data,
                       .footprint_lsb = footprint_lsb,
                       .core_id       = core_id,  // Store originating core ID
                       .wait          = wait};
    Item       resp_item{};
    Status     status = Status::Ok;

    status = Task::request(ReqItem, resp_item, wait, timeout);

    return status;
}

void Logger::string_console(AsciiArr& ascii)
{
    ascii.flag = true;
    Task::request(ascii);
}

// API
Status Logger::add_from_isr(
    EventId event, Level level, EventData data, OutputDirection output, const footprint::Id fid)
{
    auto current_core                         = nv::ipc::get_current_core();
    const auto [footprint_msb, footprint_lsb] = footprint_mask(fid);

    // coverity[dead_error_line] - core1 is possible
    const uint8_t core_id = (current_core == nv::ipc::CoreId::Core1) ? 1 : 0;

    const Item ReqItem{.timestamp     = nv::ipc::Supervisor::get_os_ticks(),
                       .direction     = output,
                       .event         = event,
                       .level         = level,
                       .footprint_msb = footprint_msb,
                       .data          = data,
                       .footprint_lsb = footprint_lsb,
                       .core_id       = core_id};  // Store originating core ID
    auto       status = Task::request_from_isr(ReqItem);
    return status;
}

// API
Status Logger::download(Dlreq& dl_req, nv::ipc::Queue::Usecs timeout)
{
    auto status = Task::download(dl_req, timeout);
    return status;
}

// API
Status Logger::clean_requset()
{
    return Task::clean();
}

void Logger::clean()
{
    nv::info("Logger handle clean log\n");
    _buffer.index    = 0;
    _buffer.ptr.tail = 0;
    _buffer.ptr.head = 0;
    update_metadata();
    erase_to_flash(0, MemorySource::LogEntry);
    memset(_buffer.entry_buffer.entries.data(), AllOnesByte, PhraseSize);
#if 0
    Item item{.event = Event::LoggerCleanLog, .level = Level::Info};
    store(item, item);
#endif
    add_ecc_fault();
}

void Logger::store(const Item& item, Item& resp_item)
{
    auto& entry         = _buffer.entry_buffer.entries.at(_buffer.ptr.tail % EntryNumInPharse);
    entry.data          = item.data;
    entry.event         = item.event;
    entry.level         = item.level;
    entry.timestamp     = item.timestamp;
    entry.footprint_msb = item.footprint_msb;
    entry.footprint_lsb = item.footprint_lsb;    // Bit field automatically masks to 7 bits
    entry.coreId        = item.core_id & 0x01u;  // Extract 1-bit core ID
    Status status{};

    _buffer.ptr.tail = (_buffer.ptr.tail + 1) % EntryNumInFlash;

    if (_buffer.ptr.tail == _buffer.ptr.head) {
        _buffer.ptr.head = (_buffer.ptr.head + EntryNumInSector) % EntryNumInFlash;
    }

    if (_buffer.ptr.tail % LogThreshold == 0) {
        status = update_metadata();
        if (status != Status::Ok) {
            resp_item.status = status;
            mark_error();
            return;
        }

        status = write_to_flash(get_pre_idx(_buffer.ptr.tail),
                                _buffer.entry_buffer.to_span(),
                                MemorySource::LogEntry);
        if (status != Status::Ok) {
            resp_item.status = status;
            mark_error();
            return;
        }
    }

    // Reset buffer if full
    if (_buffer.ptr.tail % EntryNumInPharse == 0) {
        memset(_buffer.entry_buffer.entries.data(), AllOnesByte, PhraseSize);
    }

    if (_buffer.ptr.tail % EntryNumInSector == 0) {
        status = erase_to_flash(_buffer.ptr.tail, MemorySource::LogEntry);
        if (status != Status::Ok) {
            resp_item.status = status;
            mark_error();
        }
    }
    resp_item.status = Status::Ok;
}

Status Logger::load_metadata()
{
    uint32_t valid_index = LogMetaInvalidIndex;
    // Find the latest valid log ptr in metadata
    nv::flash::Buffer read_buffer{};
    for (uint32_t i = 0; i < nv::flash::SectorSize / nv::flash::BufferSize; i++) {
        auto status = read_from_flash(i * PtrNumInPage, read_buffer, MemorySource::Metadata);
        if (status != Status::Ok) {
            clean();
            return Status::Ok;
        }

        for (uint32_t j = 0; j < PtrNumInPage; j++) {
            auto log_ptr = std::bit_cast<LogPtr*>(read_buffer.data() + j * LogPtrSize);
            if (((log_ptr->head & log_ptr->head_checksum) == 0)
                && ((log_ptr->tail & log_ptr->tail_checksum) == 0)) {
                valid_index = i * PtrNumInPage + j;
            }
        }
    }

    if (valid_index != LogMetaInvalidIndex) {
        _buffer.index = log_meta_get_next_index(valid_index);
        // read from log ptr from flash
        auto status = read_from_flash(
            valid_index, _buffer.ptr.to_span(), MemorySource::Metadata);
        if (status != Status::Ok) {
            clean();
        }
    }
    else {
        nv::info("Reset Log buffer\n");
        _buffer.index    = 0;
        _buffer.ptr.head = 0;
        _buffer.ptr.tail = 0;
        update_metadata();
    }

    return Status::Ok;
}

Status Logger::update_metadata()
{
    _buffer.ptr.head_checksum = ~_buffer.ptr.head;
    _buffer.ptr.tail_checksum = ~_buffer.ptr.tail;

    if (_buffer.index == 0) {
        auto status = erase_to_flash(_buffer.index, MemorySource::Metadata);
        if (status != Status::Ok) {
            return Status::Error;
        }
    }
#if 0
    uint32_t phrase_index = get_metadata_phrase_index(_buffer.index);
    if (phrase_index == 0) {
        memset(_buffer.ptr_buffer.ptrs, AllOnesByte, PhraseSize);
    }

    _buffer.ptr_buffer.ptrs.at(phrase_index) = _buffer.ptr;
#endif
    auto status = write_to_flash(_buffer.index, _buffer.ptr.to_span(), MemorySource::Metadata);
    _buffer.index = log_meta_get_next_index(_buffer.index);

    if (status != Status::Ok) {
        mark_error();
        return Status::Error;
    }

    return Status::Ok;
}

Status
Logger::write_to_flash(uint32_t index, const std::span<uint8_t>& buffer, MemorySource source)
{
    const nv::flash::Address VirtualAddress = source == MemorySource::LogEntry
                                                ? get_log_entry_address(index)
                                                : (source == MemorySource::Metadata
                                                       ? get_metadata_address(index)
                                                       : get_fault_entry_write_address(index));
    auto                     address        = get_log_address(VirtualAddress);

    auto status = nv::flash::Flash::write(address, buffer);
    if (status != nv::flash::Status::Ok) {
        // nv::info("Log write error at 0x%x\n",address);
        return Status::Error;
    }

    return Status::Ok;
}

Status Logger::erase_to_flash(uint32_t page_index, MemorySource source)
{
    const nv::flash::Address VirtualAddress = source == MemorySource::LogEntry
                                                ? get_log_entry_address(page_index)
                                                : (source == MemorySource::Metadata
                                                       ? get_metadata_address(page_index)
                                                       : get_fault_entry_write_address(
                                                             page_index));

    auto address = get_log_address(VirtualAddress);

    // nv::info("Log erase at 0x%x\n",address);
    auto status = nv::flash::Flash::erase(address);

    if (status != nv::flash::Status::Ok) {
        // nv::info("Log erase error at 0x%x\n",address);
        return Status::Error;
    }

    return Status::Ok;
}

Status Logger::read_from_flash(uint32_t                  page_index,
                               const std::span<uint8_t>& buffer,
                               MemorySource              source)
{
    const nv::flash::Address VirtualAddress = source == MemorySource::LogEntry
                                                ? get_log_entry_address(page_index)
                                                : (source == MemorySource::Metadata
                                                       ? get_metadata_address(page_index)
                                                       : get_fault_entry_address(page_index));
    auto                     address        = get_log_address(VirtualAddress);

    // nv::info("Log read at 0x%x \n",address);
    auto status = nv::flash::Flash::read(address, buffer);
    if (status != nv::flash::Status::Ok) {
        // nv::info("Log read error at 0x%x\n",address);
        return Status::Error;
    }

    return Status::Ok;
}

nv::flash::Address Logger::get_log_address(nv::flash::Address address)
{
    return nv::flash::Flash::get_flash_address(address, _boot_index);
}

void Logger::process_download(const Dlreq& dl_req, Dlreq& dl_resp)
{
    switch (_download_session.state) {
        case LogDLState::Start: {
            handle_start_state(dl_req, dl_resp);
        } break;
        case LogDLState::DownloadEvent: {
            handle_event_download_state(dl_req, dl_resp);
        } break;
        case LogDLState::DownloadFatal: {
            handle_fatal_download_state(dl_req, dl_resp);
        } break;
        case LogDLState::DownloadPerf: {
            handle_perf_download_state(dl_req, dl_resp);
        } break;
        case LogDLState::End: {
            handle_end_state(dl_req, dl_resp);
        } break;
        default: {
            dl_resp.status = Status::Error;
        } break;
    }
    return;
}

void Logger::handle_start_state(const Dlreq& dl_req, Dlreq& dl_resp)
{
    if (dl_req.session != LogSessionMax) {
        dl_resp.status = Status::InvalidParam;
        return;
    }
    // auto tail = _buffer.ptr.tail;
    // auto head = _buffer.ptr.head;
    // nv::info("Download tail: %d head: %d\n", tail, head);

    _download_session.session    = (_download_session.session + 1) % LogSessionMax;
    _download_session.ptr        = _buffer.ptr.head;
    _download_session.event_tail = _buffer.ptr.tail;
    if (_download_session.ptr > _download_session.event_tail) {
        const uint32_t Tmp           = nv::common::sub(EntryNumInFlash, _download_session.ptr);
        const uint32_t LogEventNum   = nv::common::add(Tmp, _download_session.event_tail);
        _download_session.event_size = LogEventNum > UINT16_MAX
                                         ? UINT16_MAX
                                         : static_cast<uint16_t>(LogEventNum);
    }
    else {
        auto log_event_num           = nv::common::sub(_download_session.event_tail,
                                             _download_session.ptr);
        _download_session.event_size = log_event_num > UINT16_MAX
                                         ? UINT16_MAX
                                         : static_cast<uint16_t>(log_event_num);
    }

    _download_session.fatal_size = FaultLogger::get_fatal_download_size();
    nv::info("Fatal size: %d\n", _download_session.fatal_size);
    _download_session.buffer_ptr   = 0;
    _download_session.current_page = get_phrase(_download_session.ptr);

    dl_resp.session       = _download_session.session;
    _header.event_size    = _download_session.event_size;
    _header.fatal_size    = _download_session.fatal_size;
    _header.version       = 0;
    _header.major_version = MCU_FW_MAJOR;
    dl_resp.size          = sizeof(_header);

    auto perf_buffer_size = nv::perf_mon::Driver::get_buffer_size();

    auto size_per_second        = nv::perf_mon::Driver::get_measurement_size();
    auto total_perf_size        = size_per_second * perf_buffer_size;
    _download_session.perf_size = total_perf_size;
    _header.perf_size           = total_perf_size;

    memcpy(dl_resp.data.data(), &_header, sizeof(_header));
    to_event_download_state();

    dl_resp.status = Status::Ok;
    return;
}

void Logger::to_event_download_state()
{
    auto status = read_from_flash(
        _download_session.current_page, _download_session.buffer, MemorySource::LogEntry);

    if (status != Status::Ok) {
        mark_error();
        _download_session.state = LogDLState::End;
        return;
    }

    _download_session.state = LogDLState::DownloadEvent;
}

void Logger::handle_event_download_state(const Dlreq& dl_req, Dlreq& dl_resp)
{
    if (dl_req.session == LogSessionMax) {
        _download_session.state = LogDLState::Start;
        // process_download(DlReq, dl_resp);
        handle_start_state(dl_req, dl_resp);
        return;
    }
    if (dl_req.session != _download_session.session) {
        dl_resp.status = Status::InvalidParam;
        dl_resp.size   = 0;
        return;
    }

    if (_download_session.event_size == 0) {
        to_fatal_download_state();
        // process_download(dl_req, dl_resp);
        handle_fatal_download_state(dl_req, dl_resp);
        return;
    }
    if (DownloadSize < std::numeric_limits<uint8_t>::max()) {
        dl_resp.size = static_cast<uint8_t>(DownloadSize);
    }

    memcpy(dl_resp.data.data(),
           _download_session.buffer.data() + _download_session.buffer_ptr,
           DownloadSize);
    _download_session.buffer_ptr = nv::common::add(_download_session.buffer_ptr, DownloadSize);
    _download_session.ptr        = (_download_session.ptr + 1) % EntryNumInFlash;
    // nv::info("Download _download_session.ptr:%d _download_session.event_tail:%d\n",
    // _download_session.ptr, _download_session.event_tail);
    if (_download_session.ptr != _download_session.event_tail) {
        if (_download_session.buffer_ptr >= PageSize) {
            // nv::info("Download to next page
            // _download_session.buffer_ptr:%d\n",_download_session.buffer_ptr);
            _download_session.buffer_ptr   = 0;
            _download_session.current_page = get_phrase(_download_session.ptr);
            auto status                    = read_from_flash(_download_session.current_page,
                                          _download_session.buffer,
                                          MemorySource::LogEntry);
            if (status != Status::Ok) {
                mark_error();
                _download_session.state = LogDLState::End;
                dl_resp.session         = dl_req.session;
                dl_resp.status          = Status::Error;
                return;
            }
        }
    }
    else {
        to_fatal_download_state();
    }

    dl_resp.session = dl_req.session;
    dl_resp.status  = Status::Ok;
    return;
}

void Logger::handle_fatal_download_state(const Dlreq& dl_req, Dlreq& dl_resp)
{
    if (dl_req.session == LogSessionMax) {
        _download_session.state = LogDLState::Start;
        // process_download(DlReq, dl_resp);
        handle_start_state(dl_req, dl_resp);
        return;
    }
    if (dl_req.session != _download_session.session) {
        dl_resp.status = Status::InvalidParam;
        dl_resp.size   = 0;
        return;
    }

    if (_download_session.fatal_size == 0) {
        dl_resp.size = 0;
        to_perf_download_state();
        return;
    }

    if (DownloadSize < std::numeric_limits<uint8_t>::max()) {
        dl_resp.size = static_cast<uint8_t>(DownloadSize);
    }

    memcpy(dl_resp.data.data(),
           _download_session.buffer.data() + _download_session.buffer_ptr,
           DownloadSize);
    _download_session.buffer_ptr = nv::common::add(_download_session.buffer_ptr, DownloadSize);
    _download_session.ptr        = nv::common::add(_download_session.ptr, DownloadSize);

    // nv::info("Fatal Download _download_session.buffer_ptr:%d _download_session.ptr:%d
    // _download_session.event_tail:%d\n", _download_session.buffer_ptr , _download_session.ptr,
    // _download_session.fatal_size* FaultEntrySize);
    if (_download_session.ptr != _download_session.fatal_size * FaultEntrySize) {
        if (_download_session.buffer_ptr >= PageSize) {
            // nv::info("Fatal Download to next page
            // _download_session.buffer_ptr:%d\n",_download_session.buffer_ptr);
            _download_session.buffer_ptr   = 0;
            _download_session.current_page = get_fault_page(_download_session.ptr);
            auto status                    = read_from_flash(_download_session.current_page,
                                          _download_session.buffer,
                                          MemorySource::FaultEntry);
            if (status != Status::Ok) {
                mark_error();
                _download_session.state = LogDLState::End;
                dl_resp.session         = dl_req.session;
                dl_resp.status          = Status::Error;
                return;
            }
        }
    }
    else {
        to_perf_download_state();
    }

    dl_resp.session = dl_req.session;
    dl_resp.status  = Status::Ok;
    return;
}

void Logger::handle_perf_download_state(const Dlreq& dl_req, Dlreq& dl_resp)
{
    if (dl_req.session == LogSessionMax) {
        _download_session.state = LogDLState::Start;
        handle_start_state(dl_req, dl_resp);
        return;
    }
    if (dl_req.session != _download_session.session) {
        dl_resp.status = Status::InvalidParam;
        dl_resp.size   = 0;
        return;
    }

    if (_download_session.perf_size == 0) {
        dl_resp.size            = 0;
        _download_session.state = LogDLState::End;
        return;
    }

    if (DownloadSize < std::numeric_limits<uint8_t>::max()) {
        dl_resp.size = static_cast<uint8_t>(DownloadSize);
    }

    memcpy(dl_resp.data.data(),
           _download_session.buffer.data() + _download_session.buffer_ptr,
           DownloadSize);
    _download_session.buffer_ptr = nv::common::add(_download_session.buffer_ptr, DownloadSize);
    _download_session.ptr        = nv::common::add(_download_session.ptr, DownloadSize);

    if (_download_session.ptr != _download_session.perf_size) {
        if (_download_session.buffer_ptr >= PageSize) {
            auto&      perf_mon_driver = nv::perf_mon::Driver::inst();
            const bool CrossPage       = (_download_session.perf_offset + PageSize)
                                 > nv::perf_mon::Driver::get_total_size();
            if (CrossPage) {
                const uint32_t First  = nv::common::sub(nv::perf_mon::Driver::get_total_size(),
                                                       _download_session.perf_offset);
                const uint32_t Second = nv::common::sub(PageSize, First);
                memcpy(_download_session.buffer.data(),
                       (std::bit_cast<uint8_t*>(perf_mon_driver.buffer.data())
                        + _download_session.perf_offset),
                       First);
                memcpy(_download_session.buffer.data() + First,
                       (std::bit_cast<uint8_t*>(perf_mon_driver.buffer.data())),
                       Second);
                _download_session.perf_offset = Second;
            }
            else {
                memcpy(_download_session.buffer.data(),
                       (std::bit_cast<uint8_t*>(perf_mon_driver.buffer.data())
                        + _download_session.perf_offset),
                       PageSize);
                _download_session.perf_offset = nv::common::add(_download_session.perf_offset,
                                                                PageSize);
            }
            _download_session.buffer_ptr = 0;
        }
    }
    else {
        _download_session.state = LogDLState::End;
    }

    dl_resp.session = dl_req.session;
    dl_resp.status  = Status::Ok;
    return;
}

void Logger::handle_end_state(const Dlreq& dl_req, Dlreq& dl_resp)
{
    if (dl_req.session == LogSessionMax) {
        _download_session.state = LogDLState::Start;
        // return process_download(DlReq, dl_resp);
        handle_start_state(dl_req, dl_resp);
        return;
    }

    if (dl_req.session != _download_session.session) {
        dl_resp.status = Status::InvalidParam;
        return;
    }
    nv::perf_mon::Driver::dump();
    nv::perf_mon::Driver::mode_change(perf_mon_original_mode);
    perf_mon_original_mode = {};

    dl_resp.size   = 0;
    dl_resp.status = Status::Ok;

    return;
}

void Logger::to_fatal_download_state()
{
    _download_session.buffer_ptr = 0;
    _download_session.ptr        = 0;
    auto status                  = read_from_flash(
        get_phrase(_download_session.ptr), _download_session.buffer, MemorySource::FaultEntry);

    if (status != Status::Ok) {
        mark_error();
        _download_session.state = LogDLState::End;
        return;
    }

    _download_session.state = LogDLState::DownloadFatal;
    return;
}

void Logger::to_perf_download_state()
{
    _download_session.state      = LogDLState::DownloadPerf;
    _download_session.buffer_ptr = 0;
    _download_session.ptr        = 0;
    auto& perf_mon_driver        = nv::perf_mon::Driver::inst();
    auto  measurement_size       = nv::perf_mon::Driver::get_measurement_size();
    perf_mon_original_mode       = nv::perf_mon::Driver::get_current_mode();
    nv::perf_mon::Driver::mode_change(nv::perf_mon::Mode::Disable);

    _download_session.perf_offset = (nv::common::mul(nv::perf_mon::Driver::get_index() + 1,
                                                     measurement_size))
                                  % nv::perf_mon::Driver::get_total_size();
    memcpy(_download_session.buffer.data(),
           std::bit_cast<uint8_t*>(perf_mon_driver.buffer.data())
               + _download_session.perf_offset,
           PageSize);
    _download_session.perf_offset += PageSize;
}

bool Logger::need_handle_error()
{
    return log_event_flag & nv::common::to_underlying(LogEventFlag::ErrorAndClear);
}

void Logger::handle_error()
{
    nv::info("Handle error\n");
    clean();
    log_event_flag = 0;
}

void Logger::mark_error()
{
    log_event_flag |= nv::common::to_underlying(LogEventFlag::ErrorAndClear);
}

void Logger::add_ecc_fault()
{
    // Add Fault
    nv::flash::Buffer  read_buffer{};
    constexpr uint8_t  EmptyContent      = 0xFF;
    uint32_t           index             = 0;
    constexpr uint16_t FatalInvalidIndex = 0x5A5A;
    for (index = 0; index < FaultEntryNum;) {
        // nv::flash::Address cur_address = FaultEntryStart + index * FaultEntrySize;
        const nv::flash::Address Address = nv::flash::Flash::get_flash_address(
            FaultEntryStart + index * FaultEntrySize,
            nv::bootloader::Driver::current_boot_index());

        auto read_status = nv::flash::Flash::read(Address, read_buffer);
        if (read_status != nv::flash::Status::Ok) {
            index = FatalInvalidIndex;
            break;
        }

        const uint32_t EntryPerBuffer = (nv::flash::BufferSize / FaultEntrySize);
        bool           is_empty       = true;
        for (uint32_t i = 0; i < EntryPerBuffer; i++) {
            is_empty = true;
            for (uint32_t j = 0; j < FaultEntrySize; j++) {
                if (read_buffer.at(i * FaultEntrySize + j) != EmptyContent) {
                    is_empty = false;
                    break;
                }
            }
            if (is_empty) {
                break;
            }
            index++;
        }
        if (is_empty) {
            break;
        }
    }

    if (index == FatalInvalidIndex || index == FaultEntryNum) {
        erase_to_flash(0, MemorySource::FaultEntry);
        index = 0;
    }

    // nv::info("Fault index: %d\n", index);
    FaultItem item{};
    item.event   = Fault::EccHandle;
    item.version = get_fw_version();

    write_to_flash(index, item.to_span(), MemorySource::FaultEntry);
}
