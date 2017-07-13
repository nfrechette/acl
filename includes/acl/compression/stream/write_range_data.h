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
#include "acl/core/track_types.h"
#include "acl/math/quat_64.h"
#include "acl/math/quat_packing.h"
#include "acl/math/vector4_64.h"
#include "acl/math/vector4_packing.h"
#include "acl/compression/stream/track_stream.h"

#include <stdint.h>

namespace acl
{
	inline uint32_t get_stream_range_data_size(const BoneStreams* bone_streams, uint16_t num_bones, RangeReductionFlags8 range_reduction, RotationFormat8 rotation_format, VectorFormat8 translation_format)
	{
		uint32_t rotation_size = is_enum_flag_set(range_reduction, RangeReductionFlags8::Rotations) ? get_range_reduction_rotation_size(rotation_format) : 0;
		uint32_t translation_size = is_enum_flag_set(range_reduction, RangeReductionFlags8::Translations) ? get_range_reduction_vector_size(translation_format) : 0;
		uint32_t range_data_size = 0;

		for (uint16_t bone_index = 0; bone_index < num_bones; ++bone_index)
		{
			const BoneStreams& bone_stream = bone_streams[bone_index];

			if (bone_stream.is_rotation_animated())
				range_data_size += rotation_size;

			if (bone_stream.is_translation_animated())
				range_data_size += translation_size;
		}

		return range_data_size;
	}

	inline void write_range_track_data(const BoneStreams* bone_streams, uint16_t num_bones,
		RangeReductionFlags8 range_reduction, RotationFormat8 rotation_format, VectorFormat8 translation_format,
		uint8_t* range_data, uint32_t range_data_size)
	{
		const uint8_t* range_data_end = add_offset_to_ptr<uint8_t>(range_data, range_data_size);

		uint32_t rotation_size = is_enum_flag_set(range_reduction, RangeReductionFlags8::Rotations) ? get_range_reduction_rotation_size(rotation_format) : 0;
		uint32_t translation_size = is_enum_flag_set(range_reduction, RangeReductionFlags8::Translations) ? get_range_reduction_vector_size(translation_format) : 0;

		// Our min/extent have the same size, divide it in half
		rotation_size /= 2;
		translation_size /= 2;

		for (uint16_t bone_index = 0; bone_index < num_bones; ++bone_index)
		{
			const BoneStreams& bone_stream = bone_streams[bone_index];

			// normalized value is between [0.0 .. 1.0]
			// value = (normalized value * range extent) + range min
			// normalized value = (value - range min) / range extent

			if (is_enum_flag_set(range_reduction, RangeReductionFlags8::Rotations) && bone_stream.is_rotation_animated())
			{
				Vector4_32 range_min = vector_cast(bone_stream.rotation_range.get_min());
				Vector4_32 range_extent = vector_cast(bone_stream.rotation_range.get_extent());

				memcpy(range_data, vector_as_float_ptr(range_min), rotation_size);
				range_data += rotation_size;
				memcpy(range_data, vector_as_float_ptr(range_extent), rotation_size);
				range_data += rotation_size;
			}

			if (is_enum_flag_set(range_reduction, RangeReductionFlags8::Translations) && bone_stream.is_translation_animated())
			{
				Vector4_32 range_min = vector_cast(bone_stream.translation_range.get_min());
				Vector4_32 range_extent = vector_cast(bone_stream.translation_range.get_extent());

				memcpy(range_data, vector_as_float_ptr(range_min), translation_size);
				range_data += translation_size;
				memcpy(range_data, vector_as_float_ptr(range_extent), translation_size);
				range_data += translation_size;
			}

			ACL_ENSURE(range_data <= range_data_end, "Invalid range data offset. Wrote too much data.");
		}

		ACL_ENSURE(range_data == range_data_end, "Invalid range data offset. Wrote too little data.");
	}
}
