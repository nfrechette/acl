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

#include "acl/core/compiler_utils.h"
#include "acl/core/error.h"

#include <cstdint>

#if !defined(ACL_USE_POPCOUNT)
	#if defined(ACL_NEON_INTRINSICS)
		// Enable pop-count type instructions on ARM NEON
		#define ACL_USE_POPCOUNT
	#elif defined(_DURANGO) || defined(_XBOX_ONE)
		// Enable pop-count type instructions on Xbox One
		#define ACL_USE_POPCOUNT
	// TODO: Enable this for PlayStation 4 as well, what is the define and can we use it in public code?
	#endif
#endif

#if defined(ACL_USE_POPCOUNT)
	#if defined(_MSC_VER)
		#include <nmmintrin.h>
	#endif
#endif

#if defined(ACL_AVX_INTRINSICS)
	// Use BMI
	#include <immintrin.h>
#endif

ACL_IMPL_FILE_PRAGMA_PUSH

namespace acl
{
	inline uint8_t count_set_bits(uint8_t value)
	{
#if defined(ACL_USE_POPCOUNT) && defined(_MSC_VER)
		return (uint8_t)_mm_popcnt_u32(value);
#elif defined(ACL_USE_POPCOUNT) && (defined(__GNUC__) || defined(__clang__))
		return (uint8_t)__builtin_popcount(value);
#else
		value = value - ((value >> 1) & 0x55);
		value = (value & 0x33) + ((value >> 2) & 0x33);
		return ((value + (value >> 4)) & 0x0F);
#endif
	}

	inline uint16_t count_set_bits(uint16_t value)
	{
#if defined(ACL_USE_POPCOUNT) && defined(_MSC_VER)
		return (uint16_t)_mm_popcnt_u32(value);
#elif defined(ACL_USE_POPCOUNT) && (defined(__GNUC__) || defined(__clang__))
		return (uint16_t)__builtin_popcount(value);
#else
		value = value - ((value >> 1) & 0x5555);
		value = (value & 0x3333) + ((value >> 2) & 0x3333);
		return uint16_t(((value + (value >> 4)) & 0x0F0F) * 0x0101) >> 8;
#endif
	}

	inline uint32_t count_set_bits(uint32_t value)
	{
#if defined(ACL_USE_POPCOUNT) && defined(_MSC_VER)
		return _mm_popcnt_u32(value);
#elif defined(ACL_USE_POPCOUNT) && (defined(__GNUC__) || defined(__clang__))
		return __builtin_popcount(value);
#else
		value = value - ((value >> 1) & 0x55555555);
		value = (value & 0x33333333) + ((value >> 2) & 0x33333333);
		return (((value + (value >> 4)) & 0x0F0F0F0F) * 0x01010101) >> 24;
#endif
	}

	inline uint64_t count_set_bits(uint64_t value)
	{
#if defined(ACL_USE_POPCOUNT) && defined(_MSC_VER)
		return _mm_popcnt_u64(value);
#elif defined(ACL_USE_POPCOUNT) && (defined(__GNUC__) || defined(__clang__))
		return __builtin_popcountll(value);
#else
		value = value - ((value >> 1) & 0x5555555555555555ull);
		value = (value & 0x3333333333333333ull) + ((value >> 2) & 0x3333333333333333ull);
		return (((value + (value >> 4)) & 0x0F0F0F0F0F0F0F0Full) * 0x0101010101010101ull) >> 56;
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
#if defined(ACL_AVX_INTRINSICS)
		// Use BMI
		return _andn_u32(not_value, and_value);
#else
		return ~not_value & and_value;
#endif
	}
}

ACL_IMPL_FILE_PRAGMA_POP
