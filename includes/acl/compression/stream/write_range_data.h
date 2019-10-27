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

#include "acl/core/iallocator.h"
#include "acl/core/compiler_utils.h"
#include "acl/core/error.h"
#include "acl/core/enum_utils.h"
#include "acl/core/track_types.h"
#include "acl/core/range_reduction_types.h"
#include "acl/math/quat_32.h"
#include "acl/math/quat_packing.h"
#include "acl/math/vector4_32.h"
#include "acl/math/vector4_packing.h"
#include "acl/compression/stream/clip_context.h"

#include <cstdint>

ACL_IMPL_FILE_PRAGMA_PUSH

namespace acl
{
	inline uint32_t get_stream_range_data_size(const ClipContext& clip_context, RangeReductionFlags8 range_reduction, RotationFormat8 rotation_format)
	{
		const uint32_t rotation_size = are_any_enum_flags_set(range_reduction, RangeReductionFlags8::Rotations) ? get_range_reduction_rotation_size(rotation_format) : 0;
		const uint32_t translation_size = are_any_enum_flags_set(range_reduction, RangeReductionFlags8::Translations) ? k_clip_range_reduction_vector3_range_size : 0;
		const uint32_t scale_size = are_any_enum_flags_set(range_reduction, RangeReductionFlags8::Scales) ? k_clip_range_reduction_vector3_range_size : 0;
		uint32_t range_data_size = 0;

		// Only use the first segment, it contains the necessary information
		const SegmentContext& segment = clip_context.segments[0];
		for (const BoneStreams& bone_stream : segment.const_bone_iterator())
		{
			if (!bone_stream.is_rotation_constant)
				range_data_size += rotation_size;

			if (!bone_stream.is_translation_constant)
				range_data_size += translation_size;

			if (!bone_stream.is_scale_constant)
				range_data_size += scale_size;
		}

		return range_data_size;
	}

	inline void write_range_track_data_impl(const TrackStream& track, const TrackStreamRange& range, bool is_clip_range_data, uint8_t*& out_range_data)
	{
		const Vector4_32 range_min = range.get_min();
		const Vector4_32 range_extent = range.get_extent();

		if (is_clip_range_data)
		{
			const uint32_t range_member_size = sizeof(float) * 3;

			std::memcpy(out_range_data, vector_as_float_ptr(range_min), range_member_size);
			out_range_data += range_member_size;
			std::memcpy(out_range_data, vector_as_float_ptr(range_extent), range_member_size);
			out_range_data += range_member_size;
		}
		else
		{
			if (is_constant_bit_rate(track.get_bit_rate()))
			{
				const uint8_t* sample_ptr = track.get_raw_sample_ptr(0);
				std::memcpy(out_range_data, sample_ptr, sizeof(uint16_t) * 3);
				out_range_data += sizeof(uint16_t) * 3;
			}
			else
			{
				pack_vector3_u24_unsafe(range_min, out_range_data);
				out_range_data += sizeof(uint8_t) * 3;
				pack_vector3_u24_unsafe(range_extent, out_range_data);
				out_range_data += sizeof(uint8_t) * 3;
			}
		}
	}

	inline void write_range_track_data(const ClipContext& clip_context, const BoneStreams* bone_streams, const BoneRanges* bone_ranges,
		RangeReductionFlags8 range_reduction, bool is_clip_range_data,
		uint8_t* range_data, uint32_t range_data_size,
		const uint16_t* output_bone_mapping, uint16_t num_output_bones)
	{
		ACL_ASSERT(range_data != nullptr, "'range_data' cannot be null!");
		(void)range_data_size;
		(void)clip_context;	// TODO: Remove in 2.0

#if defined(ACL_HAS_ASSERT_CHECKS)
		const uint8_t* range_data_end = add_offset_to_ptr<uint8_t>(range_data, range_data_size);
#endif

		for (uint16_t output_index = 0; output_index < num_output_bones; ++output_index)
		{
			const uint16_t bone_index = output_bone_mapping[output_index];
			const BoneStreams& bone_stream = bone_streams[bone_index];
			const BoneRanges& bone_range = bone_ranges[bone_index];

			// normalized value is between [0.0 .. 1.0]
			// value = (normalized value * range extent) + range min
			// normalized value = (value - range min) / range extent

			if (are_any_enum_flags_set(range_reduction, RangeReductionFlags8::Rotations) && !bone_stream.is_rotation_constant)
			{
				const Vector4_32 range_min = bone_range.rotation.get_min();
				const Vector4_32 range_extent = bone_range.rotation.get_extent();

				if (is_clip_range_data)
				{
					const uint32_t range_member_size = bone_stream.rotations.get_rotation_format() == RotationFormat8::Quat_128 ? (sizeof(float) * 4) : (sizeof(float) * 3);

					std::memcpy(range_data, vector_as_float_ptr(range_min), range_member_size);
					range_data += range_member_size;
					std::memcpy(range_data, vector_as_float_ptr(range_extent), range_member_size);
					range_data += range_member_size;
				}
				else
				{
					if (bone_stream.rotations.get_rotation_format() == RotationFormat8::Quat_128)
					{
						pack_vector4_32(range_min, true, range_data);
						range_data += sizeof(uint8_t) * 4;
						pack_vector4_32(range_extent, true, range_data);
						range_data += sizeof(uint8_t) * 4;
					}
					else
					{
						if (is_constant_bit_rate(bone_stream.rotations.get_bit_rate()))
						{
							const uint8_t* rotation = bone_stream.rotations.get_raw_sample_ptr(0);
							std::memcpy(range_data, rotation, sizeof(uint16_t) * 3);
							range_data += sizeof(uint16_t) * 3;
						}
						else
						{
							pack_vector3_u24_unsafe(range_min, range_data);
							range_data += sizeof(uint8_t) * 3;
							pack_vector3_u24_unsafe(range_extent, range_data);
							range_data += sizeof(uint8_t) * 3;
						}
					}
				}
			}

			if (are_any_enum_flags_set(range_reduction, RangeReductionFlags8::Translations) && !bone_stream.is_translation_constant)
				write_range_track_data_impl(bone_stream.translations, bone_range.translation, is_clip_range_data, range_data);

			if (are_any_enum_flags_set(range_reduction, RangeReductionFlags8::Scales) && !bone_stream.is_scale_constant)
				write_range_track_data_impl(bone_stream.scales, bone_range.scale, is_clip_range_data, range_data);

			ACL_ASSERT(range_data <= range_data_end, "Invalid range data offset. Wrote too much data.");
		}

		ACL_ASSERT(range_data == range_data_end, "Invalid range data offset. Wrote too little data.");
	}

	inline void write_clip_range_data(const ClipContext& clip_context, RangeReductionFlags8 range_reduction, uint8_t* range_data, uint32_t range_data_size, const uint16_t* output_bone_mapping, uint16_t num_output_bones)
	{
		// Only use the first segment, it contains the necessary information
		const SegmentContext& segment = clip_context.segments[0];

		write_range_track_data(clip_context, segment.bone_streams, clip_context.ranges, range_reduction, true, range_data, range_data_size, output_bone_mapping, num_output_bones);
	}

	inline void write_segment_range_data(const ClipContext& clip_context, const SegmentContext& segment, RangeReductionFlags8 range_reduction, uint8_t* range_data, uint32_t range_data_size, const uint16_t* output_bone_mapping, uint16_t num_output_bones)
	{
		write_range_track_data(clip_context, segment.bone_streams, segment.ranges, range_reduction, false, range_data, range_data_size, output_bone_mapping, num_output_bones);
	}

	namespace acl_impl
	{
		inline uint32_t write_clip_range_data(const track_database& mutable_database, RangeReductionFlags8 range_reduction, const uint16_t* output_transform_mapping, uint16_t num_output_transforms, uint8_t* out_range_data)
		{
			const uint32_t num_rotation_floats = mutable_database.get_rotation_format() == RotationFormat8::Quat_128 ? 4 : 3;

			uint8_t* output_buffer = out_range_data;
			const uint8_t* output_buffer_start = output_buffer;

			for (uint16_t output_index = 0; output_index < num_output_transforms; ++output_index)
			{
				const uint32_t transform_index = output_transform_mapping[output_index];
				const qvvf_ranges& transform_range = mutable_database.get_range(transform_index);

				if (are_any_enum_flags_set(range_reduction, RangeReductionFlags8::Rotations) && !transform_range.is_rotation_constant)
				{
					if (out_range_data != nullptr)
					{
						const Vector4_32 range_min = transform_range.rotation_min;
						const Vector4_32 range_extent = transform_range.rotation_extent;

						if (num_rotation_floats == 3)
						{
							vector_unaligned_write3(range_min, output_buffer);
							vector_unaligned_write3(range_extent, output_buffer + sizeof(float) * 3);
						}
						else
						{
							vector_unaligned_write(range_min, output_buffer);
							vector_unaligned_write(range_extent, output_buffer + sizeof(float) * 4);
						}
					}

					output_buffer += sizeof(float) * num_rotation_floats * 2;
				}

				if (are_any_enum_flags_set(range_reduction, RangeReductionFlags8::Translations) && !transform_range.is_translation_constant)
				{
					if (out_range_data != nullptr)
					{
						const Vector4_32 range_min = transform_range.translation_min;
						const Vector4_32 range_extent = transform_range.translation_extent;

						vector_unaligned_write3(range_min, output_buffer);
						vector_unaligned_write3(range_extent, output_buffer + sizeof(float) * 3);
					}

					output_buffer += sizeof(float) * 3 * 2;
				}

				if (are_any_enum_flags_set(range_reduction, RangeReductionFlags8::Scales) && !transform_range.is_scale_constant)
				{
					if (out_range_data != nullptr)
					{
						const Vector4_32 range_min = transform_range.scale_min;
						const Vector4_32 range_extent = transform_range.scale_extent;

						vector_unaligned_write3(range_min, output_buffer);
						vector_unaligned_write3(range_extent, output_buffer + sizeof(float) * 3);
					}

					output_buffer += sizeof(float) * 3 * 2;
				}
			}

			return safe_static_cast<uint32_t>(output_buffer - output_buffer_start);
		}

		inline uint32_t write_segment_range_data(const track_database& mutable_database, const segment_context& segment, RangeReductionFlags8 range_reduction, const uint16_t* output_transform_mapping, uint16_t num_output_transforms, uint8_t* out_range_data)
		{
			const uint32_t num_rotation_floats = mutable_database.get_rotation_format() == RotationFormat8::Quat_128 ? 4 : 3;

			uint8_t* output_buffer = out_range_data;
			const uint8_t* output_buffer_start = output_buffer;

			for (uint16_t output_index = 0; output_index < num_output_transforms; ++output_index)
			{
				const uint32_t transform_index = output_transform_mapping[output_index];
				const qvvf_ranges& transform_range = segment.ranges[transform_index];
				const BoneBitRate& bit_rate = segment.bit_rates[transform_index];

				if (are_any_enum_flags_set(range_reduction, RangeReductionFlags8::Rotations) && !transform_range.is_rotation_constant)
				{
					if (mutable_database.get_rotation_format() == RotationFormat8::Quat_128)
					{
						if (out_range_data != nullptr)
						{
							const Vector4_32 range_min = transform_range.rotation_min;
							const Vector4_32 range_extent = transform_range.rotation_extent;

							pack_vector4_32(range_min, true, output_buffer);
							pack_vector4_32(range_extent, true, output_buffer + sizeof(uint8_t) * 4);
						}

						output_buffer += sizeof(uint8_t) * 4 * 2;
					}
					else
					{
						if (is_constant_bit_rate(bit_rate.rotation))
						{
							if (out_range_data != nullptr)
							{
								const Vector4_32 sample = mutable_database.get_rotation(segment, transform_index, 0);
								const uint32_t* sample_u32 = safe_ptr_cast<const uint32_t>(vector_as_float_ptr(sample));

								uint16_t* data = safe_ptr_cast<uint16_t>(output_buffer);
								data[0] = safe_static_cast<uint16_t>(sample_u32[0]);
								data[1] = safe_static_cast<uint16_t>(sample_u32[1]);
								data[2] = safe_static_cast<uint16_t>(sample_u32[2]);
							}

							output_buffer += sizeof(uint16_t) * 3;
						}
						else
						{
							if (out_range_data != nullptr)
							{
								const Vector4_32 range_min = transform_range.rotation_min;
								const Vector4_32 range_extent = transform_range.rotation_extent;

								pack_vector3_u24_unsafe(range_min, output_buffer);
								pack_vector3_u24_unsafe(range_extent, output_buffer + sizeof(uint8_t) * 3);
							}

							output_buffer += sizeof(uint8_t) * 3 * 2;
						}
					}
				}

				if (are_any_enum_flags_set(range_reduction, RangeReductionFlags8::Translations) && !transform_range.is_translation_constant)
				{
					if (is_constant_bit_rate(bit_rate.translation))
					{
						if (out_range_data != nullptr)
						{
							const Vector4_32 sample = mutable_database.get_translation(segment, transform_index, 0);
							const uint32_t* sample_u32 = safe_ptr_cast<const uint32_t>(vector_as_float_ptr(sample));

							uint16_t* data = safe_ptr_cast<uint16_t>(output_buffer);
							data[0] = safe_static_cast<uint16_t>(sample_u32[0]);
							data[1] = safe_static_cast<uint16_t>(sample_u32[1]);
							data[2] = safe_static_cast<uint16_t>(sample_u32[2]);
						}

						output_buffer += sizeof(uint16_t) * 3;
					}
					else
					{
						if (out_range_data != nullptr)
						{
							const Vector4_32 range_min = transform_range.translation_min;
							const Vector4_32 range_extent = transform_range.translation_extent;

							pack_vector3_u24_unsafe(range_min, output_buffer);
							pack_vector3_u24_unsafe(range_extent, output_buffer + sizeof(uint8_t) * 3);
						}

						output_buffer += sizeof(uint8_t) * 3 * 2;
					}
				}

				if (are_any_enum_flags_set(range_reduction, RangeReductionFlags8::Scales) && !transform_range.is_scale_constant)
				{
					if (is_constant_bit_rate(bit_rate.scale))
					{
						if (out_range_data != nullptr)
						{
							const Vector4_32 sample = mutable_database.get_scale(segment, transform_index, 0);
							const uint32_t* sample_u32 = safe_ptr_cast<const uint32_t>(vector_as_float_ptr(sample));

							uint16_t* data = safe_ptr_cast<uint16_t>(output_buffer);
							data[0] = safe_static_cast<uint16_t>(sample_u32[0]);
							data[1] = safe_static_cast<uint16_t>(sample_u32[1]);
							data[2] = safe_static_cast<uint16_t>(sample_u32[2]);
						}

						output_buffer += sizeof(uint16_t) * 3;
					}
					else
					{
						if (out_range_data != nullptr)
						{
							const Vector4_32 range_min = transform_range.scale_min;
							const Vector4_32 range_extent = transform_range.scale_extent;

							pack_vector3_u24_unsafe(range_min, output_buffer);
							pack_vector3_u24_unsafe(range_extent, output_buffer + sizeof(uint8_t) * 3);
						}

						output_buffer += sizeof(uint8_t) * 3 * 2;
					}
				}
			}

			return safe_static_cast<uint32_t>(output_buffer - output_buffer_start);
		}
	}
}

ACL_IMPL_FILE_PRAGMA_POP
