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

#include "acl/core/impl/compiler_utils.h"
#include "acl/core/error.h"
#include "acl/core/memory_utils.h"

#include <rtm/scalarf.h>

#include <cstdint>
#include <limits>

ACL_IMPL_FILE_PRAGMA_PUSH

namespace acl
{
	//////////////////////////////////////////////////////////////////////////
	// Calculate the number of samples present from a duration and sample rate.
	// Conceptually, a clip with 1 sample at any sample rate has a single static
	// pose and as such no definite duration. A clip with 2 samples at 30 FPS
	// has a sample at time 0.0 and another at time 1/30s for a duration of 1/30s.
	// We consider a 0.0 duration as having no samples, an infinite duration as
	// having 1 sample, and otherwise having at least 1 sample.
	//////////////////////////////////////////////////////////////////////////
	inline uint32_t calculate_num_samples(float duration, float sample_rate)
	{
		ACL_ASSERT(duration >= 0.0F, "Invalid duration: %f", duration);
		ACL_ASSERT(sample_rate > 0.0F, "Invalid sample rate: %f", sample_rate);
		if (duration == 0.0F)
			return 0;	// No duration whatsoever, we have no samples

		if (duration == std::numeric_limits<float>::infinity())
			return 1;	// An infinite duration, we have a single sample (static pose)

		// Otherwise we have at least 1 sample
		return safe_static_cast<uint32_t>(rtm::scalar_floor((duration * sample_rate) + 0.5F)) + 1;
	}

	//////////////////////////////////////////////////////////////////////////
	// Calculate a clip duration from its number of samples and sample rate.
	// Conceptually, a clip with 1 sample at any sample rate has a single static
	// pose and as such no definite duration. A clip with 2 samples at 30 FPS
	// has a sample at time 0.0 and another at time 1/30s for a duration of 1/30s.
	// We consider a 0.0 duration as having no samples, an infinite duration as
	// having 1 sample, and otherwise having at least 1 sample.
	//////////////////////////////////////////////////////////////////////////
	inline float calculate_duration(uint32_t num_samples, float sample_rate)
	{
		ACL_ASSERT(sample_rate > 0.0F, "Invalid sample rate: %f", sample_rate);
		if (num_samples == 0)
			return 0.0F;	// No samples means we have no duration

		if (num_samples == 1)
			return std::numeric_limits<float>::infinity();	// A single sample means we have an indefinite duration (static pose)

		// Otherwise we have some duration
		return float(num_samples - 1) / sample_rate;
	}
}

ACL_IMPL_FILE_PRAGMA_POP
