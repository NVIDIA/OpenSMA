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
#pragma once
#include <source_location>
#include "nv/bootloader.h"
#include "nv/flash/flash.h"
#include "nv/logger/common.h"
#include "nv/logger/uart_string.h"
#include "nv/nv.h"
#include "nv/perf_mon/perf_mon.h"

namespace nv::logger {

enum class LogEventFlag : uint32_t
{
    None          = 0,
    ErrorAndClear = 1,
};

class Logger
{
public:
    Logger() : _boot_index(nv::bootloader::Driver::current_boot_index()){};
    Status init();
    static Status
                  add(EventId               event,
                      Level                 level,
                      EventData             data,
                      OutputDirection       output,
                      bool                  wait    = false,
                      nv::ipc::Queue::Usecs timeout = nv::ipc::Queue::Usecs::max(),
                      const footprint::Id   id = footprint::filefunc12(std::source_location::current()));
    static Status add_from_isr(
        EventId             event,
        Level               level,
        EventData           data,
        OutputDirection     output = OutputDirection::Both,
        const footprint::Id id     = footprint::filefunc12(std::source_location::current()));

    static Status download(Dlreq&                dl_req,
                           nv::ipc::Queue::Usecs timeout = nv::ipc::Queue::Usecs::max());

    static Status clean_requset();
    static void   string_console(AsciiArr& ascii);

    void store(const Item& item, Item& resp_item);
    void process_download(const Dlreq& dl_req, Dlreq& dl_resp);
    void clean();

    bool need_handle_error();
    void handle_error();

private:
    nv::bootloader::Driver::ImageIndex _boot_index;

    Status load_metadata();
    Status update_metadata();
    Status
    write_to_flash(uint32_t page_index, const std::span<uint8_t>& buffer, MemorySource source);
    Status erase_to_flash(uint32_t page_index, MemorySource source);
    Status
    read_from_flash(uint32_t page_index, const std::span<uint8_t>& buffer, MemorySource source);
    nv::flash::Address get_log_address(nv::flash::Address address);
    LogBuffer          _buffer{};
    DownloadSession    _download_session{};
    LogDLHdr           _header{};

    static constexpr uint32_t LogThreshold = 1;
    static constexpr uint8_t  AllOnesByte  = 0xFF;

    // Download Log
    void handle_start_state(const Dlreq& dl_req, Dlreq& dl_resp);
    void handle_event_download_state(const Dlreq& dl_req, Dlreq& dl_resp);
    void handle_fatal_download_state(const Dlreq& dl_req, Dlreq& dl_resp);
    void handle_perf_download_state(const Dlreq& dl_req, Dlreq& dl_resp);
    void handle_end_state(const Dlreq& dl_req, Dlreq& dl_resp);
    void to_event_download_state();
    void to_fatal_download_state();
    void to_perf_download_state();
    void to_end_state();
    void mark_error();
    void add_ecc_fault();

    // Perf data download
    nv::perf_mon::Mode perf_mon_original_mode{};

    // Error handling
    uint32_t log_event_flag{};
};

inline auto get_fw_version()
{
    FwVersion version{.major = MCU_FW_MAJOR,
                      .minor = MCU_FW_MINOR,
                      .patch = MCU_FW_PATCH,
                      .build = MCU_FW_BUILD};
    return version;
}

inline EventData data_from_two_u32(uint32_t data1, uint32_t data2)
{
    constexpr uint8_t ByteMask = 0xFF;
    return {
        static_cast<uint8_t>(data1 & ByteMask),
        static_cast<uint8_t>((data1 >> 8) & ByteMask),
        static_cast<uint8_t>((data1 >> 16) & ByteMask),
        static_cast<uint8_t>((data1 >> 24) & ByteMask),
        static_cast<uint8_t>(data2 & ByteMask),
        static_cast<uint8_t>((data2 >> 8) & ByteMask),
        static_cast<uint8_t>((data2 >> 16) & ByteMask),
        static_cast<uint8_t>((data2 >> 24) & ByteMask),
    };
}

inline EventData data_from_u32(uint32_t data)
{
    constexpr uint8_t ByteMask = 0xFF;
    return {static_cast<uint8_t>(data & ByteMask),
            static_cast<uint8_t>((data >> 8) & ByteMask),
            static_cast<uint8_t>((data >> 16) & ByteMask),
            static_cast<uint8_t>((data >> 24) & ByteMask)};
}

inline auto
info(EventStructItem       event,
     EventData             data    = {0},
     OutputDirection       dir     = OutputDirection::Both,
     nv::ipc::Queue::Usecs timeout = nv::ipc::Queue::Usecs(10000),
     const footprint::Id   id      = footprint::filefunc12(std::source_location::current()))
{
    const Level base_level = event.default_level == logger::Level::Unknown
                               ? Level::Info
                               : event.default_level;
    return Logger::add(event.unique_id, base_level, data, dir, false, timeout, id);
}

inline auto
info_wait(EventStructItem       event,
          EventData             data    = {0},
          OutputDirection       dir     = OutputDirection::Both,
          nv::ipc::Queue::Usecs timeout = nv::ipc::Queue::Usecs::max(),
          const footprint::Id   id = footprint::filefunc12(std::source_location::current()))
{
    const Level base_level = event.default_level == logger::Level::Unknown
                               ? Level::Info
                               : event.default_level;
    return Logger::add(event.unique_id, base_level, data, dir, true, timeout, id);
}

inline auto
error(EventStructItem       event,
      EventData             data    = {0},
      OutputDirection       dir     = OutputDirection::Both,
      nv::ipc::Queue::Usecs timeout = nv::ipc::Queue::Usecs::max(),
      const footprint::Id   id      = footprint::filefunc12(std::source_location::current()))
{
    const Level base_level = event.default_level == logger::Level::Unknown
                               ? Level::Error
                               : event.default_level;
    return Logger::add(event.unique_id, base_level, data, dir, true, timeout, id);
}

inline auto
error_no_wait(EventStructItem       event,
              EventData             data    = {0},
              OutputDirection       dir     = OutputDirection::Both,
              nv::ipc::Queue::Usecs timeout = nv::ipc::Queue::Usecs::max(),
              const footprint::Id   id = footprint::filefunc12(std::source_location::current()))
{
    const Level base_level = event.default_level == logger::Level::Unknown
                               ? Level::Error
                               : event.default_level;
    return Logger::add(event.unique_id, base_level, data, dir, false, timeout, id);
}

}  // namespace nv::logger
