/*----------------------------------------------------------------------------------------------
--                 Copyright (c) 2025, NVIDIA Corporation.  All Rights Reserved.              --
------------------------------------------------------------------------------------------------
--   NVIDIA Corporation and its licensors retain all intellectual property and proprietary    --
--   rights in and to this software and related documentation.  Any use, reproduction,        --
--   disclosure or distribution of this software and related documentation without an         --
--   express license agreement from NVIDIA Corporation is strictly prohibited.                --
----------------------------------------------------------------------------------------------*/
#pragma once
#include <limits>
#include <utility>

#include "pdk/cmn/log/debuglevel.h"
#include "pdk/cmn/log/plat.h"
#include "pdk/cmn/log/types.h"

namespace pdk::cmn::log {

namespace internal {

/// Internal logging implementation with compile-time destination and level selection.
///
/// Uses `if constexpr` to eliminate dead code when logging is disabled at compile-time.
/// Supports both console and persistent logging with zero overhead when disabled.
///
/// @tparam Dest       Logging destination (Console, Persistent, or Both)
/// @tparam Lvl        Debug level for this log message
/// @tparam FormatArgs Variadic format argument types
/// @param[in] loc     Console source location (file, line, function)
/// @param[in] ploc    Persistent source location (file, line, function)
/// @param[in] timeout Maximum time to wait for logging operation (microseconds)
/// @param[in] fmt     Printf-style format string
/// @param[in] args    Format arguments matching the format string
/// @returns Status indicating success or failure of logging operation
template<Destination Dest, DebugLevel Lvl, typename... FormatArgs>
inline Status log_impl(const SourceLocationConsole&    loc,
                       const SourceLocationPersistent& ploc,
                       Usecs                           timeout,
                       const char*                     fmt,
                       FormatArgs&&... args)
{
    constexpr auto int_dest      = std::to_underlying(Dest);
    constexpr bool EnableConsole = ((int_dest & std::to_underlying(Destination::Console)) != 0)
                                && (internal::DbgLevelConsole >= Lvl);

    auto status = Status::Ok;

    // all code is constexpr for compile-type zero-overheaad disable
    if constexpr (EnableConsole) {
        // run-time selectable check
        if (DebugLevelConsole >= Lvl) {
            status = plat::log(Lvl, timeout, loc, fmt, std::forward<decltype(args)>(args)...);
        }
    }

    // also send to persistent storage if enabled
    constexpr bool EnablePersistent = ((int_dest & std::to_underlying(Destination::Persistent))
                                       != 0)
                                   && (internal::DbgLevelPersistent >= Lvl);
    // compile-type zero-overheaad disable
    if constexpr (EnablePersistent) {
        // run-time selectable check
        if (DebugLevelPersistent >= Lvl && status == Status::Ok) {
            status = plat::log_p(
                Lvl, timeout, ploc, fmt, std::forward<decltype(args)>(args)...);
        }
    }
    return status;
}

/// Internal logging implementation for event-based logging.
///
/// Event-based variant that logs structured event data instead of format strings.
/// Uses `if constexpr` for zero-overhead elimination when logging is disabled.
///
/// @tparam Dest       Logging destination (Console, Persistent, or Both)
/// @tparam Lvl        Debug level for this log message
/// @tparam Event      Event type derived from EventType
/// @param[in] loc     Console source location (file, line, function)
/// @param[in] ploc    Persistent source location (file, line, function)
/// @param[in] timeout Maximum time to wait for logging operation (microseconds)
/// @param[in] ev      Event object to log
/// @returns Status indicating success or failure of logging operation
template<Destination Dest, DebugLevel Lvl, typename Event>
inline Status log_impl(const SourceLocationConsole&    loc,
                       const SourceLocationPersistent& ploc,
                       Usecs                           timeout,
                       const Event&                    ev)
{
    constexpr auto int_dest      = std::to_underlying(Dest);
    constexpr bool EnableConsole = ((int_dest & std::to_underlying(Destination::Console)) != 0)
                                && (internal::DbgLevelConsole >= Lvl);

    auto status = Status::Ok;

    // Console output first
    // compile-type zero-overheaad disable
    if constexpr (EnableConsole) {
        // run-time selectable check
        if (DebugLevelConsole >= Lvl) {
            status = plat::log(Lvl, timeout, loc, ev);
        }
    }

    // also send to persistent storage
    constexpr bool EnablePersistent = ((int_dest & std::to_underlying(Destination::Persistent))
                                       != 0)
                                   && (internal::DbgLevelPersistent >= Lvl);
    // compile-type zero-overheaad disable
    if constexpr (EnablePersistent) {
        // run-time selectable check
        if (DebugLevelPersistent >= Lvl && status == Status::Ok) {
            status = plat::log_p(Lvl, timeout, ploc, ev);
        }
    }
    return status;
}

/// Base class providing logging methods for all severity levels.
///
/// Contains the core logging API with methods for fatal, error, warn, debug, and info levels.
/// Each method supports both format string and event-based logging with compile-time
/// destination selection. Designed for zero-overhead when logging is disabled.
struct Base
{
    internal::SourceLocationConsole    cloc;  ///< Console source location
    internal::SourceLocationPersistent ploc;  ///< Persistent source location

    /// Constructs Base with specified source locations.
    /// @param[in] cloc Console source location
    /// @param[in] ploc Persistent source location
    constexpr explicit Base(SourceLocationConsole cloc, SourceLocationPersistent ploc)
    : cloc{cloc}
    , ploc{ploc}
    {}

    // -- FATAL -------------------------------------------------------------------------------

    /// Logs a fatal message with specified timeout.
    /// @tparam Dest Logging destination (default: Both)
    /// @param[in] timeout Maximum wait time in microseconds
    /// @param[in] fmt Printf-style format string
    /// @param[in] args Format arguments
    /// @returns Status indicating success or failure
    template<Destination Dest = Destination::Both, typename... FormatArgs>
    inline Status fatal(Usecs timeout, const char* fmt, FormatArgs&&... args)
    {
        return log_impl<Dest, DebugLevel::Fatal>(
            cloc, ploc, timeout, fmt, std::forward<decltype(args)>(args)...);
    }

    /// Logs a fatal event with optional timeout.
    /// @tparam Dest Logging destination (default: Both)
    /// @param[in] event Event object to log
    /// @param[in] timeout Maximum wait time in microseconds (default: 0)
    /// @returns Status indicating success or failure
    template<Destination Dest = Destination::Both, typename Event>
    inline Status fatal(const Event& event, Usecs timeout = 0)
    requires derived_from_event_type<Event>
    {
        return log_impl<Dest, DebugLevel::Fatal>(cloc, ploc, timeout, event);
    }

    /// Logs a fatal message with infinite timeout.
    /// @tparam Dest Logging destination (default: Both)
    /// @param[in] fmt Printf-style format string
    /// @param[in] args Format arguments
    /// @returns Status indicating success or failure
    template<Destination Dest = Destination::Both, typename... FormatArgs>
    inline Status fatal(const char* fmt, FormatArgs&&... args)
    {
        return fatal<Dest>(
            std::numeric_limits<Usecs>::max(), fmt, std::forward<decltype(args)>(args)...);
    }

    // -- ERROR -------------------------------------------------------------------------------

    /// Logs an error message with specified timeout.
    /// @tparam Dest Logging destination (default: Both)
    /// @param[in] timeout Maximum wait time in microseconds
    /// @param[in] fmt Printf-style format string
    /// @param[in] args Format arguments
    /// @returns Status indicating success or failure
    template<Destination Dest = Destination::Both, typename... FormatArgs>
    inline Status error(Usecs timeout, const char* fmt, FormatArgs&&... args)
    {
        return log_impl<Dest, DebugLevel::Error>(
            cloc, ploc, timeout, fmt, std::forward<decltype(args)>(args)...);
    }

    /// Logs an error event with optional timeout.
    /// @tparam Dest Logging destination (default: Both)
    /// @param[in] event Event object to log
    /// @param[in] timeout Maximum wait time in microseconds (default: 0)
    /// @returns Status indicating success or failure
    template<Destination Dest = Destination::Both, typename Event>
    inline Status error(const Event& event, Usecs timeout = 0)
    requires derived_from_event_type<Event>
    {
        return log_impl<Dest, DebugLevel::Error>(cloc, ploc, timeout, event);
    }

    /// Logs an error message with infinite timeout.
    /// @tparam Dest Logging destination (default: Both)
    /// @param[in] fmt Printf-style format string
    /// @param[in] args Format arguments
    /// @returns Status indicating success or failure
    template<Destination Dest = Destination::Both, typename... FormatArgs>
    inline Status error(const char* fmt, FormatArgs&&... args)
    {
        return error<Dest>(
            std::numeric_limits<Usecs>::max(), fmt, std::forward<decltype(args)>(args)...);
    }

    // -- WARN --------------------------------------------------------------------------------

    /// Logs a warning message with specified timeout.
    /// @tparam Dest Logging destination (default: Both)
    /// @param[in] timeout Maximum wait time in microseconds
    /// @param[in] fmt Printf-style format string
    /// @param[in] args Format arguments
    /// @returns Status indicating success or failure
    template<Destination Dest = Destination::Both, typename... FormatArgs>
    inline Status warn(Usecs timeout, const char* fmt, FormatArgs&&... args)
    {
        return log_impl<Dest, DebugLevel::Warn>(
            cloc, ploc, timeout, fmt, std::forward<decltype(args)>(args)...);
    }

    /// Logs a warning event with optional timeout.
    /// @tparam Dest Logging destination (default: Both)
    /// @param[in] event Event object to log
    /// @param[in] timeout Maximum wait time in microseconds (default: 0)
    /// @returns Status indicating success or failure
    template<Destination Dest = Destination::Both, typename Event>
    inline Status warn(const Event& event, Usecs timeout = 0)
    requires derived_from_event_type<Event>
    {
        return log_impl<Dest, DebugLevel::Warn>(cloc, ploc, timeout, event);
    }

    /// Logs a warning message with infinite timeout.
    /// @tparam Dest Logging destination (default: Both)
    /// @param[in] fmt Printf-style format string
    /// @param[in] args Format arguments
    /// @returns Status indicating success or failure
    template<Destination Dest = Destination::Both, typename... FormatArgs>
    inline Status warn(const char* fmt, FormatArgs&&... args)
    {
        return warn<Dest>(
            std::numeric_limits<Usecs>::max(), fmt, std::forward<decltype(args)>(args)...);
    }

    // -- DEBUG -------------------------------------------------------------------------------

    /// Logs a debug message with specified timeout.
    /// @tparam Dest Logging destination (default: Both)
    /// @param[in] timeout Maximum wait time in microseconds
    /// @param[in] fmt Printf-style format string
    /// @param[in] args Format arguments
    /// @returns Status indicating success or failure
    template<Destination Dest = Destination::Both, typename... FormatArgs>
    inline Status debug(Usecs timeout, const char* fmt, FormatArgs&&... args)
    {
        return log_impl<Dest, DebugLevel::Debug>(
            cloc, ploc, timeout, fmt, std::forward<decltype(args)>(args)...);
    }

    /// Logs a debug event with optional timeout.
    /// @tparam Dest Logging destination (default: Both)
    /// @param[in] event Event object to log
    /// @param[in] timeout Maximum wait time in microseconds (default: 0)
    /// @returns Status indicating success or failure
    template<Destination Dest = Destination::Both, typename Event>
    inline Status debug(const Event& event, Usecs timeout = 0)
    requires derived_from_event_type<Event>
    {
        return log_impl<Dest, DebugLevel::Debug>(cloc, ploc, timeout, event);
    }

    /// Logs a debug message with infinite timeout.
    /// @tparam Dest Logging destination (default: Both)
    /// @param[in] fmt Printf-style format string
    /// @param[in] args Format arguments
    /// @returns Status indicating success or failure
    template<Destination Dest = Destination::Both, typename... FormatArgs>
    inline Status debug(const char* fmt, FormatArgs&&... args)
    {
        return debug<Dest>(
            std::numeric_limits<Usecs>::max(), fmt, std::forward<decltype(args)>(args)...);
    }

    // -- INFO --------------------------------------------------------------------------------

    /// Logs an info message with specified timeout.
    /// @tparam Dest Logging destination (default: Both)
    /// @param[in] timeout Maximum wait time in microseconds
    /// @param[in] fmt Printf-style format string
    /// @param[in] args Format arguments
    /// @returns Status indicating success or failure
    template<Destination Dest = Destination::Both, typename... FormatArgs>
    inline Status info(Usecs timeout, const char* fmt, FormatArgs&&... args)
    {
        return log_impl<Dest, DebugLevel::Info>(
            cloc, ploc, timeout, fmt, std::forward<decltype(args)>(args)...);
    }

    /// Logs an info event with optional timeout.
    /// @tparam Dest Logging destination (default: Both)
    /// @param[in] event Event object to log
    /// @param[in] timeout Maximum wait time in microseconds (default: 0)
    /// @returns Status indicating success or failure
    template<Destination Dest = Destination::Both, typename Event>
    inline Status info(const Event& event, Usecs timeout = 0)
    requires derived_from_event_type<Event>
    {
        return log_impl<Dest, DebugLevel::Info>(cloc, ploc, timeout, event);
    }

    /// Logs an info message with infinite timeout.
    /// @tparam Dest Logging destination (default: Both)
    /// @param[in] fmt Printf-style format string
    /// @param[in] args Format arguments
    /// @returns Status indicating success or failure
    template<Destination Dest = Destination::Both, typename... FormatArgs>
    inline Status info(const char* fmt, FormatArgs&&... args)
    {
        return info<Dest>(
            std::numeric_limits<Usecs>::max(), fmt, std::forward<decltype(args)>(args)...);
    }
};

}  // namespace internal

// -- Public API ------------------------------------------------------------------------------

/// Clears the console screen
/// @returns Status indicating success or failure
inline Status clear_console()
{
    return plat::clear();
}

/// Clears the persistent log buffer.
/// @returns Status indicating success or failure
inline Status clear_persistent()
{
    return plat::clear_p();
}

/// Logger that captures source location at call site.
///
/// Automatically captures file, line, and function information when constructed.
/// Use this for normal logging where you want to see where the log originated.
///
/// Example Usage:
/// @code{.cpp}
/// using enum log::Destination;
///
/// log::here().info("printf style %d to console and persistent storage\n", 42);
/// log::here().warn("printf style %d warning to console/persistent\n", 42);
/// log::here().error<Console>("send %d to console only\n", 42);
/// log::here().error<Persistent>("send %d to persistent storage only\n", 42);
/// log::here().debug(event); // sends a debug event to both console/persistent
/// log::here().fatal<Persistent>(event); // sends a fatal event to persistent storage only
/// @endcode
/// ```
struct here : internal::Base
{
    /// Constructs logger with current source location.
    /// @param[in] cloc Console source location (defaults to current location)
    /// @param[in] ploc Persistent source location (defaults to current location)
    constexpr explicit here(
        internal::SourceLocationConsole    cloc = internal::SourceLocationConsole::current(),
        internal::SourceLocationPersistent ploc = internal::SourceLocationPersistent::current())
    : internal::Base{cloc, ploc}
    {}
};

/// Logger that hides source location information.
///
/// Uses empty/default source locations, resulting in no file/line/function metadata.
/// Use this for frequent logs where location tracking adds unwanted overhead or noise.
///
/// Example Usage:
/// @code{.cpp}
/// log::hide().info("printf style %d to both console and persistent storage\n", 42);
/// log::hide().warn("printf style %d warning to console/persistent \n", 42);
/// log::hide().error<Console>("send %d to console only\n", 42);
/// log::hide().error<Persistent>("send %d to persistent storage only\n", 42);
/// log::hide().debug(event); // sends a debug event
/// @endcode
struct hide : internal::Base
{
    /// Constructs logger with empty source locations.
    /// @param[in] cloc Console source location (defaults to empty)
    /// @param[in] ploc Persistent source location (defaults to empty)
    constexpr explicit hide(internal::SourceLocationConsole    cloc = {},
                            internal::SourceLocationPersistent ploc = {})
    : internal::Base{cloc, ploc}
    {}
};

}  // namespace pdk::cmn::log
