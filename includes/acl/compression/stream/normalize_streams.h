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

#include "acl/core/memory.h"
#include "acl/core/error.h"
#include "acl/core/enum_utils.h"
#include "acl/core/algorithm_types.h"
#include "acl/math/quat_64.h"
#include "acl/math/vector4_64.h"
#include "acl/compression/stream/track_stream.h"

#include <stdint.h>

namespace acl
{
	inline void normalize_rotation_streams(BoneStreams* bone_streams, uint16_t num_bones, RangeReductionFlags8 range_reduction, RotationFormat8 rotation_format)
	{
		if (!is_enum_flag_set(range_reduction, RangeReductionFlags8::Rotations))
			return;

		for (uint16_t bone_index = 0; bone_index < num_bones; ++bone_index)
		{
			BoneStreams& bone_stream = bone_streams[bone_index];

			// We expect all our samples to have the same width of sizeof(Vector4_64)
			ACL_ENSURE(bone_stream.rotations.get_sample_size() == sizeof(Vector4_64), "Unexpected rotation sample size. %u != %u", bone_stream.rotations.get_sample_size(), sizeof(Vector4_64));

			if (!bone_stream.is_rotation_animated())
				continue;

			uint32_t num_samples = bone_stream.rotations.get_num_samples();

			Vector4_64 range_min = bone_stream.rotation_range.get_min();
			Vector4_64 range_extent = bone_stream.rotation_range.get_extent();

			for (uint32_t sample_index = 0; sample_index < num_samples; ++sample_index)
			{
				// normalized value is between [0.0 .. 1.0]
				// value = (normalized value * range extent) + range min
				// normalized value = (value - range min) / range extent
				Vector4_64 rotation = bone_stream.rotations.get_sample<Vector4_64>(sample_index);
				Vector4_64 normalized_rotation = vector_div(vector_sub(rotation, range_min), range_extent);

#if defined(ACL_USE_ERROR_CHECKS)
				switch (rotation_format)
				{
				case RotationFormat8::Quat_128:
					ACL_ENSURE(vector_all_greater_equal(normalized_rotation, vector_zero_64()) && vector_all_less_equal(normalized_rotation, vector_set(1.0)), "Invalid normalized rotation. 0.0 <= [%f, %f, %f, %f] <= 1.0", vector_get_x(normalized_rotation), vector_get_y(normalized_rotation), vector_get_z(normalized_rotation), vector_get_w(normalized_rotation));
					break;
				case RotationFormat8::Quat_96:
				case RotationFormat8::Quat_48:
				case RotationFormat8::Quat_32:
					ACL_ENSURE(vector_all_greater_equal3(normalized_rotation, vector_zero_64()) && vector_all_less_equal3(normalized_rotation, vector_set(1.0)), "Invalid normalized rotation. 0.0 <= [%f, %f, %f] <= 1.0", vector_get_x(normalized_rotation), vector_get_y(normalized_rotation), vector_get_z(normalized_rotation));
					break;
				}
#endif

				bone_stream.rotations.set_sample(sample_index, normalized_rotation);
			}

			bone_stream.are_rotations_normalized = true;
		}
	}

	inline void normalize_translation_streams(BoneStreams* bone_streams, uint16_t num_bones, RangeReductionFlags8 range_reduction)
	{
		if (!is_enum_flag_set(range_reduction, RangeReductionFlags8::Translations))
			return;

		for (uint16_t bone_index = 0; bone_index < num_bones; ++bone_index)
		{
			BoneStreams& bone_stream = bone_streams[bone_index];

			// We expect all our samples to have the same width of sizeof(Vector4_64)
			ACL_ENSURE(bone_stream.translations.get_sample_size() == sizeof(Vector4_64), "Unexpected translation sample size. %u != %u", bone_stream.translations.get_sample_size(), sizeof(Vector4_64));

			if (!bone_stream.is_translation_animated())
				continue;

			uint32_t num_samples = bone_stream.translations.get_num_samples();

			Vector4_64 range_min = bone_stream.translation_range.get_min();
			Vector4_64 range_extent = bone_stream.translation_range.get_extent();

			for (uint32_t sample_index = 0; sample_index < num_samples; ++sample_index)
			{
				// normalized value is between [0.0 .. 1.0]
				// value = (normalized value * range extent) + range min
				// normalized value = (value - range min) / range extent
				Vector4_64 translation = bone_stream.translations.get_sample<Vector4_64>(sample_index);
				Vector4_64 normalized_translation = vector_div(vector_sub(translation, range_min), range_extent);

				ACL_ENSURE(vector_all_greater_equal3(normalized_translation, vector_zero_64()) && vector_all_less_equal3(normalized_translation, vector_set(1.0)), "Invalid normalized translation. 0.0 <= [%f, %f, %f] <= 1.0", vector_get_x(normalized_translation), vector_get_y(normalized_translation), vector_get_z(normalized_translation));

				bone_stream.translations.set_sample(sample_index, normalized_translation);
			}

			bone_stream.are_translations_normalized = true;
		}
	}
}
