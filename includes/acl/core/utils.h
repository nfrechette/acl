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

#include "acl/core/error.h"
#include "acl/core/memory_utils.h"
#include "acl/math/scalar_32.h"

#include <cstdint>

namespace acl
{
	inline uint32_t calculate_num_samples(float duration, uint32_t sample_rate)
	{
		ACL_ASSERT(duration >= 0.0f, "Invalid duration: %f", duration);
		ACL_ASSERT(sample_rate > 0, "Invalid sample rate: %u", sample_rate);
		return safe_static_cast<uint32_t>(floor((duration * float(sample_rate)) + 0.5f)) + 1;
	}

	inline float calculate_duration(uint32_t num_samples, uint32_t sample_rate)
	{
		ACL_ASSERT(sample_rate > 0, "Invalid sample rate: %u", sample_rate);
		return num_samples != 0 ? (float(num_samples - 1) / float(sample_rate)) : 0.0f;
	}
}
