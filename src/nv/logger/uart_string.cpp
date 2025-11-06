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
#include "nv/logger/uart_string.h"

#include <cstdarg>
#include <cstring>

#include "nv/common/debug.h"
#include "nv/ipc/supervisor.h"
#include "nv/logger/log.h"
#include "nv/uart/driver.h"
#include "sys/common/common.h"
#include "sys/ipc/driver.h"
#include "sys/common/c2c_fault.h"
// #include NV_IPC_CONFIG_H

using namespace nv::logger;
namespace {

enum class BaseRadix : uint32_t
{
    DecBase = 10,
    HexBase = 16,
};

void process_request(uint8_t* ascii_char, uint32_t size)
{
    constexpr uint8_t Core0Symbol      = 21;  // NAK in ASCII
    constexpr uint8_t Core1Symbol      = 6;   // ACK in ASCII
    const bool        is_scheduler_run = nv::ipc::Supervisor::is_scheduler_run();
    const bool        is_under_fault   = c2c_fault::is_under_fault_state();
    // If not in scheduler, and the logger task can be accessed directly on the current core,
    // print out directly to uart
    if (!is_scheduler_run
        && (sys::ipc::task::Driver::can_direct_access_on_current_core(
            nv::ipc::TaskId::Logger))) {
        constexpr uint32_t AsciiLen = AsciiStrLen + 1;  // 1 for core_id
        std::array<uint8_t, DumpCheckSize + AsciiLen> dump_buffer{};
        // last bit (-1) for dump tail magic, so -2 for core_id
        constexpr uint32_t CoreIdOffset = DumpCheckSize + AsciiLen - 2;
        dump_buffer[0]                  = DumpHeadMagicRaw;
        dump_buffer.back()              = DumpTailMagicRaw;
        auto current_core               = nv::ipc::get_current_core();
        // coverity[dead_error_line] - core1 is possible
        dump_buffer[CoreIdOffset] = (current_core == nv::ipc::CoreId::Core1) ? Core1Symbol
                                                                             : Core0Symbol;
        std::memcpy(dump_buffer.data() + 1, ascii_char, size);
        auto driver = nv::uart::Driver(static_cast<nv::uart::Port>(nv::ipc::UartInstance));
        driver.tx(dump_buffer);
    }
    else if (!is_under_fault) {
        nv::logger::AsciiArr ascii{};
        ascii.flag = true;

        // Detect and store core ID for logger task path
        auto current_core = nv::ipc::get_current_core();
        // coverity[dead_error_line] - core1 is possible
        ascii.core_id = (current_core == nv::ipc::CoreId::Core1) ? Core1Symbol
                                                                 : Core0Symbol;  // 21=Core0,
                                                                                 // 6=Core1

        memcpy(ascii.ascii_arr.data(), ascii_char, size);
        nv::logger::Logger::string_console(ascii);
    }
    else {
        // TBD: Fault case
    }
}

void check_buffer_and_process(uint8_t* ascii_char, uint32_t size, uint32_t& index)
{
    if (index >= size && index > 0) {
        process_request(ascii_char, index);
        index = 0;
    }
}

uint32_t convert_radix_num_to_string(char*     output_string,
                                     void*     number_pointer,
                                     BaseRadix base_radix,
                                     bool      use_uppercase)
{
    uint32_t remaining_value{};
    uint32_t quotient{};
    uint32_t remainder{};

    uint32_t string_length{};
    char*    string_position{};

    string_length      = 0;
    string_position    = output_string;
    *string_position++ = '\0';

    remaining_value = *static_cast<uint32_t*>(number_pointer);

    if (remaining_value == 0) {
        *string_position = '0';
        string_length    = nv::common::add(string_length, static_cast<uint32_t>(1));
        return string_length;
    }

    while (remaining_value) {
        quotient  = remaining_value / static_cast<uint32_t>(base_radix);
        remainder = remaining_value % static_cast<uint32_t>(base_radix);

        if (remainder < static_cast<uint32_t>(BaseRadix::DecBase)) {
            remainder = remainder + static_cast<uint32_t>('0');
        }
        else {
            remainder = remainder - static_cast<uint32_t>(BaseRadix::DecBase)
                      + static_cast<uint32_t>(
                            static_cast<unsigned char>(use_uppercase ? 'A' : 'a'));
        }
        remaining_value    = quotient;
        *string_position++ = (char)remainder;
        string_length      = nv::common::add(string_length, static_cast<uint32_t>(1));
    }
    return string_length;
}

uint32_t formatt_data(const char*    format_string,
                      va_list        argument_list,
                      uint8_t*       output_buffer,
                      const uint32_t buffer_size)
{
    const char*   current_position{};
    unsigned char current_character{};

    constexpr auto                    MAX_DIGIT_COUNT = 33;
    std::array<char, MAX_DIGIT_COUNT> number_string_buffer{};

    char*    string_argument       = nullptr;
    char*    number_string_pointer = nullptr;
    uint32_t number_length         = 0;

    uint32_t total_characters_written = 0;

    bool use_uppercase_hex{};

    int      signed_integer_value{};
    uint32_t unsigned_integer_value = 0;

    uint32_t buffer_index = 0;
    current_position      = format_string;
    while (true) {
        if ('\0' == *current_position) {
            break;
        }
        current_character = *current_position;
        if (current_character != '%') {
            if (buffer_index < buffer_size) {
                output_buffer[buffer_index] = static_cast<uint8_t>(current_character);
            }
            buffer_index = nv::common::add(buffer_index, static_cast<uint32_t>(1));
            check_buffer_and_process(output_buffer, buffer_size, buffer_index);
            total_characters_written = nv::common::add(total_characters_written,
                                                       static_cast<uint32_t>(1));
            current_position++;
            continue;
        }

        current_character = *++current_position;
        switch (current_character) {
            case 'd':
            case 'x':
            case 'X': {
                if (current_character == 'd') {
                    {
                        signed_integer_value = (int)va_arg(argument_list,
                                                           int);  // NOLINT(*vararg)
                    }
                    number_length = convert_radix_num_to_string(number_string_buffer.data(),
                                                                &signed_integer_value,
                                                                BaseRadix::DecBase,
                                                                use_uppercase_hex);
                    number_string_pointer = &number_string_buffer.at(number_length);
                }

                if ((current_character == 'X') || (current_character == 'x')) {
                    if (current_character == 'x') {
                        use_uppercase_hex = false;
                    }

                    {
                        unsigned_integer_value = (uint32_t)va_arg(argument_list,
                                                                  uint32_t);  // NOLINT(*vararg)
                    }
                    number_length = convert_radix_num_to_string(number_string_buffer.data(),
                                                                &unsigned_integer_value,
                                                                BaseRadix::HexBase,
                                                                use_uppercase_hex);
                    number_string_pointer = &number_string_buffer.at(number_length);
                }
                if (number_string_pointer != nullptr) {
                    while ('\0' != *number_string_pointer) {
                        const uint8_t digit_character = *number_string_pointer;
                        number_string_pointer--;
                        if (buffer_index < buffer_size) {
                            output_buffer[buffer_index] = digit_character;
                        }
                        buffer_index = nv::common::add(buffer_index, static_cast<uint32_t>(1));
                        total_characters_written = nv::common::add(total_characters_written,
                                                                   static_cast<uint32_t>(1));
                        check_buffer_and_process(output_buffer, buffer_size, buffer_index);
                    }
                }
                break;
            }
            case 's': {
                string_argument = (char*)va_arg(argument_list, char*);  // NOLINT(*vararg)
                if (nullptr != string_argument) {
                    while ('\0' != *string_argument) {
                        const uint8_t string_character = *string_argument;
                        if (buffer_index < buffer_size) {
                            output_buffer[buffer_index] = string_character;
                        }
                        buffer_index = nv::common::add(buffer_index, static_cast<uint32_t>(1));
                        string_argument++;
                        total_characters_written = nv::common::add(total_characters_written,
                                                                   static_cast<uint32_t>(1));
                        check_buffer_and_process(output_buffer, buffer_size, buffer_index);
                    }
                }
                break;
            }
            default: {
                if (buffer_index < buffer_size) {
                    output_buffer[buffer_index] = static_cast<uint8_t>(current_character);
                }
                buffer_index = nv::common::add(buffer_index, static_cast<uint32_t>(1));
                total_characters_written = nv::common::add(total_characters_written,
                                                           static_cast<uint32_t>(1));
                check_buffer_and_process(output_buffer, buffer_size, buffer_index);
                break;
            }
        }
        current_position++;
    }
    check_buffer_and_process(output_buffer, buffer_index, buffer_index);
    return total_characters_written;
}
}  // namespace

void nv::logger::format_to_logger(const char* fmt, va_list ap)
{
    nv::logger::AsciiArr ascii{};
    ascii.flag                    = true;
    constexpr uint8_t Core0Symbol = 21;
    constexpr uint8_t Core1Symbol = 6;
    // Detect and store core ID using correct namespace
    auto current_core = nv::ipc::get_current_core();
    // coverity[dead_error_line] - core1 is possible
    ascii.core_id = (current_core == nv::ipc::CoreId::Core1) ? Core1Symbol
                                                             : Core0Symbol;  // 21=Core0,
                                                                             // 6=Core1

    formatt_data(fmt, ap, ascii.ascii_arr.data(), ascii.ascii_arr.size());
}
