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

#include "acl/core/hash.h"
#include "acl/core/track_types.h"
#include "acl/core/range_reduction_types.h"

#include <stdint.h>

namespace acl
{
	struct SegmentingSettings
	{
		bool enabled;

		uint16_t ideal_num_samples;
		uint16_t max_num_samples;

		RangeReductionFlags8 range_reduction;

		SegmentingSettings()
			: enabled(false)
			, ideal_num_samples(16)
			, max_num_samples(31)
			, range_reduction(RangeReductionFlags8::None)
		{}

		uint32_t hash() const
		{
			return hash_combine(hash_combine(hash_combine(hash32(enabled), hash32(ideal_num_samples)), hash32(max_num_samples)), hash32(range_reduction));
		}
	};

	struct CompressionSettings
	{
		RotationFormat8 rotation_format;
		VectorFormat8 translation_format;
		VectorFormat8 scale_format;

		RangeReductionFlags8 range_reduction;

		SegmentingSettings segmenting;

		CompressionSettings()
			: rotation_format(RotationFormat8::Quat_128)
			, translation_format(VectorFormat8::Vector3_96)
			, scale_format(VectorFormat8::Vector3_96)
			, range_reduction(RangeReductionFlags8::None)
			, segmenting()
		{}

		uint32_t hash() const
		{
			uint32_t hash_value = hash_combine(hash_combine(hash_combine(hash32(rotation_format), hash32(translation_format)), hash32(scale_format)), hash32(range_reduction));
			if (segmenting.enabled)
				hash_value = hash_combine(hash_value, segmenting.hash());
			return hash_value;
		}
	};
}
