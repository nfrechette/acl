#pragma once

////////////////////////////////////////////////////////////////////////////////
// The MIT License (MIT)
//
// Copyright (c) 2017 Nicholas Frechette & Animation Compression Library contributors
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
#include "acl/math/scalar_32.h"

#include <cstdint>

ACL_IMPL_FILE_PRAGMA_PUSH

namespace acl
{

#ifdef ACL_PACKING

	inline uint32_t pack_scalar_signed_normalized(float input, uint8_t num_bits)
	{
		ACL_ASSERT(num_bits > 0, "Attempting to unpack from too few bits");
		ACL_ASSERT(num_bits < 25, "Attempting to unpack from too many bits");
		ACL_ASSERT(input >= -0.5F && input <= 0.5F, "Expected normalized signed input value: %f", input);
		const float mid_value = safe_to_float(1 << (num_bits - 1));
		const float max_value = 2.0F*mid_value - 1.0F;
		return static_cast<uint32_t>(floor(input * max_value) + mid_value);
	}

	inline uint32_t pack_scalar_unsigned(float input, uint8_t num_bits)
	{
		return pack_scalar_signed_normalized(input - 0.5F, num_bits);
	}

#else

	inline uint32_t pack_scalar_unsigned(float input, uint8_t num_bits)
	{
		ACL_ASSERT(num_bits < 31, "Attempting to pack on too many bits");
		ACL_ASSERT(input >= 0.0F && input <= 1.0F, "Expected normalized unsigned input value: %f", input);
		const uint32_t max_value = (1 << num_bits) - 1;
		return static_cast<uint32_t>(symmetric_round(input * safe_to_float(max_value)));
	}

#endif

	inline float unpack_scalar_unsigned(uint32_t input, uint8_t num_bits)
	{
		ACL_ASSERT(num_bits < 31, "Attempting to unpack from too many bits");
		const uint32_t max_value = (1 << num_bits) - 1;
		ACL_ASSERT(input <= max_value, "Input value too large: %ull", input);

#ifdef ACL_BIT_RATE

		if (num_bits == 24)
		{
			// Simple precision boost for the highest bitrate.  1-1 mapping between fixed and float, omitting the middle float.
			// Replicated in vector4_packing.
			return safe_to_float(input + ((input < (1 << 23))? 0: 1)) / (1 << 24);
		}

#endif

		// For performance reasons, unpacking is faster when multiplying with the reciprocal
		const float inv_max_value = 1.0F / safe_to_float(max_value);

#ifdef ACL_PACKING

		// To date, we haven't needed to replicate maximum bounds in vector4_packing.
		return min(safe_to_float(input) * inv_max_value, 1.0F);

#else

		return safe_to_float(input) * inv_max_value;

#endif

	}

#ifdef ACL_PACKING

	inline uint32_t pack_scalar_signed(float input, uint8_t num_bits)
	{
		return pack_scalar_signed_normalized(input * 0.5F, num_bits);
	}

#else

	inline uint32_t pack_scalar_signed(float input, uint8_t num_bits)
	{
		return pack_scalar_unsigned((input * 0.5F) + 0.5F, num_bits);
	}

#endif

	inline float unpack_scalar_signed(uint32_t input, uint8_t num_bits)
	{
		return (unpack_scalar_unsigned(input, num_bits) * 2.0F) - 1.0F;
	}
}

ACL_IMPL_FILE_PRAGMA_POP
