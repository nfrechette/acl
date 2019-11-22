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
#include "acl/core/compiler_utils.h"

#include <rtm/scalarf.h>

#include <cstdint>
#include <algorithm>

ACL_IMPL_FILE_PRAGMA_PUSH

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
	// The returned sample indices are clamped and do not loop.
	// If the sample rate is available, prefer using find_linear_interpolation_samples_with_sample_rate
	// instead. It is faster and more accurate.
	inline void find_linear_interpolation_samples_with_duration(uint32_t num_samples, float duration, float sample_time, SampleRoundingPolicy rounding_policy,
		uint32_t& out_sample_index0, uint32_t& out_sample_index1, float& out_interpolation_alpha)
	{
		// Samples are evenly spaced, trivially calculate the indices that we need
		ACL_ASSERT(duration >= 0.0F, "Invalid duration: %f", duration);
		ACL_ASSERT(sample_time >= 0.0F && sample_time <= duration, "Invalid sample time: 0.0 <= %f <= %f", sample_time, duration);
		ACL_ASSERT(num_samples > 0, "Invalid num_samples: %u", num_samples);

		const float sample_rate = duration == 0.0F ? 0.0F : (float(num_samples - 1) / duration);
		ACL_ASSERT(sample_rate >= 0.0F && rtm::scalar_is_finite(sample_rate), "Invalid sample_rate: %f", sample_rate);

		const float sample_index = sample_time * sample_rate;
		const uint32_t sample_index0 = static_cast<uint32_t>(sample_index);
		const uint32_t sample_index1 = std::min(sample_index0 + 1, num_samples - 1);
		ACL_ASSERT(sample_index0 <= sample_index1 && sample_index1 < num_samples, "Invalid sample indices: 0 <= %u <= %u < %u", sample_index0, sample_index1, num_samples);

		const float interpolation_alpha = sample_index - float(sample_index0);
		ACL_ASSERT(interpolation_alpha >= 0.0F && interpolation_alpha <= 1.0F, "Invalid interpolation alpha: 0.0 <= %f <= 1.0", interpolation_alpha);

		out_sample_index0 = sample_index0;
		out_sample_index1 = sample_index1;

		switch (rounding_policy)
		{
		default:
		case SampleRoundingPolicy::None:
			out_interpolation_alpha = interpolation_alpha;
			break;
		case SampleRoundingPolicy::Floor:
			out_interpolation_alpha = 0.0F;
			break;
		case SampleRoundingPolicy::Ceil:
			out_interpolation_alpha = 1.0F;
			break;
		case SampleRoundingPolicy::Nearest:
			out_interpolation_alpha = rtm::scalar_floor(interpolation_alpha + 0.5F);
			break;
		}
	}

	ACL_DEPRECATED("Use find_linear_interpolation_samples_with_duration instead, to be removed in v2.0")
	inline void find_linear_interpolation_samples(uint32_t num_samples, float duration, float sample_time, SampleRoundingPolicy rounding_policy,
		uint32_t& out_sample_index0, uint32_t& out_sample_index1, float& out_interpolation_alpha)
	{
		find_linear_interpolation_samples_with_duration(num_samples, duration, sample_time, rounding_policy, out_sample_index0, out_sample_index1, out_interpolation_alpha);
	}

	//////////////////////////////////////////////////////////////////////////
	// Calculates the sample indices and the interpolation required to linearly
	// interpolate when the samples are uniform.
	// The returned sample indices are clamped and do not loop.
	inline void find_linear_interpolation_samples_with_sample_rate(uint32_t num_samples, float sample_rate, float sample_time, SampleRoundingPolicy rounding_policy,
		uint32_t& out_sample_index0, uint32_t& out_sample_index1, float& out_interpolation_alpha)
	{
		// Samples are evenly spaced, trivially calculate the indices that we need
		ACL_ASSERT(sample_rate >= 0.0F, "Invalid sample rate: %f", sample_rate);
		ACL_ASSERT(num_samples > 0, "Invalid num_samples: %u", num_samples);

		// TODO: Would it be faster to do the index calculation entirely with floating point?
		// SSE4 can floor with a single instruction.
		// We don't need the index1, there are no dependencies there, we can still convert the float index0 into an integer, do the min.
		// This would break the dependency chains, right now the index0 depends on sample_index and interpolation_alpha depends on index0.
		// Generating index0 is slow, and converting it back to float is slow.
		// If we keep index0 as a float and floor it as a float, we can calculate index1 at the same time as the interpolation alpha.
		const float sample_index = sample_time * sample_rate;
		const uint32_t sample_index0 = static_cast<uint32_t>(sample_index);
		const uint32_t sample_index1 = std::min(sample_index0 + 1, num_samples - 1);
		ACL_ASSERT(sample_index0 <= sample_index1 && sample_index1 < num_samples, "Invalid sample indices: 0 <= %u <= %u < %u", sample_index0, sample_index1, num_samples);

		const float interpolation_alpha = sample_index - float(sample_index0);
		ACL_ASSERT(interpolation_alpha >= 0.0F && interpolation_alpha <= 1.0F, "Invalid interpolation alpha: 0.0 <= %f <= 1.0", interpolation_alpha);

		out_sample_index0 = sample_index0;
		out_sample_index1 = sample_index1;

		switch (rounding_policy)
		{
		default:
		case SampleRoundingPolicy::None:
			out_interpolation_alpha = interpolation_alpha;
			break;
		case SampleRoundingPolicy::Floor:
			out_interpolation_alpha = 0.0F;
			break;
		case SampleRoundingPolicy::Ceil:
			out_interpolation_alpha = 1.0F;
			break;
		case SampleRoundingPolicy::Nearest:
			out_interpolation_alpha = rtm::scalar_floor(interpolation_alpha + 0.5F);
			break;
		}
	}
}

ACL_IMPL_FILE_PRAGMA_POP
