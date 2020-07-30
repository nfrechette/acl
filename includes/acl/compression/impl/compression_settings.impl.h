#pragma once

////////////////////////////////////////////////////////////////////////////////
// The MIT License (MIT)
//
// Copyright (c) 2020 Nicholas Frechette & Animation Compression Library contributors
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

// Included only once from compression_settings.h

#include "acl/core/error_result.h"
#include "acl/core/hash.h"
#include "acl/core/track_formats.h"

#include <cstdint>

namespace acl
{
	inline uint32_t segmenting_settings::get_hash() const
	{
		uint32_t hash_value = 0;
		hash_value = hash_combine(hash_value, hash32(ideal_num_samples));
		hash_value = hash_combine(hash_value, hash32(max_num_samples));
		return hash_value;
	}

	inline error_result segmenting_settings::is_valid() const
	{
		if (ideal_num_samples < 8)
			return error_result("ideal_num_samples must be greater or equal to 8");

		if (ideal_num_samples > max_num_samples)
			return error_result("ideal_num_samples must be smaller or equal to max_num_samples");

		return error_result();
	}

	inline uint32_t compression_settings::get_hash() const
	{
		uint32_t hash_value = 0;
		hash_value = hash_combine(hash_value, hash32(level));
		hash_value = hash_combine(hash_value, hash32(rotation_format));
		hash_value = hash_combine(hash_value, hash32(translation_format));
		hash_value = hash_combine(hash_value, hash32(scale_format));

		hash_value = hash_combine(hash_value, segmenting.get_hash());

		if (error_metric != nullptr)
			hash_value = hash_combine(hash_value, error_metric->get_hash());

		hash_value = hash_combine(hash_value, hash32(include_track_list_name));
		hash_value = hash_combine(hash_value, hash32(include_track_names));
		hash_value = hash_combine(hash_value, hash32(include_parent_track_indices));
		hash_value = hash_combine(hash_value, hash32(include_track_descriptions));

		return hash_value;
	}

	inline error_result compression_settings::is_valid() const
	{
		if (error_metric == nullptr)
			return error_result("error_metric cannot be NULL");

		return segmenting.is_valid();
	}

	inline compression_settings get_raw_compression_settings()
	{
		return compression_settings();
	}

	inline compression_settings get_default_compression_settings()
	{
		compression_settings settings;
		settings.level = compression_level8::medium;
		settings.rotation_format = rotation_format8::quatf_drop_w_variable;
		settings.translation_format = vector_format8::vector3f_variable;
		settings.scale_format = vector_format8::vector3f_variable;
		return settings;
	}
}
