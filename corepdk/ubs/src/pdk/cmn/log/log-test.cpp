/*----------------------------------------------------------------------------------------------
--                 Copyright (c) 2025, NVIDIA Corporation.  All Rights Reserved.              --
------------------------------------------------------------------------------------------------
--   NVIDIA Corporation and its licensors retain all intellectual property and proprietary    --
--   rights in and to this software and related documentation.  Any use, reproduction,        --
--   disclosure or distribution of this software and related documentation without an         --
--   express license agreement from NVIDIA Corporation is strictly prohibited.                --
----------------------------------------------------------------------------------------------*/
#include "pdk/cmn/log/log.h"
#include "ubs/unittest.hpp"

using namespace ubs::unittest;
using namespace pdk::cmn;
using pdk::cmn::log::Status;

// NOLINTBEGIN

UBS_TEST(PdkCmnLog, ConsoleInfo)
{
    using enum log::Destination;

    ensure::is_eq(Status::Ok, log::clear_persistent());

    ensure::is_eq(Status::Ok, log::here().info("info %d\n", 22));
    ensure::is_eq(Status::Ok, log::here().info<Console>("info %d\n", 23));
    ensure::is_eq(Status::Ok, log::here().info<Persistent>("info %d\n", 24));
    ensure::is_eq(Status::Ok, log::here().info<Both>("info %d\n", 25));

    ensure::is_eq(Status::Ok, log::hide().info("info %d\n", 26));
    ensure::is_eq(Status::Ok, log::hide().info<Console>("info %d\n", 27));
    ensure::is_eq(Status::Ok, log::hide().info<Persistent>("info %d\n", 28));
    ensure::is_eq(Status::Ok, log::hide().info<Both>("info %d\n", 29));
};

UBS_TEST(PdkCmnLog, ConsoleFatal)
{
    using enum log::Destination;

    ensure::is_eq(Status::Ok, log::clear_persistent());

    ensure::is_eq(Status::Ok, log::here().fatal("fatal %d\n", 31));
    ensure::is_eq(Status::Ok, log::here().fatal<Console>("fatal %d\n", 32));
    ensure::is_eq(Status::Ok, log::here().fatal<Persistent>("fatal %d\n", 33));
    ensure::is_eq(Status::Ok, log::here().fatal<Both>("fatal %d\n", 34));

    ensure::is_eq(Status::Ok, log::hide().fatal("fatal %d\n", 35));
    ensure::is_eq(Status::Ok, log::hide().fatal<Console>("fatal %d\n", 36));
    ensure::is_eq(Status::Ok, log::hide().fatal<Persistent>("fatal %d\n", 37));
    ensure::is_eq(Status::Ok, log::hide().fatal<Both>("fatal %d\n", 38));
};

UBS_TEST(PdkCmnLog, ConsoleError)
{
    using enum log::Destination;

    ensure::is_eq(Status::Ok, log::clear_persistent());

    ensure::is_eq(Status::Ok, log::here().error("error %d\n", 41));
    ensure::is_eq(Status::Ok, log::here().error<Console>("error %d\n", 42));
    ensure::is_eq(Status::Ok, log::here().error<Persistent>("error %d\n", 43));
    ensure::is_eq(Status::Ok, log::here().error<Both>("error %d\n", 44));

    ensure::is_eq(Status::Ok, log::hide().error("error %d\n", 45));
    ensure::is_eq(Status::Ok, log::hide().error<Console>("error %d\n", 46));
    ensure::is_eq(Status::Ok, log::hide().error<Persistent>("error %d\n", 47));
    ensure::is_eq(Status::Ok, log::hide().error<Both>("error %d\n", 48));
};

UBS_TEST(PdkCmnLog, ConsoleWarn)
{
    using enum log::Destination;

    ensure::is_eq(Status::Ok, log::clear_persistent());

    ensure::is_eq(Status::Ok, log::here().warn("warn %d\n", 51));
    ensure::is_eq(Status::Ok, log::here().warn<Console>("warn %d\n", 52));
    ensure::is_eq(Status::Ok, log::here().warn<Persistent>("warn %d\n", 53));
    ensure::is_eq(Status::Ok, log::here().warn<Both>("warn %d\n", 54));

    ensure::is_eq(Status::Ok, log::hide().warn("warn %d\n", 55));
    ensure::is_eq(Status::Ok, log::hide().warn<Console>("warn %d\n", 56));
    ensure::is_eq(Status::Ok, log::hide().warn<Persistent>("warn %d\n", 57));
    ensure::is_eq(Status::Ok, log::hide().warn<Both>("warn %d\n", 58));
};

UBS_TEST(PdkCmnLog, ConsoleDebug)
{
    using enum log::Destination;

    ensure::is_eq(Status::Ok, log::clear_persistent());

    ensure::is_eq(Status::Ok, log::here().debug("debug %d\n", 61));
    ensure::is_eq(Status::Ok, log::here().debug<Console>("debug %d\n", 62));
    ensure::is_eq(Status::Ok, log::here().debug<Persistent>("debug %d\n", 63));
    ensure::is_eq(Status::Ok, log::here().debug<Both>("debug %d\n", 64));

    ensure::is_eq(Status::Ok, log::hide().debug("debug %d\n", 65));
    ensure::is_eq(Status::Ok, log::hide().debug<Console>("debug %d\n", 66));
    ensure::is_eq(Status::Ok, log::hide().debug<Persistent>("debug %d\n", 67));
    ensure::is_eq(Status::Ok, log::hide().debug<Both>("debug %d\n", 68));
};

UBS_TEST(PdkCmnLog, Persistent)
{
    ensure::is_eq(Status::Ok, log::clear_persistent());
    // this is tested in platforms/x86/log-test.cpp
};

UBS_TEST(PdkCmnLog, EventInfo)
{
    using enum log::Destination;

    log::plat::Event ev{.id = 1, .x = 10, .y = 20, .z = 30};

    ensure::is_eq(Status::Ok, log::here().info(ev));
    ev.id++;
    ensure::is_eq(Status::Ok, log::here().info<Console>(ev));
    ev.id++;
    ensure::is_eq(Status::Ok, log::here().info<Persistent>(ev));
    ev.id++;
    ensure::is_eq(Status::Ok, log::here().info<Both>(ev));
    ev.id++;

    ensure::is_eq(Status::Ok, log::hide().info(ev));
    ev.id++;
    ensure::is_eq(Status::Ok, log::hide().info<Console>(ev));
    ev.id++;
    ensure::is_eq(Status::Ok, log::hide().info<Persistent>(ev));
    ev.id++;
    ensure::is_eq(Status::Ok, log::hide().info<Both>(ev));
    ev.id++;
};

UBS_TEST(PdkCmnLog, EventFatal)
{
    using enum log::Destination;

    log::plat::Event ev{.id = 11, .x = 100, .y = 200, .z = 300};

    ensure::is_eq(Status::Ok, log::here().fatal(ev));
    ev.id++;
    ensure::is_eq(Status::Ok, log::here().fatal<Console>(ev));
    ev.id++;
    ensure::is_eq(Status::Ok, log::here().fatal<Persistent>(ev));
    ev.id++;
    ensure::is_eq(Status::Ok, log::here().fatal<Both>(ev));
    ev.id++;

    ensure::is_eq(Status::Ok, log::hide().fatal(ev));
    ev.id++;
    ensure::is_eq(Status::Ok, log::hide().fatal<Console>(ev));
    ev.id++;
    ensure::is_eq(Status::Ok, log::hide().fatal<Persistent>(ev));
    ev.id++;
    ensure::is_eq(Status::Ok, log::hide().fatal<Both>(ev));
    ev.id++;
};

UBS_TEST(PdkCmnLog, EventError)
{
    using enum log::Destination;

    log::plat::Event ev{.id = 21, .x = 1000, .y = 2000, .z = 3000};

    ensure::is_eq(Status::Ok, log::here().error(ev));
    ev.id++;
    ensure::is_eq(Status::Ok, log::here().error<Console>(ev));
    ev.id++;
    ensure::is_eq(Status::Ok, log::here().error<Persistent>(ev));
    ev.id++;
    ensure::is_eq(Status::Ok, log::here().error<Both>(ev));
    ev.id++;

    ensure::is_eq(Status::Ok, log::hide().error(ev));
    ev.id++;
    ensure::is_eq(Status::Ok, log::hide().error<Console>(ev));
    ev.id++;
    ensure::is_eq(Status::Ok, log::hide().error<Persistent>(ev));
    ev.id++;
    ensure::is_eq(Status::Ok, log::hide().error<Both>(ev));
    ev.id++;
};

UBS_TEST(PdkCmnLog, EventWarn)
{
    using enum log::Destination;

    log::plat::Event ev{.id = 31, .x = 10000, .y = 20000, .z = 30000};

    ensure::is_eq(Status::Ok, log::here().warn(ev));
    ev.id++;
    ensure::is_eq(Status::Ok, log::here().warn<Console>(ev));
    ev.id++;
    ensure::is_eq(Status::Ok, log::here().warn<Persistent>(ev));
    ev.id++;
    ensure::is_eq(Status::Ok, log::here().warn<Both>(ev));
    ev.id++;

    ensure::is_eq(Status::Ok, log::hide().warn(ev));
    ev.id++;
    ensure::is_eq(Status::Ok, log::hide().warn<Console>(ev));
    ev.id++;
    ensure::is_eq(Status::Ok, log::hide().warn<Persistent>(ev));
    ev.id++;
    ensure::is_eq(Status::Ok, log::hide().warn<Both>(ev));
    ev.id++;
};

UBS_TEST(PdkCmnLog, EventDebug)
{
    using enum log::Destination;

    log::plat::Event ev{.id = 41, .x = 100000, .y = 200000, .z = 300000};

    ensure::is_eq(Status::Ok, log::here().debug(ev));
    ev.id++;
    ensure::is_eq(Status::Ok, log::here().debug<Console>(ev));
    ev.id++;
    ensure::is_eq(Status::Ok, log::here().debug<Persistent>(ev));
    ev.id++;
    ensure::is_eq(Status::Ok, log::here().debug<Both>(ev));
    ev.id++;

    ensure::is_eq(Status::Ok, log::hide().debug(ev));
    ev.id++;
    ensure::is_eq(Status::Ok, log::hide().debug<Console>(ev));
    ev.id++;
    ensure::is_eq(Status::Ok, log::hide().debug<Persistent>(ev));
    ev.id++;
    ensure::is_eq(Status::Ok, log::hide().debug<Both>(ev));
    ev.id++;
};

UBS_TEST(PdkCmnLog, ZeroOverheadStringElimination)
{
    using enum log::Destination;

    // These unique strings should be eliminated when PDK_CMN_LOG_DBG_LEVEL_* = 0
    // UBS will search for them and fail if found
    auto result = log::here().info(PDK_CMN_LOG_TEST_STRING_1 ": %d", 42);
    ensure::is_eq(result, Status::Ok);

    result = log::here().info(PDK_CMN_LOG_TEST_STRING_2 ": %s", "test");
    ensure::is_eq(result, Status::Ok);
};

UBS_TEST(PdkCmnLog, ZeroOverheadCompileTimeDisable)
{
    // Verify that compile-time constants and DEFs are properly evaluated
#if PDK_CMN_LOG_DBG_LEVEL_CONSOLE == 0
    constexpr auto console_disabled = (log::internal::DbgLevelConsole == log::DebugLevel::None);
    ensure::is_true(console_disabled);
#endif

#if PDK_CMN_LOG_DBG_LEVEL_PERSISTENT == 0
    constexpr auto persistent_disabled = (log::internal::DbgLevelPersistent
                                          == log::DebugLevel::None);
    ensure::is_true(persistent_disabled);
#endif
};

// Helper to test that logging calls compile to minimal code
template<log::Destination Dest, log::DebugLevel Lvl>
constexpr bool is_logging_disabled()
{
    constexpr auto int_dest         = std::to_underlying(Dest);
    constexpr bool console_disabled = ((int_dest
                                        & std::to_underlying(log::Destination::Console))
                                       != 0)
                                   && (log::internal::DbgLevelConsole >= Lvl);
    constexpr bool persistent_disabled = ((int_dest
                                           & std::to_underlying(log::Destination::Persistent))
                                          != 0)
                                      && (log::internal::DbgLevelPersistent >= Lvl);
    return !console_disabled && !persistent_disabled;
}

UBS_TEST(PdkCmnLog, ZeroOverheadTemplateEvaluation)
{
    using enum log::Destination;
    using enum log::DebugLevel;

    // When debug levels are None, all logging should be disabled at compile-time
#if PDK_CMN_LOG_DBG_LEVEL_CONSOLE == 0 && PDK_CMN_LOG_DBG_LEVEL_PERSISTENT == 0
    constexpr bool info_disabled  = is_logging_disabled<Both, Info>();
    constexpr bool error_disabled = is_logging_disabled<Both, Error>();
    constexpr bool warn_disabled  = is_logging_disabled<Both, Warn>();

    ensure::is_true(info_disabled);
    ensure::is_true(error_disabled);
    ensure::is_true(warn_disabled);
#endif
};

// NOLINTEND
