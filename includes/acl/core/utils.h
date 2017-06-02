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

#include "acl/core/assert.h"
#include "acl/math/scalar_32.h"
#include "acl/math/scalar_64.h"

#include <stdint.h>
#include <algorithm>

namespace acl
{
	template<typename FloatType>
	inline void calculate_interpolation_keys(uint32_t num_samples, FloatType clip_duration, FloatType sample_time, uint32_t& out_key_frame0, uint32_t& out_key_frame1, FloatType& out_interpolation_alpha)
	{
		// Samples are evenly spaced, trivially calculate the indices that we need
		FloatType normalized_sample_time = (sample_time / clip_duration);
		ensure(sample_time >= 0.0f && sample_time <= clip_duration);
		ensure(normalized_sample_time >= FloatType(0.0) && normalized_sample_time <= FloatType(1.0));

		FloatType sample_key = normalized_sample_time * FloatType(num_samples - 1);
		uint32_t key_frame0 = uint32_t(floor(sample_key));
		uint32_t key_frame1 = std::min(key_frame0 + 1, num_samples - 1);
		FloatType interpolation_alpha = sample_key - FloatType(key_frame0);
		ensure(key_frame0 >= 0 && key_frame0 <= key_frame1 && key_frame1 < num_samples);
		ensure(interpolation_alpha >= FloatType(0.0) && interpolation_alpha <= FloatType(1.0));

		out_key_frame0 = key_frame0;
		out_key_frame1 = key_frame1;
		out_interpolation_alpha = interpolation_alpha;
	}
}
