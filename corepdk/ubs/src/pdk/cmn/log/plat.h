/*----------------------------------------------------------------------------------------------
--                 Copyright (c) 2025, NVIDIA Corporation.  All Rights Reserved.              --
------------------------------------------------------------------------------------------------
--   NVIDIA Corporation and its licensors retain all intellectual property and proprietary    --
--   rights in and to this software and related documentation.  Any use, reproduction,        --
--   disclosure or distribution of this software and related documentation without an         --
--   express license agreement from NVIDIA Corporation is strictly prohibited.                --
----------------------------------------------------------------------------------------------*/
#pragma once
#include <cstdarg>

#include "pdk/cmn/log/debuglevel.h"
#include "pdk/cmn/log/source_location.h"
#include "pdk/cmn/log/types.h"
#include PDK_CMN_LOG_PLATFORM_TYPES_H

namespace pdk::cmn::log::plat {

/// Platform-specific console logging with printf-style format string.
///
/// Sends log message to console output (typically stdout/stderr/uart).
/// Platform implementation must handle format string expansion and output.
///
/// @param[in] level   Debug level for this log message
/// @param[in] timeout Maximum time to wait for logging operation (microseconds)
/// @param[in] sloc    Console source location (file, line, function)
/// @param[in] fmt     Printf-style format string
/// @param[in] ...     Variadic arguments matching the format string
/// @returns Status indicating success or failure
Status log(DebugLevel                             level,
           Usecs                                  timeout,
           const internal::SourceLocationConsole& sloc,
           const char*                            fmt,
           ...);

/// Platform-specific persistent logging with printf-style format string.
///
/// Sends log message to persistent storage (typically file or flash).
/// Platform implementation must handle format string expansion and storage.
///
/// @param[in] level   Debug level for this log message
/// @param[in] timeout Maximum time to wait for logging operation (microseconds)
/// @param[in] sloc    Persistent source location (file, line, function)
/// @param[in] fmt     Printf-style format string
/// @param[in] ...     Variadic arguments matching the format string
/// @returns Status indicating success or failure
Status log_p(DebugLevel                                level,
             Usecs                                     timeout,
             const internal::SourceLocationPersistent& sloc,
             const char*                               fmt,
             ...);

/// Platform-specific console logging for event objects.
///
/// Sends event data to console output.
/// Platform implementation must handle event serialization and output.
///
/// @param[in] level   Debug level for this log message
/// @param[in] timeout Maximum time to wait for logging operation (microseconds)
/// @param[in] sloc    Console source location (file, line, function)
/// @param[in] event   Event object to log
/// @returns Status indicating success or failure
Status log(DebugLevel                             level,
           Usecs                                  timeout,
           const internal::SourceLocationConsole& sloc,
           const Event&                           event);

/// Platform-specific persistent logging for event objects.
///
/// Sends event data to persistent storage.
/// Platform implementation must handle event serialization and storage.
///
/// @param[in] level   Debug level for this log message
/// @param[in] timeout Maximum time to wait for logging operation (microseconds)
/// @param[in] sloc    Persistent source location (file, line, function)
/// @param[in] event   Event object to log
/// @returns Status indicating success or failure
Status log_p(DebugLevel                                level,
             Usecs                                     timeout,
             const internal::SourceLocationPersistent& sloc,
             const Event&                              event);

/// Outputs a single character to the console.
///
/// Platform-specific implementation for character output.
/// Typically used for low-level console operations.
///
/// @param[in] ch Character to output
/// @returns Status indicating success or failure
void putc(int ch);
void puts(const char* str);
void flush();

/// Clears the console log buffer.
///
/// Platform-specific implementation to clear console output buffer.
/// May be a no-op on platforms without buffering.
///
/// @returns Status indicating success or failure
Status clear();

/// Clears the persistent log buffer.
///
/// Platform-specific implementation to clear persistent storage.
/// May truncate log file or erase flash region.
///
/// @returns Status indicating success or failure
Status clear_p();

}  // namespace pdk::cmn::log::plat
