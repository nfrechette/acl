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
#include "acl/core/error_result.h"
#include "acl/core/hash.h"
#include "acl/core/track_types.h"
#include "acl/core/range_reduction_types.h"
#include "acl/compression/skeleton_error_metric.h"
#include "acl/math/scalar_32.h"

#include <cstdint>

ACL_IMPL_FILE_PRAGMA_PUSH

namespace acl
{
	struct SegmentingSettings
	{
		//////////////////////////////////////////////////////////////////////////
		// Whether to enable segmenting or not
		// Defaults to 'false'
		bool enabled;

		//////////////////////////////////////////////////////////////////////////
		// How many samples to try and fit in our segments
		// Defaults to '16'
		uint16_t ideal_num_samples;

		//////////////////////////////////////////////////////////////////////////
		// Maximum number of samples per segment
		// Defaults to '31'
		uint16_t max_num_samples;

		//////////////////////////////////////////////////////////////////////////
		// Whether to use range reduction or not at the segment level
		// Defaults to 'None'
		RangeReductionFlags8 range_reduction;

		SegmentingSettings()
			: enabled(false)
			, ideal_num_samples(16)
			, max_num_samples(31)
			, range_reduction(RangeReductionFlags8::None)
		{}

		//////////////////////////////////////////////////////////////////////////
		// Calculates a hash from the internal state to uniquely identify a configuration.
		uint32_t get_hash() const
		{
			uint32_t hash_value = 0;
			hash_value = hash_combine(hash_value, hash32(enabled));
			hash_value = hash_combine(hash_value, hash32(ideal_num_samples));
			hash_value = hash_combine(hash_value, hash32(max_num_samples));
			hash_value = hash_combine(hash_value, hash32(range_reduction));
			return hash_value;
		}

		//////////////////////////////////////////////////////////////////////////
		// Checks if everything is valid and if it isn't, returns an error string.
		// Returns nullptr if the settings are valid.
		ErrorResult is_valid() const
		{
			if (!enabled)
				return ErrorResult();

			if (ideal_num_samples < 8)
				return ErrorResult("ideal_num_samples must be greater or equal to 8");

			if (ideal_num_samples > max_num_samples)
				return ErrorResult("ideal_num_samples must be smaller or equal to max_num_samples");

			return ErrorResult();
		}
	};

	struct CompressionSettings
	{
		//////////////////////////////////////////////////////////////////////////
		// The rotation, translation, and scale formats to use. See functions get_rotation_format(..) and get_vector_format(..)
		// Defaults to raw: 'Quat_128' and 'Vector3_96'
		RotationFormat8 rotation_format;
		VectorFormat8 translation_format;
		VectorFormat8 scale_format;

		//////////////////////////////////////////////////////////////////////////
		// Whether to use range reduction or not at the clip level
		// Defaults to 'None'
		RangeReductionFlags8 range_reduction;

		//////////////////////////////////////////////////////////////////////////
		// Segmenting settings, if used
		SegmentingSettings segmenting;

		//////////////////////////////////////////////////////////////////////////
		// The error metric to use.
		// Defaults to 'null', this value must be set manually!
		ISkeletalErrorMetric* error_metric;

		//////////////////////////////////////////////////////////////////////////
		// Threshold angle when detecting if rotation tracks are constant or default.
		// See the Quat_32 quat_near_identity for details about how the default threshold
		// was chosen. You will typically NEVER need to change this, the value has been
		// selected to be as safe as possible and is independent of game engine units.
		// Defaults to '0.00284714461' radians
		float constant_rotation_threshold_angle;

		//////////////////////////////////////////////////////////////////////////
		// Threshold value to use when detecting if translation tracks are constant or default.
		// Note that you will need to change this value if your units are not in centimeters.
		// Defaults to '0.001' centimeters.
		float constant_translation_threshold;

		//////////////////////////////////////////////////////////////////////////
		// Threshold value to use when detecting if scale tracks are constant or default.
		// There are no units for scale as such a value that was deemed safe was selected
		// as a default.
		// Defaults to '0.00001'
		float constant_scale_threshold;

		//////////////////////////////////////////////////////////////////////////
		// The error threshold used when optimizing the bit rate.
		// Note that you will need to change this value if your units are not in centimeters.
		// Defaults to '0.01' centimeters
		float error_threshold;

		CompressionSettings()
			: rotation_format(RotationFormat8::Quat_128)
			, translation_format(VectorFormat8::Vector3_96)
			, scale_format(VectorFormat8::Vector3_96)
			, range_reduction(RangeReductionFlags8::None)
			, segmenting()
			, error_metric(nullptr)
			, constant_rotation_threshold_angle(0.00284714461f)
			, constant_translation_threshold(0.001f)
			, constant_scale_threshold(0.00001f)
			, error_threshold(0.01f)
		{}

		//////////////////////////////////////////////////////////////////////////
		// Calculates a hash from the internal state to uniquely identify a configuration.
		uint32_t get_hash() const
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

			hash_value = hash_combine(hash_value, hash32(constant_rotation_threshold_angle));
			hash_value = hash_combine(hash_value, hash32(constant_translation_threshold));
			hash_value = hash_combine(hash_value, hash32(constant_scale_threshold));

			hash_value = hash_combine(hash_value, hash32(error_threshold));

			return hash_value;
		}

		//////////////////////////////////////////////////////////////////////////
		// Checks if everything is valid and if it isn't, returns an error string.
		// Returns nullptr if the settings are valid.
		ErrorResult is_valid() const
		{
			if (translation_format != VectorFormat8::Vector3_96)
			{
				const bool has_clip_range_reduction = are_any_enum_flags_set(range_reduction, RangeReductionFlags8::Translations);
				const bool has_segment_range_reduction = segmenting.enabled && are_any_enum_flags_set(segmenting.range_reduction, RangeReductionFlags8::Translations);
				if (!has_clip_range_reduction && !has_segment_range_reduction)
					return ErrorResult("This translation format requires range reduction to be enabled at the clip or segment level");
			}

			if (scale_format != VectorFormat8::Vector3_96)
			{
				const bool has_clip_range_reduction = are_any_enum_flags_set(range_reduction, RangeReductionFlags8::Scales);
				const bool has_segment_range_reduction = segmenting.enabled && are_any_enum_flags_set(segmenting.range_reduction, RangeReductionFlags8::Scales);
				if (!has_clip_range_reduction && !has_segment_range_reduction)
					return ErrorResult("This scale format requires range reduction to be enabled at the clip or segment level");
			}

			if (segmenting.enabled && segmenting.range_reduction != RangeReductionFlags8::None)
			{
				if (range_reduction == RangeReductionFlags8::None)
					return ErrorResult("Per segment range reduction requires per clip range reduction to be enabled");
			}

			if (error_metric == nullptr)
				return ErrorResult("error_metric cannot be NULL");

			if (constant_rotation_threshold_angle < 0.0f || !is_finite(constant_rotation_threshold_angle))
				return ErrorResult("Invalid constant_rotation_threshold_angle");

			if (constant_translation_threshold < 0.0f || !is_finite(constant_translation_threshold))
				return ErrorResult("Invalid constant_translation_threshold");

			if (constant_scale_threshold < 0.0f || !is_finite(constant_scale_threshold))
				return ErrorResult("Invalid constant_scale_threshold");

			if (error_threshold < 0.0f || !is_finite(error_threshold))
				return ErrorResult("Invalid error_threshold");

			return segmenting.is_valid();
		}
	};

	//////////////////////////////////////////////////////////////////////////
	// Returns the recommended and default compression settings. These have
	// been tested in a wide range of scenarios and perform best overall.
	inline CompressionSettings get_default_compression_settings()
	{
		CompressionSettings settings;
		settings.rotation_format = RotationFormat8::QuatDropW_Variable;
		settings.translation_format = VectorFormat8::Vector3_Variable;
		settings.scale_format = VectorFormat8::Vector3_Variable;
		settings.range_reduction = RangeReductionFlags8::AllTracks;
		settings.segmenting.enabled = true;
		settings.segmenting.range_reduction = RangeReductionFlags8::AllTracks;
		return settings;
	}
}

ACL_IMPL_FILE_PRAGMA_POP
