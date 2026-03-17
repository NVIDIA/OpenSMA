/*----------------------------------------------------------------------------------------------
--                 Copyright (c) 2025, NVIDIA Corporation.  All Rights Reserved.              --
------------------------------------------------------------------------------------------------
--   NVIDIA Corporation and its licensors retain all intellectual property and proprietary    --
--   rights in and to this software and related documentation.  Any use, reproduction,        --
--   disclosure or distribution of this software and related documentation without an         --
--   express license agreement from NVIDIA Corporation is strictly prohibited.                --
----------------------------------------------------------------------------------------------*/
#include "pdk/cmn/log/log.h"

using namespace pdk::cmn;

// -- Global Variable Section Placement -------------------------------------------------------
namespace pdk::cmn::log {
// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables)
[[gnu::section(PDK_CMN_LOG_GLOBAL)]] DebugLevel DebugLevelConsole = internal::DbgLevelConsole;
[[gnu::section(
    PDK_CMN_LOG_GLOBAL)]] DebugLevel DebugLevelPersistent = internal::DbgLevelPersistent;
// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)
}  // namespace pdk::cmn::log

// -- Ada exports -----------------------------------------------------------------------------

/// Ada foreign function interface for console logging.
///
/// Maps Ada log levels (0-5) to C++ logging API.
/// Currently uses simplified implementation pending proper Ada source location support.
///
/// @param[in] lvl Log level (0=None, 1=Fatal, 2=Error, 3=Warn, 4=Debug, 5=Info)
/// @param[in] msg Null-terminated message string
/// @returns Status code as integer (0=Ok, other values indicate errors)
extern "C" int ada_log(int lvl, const char* msg)
{
    // #warning "TODO: ada logging reverted and incomplete"  //
    // NOLINT(clang-diagnostic-#warnings)
    // -----------------------------------------------------------------------------------
    // TODO: Ada GNAT.Source_Info and Magic Number calculation are not working as required
    // reverting back to basic implementation for now
    // -----------------------------------------------------------------------------------
    auto ret = log::Status::Unknown;
    switch (lvl) {
        using enum log::Destination;
        case 0 : break;
        case 1 : ret = log::here().fatal<Console>("%s\n", msg); break;
        case 2 : ret = log::here().error<Console>("%s\n", msg); break;
        case 3 : ret = log::here().warn<Console>("%s\n", msg); break;
        case 4 : ret = log::here().debug<Console>("%s\n", msg); break;
        case 5 : ret = log::here().info<Console>("%s\n", msg); break;
        default: break;
    }
    return std::to_underlying(ret);
}

/// Ada foreign function interface for persistent logging.
///
/// Maps Ada log levels (0-5) to C++ persistent logging API.
/// Currently uses simplified implementation pending proper Ada source location support.
///
/// @param[in] lvl Log level (0=None, 1=Fatal, 2=Error, 3=Warn, 4=Debug, 5=Info)
/// @param[in] msg Null-terminated message string
/// @returns Status code as integer (0=Ok, other values indicate errors)
extern "C" int ada_log_p(int lvl, const char* msg)
{
    auto ret = log::Status::Unknown;
    switch (lvl) {
        using enum log::Destination;
        case 0 : break;
        case 1 : ret = log::here().fatal<Persistent>("%s\n", msg); break;
        case 2 : ret = log::here().error<Persistent>("%s\n", msg); break;
        case 3 : ret = log::here().warn<Persistent>("%s\n", msg); break;
        case 4 : ret = log::here().debug<Persistent>("%s\n", msg); break;
        case 5 : ret = log::here().info<Persistent>("%s\n", msg); break;
        default: break;
    }
    return std::to_underlying(ret);
}

extern "C" void ada_log_puts(const char* msg)
{
    log::plat::puts(msg);
}

extern "C" void ada_log_putc(int ch)
{
    log::plat::putc(ch);
}

extern "C" void ada_log_flush()
{
    log::plat::flush();
}
