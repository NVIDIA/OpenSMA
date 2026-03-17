/*----------------------------------------------------------------------------------------------
--                 Copyright (c) 2025, NVIDIA Corporation.  All Rights Reserved.              --
------------------------------------------------------------------------------------------------
--   NVIDIA Corporation and its licensors retain all intellectual property and proprietary    --
--   rights in and to this software and related documentation.  Any use, reproduction,        --
--   disclosure or distribution of this software and related documentation without an         --
--   express license agreement from NVIDIA Corporation is strictly prohibited.                --
----------------------------------------------------------------------------------------------*/
#pragma once
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <source_location>

namespace pdk::cmn::log::internal {

constexpr auto DefaultMagicNumberBitMask = 0x0FFFu;  ///< 12bit default

/// Compact source location identifier using FNV-1a hash.
///
/// Combines file path and function name into a small integer ID for efficient logging.
/// Uses FNV-1a hash algorithm with special handling for lambdas (hashes line number).
/// Designed for compile-time evaluation to generate unique IDs for each log callsite.
///
/// @tparam Id             Integer type for the result (default: uint16_t)
/// @tparam ResultBitMask  Bit mask applied to hash result (default: 0x0FFF for 12-bit IDs)
template<typename Id = uint16_t, uint64_t ResultBitMask = DefaultMagicNumberBitMask>
struct MagicNumber
{
    constexpr static uint64_t FnvOffset = 14695981039346656037ull;  ///< FNV-1a offset basis
    constexpr static uint64_t FnvPrime  = 1099511628211ull;         ///< FNV-1a prime

    /// Extracts simple function name from fully-qualified name.
    ///
    /// Removes namespace qualifiers, leaving only the function name.
    /// For "namespace::Class::method", returns "method".
    ///
    /// @param[in] full_func_name Fully-qualified function name
    /// @returns Pointer to simple function name (or original if no qualifiers)
    constexpr static const char* extract_simple_function_name(const char* full_func_name)
    {
        if (!full_func_name) {
            return full_func_name;
        }

        // lambda check
        if (full_func_name[0] == '<' && full_func_name[1] == 'l') {
            return full_func_name;
        }

        // Find the opening parenthesis first
        const char* paren_pos = nullptr;
        for (auto p = full_func_name; *p != '\0'; ++p) {
            if (*p == '(') {
                paren_pos = p;
                break;
            }
        }

        // std::source_location::function_name() should always have '('
        // If not found, something is wrong - just return as-is
        if (!paren_pos) {
            return full_func_name;
        }

        // Find the last occurrence of "::" before the opening parenthesis
        const char* last_scope = nullptr;
        for (auto p = full_func_name; p < paren_pos; ++p) {
            if (*p == ':' && *(p + 1) == ':' && (p + 2) < paren_pos) {
                last_scope = p + 2;  // Point to character after "::"
            }
        }

        // If no "::" found before '(', start from the beginning
        return last_scope ? last_scope : full_func_name;
    }

    /// Computes compact ID from file path and function name.
    ///
    /// Uses FNV-1a hash to combine file and function into a small ID.
    /// Normalizes path separators (backslash to forward slash).
    /// For lambdas, hashes line number instead of function name for uniqueness.
    ///
    /// @param[in] loc Source location to hash
    /// @returns Compact ID representing the location
    constexpr static Id filefunc(const std::source_location& loc)
    {
        const auto& file = loc.file_name();
        const auto& func = loc.function_name();

        // Constants for hash function
        constexpr uint8_t kHashSeparator = 0xFFu;  // Separator between file and function hash

        // Hash file path directly (normalize backslashes to forward slashes)
        auto h = FnvOffset;
        for (auto p = file; *p != 0; ++p) {
            const auto c  = static_cast<unsigned char>(*p == '\\' ? '/' : *p);
            h            ^= c;
            h            *= FnvPrime;
        }

        h ^= kHashSeparator;
        h *= FnvPrime;

        // For lambdas, hash the line number to distinguish them
        // since they all have the same function name pattern
        if (func[0] == '<' && func[1] == 'l' && func[2] == 'a') {  // lambda
            const auto line_num = loc.line();

            // Hash line number byte-by-byte
            h ^= static_cast<uint8_t>(line_num & 0xFF);
            h *= FnvPrime;
            h ^= static_cast<uint8_t>((line_num >> 8) & 0xFF);
            h *= FnvPrime;
            h ^= static_cast<uint8_t>((line_num >> 16) & 0xFF);
            h *= FnvPrime;
            h ^= static_cast<uint8_t>((line_num >> 24) & 0xFF);
            h *= FnvPrime;
        }
        else {
            // Hash simple function name
            const auto simple_func = extract_simple_function_name(func);
            // hash simple function name stops before '('
            for (auto p = simple_func; *p != 0 && *p != '(' && *p != ' '; ++p) {
                h ^= static_cast<unsigned char>(*p);
                h *= FnvPrime;
            }
        }
        return static_cast<Id>(h & ResultBitMask);
    }

    /// Creates MagicNumber from current source location.
    ///
    /// Compile-time evaluation captures caller's location and generates unique ID.
    /// Use this to automatically tag log callsites with compact identifiers.
    ///
    /// @param[in] loc Source location (defaults to caller's location)
    /// @returns MagicNumber with computed ID and source location
    static consteval MagicNumber
    current(std::source_location loc = std::source_location::current())
    {
        return {filefunc(loc), loc};
    }

    Id                   id{};  ///< Compact hash ID for this location
    std::source_location sloc;  ///< Full source location information

    /// Gets line number from source location.
    /// @returns Line number
    constexpr uint_least32_t line() const noexcept { return sloc.line(); }

    /// Gets column number from source location.
    /// @returns Column number
    constexpr uint_least32_t column() const noexcept { return sloc.column(); }

    /// Gets file name from source location.
    /// @returns File name string
    constexpr const char* file_name() const noexcept { return sloc.file_name(); }

    /// Gets function name from source location.
    /// @returns Function name string
    constexpr const char* function_name() const noexcept { return sloc.function_name(); }
};

}  // namespace pdk::cmn::log::internal

// some extra sanity checks
#include PDK_CMN_LOG_PLATFORM_TYPES_H
namespace pdk::cmn::log::internal {

/// Concept verifying type has static current() method.
template<class T>
concept has_current_static = requires { T::current(); };

static_assert(has_current_static<SourceLocationConsole>,
              "SourceLocationConsole must have static member SourceLocationConsole::current()");

static_assert(
    has_current_static<SourceLocationPersistent>,
    "SourceLocationPersistent must have static member SourceLocationPersistent::current()");

}  // namespace pdk::cmn::log::internal
