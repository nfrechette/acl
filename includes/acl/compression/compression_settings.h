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
#include "acl/core/error_result.h"
#include "acl/core/hash.h"
#include "acl/core/track_types.h"
#include "acl/core/range_reduction_types.h"
#include "acl/compression/compression_level.h"
#include "acl/compression/skeleton_error_metric.h"

#include <rtm/scalarf.h>

#include <cstdint>

ACL_IMPL_FILE_PRAGMA_PUSH

namespace acl
{
	//////////////////////////////////////////////////////////////////////////
	// Encapsulates all the compression settings related to segmenting.
	// Segmenting ensures that large clips are split into smaller segments and
	// compressed independently to allow a smaller memory footprint as well as
	// faster compression and decompression.
	// See also: https://nfrechette.github.io/2016/11/10/anim_compression_uniform_segmenting/
	struct SegmentingSettings
	{
		//////////////////////////////////////////////////////////////////////////
		// How many samples to try and fit in our segments
		// Defaults to '16'
		uint16_t ideal_num_samples;

		//////////////////////////////////////////////////////////////////////////
		// Maximum number of samples per segment
		// Defaults to '31'
		uint16_t max_num_samples;

		SegmentingSettings()
			: ideal_num_samples(16)
			, max_num_samples(31)
		{}

		//////////////////////////////////////////////////////////////////////////
		// Calculates a hash from the internal state to uniquely identify a configuration.
		uint32_t get_hash() const
		{
			uint32_t hash_value = 0;
			hash_value = hash_combine(hash_value, hash32(ideal_num_samples));
			hash_value = hash_combine(hash_value, hash32(max_num_samples));
			return hash_value;
		}

		//////////////////////////////////////////////////////////////////////////
		// Checks if everything is valid and if it isn't, returns an error string.
		// Returns nullptr if the settings are valid.
		ErrorResult is_valid() const
		{
			if (ideal_num_samples < 8)
				return ErrorResult("ideal_num_samples must be greater or equal to 8");

			if (ideal_num_samples > max_num_samples)
				return ErrorResult("ideal_num_samples must be smaller or equal to max_num_samples");

			return ErrorResult();
		}
	};

	//////////////////////////////////////////////////////////////////////////
	// Encapsulates all the compression settings.
	struct CompressionSettings
	{
		//////////////////////////////////////////////////////////////////////////
		// The compression level determines how aggressively we attempt to reduce the memory
		// footprint. Higher levels will try more permutations and bit rates. The higher
		// the level, the slower the compression but the smaller the memory footprint.
		compression_level8 level;

		//////////////////////////////////////////////////////////////////////////
		// The rotation, translation, and scale formats to use. See functions get_rotation_format(..) and get_vector_format(..)
		// Defaults to raw: 'quatf_full' and 'vector3f_full'
		rotation_format8 rotation_format;
		vector_format8 translation_format;
		vector_format8 scale_format;

		//////////////////////////////////////////////////////////////////////////
		// Segmenting settings, if used
		SegmentingSettings segmenting;

		//////////////////////////////////////////////////////////////////////////
		// The error metric to use.
		// Defaults to 'null', this value must be set manually!
		itransform_error_metric* error_metric;

		//////////////////////////////////////////////////////////////////////////
		// Threshold angle when detecting if rotation tracks are constant or default.
		// See the rtm::quatf quat_near_identity for details about how the default threshold
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

		//////////////////////////////////////////////////////////////////////////
		// Default constructor sets things up to perform no compression and to leave things raw.
		CompressionSettings()
			: level(compression_level8::low)
			, rotation_format(rotation_format8::quatf_full)
			, translation_format(vector_format8::vector3f_full)
			, scale_format(vector_format8::vector3f_full)
			, segmenting()
			, error_metric(nullptr)
			, constant_rotation_threshold_angle(0.00284714461F)
			, constant_translation_threshold(0.001F)
			, constant_scale_threshold(0.00001F)
			, error_threshold(0.01F)
		{}

		//////////////////////////////////////////////////////////////////////////
		// Calculates a hash from the internal state to uniquely identify a configuration.
		uint32_t get_hash() const
		{
			uint32_t hash_value = 0;
			hash_value = hash_combine(hash_value, hash32(level));
			hash_value = hash_combine(hash_value, hash32(rotation_format));
			hash_value = hash_combine(hash_value, hash32(translation_format));
			hash_value = hash_combine(hash_value, hash32(scale_format));

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
			if (error_metric == nullptr)
				return ErrorResult("error_metric cannot be NULL");

			const float rotation_threshold_angle = constant_rotation_threshold_angle;
			if (rotation_threshold_angle < 0.0F || !rtm::scalar_is_finite(rotation_threshold_angle))
				return ErrorResult("Invalid constant_rotation_threshold_angle");

			if (constant_translation_threshold < 0.0F || !rtm::scalar_is_finite(constant_translation_threshold))
				return ErrorResult("Invalid constant_translation_threshold");

			if (constant_scale_threshold < 0.0F || !rtm::scalar_is_finite(constant_scale_threshold))
				return ErrorResult("Invalid constant_scale_threshold");

			if (error_threshold < 0.0F || !rtm::scalar_is_finite(error_threshold))
				return ErrorResult("Invalid error_threshold");

			return segmenting.is_valid();
		}
	};

	//////////////////////////////////////////////////////////////////////////
	// Returns raw compression settings. No compression is performed and
	// samples are all retained with full precision.
	inline CompressionSettings get_raw_compression_settings()
	{
		return CompressionSettings();
	}

	//////////////////////////////////////////////////////////////////////////
	// Returns the recommended and default compression settings. These have
	// been tested in a wide range of scenarios and perform best overall.
	inline CompressionSettings get_default_compression_settings()
	{
		CompressionSettings settings;
		settings.level = compression_level8::medium;
		settings.rotation_format = rotation_format8::quatf_drop_w_variable;
		settings.translation_format = vector_format8::vector3f_variable;
		settings.scale_format = vector_format8::vector3f_variable;
		return settings;
	}

	//////////////////////////////////////////////////////////////////////////
	// Encapsulates all the compression settings.
	// Note: Currently only used by scalar track compression which contain no global settings.
	struct compression_settings
	{
		//////////////////////////////////////////////////////////////////////////
		// The compression level determines how aggressively we attempt to reduce the memory
		// footprint. Higher levels will try more permutations and bit rates. The higher
		// the level, the slower the compression but the smaller the memory footprint.
		// Transform tracks only.
		compression_level8 level;

		//////////////////////////////////////////////////////////////////////////
		// The rotation, translation, and scale formats to use. See functions get_rotation_format(..) and get_vector_format(..)
		// Defaults to raw: 'quatf_full' and 'vector3f_full'
		// Transform tracks only.
		rotation_format8 rotation_format;
		vector_format8 translation_format;
		vector_format8 scale_format;

		//////////////////////////////////////////////////////////////////////////
		// Segmenting settings, if used
		// Transform tracks only.
		SegmentingSettings segmenting;

		//////////////////////////////////////////////////////////////////////////
		// The error metric to use.
		// Defaults to 'null', this value must be set manually!
		// Transform tracks only.
		itransform_error_metric* error_metric;

		compression_settings()
			: level(compression_level8::low)
			, rotation_format(rotation_format8::quatf_full)
			, translation_format(vector_format8::vector3f_full)
			, scale_format(vector_format8::vector3f_full)
			, segmenting()
			, error_metric(nullptr)
		{}

		//////////////////////////////////////////////////////////////////////////
		// Calculates a hash from the internal state to uniquely identify a configuration.
		uint32_t get_hash() const
		{
			uint32_t hash_value = 0;
			hash_value = hash_combine(hash_value, hash32(level));
			hash_value = hash_combine(hash_value, hash32(rotation_format));
			hash_value = hash_combine(hash_value, hash32(translation_format));
			hash_value = hash_combine(hash_value, hash32(scale_format));

			hash_value = hash_combine(hash_value, segmenting.get_hash());

			if (error_metric != nullptr)
				hash_value = hash_combine(hash_value, error_metric->get_hash());

			return hash_value;
		}

		//////////////////////////////////////////////////////////////////////////
		// Checks if everything is valid and if it isn't, returns an error string.
		ErrorResult is_valid() const
		{
			if (error_metric == nullptr)
				return ErrorResult("error_metric cannot be NULL");

			return segmenting.is_valid();
		}
	};

	//////////////////////////////////////////////////////////////////////////
	// Returns raw compression settings. No compression is performed and
	// samples are all retained with full precision.
	inline compression_settings get_raw_compression_settings_()
	{
		return compression_settings();
	}

	//////////////////////////////////////////////////////////////////////////
	// Returns the recommended and default compression settings. These have
	// been tested in a wide range of scenarios and perform best overall.
	inline compression_settings get_default_compression_settings_()
	{
		compression_settings settings;
		settings.level = compression_level8::medium;
		settings.rotation_format = rotation_format8::quatf_drop_w_variable;
		settings.translation_format = vector_format8::vector3f_variable;
		settings.scale_format = vector_format8::vector3f_variable;
		return settings;
	}
}

ACL_IMPL_FILE_PRAGMA_POP
