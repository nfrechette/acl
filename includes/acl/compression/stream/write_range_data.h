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
#include "acl/core/range_reduction_types.h"
#include "acl/math/quat_32.h"
#include "acl/math/quat_packing.h"
#include "acl/math/vector4_32.h"
#include "acl/math/vector4_packing.h"
#include "acl/compression/stream/clip_context.h"

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

	inline uint32_t get_stream_range_data_size(const ClipContext& clip_context, RangeReductionFlags8 range_reduction, RotationFormat8 rotation_format, VectorFormat8 translation_format)
	{
		// Only use the first segment, it contains the necessary information
		const SegmentContext& segment = clip_context.segments[0];
		return get_stream_range_data_size(segment.bone_streams, segment.num_bones, range_reduction, rotation_format, translation_format);
	}

	inline void write_range_track_data(const BoneStreams* bone_streams, const BoneRanges* bone_ranges, uint16_t num_bones,
		RangeReductionFlags8 range_reduction, bool is_clip_range_data,
		uint8_t* range_data, uint32_t range_data_size)
	{
		const uint8_t* range_data_end = add_offset_to_ptr<uint8_t>(range_data, range_data_size);

		for (uint16_t bone_index = 0; bone_index < num_bones; ++bone_index)
		{
			const BoneStreams& bone_stream = bone_streams[bone_index];
			const BoneRanges& bone_range = bone_ranges[bone_index];

			// normalized value is between [0.0 .. 1.0]
			// value = (normalized value * range extent) + range min
			// normalized value = (value - range min) / range extent

			if (is_enum_flag_set(range_reduction, RangeReductionFlags8::Rotations) && bone_stream.is_rotation_animated())
			{
				Vector4_32 range_min = bone_range.rotation.get_min();
				Vector4_32 range_extent = bone_range.rotation.get_extent();

				if (is_clip_range_data)
				{
					uint32_t range_member_size = bone_stream.rotations.get_rotation_format() == RotationFormat8::Quat_128 ? (sizeof(float) * 4) : (sizeof(float) * 3);

					memcpy(range_data, vector_as_float_ptr(range_min), range_member_size);
					range_data += range_member_size;
					memcpy(range_data, vector_as_float_ptr(range_extent), range_member_size);
					range_data += range_member_size;
				}
				else
				{
					uint16_t* data = safe_ptr_cast<uint16_t>(range_data);
					uint8_t offset = 0;

					if (bone_stream.rotations.get_rotation_format() == RotationFormat8::Quat_128)
					{
#if ACL_PER_SEGMENT_RANGE_REDUCTION_COMPONENT_BIT_SIZE == 8
						pack_vector4_32(range_min, true, range_data);
						range_data += sizeof(uint8_t) * 4;
						pack_vector4_32(range_extent, true, range_data);
						range_data += sizeof(uint8_t) * 4;
#else
						pack_vector4_64(range_min, true, range_data);
						range_data += sizeof(uint16_t) * 4;
						pack_vector4_64(range_extent, true, range_data);
						range_data += sizeof(uint16_t) * 4;
#endif
					}
					else
					{
#if ACL_PER_SEGMENT_RANGE_REDUCTION_COMPONENT_BIT_SIZE == 8
						if (is_pack_0_bit_rate(bone_stream.rotations.get_bit_rate()))
						{
							const uint8_t* rotation = bone_stream.rotations.get_raw_sample_ptr(0);
							memcpy(range_data, rotation, sizeof(uint16_t) * 3);
							range_data += sizeof(uint16_t) * 3;
						}
						else
						{
							pack_vector3_24(range_min, true, range_data);
							range_data += sizeof(uint8_t) * 3;
							pack_vector3_24(range_extent, true, range_data);
							range_data += sizeof(uint8_t) * 3;
						}
#else
						if (is_pack_0_bit_rate(bone_stream.rotations.get_bit_rate()))
						{
							const uint8_t* rotation = bone_stream.rotations.get_raw_sample_ptr(0);
							memcpy(range_data, rotation, sizeof(uint32_t) * 3);
							range_data += sizeof(uint32_t) * 3;
						}
						else
						{
							pack_vector3_48(range_min, true, range_data);
							range_data += sizeof(uint16_t) * 3;
							pack_vector3_48(range_extent, true, range_data);
							range_data += sizeof(uint16_t) * 3;
						}
#endif
					}
				}
			}

			if (is_enum_flag_set(range_reduction, RangeReductionFlags8::Translations) && bone_stream.is_translation_animated())
			{
				Vector4_32 range_min = bone_range.translation.get_min();
				Vector4_32 range_extent = bone_range.translation.get_extent();

				if (is_clip_range_data)
				{
					uint32_t range_member_size = sizeof(float) * 3;

					memcpy(range_data, vector_as_float_ptr(range_min), range_member_size);
					range_data += range_member_size;
					memcpy(range_data, vector_as_float_ptr(range_extent), range_member_size);
					range_data += range_member_size;
				}
				else
				{
#if ACL_PER_SEGMENT_RANGE_REDUCTION_COMPONENT_BIT_SIZE == 8
					if (is_pack_0_bit_rate(bone_stream.translations.get_bit_rate()))
					{
						const uint8_t* translation = bone_stream.translations.get_raw_sample_ptr(0);
						memcpy(range_data, translation, sizeof(uint16_t) * 3);
						range_data += sizeof(uint16_t) * 3;
					}
					else
					{
						pack_vector3_24(range_min, true, range_data);
						range_data += sizeof(uint8_t) * 3;
						pack_vector3_24(range_extent, true, range_data);
						range_data += sizeof(uint8_t) * 3;
					}
#else
					if (is_pack_0_bit_rate(bone_stream.translations.get_bit_rate()))
					{
						const uint8_t* translation = bone_stream.translations.get_raw_sample_ptr(0);
						memcpy(range_data, translation, sizeof(uint32_t) * 3);
						range_data += sizeof(uint32_t) * 3;
					}
					else
					{
						pack_vector3_48(range_min, true, range_data);
						range_data += sizeof(uint16_t) * 3;
						pack_vector3_48(range_extent, true, range_data);
						range_data += sizeof(uint16_t) * 3;
					}
#endif
				}
			}

			ACL_ENSURE(range_data <= range_data_end, "Invalid range data offset. Wrote too much data.");
		}

		ACL_ENSURE(range_data == range_data_end, "Invalid range data offset. Wrote too little data.");
	}

	inline void write_clip_range_data(const SegmentContext& segment, RangeReductionFlags8 range_reduction, uint8_t* range_data, uint32_t range_data_size)
	{
		write_range_track_data(segment.bone_streams, segment.clip->ranges, segment.num_bones, range_reduction, true, range_data, range_data_size);
	}

	inline void write_segment_range_data(const SegmentContext& segment, RangeReductionFlags8 range_reduction, uint8_t* range_data, uint32_t range_data_size)
	{
		write_range_track_data(segment.bone_streams, segment.ranges, segment.num_bones, range_reduction, false, range_data, range_data_size);
	}
}
