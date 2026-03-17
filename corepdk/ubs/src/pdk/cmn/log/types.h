/*----------------------------------------------------------------------------------------------
  --                 Copyright (c) 2025, NVIDIA Corporation.  All Rights Reserved. --
  ------------------------------------------------------------------------------------------------
  --   NVIDIA Corporation and its licensors retain all intellectual property and proprietary --
  --   rights in and to this software and related documentation.  Any use, reproduction, --
  --   disclosure or distribution of this software and related documentation without an --
  --   express license agreement from NVIDIA Corporation is strictly prohibited. --
  ----------------------------------------------------------------------------------------------*/
#pragma once
#include <cstdint>
#include <span>
#include <type_traits>

namespace pdk::cmn::log {

/// All Statuses related to logger module.
enum class Status
{
    Ok,         ///< No error has occurred.
    Timeout,    ///< A timeout has occured.
    Truncated,  ///< Last log message may have been Truncated
    Fatal,      ///< Fatal error has occured
    Unknown     ///< An unknown error occurred.
};

/// for timeout values in microseconds
using Usecs = std::uint64_t;

/// Platform Event Types must derive from this class.
///
/// Events must be POD-like (standard layout and trivially copyable) to ensure
/// they can be safely serialized and logged across boundaries.
template<class Derived>
struct EventType
{
    /// Converts event to byte span for serialization.
    /// Platform implementation can use this to transmit event data.
    ///
    /// @returns Read-only span of bytes representing the event
    explicit operator std::span<const std::byte>() const& noexcept
    {
        // Validate constraints when actually used (Derived is complete here)
        static_assert(
            std::is_standard_layout_v<Derived> && std::is_trivially_copyable_v<Derived>,
            "EventType requires POD-like (standard-layout, trivially copyable) types");

        auto const* p = static_cast<Derived const*>(this);
        return std::as_bytes(std::span{p, 1});
    }
};

/// Destination Sinks Bitfield values.
enum class Destination : uint8_t
{
    Console    = 0b01,  ///< Console only bit
    Persistent = 0b10,  ///< Persistent storage only bit
    Both       = 0b11   ///< Mask of all bits
};

namespace internal {
// for 'require'ing child class of EventType
template<class T>
concept derived_from_event_type = std::derived_from<std::remove_cvref_t<T>,
                                                    EventType<std::remove_cvref_t<T>>>;
}  // namespace internal
}  // namespace pdk::cmn::log
