/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES.
 * All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _NVTYPES_H__INCLUDED_
#define _NVTYPES_H__INCLUDED_

// Copied from //sw/dev/gpu_drv/chips_a/sdk/nvidia/inc/nvtypes.h

/***************************************************************************\
|*                                 Typedefs                                  *|
\***************************************************************************/

// Duplicate defines the moment we pull in libnvriscv library.
// TODO: GFWC-3540
#if FALC_UCODE_APP_ID == 9

#include <stdbool.h>

#include <nvriscv/status.h>

#else

typedef unsigned char      NvV8;  /* "void": enumerated or multiple fields   */
typedef unsigned short     NvV16; /* "void": enumerated or multiple fields   */
typedef unsigned int       NvV32; /* "void": enumerated or multiple fields   */
typedef unsigned char      NvU8;  /* 0 to 255                                */
typedef unsigned short     NvU16; /* 0 to 65535                              */
typedef unsigned int       NvU32; /* 0 to 4294967295                         */
typedef unsigned long long NvU64; /* 0 to 18446744073709551615               */
typedef long long          Nv64;  /* 0 to 18446744073709551615               */
typedef signed char        NvS8;  /* -128 to 127                             */
typedef signed short       NvS16; /* -32768 to 32767                         */
typedef signed int         NvS32; /* -2147483648 to 2147483647               */
typedef long long          NvS64; /* 2^-63 to 2^63-1                         */
typedef float              NvF32; /* IEEE Single Precision (S1E8M23)         */
typedef double             NvF64; /* IEEE Double Precision (S1E11M52)        */

#ifdef GCC_FALCON
#define NV_POINTER_TYPE NvU32
#else  // RISC-V
#define NV_POINTER_TYPE NvU64
#endif  // GCC_FALCON

/* Boolean type */
/// typedef NvU8 NvBool;
/// #define NV_TRUE           ((NvBool)(0 == 0))
/// #define NV_FALSE          ((NvBool)(0 != 0))

#include <stdbool.h>
#define NvBool   bool
#define NV_TRUE  ((NvBool)(0 == 0))
#define NV_FALSE ((NvBool)(0 != 0))

#ifndef NULL
#define NULL 0
#endif

// Macros to get the MSB and LSB of a 16 bit unsigned number
#define NvU16_HI08(n)         ((NvU8)(((NvU16)(n)) >> 8))
#define NvU16_LO08(n)         ((NvU8)((NvU16)(n)))

// Macro to build a NvU16 from msb and lsb bytes.
#define NvU16_BUILD(msb, lsb) (((msb) << 8) | (lsb))

/* Macros to extract the low and high parts of a 64-bit unsigned integer */
/* Also designed to work if someone happens to pass in a 32-bit integer */
#define NvU64_HI32(n)         ((NvU32)((((NvU64)(n)) >> 32) & 0xffffffff))
#define NvU64_LO32(n)         ((NvU32)(((NvU64)(n)) & 0xffffffff))
#define NvU40_HI32(n)         ((NvU32)((((NvU64)(n)) >> 8) & 0xffffffffU))
#define NvU40_HI24of32(n)     ((NvU32)((NvU64)(n) & 0xffffff00U))

/***************************************************************************\
|*                                                                           *|
|*  64 bit type definitions for use in interface structures.                 *|
|*                                                                           *|
\***************************************************************************/

#if defined(NV_64_BITS)

typedef void* NvP64;    /* 64 bit void pointer                     */
typedef NvU64 NvUPtr;   /* pointer sized unsigned int              */
typedef NvS64 NvSPtr;   /* pointer sized signed int                */
typedef NvU64 NvLength; /* length to agree with sizeof             */

#define NvP64_VALUE(n) (n)
#define NvP64_fmt      "%p"

#define KERNEL_POINTER_FROM_NvP64(p, v) ((p)(v))
#define NvP64_PLUS_OFFSET(p, o)         (NvP64)((NvU64)(p) + (NvU64)(o))

#else

typedef NvU64 NvP64;    /* 64 bit void pointer                     */
typedef NvU32 NvUPtr;   /* pointer sized unsigned int              */
typedef NvS32 NvSPtr;   /* pointer sized signed int                */
typedef NvU32 NvLength; /* length to agree with sizeof             */

#define NvP64_VALUE(n) ((void*)(NvUPtr)(n))
#define NvP64_fmt      "0x%llx"

#define KERNEL_POINTER_FROM_NvP64(p, v) ((p)(NvUPtr)(v))
#define NvP64_PLUS_OFFSET(p, o)         ((p) + (NvU64)(o))

#endif

/***************************************************************************\
|*                                                                           *|
|*  Limits for common types.                                                 *|
|*                                                                           *|
\***************************************************************************/

/* Explanation of the current form of these limits:
 *
 * - Decimal is used, as hex values are by default positive.
 * - Casts are not used, as usage in the preprocessor itself (#if) ends poorly.
 * - The subtraction of 1 for some MIN values is used to get around the fact
 *   that the C syntax actually treats -x as NEGATE(x) instead of a distinct
 *   number.  Since 214748648 isn't a valid positive 32-bit signed value, we
 *   take the largest valid positive signed number, negate it, and subtract 1.
 */
#define NV_S8_MIN                                       (-128)
#define NV_S8_MAX                                       (+127)
#define NV_U8_MIN                                       (0U)
#define NV_U8_MAX                                       (+255U)
#define NV_S16_MIN                                      (-32768)
#define NV_S16_MAX                                      (+32767)
#define NV_U16_MIN                                      (0U)
#define NV_U16_MAX                                      (+65535U)
#define NV_S32_MIN                                      (-2147483647 - 1)
#define NV_S32_MAX                                      (+2147483647)
#define NV_U32_MIN                                      (0U)
#define NV_U32_MAX                                      (+4294967295U)
#define NV_S64_MIN                                      (-9223372036854775807LL - 1LL)
#define NV_S64_MAX                                      (+9223372036854775807LL)
#define NV_U64_MIN                                      (0ULL)
#define NV_U64_MAX                                      (+18446744073709551615ULL)

/*!
 * Fixed-point data types.
 *
 * These are all integer types with precision indicated in the naming of the
 * form: Nv<sign>FXP<num_bits_above_radix>_<num bits below radix>.  The actual
 * size of the data type is calculated as num_bits_above_radix +
 * num_bit_below_radix.
 */
typedef NvS16 NvSFXP11_5;
typedef NvS16 NvSFXP4_12;
typedef NvS16 NvSFXP8_8;
typedef NvS32 NvSFXP8_24;
typedef NvS32 NvSFXP16_16;
typedef NvS32 NvSFXP24_8;
typedef NvS32 NvSFXP27_5;
typedef NvS32 NvSFXP29_3;
typedef NvS32 NvSFXP20_12;

typedef NvU16 NvUFXP4_12;
typedef NvU32 NvUFXP8_24;
typedef NvU32 NvUFXP16_16;
typedef NvU32 NvUFXP20_12;
typedef NvU32 NvUFXP24_8;
typedef NvU32 NvUFXP24_0;

/*!
 * Utility macros used in converting between signed integers and fixed-point
 * notation.
 *
 * - COMMON - These are used by both signed and unsigned.
 */
#define NV_TYPES_FXP_INTEGER(x, y)                      ((x) + (y) - 1) : (y)
#define NV_TYPES_FXP_FRACTIONAL(x, y)                   ((y) - 1) : 0
#define NV_TYPES_FXP_FRACTIONAL_MSB(x, y)               ((y) - 1) : ((y) - 1)
#define NV_TYPES_FXP_FRACTIONAL_MSB_ONE                 0x00000001
#define NV_TYPES_FXP_FRACTIONAL_MSB_ZERO                0x00000000
/*!
 * - UNSIGNED - These are only used for unsigned.
 */
#define NV_TYPES_UFXP_INTEGER_MAX(x, y)                 (~(NVBIT((y)) - 1U))
#define NV_TYPES_UFXP_INTEGER_MIN(x, y)                 (0U)
/*!
 * - SIGNED - These are only used for signed.
 */
#define NV_TYPES_SFXP_INTEGER_SIGN(x, y)                ((x) + (y) - 1) : ((x) + (y) - 1)
#define NV_TYPES_SFXP_INTEGER_SIGN_NEGATIVE             0x00000001
#define NV_TYPES_SFXP_INTEGER_SIGN_POSITIVE             0x00000000
#define NV_TYPES_SFXP_S32_SIGN_EXTENSION(x, y)          31 : (x)
#define NV_TYPES_SFXP_S32_SIGN_EXTENSION_POSITIVE(x, y) 0x00000000
#define NV_TYPES_SFXP_S32_SIGN_EXTENSION_NEGATIVE(x, y) (NVBIT(32 - (x)) - 1U)
#define NV_TYPES_SFXP_INTEGER_MAX(x, y)                 (NVBIT((x)) - 1U)
#define NV_TYPES_SFXP_INTEGER_MIN(x, y)                 (~(NVBIT((x)) - 1U))

/*!
 * Conversion macros used for converting between integer and fixed point
 * representations.  Both signed and unsigned variants.
 *
 * Warning:
 * Note that most of the macros below can overflow if applied on values that can
 * not fit the destination type.  It's caller responsibility to ensure that such
 * situations will not occur.
 *
 * Some conversions perform some commonly preformed tasks other than just
 * bit-shifting:
 *
 * - _SCALED:
 *   For integer -> fixed-point we add handling divisors to represent
 *   non-integer values.
 *
 * - _ROUNDED:
 *   For fixed-point -> integer we add rounding to integer values.
 *
 * Unsigned:
 */
#define NV_TYPES_U32_TO_UFXP_X_Y(x, y, integer)                                                \
    ((NvUFXP##x##_##y)(((NvU32)(integer)) << DRF_SHIFT(NV_TYPES_FXP_INTEGER((x), (y)))))

#define NV_TYPES_U32_TO_UFXP_X_Y_SCALED(x, y, integer, scale)                                  \
    ((NvUFXP##x##_##y)(                                                                        \
        (((((NvU32)(integer)) << DRF_SHIFT(NV_TYPES_FXP_INTEGER((x), (y))))) / (scale))        \
        + ((((((NvU32)(integer)) << DRF_SHIFT(NV_TYPES_FXP_INTEGER((x), (y)))) % (scale))      \
            > ((scale) >> 1))                                                                  \
               ? 1U                                                                            \
               : 0U)))

#define NV_TYPES_UFXP_X_Y_TO_U32(x, y, fxp)                                                    \
    ((NvU32)(DRF_VAL(_TYPES, _FXP, _INTEGER((x), (y)), ((NvUFXP##x##_##y)(fxp)))))

#define NV_TYPES_UFXP_X_Y_TO_U32_ROUNDED(x, y, fxp)                                            \
    (NV_TYPES_UFXP_X_Y_TO_U32(x, y, (fxp))                                                     \
     + !!DRF_VAL(_TYPES, _FXP, _FRACTIONAL_MSB((x), (y)), ((NvUFXP##x##_##y)(fxp))))

//
// 32-bit Signed FXP:
// Some compilers do not support left shift negative values
// so typecast integer to NvU32 instead of NvS32
//
#define NV_TYPES_S32_TO_SFXP_X_Y(x, y, integer)                                                \
    ((NvSFXP##x##_##y)(((NvU32)(integer)) << DRF_SHIFT(NV_TYPES_FXP_INTEGER((x), (y)))))

#define NV_TYPES_S32_TO_SFXP_X_Y_SCALED(x, y, integer, scale)                                  \
    ((NvSFXP##x##_##y)(                                                                        \
        ((((NvS32)(integer)) << DRF_SHIFT(NV_TYPES_FXP_INTEGER((x), (y)))) + ((scale) >> 1))   \
        / (scale)))

#define NV_TYPES_SFXP_X_Y_TO_S32(x, y, fxp)                                                    \
    ((NvS32)((DRF_VAL(_TYPES, _FXP, _INTEGER((x), (y)), ((NvSFXP##x##_##y)(fxp))))             \
             | ((DRF_VAL(_TYPES, _SFXP, _INTEGER_SIGN((x), (y)), (fxp))                        \
                 == NV_TYPES_SFXP_INTEGER_SIGN_NEGATIVE)                                       \
                    ? DRF_NUM(_TYPES,                                                          \
                              _SFXP,                                                           \
                              _S32_SIGN_EXTENSION((x), (y)),                                   \
                              NV_TYPES_SFXP_S32_SIGN_EXTENSION_NEGATIVE((x), (y)))             \
                    : DRF_NUM(_TYPES,                                                          \
                              _SFXP,                                                           \
                              _S32_SIGN_EXTENSION((x), (y)),                                   \
                              NV_TYPES_SFXP_S32_SIGN_EXTENSION_POSITIVE((x), (y))))))

#define NV_TYPES_SFXP_X_Y_TO_S32_ROUNDED(x, y, fxp)                                            \
    (NV_TYPES_SFXP_X_Y_TO_S32(x, y, (fxp))                                                     \
     + !!DRF_VAL(_TYPES, _FXP, _FRACTIONAL_MSB((x), (y)), ((NvSFXP##x##_##y)(fxp))))

/*
   Convert to big endian
*/
#define NvU32_byteswap(x)                                                                      \
    ((x << 24) + ((x << 8) & 0x00FF0000) + ((x >> 8) & 0x0000FF00) + (x >> 24))

/* Aligns fields in structs  so they match up between 32 and 64 bit builds */
#if defined(__GNUC__) || defined(NV_QNX)
#define NV_ALIGN_BYTES(size) __attribute__((aligned(size)))
#elif defined(__arm)
#define NV_ALIGN_BYTES(size) __align(ALIGN)
#else
// XXX This is dangerously nonportable!  We really shouldn't provide a default
// version of this that doesn't do anything.
#define NV_ALIGN_BYTES(size)
#endif

// NV_DECLARE_ALIGNED() can be used on all platforms.
// This macro form accounts for the fact that __declspec on Windows is required
// before the variable type,
// and NV_ALIGN_BYTES is required after the variable name.
#if defined(NV_WINDOWS)
#define NV_DECLARE_ALIGNED(TYPE_VAR, ALIGN) __declspec(align(ALIGN)) TYPE_VAR
#elif defined(__GNUC__) || defined(NV_QNX)
#define NV_DECLARE_ALIGNED(TYPE_VAR, ALIGN) TYPE_VAR __attribute__((aligned(ALIGN)))
#elif defined(__arm)
#define NV_DECLARE_ALIGNED(TYPE_VAR, ALIGN) __align(ALIGN) TYPE_VAR
#endif

//
// Macros representing the single-precision IEEE 754 floating point format for
// "binary32", also known as "single" and "float".
//
// http://en.wikipedia.org/wiki/Single_precision_floating-point_format
//
// _SIGN
//     Single bit representing the sign of the number.
// _EXPONENT
//     Unsigned 8-bit number representing the exponent value by which to scale
//     the mantissa.
//     _BIAS - The value by which to offset the exponent to account for sign.
// _MANTISSA
//     Explicit 23-bit significand of the value.  When exponent != 0, this is an
//     implicitly 24-bit number with a leading 1 prepended.  This 24-bit number
//     can be conceptualized as FXP 9.23.
//
// With these definitions, the value of a floating point number can be
// calculated as:
//     (-1)^(_SIGN) *
//         2^(_EXPONENT - _EXPONENT_BIAS) *
//         (1 + _MANTISSA / (1 << 23))
//
#define NV_TYPES_SINGLE_SIGN          31 : 31
#define NV_TYPES_SINGLE_SIGN_POSITIVE 0x00000000
#define NV_TYPES_SINGLE_SIGN_NEGATIVE 0x00000001
#define NV_TYPES_SINGLE_EXPONENT      30 : 23
#define NV_TYPES_SINGLE_EXPONENT_ZERO 0x00000000
#define NV_TYPES_SINGLE_EXPONENT_BIAS 0x0000007F
#define NV_TYPES_SINGLE_MANTISSA      22 : 0

//
// Helper macro to return an IEEE 754 single-precision value's exponent,
// including the bias.
//
// @param[in] single   IEEE 754 single-precision value to manipulate.
//
// @return Signed exponent value for IEEE 754 single-precision.
//
#define NV_TYPES_SINGLE_EXPONENT_BIASED(single)                                                \
    ((NvS32)(DRF_VAL(_TYPES, _SINGLE, _EXPONENT, single) - NV_TYPES_SINGLE_EXPONENT_BIAS))

// Determine signicand including implicit bit
#define NV_TYPES_SINGLE_SIGNIFICAND(single)                                                    \
    ((FLD_TEST_DRF(_TYPES, _SINGLE, _EXPONENT, _ZERO, single)                                  \
          ? 0                                                                                  \
          : 1 << (DRF_EXTENT(NV_TYPES_SINGLE_MANTISSA) + 1))                                   \
     | DRF_VAL(_TYPES, _SINGLE, _MANTISSA, single))

// Convert float to unsigned integer (ignore sign bit)
#define NV_TYPES_SINGLE_TO_UNSIGNED(single)                                                    \
    (NV_TYPES_SINGLE_SIGNIFICAND(single)                                                       \
     >> (DRF_EXTENT(NV_TYPES_SINGLE_MANTISSA) + 1 - NV_TYPES_SINGLE_EXPONENT_BIASED(single)))

#ifdef UNIT_TEST
#define NV_STATIC
#else
#define NV_STATIC static
#endif

#endif
#endif  //_NVTYPES_H__INCLUDED_
