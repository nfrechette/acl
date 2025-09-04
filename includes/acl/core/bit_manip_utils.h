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

#include "acl/config.h"
#include "acl/version.h"
#include "acl/core/impl/compiler_utils.h"
#include "acl/core/error.h"

#include <rtm/math.h>

#include <cstdint>

// See config.h for details on how to configure the hardware bitscan features for your project

#if defined(ACL_USE_POPCOUNT)
	// The popcount intrinsic is enabled
	#include <nmmintrin.h>
#endif

#if defined(ACL_BMI_INTRINSICS)
	// BMI intrinsics are enabled
	#include <ammintrin.h>		// MSVC uses this header for _andn_u32 BMI intrinsic
	#include <immintrin.h>		// Intel documentation says _andn_u32 and others are here
#endif

#if defined(RTM_COMPILER_MSVC)
	#include <intrin.h>
#endif

ACL_IMPL_FILE_PRAGMA_PUSH

#if defined(RTM_COMPILER_MSVC)
	#pragma warning(push)
	// warning C4146: unary minus operator applied to unsigned type, result still unsigned
	// This is fine because we use bitwise arithmetic and rely on the fact that unsigned
	// integers use twos complement.
	#pragma warning(disable : 4146)
#endif

namespace acl
{
	ACL_IMPL_VERSION_NAMESPACE_BEGIN

	//////////////////////////////////////////////////////////////////////////
	// Counts the number of '1' bits (aka: pop-count)
	inline uint64_t count_set_bits(uint64_t value)
	{
#if defined(ACL_USE_POPCOUNT)
	#if defined(RTM_ARCH_X86)
		uint32_t count = _mm_popcnt_u32(static_cast<uint32_t>(value >> 32));
		return count + _mm_popcnt_u32(static_cast<uint32_t>(value));
	#else
		return _mm_popcnt_u64(value);
	#endif
#elif defined(RTM_NEON_INTRINSICS)
		return vget_lane_u64(vpaddl_u32(vpaddl_u16(vpaddl_u8(vcnt_u8(vcreate_u8(value))))), 0);
#else
		value = value - ((value >> 1) & 0x5555555555555555ULL);
		value = (value & 0x3333333333333333ULL) + ((value >> 2) & 0x3333333333333333ULL);
		return (((value + (value >> 4)) & 0x0F0F0F0F0F0F0F0FULL) * 0x0101010101010101ULL) >> 56;
#endif
	}

	//////////////////////////////////////////////////////////////////////////
	// Counts the number of '1' bits (aka: pop-count)
	inline uint32_t count_set_bits(uint32_t value)
	{
#if defined(ACL_USE_POPCOUNT)
		return _mm_popcnt_u32(value);
#elif defined(RTM_NEON_INTRINSICS)
		return static_cast<uint32_t>(vget_lane_u32(vpaddl_u16(vpaddl_u8(vcnt_u8(vcreate_u8(value)))), 0));
#else
		value = value - ((value >> 1) & 0x55555555U);
		value = (value & 0x33333333U) + ((value >> 2) & 0x33333333U);
		return (((value + (value >> 4)) & 0x0F0F0F0FU) * 0x01010101U) >> 24;
#endif
	}

	//////////////////////////////////////////////////////////////////////////
	// Counts the number of '1' bits (aka: pop-count)
	inline uint16_t count_set_bits(uint16_t value)
	{
#if defined(ACL_USE_POPCOUNT)
		return static_cast<uint16_t>(_mm_popcnt_u32(value));
#elif defined(RTM_NEON_INTRINSICS)
		return static_cast<uint16_t>(vget_lane_u16(vpaddl_u8(vcnt_u8(vcreate_u8(value))), 0));
#else
		return static_cast<uint16_t>(count_set_bits(static_cast<uint32_t>(value)));
#endif
	}

	//////////////////////////////////////////////////////////////////////////
	// Counts the number of '1' bits (aka: pop-count)
	inline uint8_t count_set_bits(uint8_t value)
	{
#if defined(ACL_USE_POPCOUNT)
		return static_cast<uint8_t>(_mm_popcnt_u32(value));
#elif defined(RTM_NEON_INTRINSICS)
		return static_cast<uint8_t>(vget_lane_u8(vcnt_u8(vcreate_u8(value)), 0));
#else
		return static_cast<uint8_t>(count_set_bits(static_cast<uint32_t>(value)));
#endif
	}

	//////////////////////////////////////////////////////////////////////////
	// Starting at the MSB, counts the number of leading zeros
	inline uint32_t count_leading_zeros(uint32_t value)
	{
#if defined(ACL_USE_POPCOUNT)
		return _lzcnt_u32(value);
#elif defined(RTM_COMPILER_MSVC)
		unsigned long first_set_bit_index;
		return _BitScanReverse(&first_set_bit_index, value) ? (31 - first_set_bit_index) : 32;
#elif defined(RTM_COMPILER_GCC) || defined(RTM_COMPILER_CLANG)
		return value != 0 ? __builtin_clz(value) : 32;
#else
		value = value | (value >> 1);
		value = value | (value >> 2);
		value = value | (value >> 4);
		value = value | (value >> 8);
		value = value | (value >> 16);
		return count_set_bits(~value);
#endif
	}

	//////////////////////////////////////////////////////////////////////////
	// Starting at the MSB, counts the number of leading zeros
	inline uint64_t count_leading_zeros(uint64_t value)
	{
#if defined(ACL_USE_POPCOUNT)
	#if defined(RTM_ARCH_X86)
		uint32_t count = _lzcnt_u32(static_cast<uint32_t>(value >> 32));
		if (count != 32)
			return count;

		return 32 + _lzcnt_u32(static_cast<uint32_t>(value));
	#else
		return _lzcnt_u64(value);
	#endif
#elif defined(RTM_COMPILER_MSVC)
	#if defined(RTM_ARCH_X86)
		unsigned long first_set_bit_index;
		if (_BitScanReverse(&first_set_bit_index, static_cast<uint32_t>(value >> 32)))
			return 31 - first_set_bit_index;

		if (_BitScanReverse(&first_set_bit_index, static_cast<uint32_t>(value)))
			return 63 - first_set_bit_index;

		return 64;
	#else
		unsigned long first_set_bit_index;
		return _BitScanReverse64(&first_set_bit_index, value) ? (63 - first_set_bit_index) : 64;
	#endif
#elif defined(RTM_COMPILER_GCC) || defined(RTM_COMPILER_CLANG)
		return value != 0 ? __builtin_clzll(value) : 64;
#else
		value = value | (value >> 1);
		value = value | (value >> 2);
		value = value | (value >> 4);
		value = value | (value >> 8);
		value = value | (value >> 16);
		value = value | (value >> 32);
		return count_set_bits(~value);
#endif
	}

	//////////////////////////////////////////////////////////////////////////
	// Starting at the LSB, counts the number of trailing zeros
	inline uint32_t count_trailing_zeros(uint32_t value)
	{
#if defined(ACL_BMI_INTRINSICS)
		return _tzcnt_u32(value);
#elif defined(RTM_COMPILER_MSVC)
		unsigned long first_set_bit_index;
		return _BitScanForward(&first_set_bit_index, value) ? first_set_bit_index : 32;
#elif defined(RTM_COMPILER_GCC) || defined(RTM_COMPILER_CLANG)
		return value != 0 ? __builtin_ctz(value) : 32;
#else
		return value != 0 ? (31 - count_leading_zeros(value & -value)) : 32;
#endif
	}

	//////////////////////////////////////////////////////////////////////////
	// Starting at the LSB, counts the number of trailing zeros
	inline uint64_t count_trailing_zeros(uint64_t value)
	{
#if defined(ACL_BMI_INTRINSICS)
	#if defined(RTM_ARCH_X86)
		uint32_t count = _tzcnt_u32(static_cast<uint32_t>(value));
		if (count != 32)
			return count;

		return 32 + _tzcnt_u32(static_cast<uint32_t>(value >> 32));
	#else
		return _tzcnt_u64(value);
	#endif
#elif defined(RTM_COMPILER_MSVC)
	#if defined(RTM_ARCH_X86)
		unsigned long first_set_bit_index;
		if (_BitScanForward(&first_set_bit_index, static_cast<uint32_t>(value)))
			return first_set_bit_index;

		if (_BitScanForward(&first_set_bit_index, static_cast<uint32_t>(value >> 32)))
			return 32 + first_set_bit_index;

		return 64;
	#else
		unsigned long first_set_bit_index;
		return _BitScanForward64(&first_set_bit_index, value) ? first_set_bit_index : 64;
	#endif
#elif defined(RTM_COMPILER_GCC) || defined(RTM_COMPILER_CLANG)
		return value != 0 ? __builtin_ctzll(value) : 64;
#else
		return value != 0 ? (63 - count_leading_zeros(value & -value)) : 64;
#endif
	}

	//////////////////////////////////////////////////////////////////////////
	// Rotate the bits left by some amount
	inline uint32_t rotate_bits_left(uint32_t value, int32_t num_bits)
	{
		ACL_ASSERT(num_bits >= 0, "Attempting to rotate by negative bits");
		ACL_ASSERT(num_bits < 32, "Attempting to rotate by too many bits");
		const uint32_t mask = 32 - 1;
		num_bits &= mask;
		return (value << num_bits) | (value >> ((-num_bits) & mask));
	}

	//////////////////////////////////////////////////////////////////////////
	// Rotate the bits right by some amount
	inline uint32_t rotate_bits_right(uint32_t value, int32_t num_bits)
	{
		ACL_ASSERT(num_bits >= 0, "Attempting to rotate by negative bits");
		ACL_ASSERT(num_bits < 32, "Attempting to rotate by too many bits");
		const uint32_t mask = 32 - 1;
		num_bits &= mask;
		return (value >> num_bits) | (value << ((-num_bits) & mask));
	}

	//////////////////////////////////////////////////////////////////////////
	// Perform: ~not_value & and_value
	inline uint32_t and_not(uint32_t not_value, uint32_t and_value)
	{
#if defined(ACL_BMI_INTRINSICS)
		// Use BMI
	#if defined(RTM_COMPILER_GCC) && !defined(_andn_u32)
		return __andn_u32(not_value, and_value);	// GCC doesn't define the right intrinsic symbol
	#else
		return _andn_u32(not_value, and_value);
	#endif
#else
		return ~not_value & and_value;
#endif
	}

	ACL_IMPL_VERSION_NAMESPACE_END
}

#if defined(RTM_COMPILER_MSVC)
	#pragma warning(pop)
#endif

ACL_IMPL_FILE_PRAGMA_POP
