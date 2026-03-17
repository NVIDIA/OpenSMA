/*----------------------------------------------------------------------------------------------
--                 Copyright (c) 2025, NVIDIA Corporation.  All Rights Reserved.              --
------------------------------------------------------------------------------------------------
--   NVIDIA Corporation and its licensors retain all intellectual property and proprietary    --
--   rights in and to this software and related documentation.  Any use, reproduction,        --
--   disclosure or distribution of this software and related documentation without an         --
--   express license agreement from NVIDIA Corporation is strictly prohibited.                --
----------------------------------------------------------------------------------------------*/
#pragma once

namespace pdk::cmn::log {

/// Global debug levels in increasing verbosity.
enum class DebugLevel
{
    None = 0,  ///< No debug output, all debug strings removed from binary.
    Fatal,     ///< Fatal only messages.
    Error,     ///< Error and Fatal messages only.
    Warn,      ///< Warning, Error, and Fatal messages.
    Debug,     ///< Verbose debug messaging.
    Info,  ///< Maximum level of debug messaging that will have a heavy impact on performance.
};

// -- Compile-time configurable via UBS Global defines ----------------------------------------
namespace internal {
constexpr inline auto DbgLevelConsole    = DebugLevel{PDK_CMN_LOG_DBG_LEVEL_CONSOLE};
constexpr inline auto DbgLevelPersistent = DebugLevel{PDK_CMN_LOG_DBG_LEVEL_PERSISTENT};
// TODO: missing unittest
constexpr inline auto MaxMessageLength = int{PDK_CMN_LOG_MAX_MSG_LENGTH};
}  // namespace internal

// -- Runtime adjustable below Compile-time threshold -----------------------------------------
extern DebugLevel DebugLevelConsole;
extern DebugLevel DebugLevelPersistent;

}  // namespace pdk::cmn::log
