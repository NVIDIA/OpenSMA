/*----------------------------------------------------------------------------------------------
--                 Copyright (c) 2025, NVIDIA Corporation.  All Rights Reserved.              --
------------------------------------------------------------------------------------------------
--   NVIDIA Corporation and its licensors retain all intellectual property and proprietary    --
--   rights in and to this software and related documentation.  Any use, reproduction,        --
--   disclosure or distribution of this software and related documentation without an         --
--   express license agreement from NVIDIA Corporation is strictly prohibited.                --
----------------------------------------------------------------------------------------------*/
#include <array>
#include <cstdarg>
#include <cstdio>
#include <memory>
#include <utility>

#include "pdk/cmn/log/plat.h"

namespace pdk::cmn::log::plat {

/// File path for persistent log storage.
constexpr auto PersistentFile = "persistent.log";

namespace {

// coverity[declared_but_not_referenced]
struct FileCloser
{
    void operator()(FILE* file) const noexcept
    {
        if (file != nullptr) {
            static_cast<void>(std::fclose(file));  // NOLINT(cppcoreguidelines-owning-memory)
        }
    }
};

using FilePtr = std::unique_ptr<FILE, FileCloser>;

// coverity[declared_but_not_referenced]
[[nodiscard]] FilePtr open_file(const char* path, const char* mode)
{
    return FilePtr(std::fopen(path, mode));
}

}  // namespace

// Function implementations are documented in plat.h header
// Implementation details specific to x86 platform:
// - Uses stdout/stderr with ANSI color codes for console output
// - Stores persistent logs to local file (persistent.log)
// - Supports both std::source_location and MagicNumber formats

// coverity[cert_dcl50_cpp_violation] allow C-style variadic function
Status log(DebugLevel                             level,  // NOLINT(cert-dcl50-cpp)
           [[maybe_unused]] Usecs                 timeout,
           const internal::SourceLocationConsole& sloc,
           const char*                            fmt,
           ...)
{
    // Configuration: maps debug level to output stream and ANSI color prefix
    static std::array<std::pair<FILE*, const char*>, 6> cfg{
        {{stdout, ""},                         // None
         {stderr, "\x1b[0;31mFATAL \x1B[0m"},  // Red
         {stderr, "\x1b[0;31mERROR \x1B[0m"},  // Red
         {stderr, "\x1b[0;33mWARN  \x1B[0m"},  // Yellow
         {stdout, "\x1b[0;32mDBG   \x1B[0m"},  // Green
         {stdout, "\x1b[1;37mINFO  \x1B[0m"}}  // Bright white
    };

    const auto& v = cfg.at(std::to_underlying(level));

    // NOLINTBEGIN
    // Output source location if using std::source_location
    if constexpr (std::same_as<internal::SourceLocationConsole, std::source_location>) {
        const auto* fn = sloc.function_name();
        if (fn != nullptr && fn[0] != '\0') {  // hide()
            if (std::fprintf(v.first, "[%s] ", v.second) < 0) {
                return Status::Fatal;
            }
            if (std::fprintf(v.first, "%s:%d (%s)\n", sloc.file_name(), sloc.line(), fn) < 0) {
                return Status::Fatal;
            }
        }
    }
    // Output compact magic number ID if using MagicNumber format
    if constexpr (std::same_as<internal::SourceLocationConsole, internal::MagicNumberType>) {
        const auto& mn = *reinterpret_cast<const internal::MagicNumber<uint16_t, 0x0FFF>*>(
            &sloc);
        if (mn.id != 0) {
            if (std::fprintf(v.first, "[%s] ", v.second) < 0) {
                return Status::Fatal;
            }
            if (std::fprintf(v.first, "magic = %d\n", mn.id) < 0) {
                return Status::Fatal;
            }
        }
    }
    va_list va;
    va_start(va, fmt);
    if (std::fprintf(v.first, "[%s] ", v.second) < 0) {
        va_end(va);
        return Status::Fatal;
    }
    if (std::vfprintf(v.first, fmt, va) < 0) {
        va_end(va);
        return Status::Fatal;
    }
    va_end(va);
    // coverity[cert_err33_c_violation] allow no status check for fflush
    fflush(v.first);

    // NOLINTEND
    return Status::Ok;
}

// coverity[cert_dcl50_cpp_violation] allow C-style variadic function
Status log_p(DebugLevel                                level,  // NOLINT(cert-dcl50-cpp)
             [[maybe_unused]] Usecs                    timeout,
             const internal::SourceLocationPersistent& sloc,
             const char*                               fmt,
             ...)
{
    // Plain text prefixes for persistent logs (no ANSI codes)
    static std::array<const char*, 6> cfg{"", "FATAL ", "ERROR ", "WARN  ", "DBG   ", "INFO  "};

    const auto& v = cfg.at(std::to_underlying(level));

    // NOLINTBEGIN
    if (auto file_ptr = open_file(PersistentFile, "at+"); file_ptr != nullptr) {
        auto* fout = file_ptr.get();
        // Output source location if using std::source_location
        if constexpr (std::same_as<internal::SourceLocationPersistent, std::source_location>) {
            // coverity[string_lit_comparison] allow string literal comparison
            if (sloc.function_name() != internal::SourceLocationPersistent{}.function_name()) {
                fprintf(fout, "[%s] ", v);
                fprintf(
                    fout, "%s:%d (%s)\n", sloc.file_name(), sloc.line(), sloc.function_name());
            }
        }
        // Output compact magic number ID if using MagicNumber format
        if constexpr (std::same_as<internal::SourceLocationPersistent,
                                   internal::MagicNumberType>) {
            const auto& mn = *reinterpret_cast<const internal::MagicNumber<uint16_t, 0x0FFF>*>(
                &sloc);
            if (mn.id != 0) {
                // coverity[cert_err33_c_violation] allow no status check for fprintf
                fprintf(fout, "[%s] ", v);
                // coverity[cert_err33_c_violation] allow no status check for fprintf
                fprintf(fout, "magic = %d\n", mn.id);
            }
        }

        va_list va;
        va_start(va, fmt);
        // coverity[cert_err33_c_violation] allow no status check for fprintf
        fprintf(fout, "[%s] ", v);
        // coverity[cert_err33_c_violation] allow no status check for vfprintf
        vfprintf(fout, fmt, va);
        va_end(va);
    }
    // NOLINTEND
    else {
        return Status::Fatal;
    }

    return Status::Ok;
}

void putc(int ch)
{
    // coverity[cert_err33_c_violation] allow no status check for putc
    std::putc(ch, stdout);  // NOLINT
}

void puts(const char* str)
{
    if (std::puts(str) == EOF) {
        std::clearerr(stdout);
    }
}

void flush()
{
    // NOLINTBEGIN
    // coverity[cert_err33_c_violation] allow no status check for fflush
    // coverity[cert_pos54_c_violation] allow no status check for fflush
    std::fflush(stdout);
    // coverity[cert_err33_c_violation] allow no status check for fflush
    // coverity[cert_pos54_c_violation] allow no status check for fflush
    std::fflush(stderr);
    // NOLINTEND
}

Status clear()
{
    // TODO: ^L (form feed for terminal clear)
    return Status::Ok;
}

Status clear_p()
{
    // NOLINTBEGIN
    // coverity[cert_err33_c_violation] allow no status check for fopen
    if (auto file_ptr = open_file(PersistentFile, "wt"); file_ptr != nullptr) {
        return Status::Ok;
    }
    // NOLINTEND
    return Status::Unknown;
}

// Event logging implementations - convert events to formatted strings
Status log(DebugLevel                             level,
           Usecs                                  timeout,
           const internal::SourceLocationConsole& sloc,
           const Event&                           event)
{
    return log(
        level, timeout, sloc, "ev.%d:x=%d,y=%d,z=%d\n", event.id, event.x, event.y, event.z);
}

Status log_p(DebugLevel                                level,
             Usecs                                     timeout,
             const internal::SourceLocationPersistent& sloc,
             const Event&                              event)
{
    return log_p(
        level, timeout, sloc, "ev.%d:x=%d,y=%d,z=%d\n", event.id, event.x, event.y, event.z);
}

}  // namespace pdk::cmn::log::plat
