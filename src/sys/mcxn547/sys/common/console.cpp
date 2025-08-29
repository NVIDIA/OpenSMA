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
#include "nv/common/console.h"

#include <array>
#include <cstdarg>

#include "fsl_debug_console.h"

#include "nv/common/debuglevel.h"
// #include "nv/common/system.h" //critical temporary removed
#include <cstdint>
#include <cstring>

#include "nv/common/preproc.h"
#include "nv/common/utils.h"
#include "nv/ipc/supervisor.h"
#include "nv/logger/uart_string.h"

using namespace nv::common;

// We are using varargs here to avoid code bloating from templating.
// coverity[cert_dcl50_cpp_violation] - intentional use of varargs
void Console::print(DebugLevel lvl, const char* fmt, ...)  // NOLINT(cert-dcl50-cpp)
{
    if (!_enabled) {
        return;
    }  // disabled
    (void)lvl;
#if 0
    // Select terminal color and output pipe by debuglevel
    NV_SHARED_DATA static std::array<const char*, 6> cfg{"",  // None
                                                         "\x1b[0;31mFATAL \x1B[0m",
                                                         "\x1b[0;31mERROR \x1B[0m",
                                                         "\x1b[0;33mWARN  \x1B[0m",
                                                         "\x1b[0;32mDBG   \x1B[0m",
                                                         "\x1b[1;37mINFO  \x1B[0m"};
    auto&                                            v = cfg.at(common::to_underlying(lvl));
    auto& task = nv::ipc::Supervisor::inst().current_task().name();

    // critical is privilege function, cant call directly
    // const ScopedCritical critical;  // TODO:

    // NOLINTBEGIN
    DbgConsole_Printf("%s[%s] ", v, task.data());
    va_list va;
    va_start(va, fmt);
    DbgConsole_Vprintf(fmt, va);
    va_end(va);
    // NOLINTEND
#endif

    // NOLINTBEGIN
    va_list va;
    va_start(va, fmt);
    nv::logger::format_to_logger(fmt, va);
    va_end(va);
    // NOLINTEND
}

#include <cstdarg>
#include <cstring>

#include "nv/common/debug.h"
#include "nv/ipc/supervisor.h"
#include "nv/logger/log.h"
#include "nv/uart/driver.h"
// #include NV_IPC_CONFIG_H

using namespace nv::logger;
namespace {

constexpr auto DecBase = 10;
constexpr auto HexBase = 16;

void process_request(uint8_t* ascii_char, uint32_t size)
{
#if defined(CPU_MCXN547VDF_cm33_core0)
    if (!nv::ipc::Supervisor::is_scheduler_run()) {
        std::array<uint8_t, DumpCheckSize + AsciiStrLen> dump_buffer{};
        dump_buffer[0]     = DumpHeadMagicRaw;
        dump_buffer.back() = DumpTailMagicRaw;
        std::memcpy(dump_buffer.data() + 1, ascii_char, size);
        auto driver = nv::uart::Driver(static_cast<nv::uart::Port>(nv::ipc::UartInstance));
        driver.tx(dump_buffer);
    }
    else {
        nv::logger::AsciiArr ascii{};
        ascii.flag = true;
        memcpy(ascii.ascii_arr.data(), ascii_char, size);
        nv::logger::Logger::string_console(ascii);
    }
#endif

#if defined(CPU_MCXN547VDF_cm33_core1)
    std::array<uint8_t, DumpCheckSize + AsciiStrLen> dump_buffer{};
    dump_buffer[0]     = DumpHeadMagicRaw;
    dump_buffer.back() = DumpTailMagicRaw;
    std::memcpy(dump_buffer.data() + 1, ascii_char, size);
    auto driver = nv::uart::Driver(static_cast<nv::uart::Port>(nv::ipc::UartInstance));
    driver.tx(dump_buffer);
#endif
}

void check_buffer_and_process(uint8_t* ascii_char, uint32_t size, uint32_t& index)
{
    if (index >= size && index > 0) {
        process_request(ascii_char, index);
        index = 0;
    }
}

int32_t convert_radix_num_to_string(char* numstr, void* nump, int32_t radix, bool use_caps)
{
    uint32_t ua{};
    uint32_t ub{};
    uint32_t uc{};

    int32_t nlen{};
    char*   nstrp{};

    nlen     = 0;
    nstrp    = numstr;
    *nstrp++ = '\0';

    ua = *static_cast<uint32_t*>(nump);

    if (ua == 0) {
        *nstrp = '0';
        ++nlen;
        return nlen;
    }

    while (ua) {
        ub = ua / (unsigned int)radix;
        uc = ua - (ub * (unsigned int)radix);

        if (uc < DecBase) {
            uc = uc + (unsigned int)'0';
        }
        else {
            uc = uc - DecBase + (unsigned int)(use_caps ? 'A' : 'a');
        }
        ua       = ub;
        *nstrp++ = (char)uc;
        ++nlen;
    }
    return nlen;
}

int format_data(const char* fmt, va_list ap, uint8_t* ascii_char, const uint32_t size)
{
    const char* p{};
    char        c{};

    constexpr auto                MaxDigitNum = 33;
    std::array<char, MaxDigitNum> vstr{};

    char*   sval  = nullptr;
    char*   vstrp = nullptr;
    int32_t vlen  = 0;

    int32_t count = 0;

    bool use_caps{};

    int          ival{};
    unsigned int uval = 0;

    uint32_t index = 0;
    p              = fmt;
    while (true) {
        if ('\0' == *p) {
            break;
        }
        c = *p;
        if (c != '%') {
            ascii_char[index++] = static_cast<uint8_t>(c);
            check_buffer_and_process(ascii_char, size, index);
            count++;
            p++;
            continue;
        }

        c = *++p;
        if (c == 'd' || c == 'x' || c == 'X') {
            if (c == 'd') {
                {
                    ival = (int)va_arg(ap, int);  // NOLINT(*vararg)
                }
                vlen  = convert_radix_num_to_string(vstr.data(), &ival, DecBase, use_caps);
                vstrp = &vstr.at(vlen);
            }

            if ((c == 'X') || (c == 'x')) {
                if (c == 'x') {
                    use_caps = false;
                }

                {
                    uval = (unsigned int)va_arg(ap, unsigned int);  // NOLINT(*vararg)
                }
                vlen  = convert_radix_num_to_string(vstr.data(), &uval, HexBase, use_caps);
                vstrp = &vstr.at(vlen);
            }
            if (vstrp != nullptr) {
                while ('\0' != *vstrp) {
                    const uint8_t Word = *vstrp;
                    vstrp--;
                    ascii_char[index++] = Word;
                    count++;
                    check_buffer_and_process(ascii_char, size, index);
                }
            }
        }
        else if (c == 's') {
            sval = (char*)va_arg(ap, char*);  // NOLINT(*vararg)
            if (nullptr != sval) {
                while ('\0' != *sval) {
                    const uint8_t Word  = *sval;
                    ascii_char[index++] = Word;
                    sval++;
                    count++;
                    check_buffer_and_process(ascii_char, size, index);
                }
            }
        }
        else {
            ascii_char[index++] = static_cast<uint8_t>(c);
            count++;
            check_buffer_and_process(ascii_char, size, index);
        }
        p++;
    }
    check_buffer_and_process(ascii_char, index, index);
    return count;
}

void format_to_core1_logger(const char* fmt, va_list ap)
{
    nv::logger::AsciiArr ascii{};
    ascii.flag = true;
    format_data(fmt, ap, ascii.ascii_arr.data(), ascii.ascii_arr.size());
}

}  // namespace

void Console::print_core(DebugLevel lvl, const char* fmt, ...)  // NOLINT(cert-dcl50-cpp)
{
    if (!_enabled) {
        return;
    }  // disabled
    (void)lvl;
#if 0
    // Select terminal color and output pipe by debuglevel
    NV_SHARED_DATA static std::array<const char*, 6> cfg{"",  // None
                                                         "\x1b[0;31mFATAL \x1B[0m",
                                                         "\x1b[0;31mERROR \x1B[0m",
                                                         "\x1b[0;33mWARN  \x1B[0m",
                                                         "\x1b[0;32mDBG   \x1B[0m",
                                                         "\x1b[1;37mINFO  \x1B[0m"};
    auto&                                            v = cfg.at(common::to_underlying(lvl));
    auto& task = nv::ipc::Supervisor::inst().current_task().name();

    // critical is privilege function, cant call directly
    // const ScopedCritical critical;  // TODO:

    // NOLINTBEGIN
    DbgConsole_Printf("%s[%s] ", v, task.data());
    va_list va;
    va_start(va, fmt);
    DbgConsole_Vprintf(fmt, va);
    va_end(va);
    // NOLINTEND
#endif

    // NOLINTBEGIN
    va_list va;
    va_start(va, fmt);
    format_to_core1_logger(fmt, va);
    va_end(va);
    // NOLINTEND
}