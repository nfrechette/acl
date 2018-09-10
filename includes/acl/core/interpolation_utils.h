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

#include "acl/core/error.h"
#include "acl/math/scalar_32.h"

#include <cstdint>
#include <algorithm>

namespace acl
{
	//////////////////////////////////////////////////////////////////////////
	// This enum dictates how interpolation samples are calculated based on the sample time.
	enum class SampleRoundingPolicy
	{
		//////////////////////////////////////////////////////////////////////////
		// If the sample time lies between two samples, both sample indices
		// are returned and the interpolation alpha lies in between.
		None,

		//////////////////////////////////////////////////////////////////////////
		// If the sample time lies between two samples, both sample indices
		// are returned and the interpolation will be 0.0.
		Floor,

		//////////////////////////////////////////////////////////////////////////
		// If the sample time lies between two samples, both sample indices
		// are returned and the interpolation will be 1.0.
		Ceil,

		//////////////////////////////////////////////////////////////////////////
		// If the sample time lies between two samples, both sample indices
		// are returned and the interpolation will be 0.0 or 1.0 depending
		// on which sample is nearest.
		Nearest,
	};

	//////////////////////////////////////////////////////////////////////////
	// Calculates the sample indices and the interpolation required to linearly
	// interpolate when the samples are uniform.
	inline void find_linear_interpolation_samples(uint32_t num_samples, float duration, float sample_time, SampleRoundingPolicy rounding_policy,
		uint32_t& out_sample_index0, uint32_t& out_sample_index1, float& out_interpolation_alpha)
	{
		// Samples are evenly spaced, trivially calculate the indices that we need
		ACL_ASSERT(duration >= 0.0f, "Invalid duration: %f", duration);
		ACL_ASSERT(sample_time >= 0.0f && sample_time <= duration, "Invalid sample time: 0.0 <= %f <= %f", sample_time, duration);

		const float sample_rate = duration == 0.0f ? 0.0f : floor((float(num_samples - 1) / duration) + 0.5f);
		const float sample_index = sample_time * sample_rate;
		const uint32_t sample_index0 = uint32_t(floor(sample_index));
		const uint32_t sample_index1 = std::min(sample_index0 + 1, num_samples - 1);
		const float interpolation_alpha = sample_index - float(sample_index0);
		ACL_ASSERT(sample_index0 <= sample_index1 && sample_index1 < num_samples, "Invalid sample indices: 0 <= %u <= %u < %u", sample_index0, sample_index1, num_samples);
		ACL_ASSERT(interpolation_alpha >= 0.0f && interpolation_alpha <= 1.0f, "Invalid interpolation alpha: 0.0 <= %f <= 1.0", interpolation_alpha);

		out_sample_index0 = sample_index0;
		out_sample_index1 = sample_index1;

		switch (rounding_policy)
		{
		default:
		case SampleRoundingPolicy::None:
			out_interpolation_alpha = interpolation_alpha;
			break;
		case SampleRoundingPolicy::Floor:
			out_interpolation_alpha = 0.0f;
			break;
		case SampleRoundingPolicy::Ceil:
			out_interpolation_alpha = 1.0f;
			break;
		case SampleRoundingPolicy::Nearest:
			out_interpolation_alpha = floor(interpolation_alpha + 0.5f);
			break;
		}
	}
}
