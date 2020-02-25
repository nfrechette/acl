#pragma once

////////////////////////////////////////////////////////////////////////////////
// The MIT License (MIT)
//
// Copyright (c) 2018 Nicholas Frechette & Animation Compression Library contributors
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
////////////////////////////////////////////////////////////////////////////////

#include "acl/core/impl/compiler_utils.h"
#include "acl/core/error.h"

#include <rtm/math.h>

#include <cstdint>

#if !defined(ACL_USE_POPCOUNT) && !defined(RTM_NO_INTRINSICS)
	// TODO: Enable this for PlayStation 4 as well, what is the define and can we use it in public code?
	#if defined(_DURANGO) || defined(_XBOX_ONE)
		// Enable pop-count type instructions on Xbox One
		#define ACL_USE_POPCOUNT
	#endif
#endif

#if defined(ACL_USE_POPCOUNT)
	#include <nmmintrin.h>
#endif

// Note: It seems that the Clang toolchain with MSVC enables BMI only with AVX2 unlike
// MSVC which enables it with AVX
#if defined(RTM_AVX_INTRINSICS) && !(defined(_MSC_VER) && defined(__clang__))
	// Use BMI
	#include <ammintrin.h>		// MSVC uses this header for _andn_u32 BMI intrinsic
	#include <immintrin.h>		// Intel documentation says _andn_u32 and others are here

	#define ACL_BMI_INTRINSICS
#endif

ACL_IMPL_FILE_PRAGMA_PUSH

namespace acl
{
	inline uint8_t count_set_bits(uint8_t value)
	{
#if defined(ACL_USE_POPCOUNT)
		return (uint8_t)_mm_popcnt_u32(value);
#elif defined(RTM_NEON_INTRINSICS)
		return (uint8_t)vget_lane_u64(vcnt_u8(vcreate_u8(value)), 0);
#else
		value = value - ((value >> 1) & 0x55);
		value = (value & 0x33) + ((value >> 2) & 0x33);
		return ((value + (value >> 4)) & 0x0F);
#endif
	}

	inline uint16_t count_set_bits(uint16_t value)
	{
#if defined(ACL_USE_POPCOUNT)
		return (uint16_t)_mm_popcnt_u32(value);
#elif defined(RTM_NEON_INTRINSICS)
		return (uint16_t)vget_lane_u64(vpaddl_u8(vcnt_u8(vcreate_u8(value))), 0);
#else
		value = value - ((value >> 1) & 0x5555);
		value = (value & 0x3333) + ((value >> 2) & 0x3333);
		return uint16_t(((value + (value >> 4)) & 0x0F0F) * 0x0101) >> 8;
#endif
	}

	inline uint32_t count_set_bits(uint32_t value)
	{
#if defined(ACL_USE_POPCOUNT)
		return _mm_popcnt_u32(value);
#elif defined(RTM_NEON_INTRINSICS)
		return (uint32_t)vget_lane_u64(vpaddl_u16(vpaddl_u8(vcnt_u8(vcreate_u8(value)))), 0);
#else
		value = value - ((value >> 1) & 0x55555555);
		value = (value & 0x33333333) + ((value >> 2) & 0x33333333);
		return (((value + (value >> 4)) & 0x0F0F0F0F) * 0x01010101) >> 24;
#endif
	}

	inline uint64_t count_set_bits(uint64_t value)
	{
#if defined(ACL_USE_POPCOUNT)
		return _mm_popcnt_u64(value);
#elif defined(RTM_NEON_INTRINSICS)
		return vget_lane_u64(vpaddl_u32(vpaddl_u16(vpaddl_u8(vcnt_u8(vcreate_u8(value))))), 0);
#else
		value = value - ((value >> 1) & 0x5555555555555555ULL);
		value = (value & 0x3333333333333333ULL) + ((value >> 2) & 0x3333333333333333ULL);
		return (((value + (value >> 4)) & 0x0F0F0F0F0F0F0F0FULL) * 0x0101010101010101ULL) >> 56;
#endif
	}

	inline uint32_t rotate_bits_left(uint32_t value, int32_t num_bits)
	{
		ACL_ASSERT(num_bits >= 0, "Attempting to rotate by negative bits");
		ACL_ASSERT(num_bits < 32, "Attempting to rotate by too many bits");
		const uint32_t mask = 32 - 1;
		num_bits &= mask;
		return (value << num_bits) | (value >> ((-num_bits) & mask));
	}

	inline uint32_t and_not(uint32_t not_value, uint32_t and_value)
	{
#if defined(ACL_BMI_INTRINSICS)
		// Use BMI
#if defined(__GNUC__) && !defined(__clang__) && !defined(_andn_u32)
		return __andn_u32(not_value, and_value);	// GCC doesn't define the right intrinsic symbol
#else
		return _andn_u32(not_value, and_value);
#endif
#else
		return ~not_value & and_value;
#endif
	}
}

ACL_IMPL_FILE_PRAGMA_POP
