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
#include "acl/core/iallocator.h"
#include "acl/core/error.h"
#include "acl/core/compressed_clip.h"
#include "acl/compression/stream/clip_context.h"

#include <cstdint>

ACL_IMPL_FILE_PRAGMA_PUSH

namespace acl
{
	inline uint32_t get_constant_data_size(const ClipContext& clip_context, const uint16_t* output_bone_mapping, uint16_t num_output_bones)
	{
		// Only use the first segment, it contains the necessary information
		const SegmentContext& segment = clip_context.segments[0];

		uint32_t constant_data_size = 0;

		for (uint16_t output_index = 0; output_index < num_output_bones; ++output_index)
		{
			const uint16_t bone_index = output_bone_mapping[output_index];
			const BoneStreams& bone_stream = segment.bone_streams[bone_index];

			if (!bone_stream.is_rotation_default && bone_stream.is_rotation_constant)
				constant_data_size += bone_stream.rotations.get_packed_sample_size();

			if (!bone_stream.is_translation_default && bone_stream.is_translation_constant)
				constant_data_size += bone_stream.translations.get_packed_sample_size();

			if (clip_context.has_scale && !bone_stream.is_scale_default && bone_stream.is_scale_constant)
				constant_data_size += bone_stream.scales.get_packed_sample_size();
		}

		return constant_data_size;
	}

	inline void get_animated_variable_bit_rate_data_size(const TrackStream& track_stream, bool has_mixed_packing, uint32_t num_samples, uint32_t& out_num_animated_data_bits, uint32_t& out_num_animated_pose_bits)
	{
		const uint8_t bit_rate = track_stream.get_bit_rate();
		uint32_t num_bits_at_bit_rate = get_num_bits_at_bit_rate(bit_rate) * 3;	// 3 components
		if (has_mixed_packing)
			num_bits_at_bit_rate = align_to(num_bits_at_bit_rate, k_mixed_packing_alignment_num_bits);
		out_num_animated_data_bits += num_bits_at_bit_rate * num_samples;
		out_num_animated_pose_bits += num_bits_at_bit_rate;
	}

	inline void calculate_animated_data_size(const TrackStream& track_stream, bool has_mixed_packing, uint32_t& num_animated_data_bits, uint32_t& num_animated_pose_bits)
	{
		const uint32_t num_samples = track_stream.get_num_samples();

		if (track_stream.is_bit_rate_variable())
		{
			get_animated_variable_bit_rate_data_size(track_stream, has_mixed_packing, num_samples, num_animated_data_bits, num_animated_pose_bits);
		}
		else
		{
			const uint32_t sample_size = track_stream.get_packed_sample_size();
			num_animated_data_bits += sample_size * num_samples * 8;
			num_animated_pose_bits += sample_size * 8;
		}
	}

	inline void calculate_animated_data_size(ClipContext& clip_context, RotationFormat8 rotation_format, VectorFormat8 translation_format, VectorFormat8 scale_format, const uint16_t* output_bone_mapping, uint16_t num_output_bones)
	{
		// If all tracks are variable, no need for any extra padding except at the very end of the data
		// If our tracks are mixed variable/not variable, we need to add some padding to ensure alignment
		const bool is_every_format_variable = is_rotation_format_variable(rotation_format) && is_vector_format_variable(translation_format) && is_vector_format_variable(scale_format);
		const bool is_any_format_variable = is_rotation_format_variable(rotation_format) || is_vector_format_variable(translation_format) || is_vector_format_variable(scale_format);
		const bool has_mixed_packing = !is_every_format_variable && is_any_format_variable;

		for (SegmentContext& segment : clip_context.segment_iterator())
		{
			uint32_t num_animated_data_bits = 0;
			uint32_t num_animated_pose_bits = 0;

			for (uint16_t output_index = 0; output_index < num_output_bones; ++output_index)
			{
				const uint16_t bone_index = output_bone_mapping[output_index];
				const BoneStreams& bone_stream = segment.bone_streams[bone_index];

				if (!bone_stream.is_rotation_constant)
					calculate_animated_data_size(bone_stream.rotations, has_mixed_packing, num_animated_data_bits, num_animated_pose_bits);

				if (!bone_stream.is_translation_constant)
					calculate_animated_data_size(bone_stream.translations, has_mixed_packing, num_animated_data_bits, num_animated_pose_bits);

				if (!bone_stream.is_scale_constant)
					calculate_animated_data_size(bone_stream.scales, has_mixed_packing, num_animated_data_bits, num_animated_pose_bits);
			}

			segment.animated_data_size = align_to(num_animated_data_bits, 8) / 8;
			segment.animated_pose_bit_size = num_animated_pose_bits;
		}
	}

	inline uint32_t get_format_per_track_data_size(const ClipContext& clip_context, RotationFormat8 rotation_format, VectorFormat8 translation_format, VectorFormat8 scale_format)
	{
		const bool is_rotation_variable = is_rotation_format_variable(rotation_format);
		const bool is_translation_variable = is_vector_format_variable(translation_format);
		const bool is_scale_variable = is_vector_format_variable(scale_format);

		// Only use the first segment, it contains the necessary information
		const SegmentContext& segment = clip_context.segments[0];

		uint32_t format_per_track_data_size = 0;

		for (const BoneStreams& bone_stream : segment.const_bone_iterator())
		{
			if (bone_stream.is_stripped_from_output())
				continue;

			if (!bone_stream.is_rotation_constant && is_rotation_variable)
				format_per_track_data_size++;

			if (!bone_stream.is_translation_constant && is_translation_variable)
				format_per_track_data_size++;

			if (!bone_stream.is_scale_constant && is_scale_variable)
				format_per_track_data_size++;
		}

		return format_per_track_data_size;
	}

	inline void write_constant_track_data(const ClipContext& clip_context, uint8_t* constant_data, uint32_t constant_data_size, const uint16_t* output_bone_mapping, uint16_t num_output_bones)
	{
		ACL_ASSERT(constant_data != nullptr, "'constant_data' cannot be null!");
		(void)constant_data_size;

		// Only use the first segment, it contains the necessary information
		const SegmentContext& segment = clip_context.segments[0];

#if defined(ACL_HAS_ASSERT_CHECKS)
		const uint8_t* constant_data_end = add_offset_to_ptr<uint8_t>(constant_data, constant_data_size);
#endif

		for (uint16_t output_index = 0; output_index < num_output_bones; ++output_index)
		{
			const uint16_t bone_index = output_bone_mapping[output_index];
			const BoneStreams& bone_stream = segment.bone_streams[bone_index];

			if (!bone_stream.is_rotation_default && bone_stream.is_rotation_constant)
			{
				const uint8_t* rotation_ptr = bone_stream.rotations.get_raw_sample_ptr(0);
				uint32_t sample_size = bone_stream.rotations.get_sample_size();
				std::memcpy(constant_data, rotation_ptr, sample_size);
				constant_data += sample_size;
			}

			if (!bone_stream.is_translation_default && bone_stream.is_translation_constant)
			{
				const uint8_t* translation_ptr = bone_stream.translations.get_raw_sample_ptr(0);
				uint32_t sample_size = bone_stream.translations.get_sample_size();
				std::memcpy(constant_data, translation_ptr, sample_size);
				constant_data += sample_size;
			}

			if (clip_context.has_scale && !bone_stream.is_scale_default && bone_stream.is_scale_constant)
			{
				const uint8_t* scale_ptr = bone_stream.scales.get_raw_sample_ptr(0);
				uint32_t sample_size = bone_stream.scales.get_sample_size();
				std::memcpy(constant_data, scale_ptr, sample_size);
				constant_data += sample_size;
			}

			ACL_ASSERT(constant_data <= constant_data_end, "Invalid constant data offset. Wrote too much data.");
		}

		ACL_ASSERT(constant_data == constant_data_end, "Invalid constant data offset. Wrote too little data.");
	}

	inline void write_animated_track_data(const TrackStream& track_stream, uint32_t sample_index, bool has_mixed_packing, uint8_t* animated_track_data_begin, uint8_t*& out_animated_track_data, uint64_t& out_bit_offset)
	{
		if (track_stream.is_bit_rate_variable())
		{
			const uint8_t bit_rate = track_stream.get_bit_rate();
			uint64_t num_bits_at_bit_rate = get_num_bits_at_bit_rate(bit_rate) * 3;	// 3 components

			// Track is constant, our constant sample is stored in the range information
			ACL_ASSERT(!is_constant_bit_rate(bit_rate), "Cannot write constant variable track data");
			const uint8_t* raw_sample_ptr = track_stream.get_raw_sample_ptr(sample_index);

			if (is_raw_bit_rate(bit_rate))
			{
				const uint32_t* raw_sample_u32 = safe_ptr_cast<const uint32_t>(raw_sample_ptr);
				const uint32_t x = byte_swap(raw_sample_u32[0]);
				memcpy_bits(animated_track_data_begin, out_bit_offset + 0, &x, 0, 32);
				const uint32_t y = byte_swap(raw_sample_u32[1]);
				memcpy_bits(animated_track_data_begin, out_bit_offset + 32, &y, 0, 32);
				const uint32_t z = byte_swap(raw_sample_u32[2]);
				memcpy_bits(animated_track_data_begin, out_bit_offset + 64, &z, 0, 32);
			}
			else
			{
				const uint64_t raw_sample_u64 = *safe_ptr_cast<const uint64_t>(raw_sample_ptr);
				memcpy_bits(animated_track_data_begin, out_bit_offset, &raw_sample_u64, 0, num_bits_at_bit_rate);
			}

			if (has_mixed_packing)
				num_bits_at_bit_rate = align_to(num_bits_at_bit_rate, k_mixed_packing_alignment_num_bits);

			out_bit_offset += num_bits_at_bit_rate;
			out_animated_track_data = animated_track_data_begin + (out_bit_offset / 8);
		}
		else
		{
			const uint8_t* raw_sample_ptr = track_stream.get_raw_sample_ptr(sample_index);
			const uint32_t sample_size = track_stream.get_packed_sample_size();
			std::memcpy(out_animated_track_data, raw_sample_ptr, sample_size);
			out_animated_track_data += sample_size;
			out_bit_offset = (out_animated_track_data - animated_track_data_begin) * 8;
		}
	}

	inline void write_animated_track_data(const SegmentContext& segment, RotationFormat8 rotation_format, VectorFormat8 translation_format, VectorFormat8 scale_format, uint8_t* animated_track_data, uint32_t animated_data_size, const uint16_t* output_bone_mapping, uint16_t num_output_bones)
	{
		ACL_ASSERT(animated_track_data != nullptr, "'animated_track_data' cannot be null!");
		(void)animated_data_size;

		uint8_t* animated_track_data_begin = animated_track_data;

#if defined(ACL_HAS_ASSERT_CHECKS)
		const uint8_t* animated_track_data_end = add_offset_to_ptr<uint8_t>(animated_track_data, animated_data_size);
#endif

		// If all tracks are variable, no need for any extra padding except at the very end of the data
		// If our tracks are mixed variable/not variable, we need to add some padding to ensure alignment
		const bool is_every_format_variable = is_rotation_format_variable(rotation_format) && is_vector_format_variable(translation_format) && is_vector_format_variable(scale_format);
		const bool is_any_format_variable = is_rotation_format_variable(rotation_format) || is_vector_format_variable(translation_format) || is_vector_format_variable(scale_format);
		const bool has_mixed_packing = !is_every_format_variable && is_any_format_variable;

		uint64_t bit_offset = 0;

		// Data is sorted first by time, second by bone.
		// This ensures that all bones are contiguous in memory when we sample a particular time.
		for (uint32_t sample_index = 0; sample_index < segment.num_samples; ++sample_index)
		{
			for (uint16_t output_index = 0; output_index < num_output_bones; ++output_index)
			{
				const uint16_t bone_index = output_bone_mapping[output_index];
				const BoneStreams& bone_stream = segment.bone_streams[bone_index];

				if (!bone_stream.is_rotation_constant && !is_constant_bit_rate(bone_stream.rotations.get_bit_rate()))
					write_animated_track_data(bone_stream.rotations, sample_index, has_mixed_packing, animated_track_data_begin, animated_track_data, bit_offset);

				if (!bone_stream.is_translation_constant && !is_constant_bit_rate(bone_stream.translations.get_bit_rate()))
					write_animated_track_data(bone_stream.translations, sample_index, has_mixed_packing, animated_track_data_begin, animated_track_data, bit_offset);

				if (!bone_stream.is_scale_constant && !is_constant_bit_rate(bone_stream.scales.get_bit_rate()))
					write_animated_track_data(bone_stream.scales, sample_index, has_mixed_packing, animated_track_data_begin, animated_track_data, bit_offset);

				ACL_ASSERT(animated_track_data <= animated_track_data_end, "Invalid animated track data offset. Wrote too much data.");
			}
		}

		if (bit_offset != 0)
			animated_track_data = animated_track_data_begin + (align_to(bit_offset, 8) / 8);

		ACL_ASSERT(animated_track_data == animated_track_data_end, "Invalid animated track data offset. Wrote too little data.");
	}

	inline void write_format_per_track_data(const SegmentContext& segment, uint8_t* format_per_track_data, uint32_t format_per_track_data_size, const uint16_t* output_bone_mapping, uint16_t num_output_bones)
	{
		ACL_ASSERT(format_per_track_data != nullptr, "'format_per_track_data' cannot be null!");
		(void)format_per_track_data_size;

#if defined(ACL_HAS_ASSERT_CHECKS)
		const uint8_t* format_per_track_data_end = add_offset_to_ptr<uint8_t>(format_per_track_data, format_per_track_data_size);
#endif

		for (uint16_t output_index = 0; output_index < num_output_bones; ++output_index)
		{
			const uint16_t bone_index = output_bone_mapping[output_index];
			const BoneStreams& bone_stream = segment.bone_streams[bone_index];

			if (!bone_stream.is_rotation_constant && bone_stream.rotations.is_bit_rate_variable())
				*format_per_track_data++ = bone_stream.rotations.get_bit_rate();

			if (!bone_stream.is_translation_constant && bone_stream.translations.is_bit_rate_variable())
				*format_per_track_data++ = bone_stream.translations.get_bit_rate();

			if (!bone_stream.is_scale_constant && bone_stream.scales.is_bit_rate_variable())
				*format_per_track_data++ = bone_stream.scales.get_bit_rate();

			ACL_ASSERT(format_per_track_data <= format_per_track_data_end, "Invalid format per track data offset. Wrote too much data.");
		}

		ACL_ASSERT(format_per_track_data == format_per_track_data_end, "Invalid format per track data offset. Wrote too little data.");
	}
}

ACL_IMPL_FILE_PRAGMA_POP
