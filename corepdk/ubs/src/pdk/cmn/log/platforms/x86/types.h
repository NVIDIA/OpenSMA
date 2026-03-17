/*----------------------------------------------------------------------------------------------
--                 Copyright (c) 2024, NVIDIA Corporation.  All Rights Reserved.              --
------------------------------------------------------------------------------------------------
--   NVIDIA Corporation and its licensors retain all intellectual property and proprietary    --
--   rights in and to this software and related documentation.  Any use, reproduction,        --
--   disclosure or distribution of this software and related documentation without an         --
--   express license agreement from NVIDIA Corporation is strictly prohibited.                --
----------------------------------------------------------------------------------------------*/
#pragma once
#include <cstdint>
#include <source_location>
#include <type_traits>

#include "pdk/cmn/log/source_location.h"
#include "pdk/cmn/log/types.h"

namespace pdk::cmn::log::plat {

/// Platform-specific event type for x86.
///
/// Sample implementation that can be replaced by platform-specific events.
/// Point GD_PDK_CMN_LOG_PLATFORM_TYPES_H to custom file to override.
struct Event : EventType<Event>
{
    int id = 0;
    int x = 0, y = 0, z = 0;
};

}  // namespace pdk::cmn::log::plat

namespace pdk::cmn::log::internal {

/// Compact source location type using 12-bit hash IDs.
using MagicNumberType = MagicNumber<uint16_t, DefaultMagicNumberBitMask>;

/// Console source location type selection for unittesting
///
/// Conditionally selects between full std::source_location or compact MagicNumber
/// based on PDK_CMN_LOG_SL_CONSOLE configuration (0 = full, 1 = compact).
using SourceLocationConsole = std::
    conditional_t<PDK_CMN_LOG_SL_CONSOLE == 0, std::source_location, MagicNumberType>;

/// Persistent source location type selection for unittesting
///
/// Conditionally selects between full std::source_location or compact MagicNumber
/// based on PDK_CMN_LOG_SL_PERSISTENT configuration (0 = full, 1 = compact).
using SourceLocationPersistent = std::
    conditional_t<PDK_CMN_LOG_SL_PERSISTENT == 0, std::source_location, MagicNumberType>;

}  // namespace pdk::cmn::log::internal
