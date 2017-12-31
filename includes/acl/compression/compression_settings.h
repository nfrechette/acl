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
#include "acl/compression/skeleton_error_metric.h"

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

		uint32_t get_hash() const
		{
			uint32_t hash_value = 0;
			hash_value = hash_combine(hash_value, hash32(enabled));
			hash_value = hash_combine(hash_value, hash32(ideal_num_samples));
			hash_value = hash_combine(hash_value, hash32(max_num_samples));
			hash_value = hash_combine(hash_value, hash32(range_reduction));
			return hash_value;
		}

		const char* get_error() const
		{
			if (!enabled)
				return nullptr;

			if (ideal_num_samples == 0)
				return "ideal_num_samples cannot be 0";

			if (max_num_samples == 0)
				return "max_num_samples cannot be 0";

			if (ideal_num_samples > max_num_samples)
				return "ideal_num_samples must be smaller or equal to max_num_samples";

			return nullptr;
		}
	};

	struct CompressionSettings
	{
		RotationFormat8 rotation_format;
		VectorFormat8 translation_format;
		VectorFormat8 scale_format;

		RangeReductionFlags8 range_reduction;

		SegmentingSettings segmenting;

		ISkeletalErrorMetric* error_metric;

		CompressionSettings()
			: rotation_format(RotationFormat8::Quat_128)
			, translation_format(VectorFormat8::Vector3_96)
			, scale_format(VectorFormat8::Vector3_96)
			, range_reduction(RangeReductionFlags8::None)
			, segmenting()
			, error_metric(nullptr)
		{}

		uint32_t hash() const
		{
			uint32_t hash_value = 0;
			hash_value = hash_combine(hash_value, hash32(rotation_format));
			hash_value = hash_combine(hash_value, hash32(translation_format));
			hash_value = hash_combine(hash_value, hash32(scale_format));
			hash_value = hash_combine(hash_value, hash32(range_reduction));

			if (segmenting.enabled)
				hash_value = hash_combine(hash_value, segmenting.get_hash());

			if (error_metric != nullptr)
				hash_value = hash_combine(hash_value, error_metric->get_hash());

			return hash_value;
		}

		const char* get_error() const
		{
			if (translation_format != VectorFormat8::Vector3_96)
			{
				const bool has_clip_range_reduction = are_any_enum_flags_set(range_reduction, RangeReductionFlags8::Translations);
				const bool has_segment_range_reduction = segmenting.enabled && are_any_enum_flags_set(segmenting.range_reduction, RangeReductionFlags8::Translations);
				if (!has_clip_range_reduction && !has_segment_range_reduction)
					return "This translation format requires range reduction to be enabled at the clip or segment level";
			}

			if (scale_format != VectorFormat8::Vector3_96)
			{
				const bool has_clip_range_reduction = are_any_enum_flags_set(range_reduction, RangeReductionFlags8::Scales);
				const bool has_segment_range_reduction = segmenting.enabled && are_any_enum_flags_set(segmenting.range_reduction, RangeReductionFlags8::Scales);
				if (!has_clip_range_reduction && !has_segment_range_reduction)
					return "This scale format requires range reduction to be enabled at the clip or segment level";
			}

			if (segmenting.enabled && segmenting.range_reduction != RangeReductionFlags8::None)
			{
				if (range_reduction == RangeReductionFlags8::None)
					return "Per segment range reduction requires per clip range reduction to be enabled";
			}

			if (error_metric == nullptr)
				return "error_metric cannot be NULL";

			return segmenting.get_error();
		}
	};
}
