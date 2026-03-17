/*----------------------------------------------------------------------------------------------
--                 Copyright (c) 2025, NVIDIA Corporation.  All Rights Reserved.              --
------------------------------------------------------------------------------------------------
--   NVIDIA Corporation and its licensors retain all intellectual property and proprietary    --
--   rights in and to this software and related documentation.  Any use, reproduction,        --
--   disclosure or distribution of this software and related documentation without an         --
--   express license agreement from NVIDIA Corporation is strictly prohibited.                --
----------------------------------------------------------------------------------------------*/
#include <cstring>

#include "pdk/cmn/log/log.h"
#include "ubs/unittest.hpp"

// WARNING: do not modify anything above // HERE -------------

// TODO: this is duplicated
constexpr auto PersistentFile = "persistent.log";

using namespace ubs::unittest;
using namespace pdk::cmn;
using pdk::cmn::log::Status;

// NOLINTBEGIN

UBS_TEST(PdkCmnLog, X64Persistent)
{
    using enum log::Destination;

    ensure::is_eq(Status::Ok, log::clear_persistent());

    ensure::is_eq(Status::Ok, log::hide().info<Persistent>("info%d\n", 24));
    ensure::is_eq(Status::Ok, log::hide().debug<Persistent>("debug%d\n", 25));
    ensure::is_eq(Status::Ok, log::hide().warn<Persistent>("warn%d\n", 26));
    ensure::is_eq(Status::Ok, log::hide().error<Persistent>("error%d\n", 27));
    ensure::is_eq(Status::Ok, log::hide().fatal<Persistent>("fatal%d\n", 28));

    if (auto f = fopen(PersistentFile, "rt"); f != nullptr) {
        const auto gold =
            "[INFO  ] info24\n"
            "[DBG   ] debug25\n"
            "[WARN  ] warn26\n"
            "[ERROR ] error27\n"
            "[FATAL ] fatal28\n";

        const auto             len = strlen(gold);
        std::array<char, 1024> buf;
        if (log::internal::DbgLevelPersistent != log::DebugLevel::None) {
            ensure::is_eq(fread(buf.data(), sizeof(char), len, f), len);
            ensure::is_eq(strncmp(buf.data(), gold, len), 0);
        }
        else {
            ensure::is_eq(fread(buf.data(), sizeof(char), len, f), 0);
            ensure::is_ne(strncmp(buf.data(), gold, len), 0);
        }
        fclose(f);
    }
    else {
        ensure::is_true(false);
    }
};

UBS_TEST(PdkCmnLog, X64PersistentEvent)
{
    using enum log::Destination;

    ensure::is_eq(Status::Ok, log::clear_persistent());

    log::plat::Event ev{.id = 1, .x = 10, .y = 20, .z = 30};
    ensure::is_eq(Status::Ok, log::hide().info<Persistent>(ev));
    ev.id++;
    ensure::is_eq(Status::Ok, log::hide().debug<Persistent>(ev));
    ev.id++;
    ensure::is_eq(Status::Ok, log::hide().warn<Persistent>(ev));
    ev.id++;
    ensure::is_eq(Status::Ok, log::hide().error<Persistent>(ev));
    ev.id++;
    ensure::is_eq(Status::Ok, log::hide().fatal<Persistent>(ev));
    ev.id++;

    if (auto f = fopen(PersistentFile, "rt"); f != nullptr) {
        const auto gold =
            "[INFO  ] ev.1:x=10,y=20,z=30\n"
            "[DBG   ] ev.2:x=10,y=20,z=30\n"
            "[WARN  ] ev.3:x=10,y=20,z=30\n"
            "[ERROR ] ev.4:x=10,y=20,z=30\n"
            "[FATAL ] ev.5:x=10,y=20,z=30\n";

        const auto             len = strlen(gold);
        std::array<char, 1024> buf;
        if (log::internal::DbgLevelPersistent != log::DebugLevel::None) {
            ensure::is_eq(fread(buf.data(), sizeof(char), len, f), len);
            ensure::is_eq(strncmp(buf.data(), gold, len), 0);
        }
        else {
            ensure::is_eq(fread(buf.data(), sizeof(char), len, f), 0);
            ensure::is_ne(strncmp(buf.data(), gold, len), 0);
        }
        fclose(f);
    }
    else {
        ensure::is_true(false);
    }
};

UBS_TEST(PdkCmnLog, MagicNumber)
{
    using enum log::Destination;

    ensure::is_eq(Status::Ok, log::clear_persistent());

    log::plat::Event ev{.id = 1, .x = 10, .y = 20, .z = 30};

    ensure::is_eq(Status::Ok, log::here().info<Persistent>(ev));

    // TODO: this is a manual only test as filename's are not consistent across
    // CICD runs
    if constexpr (false) {
        // dont test if we are using source_location and not magic number
        if constexpr (!std::same_as<log::internal::SourceLocationPersistent,
                                    std::source_location>) {
            if (auto f = fopen(PersistentFile, "rt"); f != nullptr) {
                const auto gold =
                    "[INFO  ] magic = 1500\n"
                    "[INFO  ] ev.1:x=10,y=20,z=30\n";

                const auto             len = strlen(gold);
                std::array<char, 1024> buf;
                if (log::internal::DbgLevelPersistent != log::DebugLevel::None) {
                    ensure::is_eq(fread(buf.data(), sizeof(char), len, f), len);
                    ensure::is_eq(strncmp(buf.data(), gold, len), 0);
                }
                else {
                    ensure::is_eq(fread(buf.data(), sizeof(char), len, f), 0);
                    ensure::is_ne(strncmp(buf.data(), gold, len), 0);
                }
                fclose(f);
            }
            else {
                ensure::is_true(false);
            }
        }
    }
};

// HERE ---------------------------------------------------------------------------------------
// NOLINTEND
