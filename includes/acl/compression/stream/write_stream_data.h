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
#include "acl/math/quat_64.h"
#include "acl/math/quat_packing.h"
#include "acl/math/vector4_64.h"
#include "acl/compression/stream/track_stream.h"

#include <stdint.h>

namespace acl
{
	inline uint32_t get_constant_data_size(const BoneStreams* bone_streams, uint16_t num_bones)
	{
		uint32_t constant_data_size = 0;
		for (uint16_t bone_index = 0; bone_index < num_bones; ++bone_index)
		{
			const BoneStreams& bone_stream = bone_streams[bone_index];

			if (!bone_stream.is_rotation_default && bone_stream.is_rotation_constant)
			{
				RotationFormat8 format = bone_stream.rotations.get_rotation_format();
				uint32_t sample_size = get_packed_rotation_size(format);
				constant_data_size += sample_size;
			}

			if (!bone_stream.is_translation_default && bone_stream.is_translation_constant)
			{
				VectorFormat8 format = bone_stream.translations.get_vector_format();
				uint32_t sample_size = get_packed_vector_size(format);
				constant_data_size += sample_size;
			}
		}

		return constant_data_size;
	}

	inline uint32_t get_animated_data_size(const BoneStreams* bone_streams, uint16_t num_bones, RotationFormat8 rotation_format, VectorFormat8 translation_format, uint32_t& out_animated_pose_bit_size)
	{
		// If all tracks are variable, no need for any extra padding except at the very end of the data
		// If our tracks are mixed variable/not variable, we need to add some padding to ensure alignment
		bool has_mixed_packing = is_rotation_format_variable(rotation_format) != is_vector_format_variable(translation_format);

		uint64_t num_animated_data_bits = 0;
		uint64_t num_animated_pose_bits = 0;

		for (uint16_t bone_index = 0; bone_index < num_bones; ++bone_index)
		{
			const BoneStreams& bone_stream = bone_streams[bone_index];

			if (bone_stream.is_rotation_animated())
			{
				uint32_t num_samples = bone_stream.rotations.get_num_samples();

				if (bone_stream.rotations.is_bit_rate_variable())
				{
					uint8_t bit_rate = bone_stream.rotations.get_bit_rate();
					uint64_t num_bits_at_bit_rate = get_num_bits_at_bit_rate(bit_rate) * 3;	// 3 components
					if (has_mixed_packing)
						num_bits_at_bit_rate = align_to(num_bits_at_bit_rate, MIXED_PACKING_ALIGNMENT_NUM_BITS);
					num_animated_data_bits += num_bits_at_bit_rate * num_samples;
					num_animated_pose_bits += num_bits_at_bit_rate;
				}
				else
				{
					RotationFormat8 format = bone_stream.rotations.get_rotation_format();
					uint32_t sample_size = get_packed_rotation_size(format);
					num_animated_data_bits += sample_size * num_samples * 8;
					num_animated_pose_bits += sample_size * 8;
				}
			}

			if (bone_stream.is_translation_animated())
			{
				uint32_t num_samples = bone_stream.translations.get_num_samples();

				if (bone_stream.translations.is_bit_rate_variable())
				{
					uint8_t bit_rate = bone_stream.translations.get_bit_rate();
					uint64_t num_bits_at_bit_rate = get_num_bits_at_bit_rate(bit_rate) * 3;	// 3 components
					if (has_mixed_packing)
						num_bits_at_bit_rate = align_to(num_bits_at_bit_rate, MIXED_PACKING_ALIGNMENT_NUM_BITS);
					num_animated_data_bits += num_bits_at_bit_rate * num_samples;
					num_animated_pose_bits += num_bits_at_bit_rate;
				}
				else
				{
					VectorFormat8 format = bone_stream.translations.get_vector_format();
					uint32_t sample_size = get_packed_vector_size(format);
					num_animated_data_bits += sample_size * num_samples * 8;
					num_animated_pose_bits += sample_size * 8;
				}
			}
		}

		uint32_t animated_data_size = safe_static_cast<uint32_t>(align_to(num_animated_data_bits, 8) / 8);

		out_animated_pose_bit_size = (uint32_t)num_animated_pose_bits;
		return animated_data_size;
	}

	inline uint32_t get_format_per_track_data_size(const BoneStreams* bone_streams, uint16_t num_bones)
	{
		uint32_t format_per_track_data_size = 0;

		for (uint16_t bone_index = 0; bone_index < num_bones; ++bone_index)
		{
			const BoneStreams& bone_stream = bone_streams[bone_index];

			if (bone_stream.is_rotation_animated() && bone_stream.rotations.is_bit_rate_variable())
				format_per_track_data_size++;

			if (bone_stream.is_translation_animated() && bone_stream.translations.is_bit_rate_variable())
				format_per_track_data_size++;
		}

		return format_per_track_data_size;
	}

	inline void write_constant_track_data(const BoneStreams* bone_streams, uint16_t num_bones, uint8_t* constant_data, uint32_t constant_data_size)
	{
		const uint8_t* constant_data_end = add_offset_to_ptr<uint8_t>(constant_data, constant_data_size);

		for (uint16_t bone_index = 0; bone_index < num_bones; ++bone_index)
		{
			const BoneStreams& bone_stream = bone_streams[bone_index];

			if (!bone_stream.is_rotation_default && bone_stream.is_rotation_constant)
			{
				const uint8_t* rotation_ptr = bone_stream.rotations.get_raw_sample_ptr(0);
				uint32_t sample_size = bone_stream.rotations.get_sample_size();
				memcpy(constant_data, rotation_ptr, sample_size);
				constant_data += sample_size;
			}

			if (!bone_stream.is_translation_default && bone_stream.is_translation_constant)
			{
				const uint8_t* translation_ptr = bone_stream.translations.get_raw_sample_ptr(0);
				uint32_t sample_size = bone_stream.translations.get_sample_size();
				memcpy(constant_data, translation_ptr, sample_size);
				constant_data += sample_size;
			}

			ACL_ENSURE(constant_data <= constant_data_end, "Invalid constant data offset. Wrote too much data.");
		}

		ACL_ENSURE(constant_data == constant_data_end, "Invalid constant data offset. Wrote too little data.");
	}

	inline void write_animated_track_data(const BoneStreams* bone_streams, uint16_t num_bones, RotationFormat8 rotation_format, VectorFormat8 translation_format, uint8_t* animated_track_data, uint32_t animated_data_size)
	{
		uint8_t* animated_track_data_begin = animated_track_data;
		const uint8_t* animated_track_data_end = add_offset_to_ptr<uint8_t>(animated_track_data, animated_data_size);

		uint32_t num_samples = get_animated_num_samples(bone_streams, num_bones);
		ACL_ENSURE(num_samples > 1, "No samples to write!");

		// If all tracks are variable, no need for any extra padding except at the very end of the data
		// If our tracks are mixed variable/not variable, we need to add some padding to ensure alignment
		bool has_mixed_packing = is_rotation_format_variable(rotation_format) != is_vector_format_variable(translation_format);

		uint64_t bit_offset = 0;

		// Data is sorted first by time, second by bone.
		// This ensures that all bones are contiguous in memory when we sample a particular time.
		for (uint32_t sample_index = 0; sample_index < num_samples; ++sample_index)
		{
			for (uint16_t bone_index = 0; bone_index < num_bones; ++bone_index)
			{
				const BoneStreams& bone_stream = bone_streams[bone_index];

				if (bone_stream.is_rotation_animated())
				{
					const uint8_t* rotation_ptr = bone_stream.rotations.get_raw_sample_ptr(sample_index);

					if (bone_stream.rotations.is_bit_rate_variable())
					{
						uint8_t bit_rate = bone_stream.rotations.get_bit_rate();
						uint64_t num_bits_at_bit_rate = get_num_bits_at_bit_rate(bit_rate) * 3;	// 3 components
						uint64_t rotation_u64 = byte_swap(*safe_ptr_cast<const uint64_t>(rotation_ptr));
						uint64_t rotation_offset = 64 - num_bits_at_bit_rate;
						memcpy_bits(animated_track_data_begin, bit_offset, &rotation_u64, rotation_offset, num_bits_at_bit_rate);

						if (has_mixed_packing)
							num_bits_at_bit_rate = align_to(num_bits_at_bit_rate, MIXED_PACKING_ALIGNMENT_NUM_BITS);

						bit_offset += num_bits_at_bit_rate;
						animated_track_data = animated_track_data_begin + (bit_offset / 8);
					}
					else
					{
						uint32_t sample_size = bone_stream.rotations.get_sample_size();
						memcpy(animated_track_data, rotation_ptr, sample_size);
						animated_track_data += sample_size;
						bit_offset = (animated_track_data - animated_track_data_begin) * 8;
					}
				}

				if (bone_stream.is_translation_animated())
				{
					const uint8_t* translation_ptr = bone_stream.translations.get_raw_sample_ptr(sample_index);

					if (bone_stream.translations.is_bit_rate_variable())
					{
						uint8_t bit_rate = bone_stream.translations.get_bit_rate();
						uint64_t num_bits_at_bit_rate = BIT_RATE_NUM_BITS[bit_rate] * 3;	// 3 components
						uint64_t translation_u64 = byte_swap(*safe_ptr_cast<const uint64_t>(translation_ptr));
						uint64_t translation_offset = 64 - num_bits_at_bit_rate;
						memcpy_bits(animated_track_data_begin, bit_offset, &translation_u64, translation_offset, num_bits_at_bit_rate);

						if (has_mixed_packing)
							num_bits_at_bit_rate = align_to(num_bits_at_bit_rate, MIXED_PACKING_ALIGNMENT_NUM_BITS);

						bit_offset += num_bits_at_bit_rate;
						animated_track_data = animated_track_data_begin + (bit_offset / 8);
					}
					else
					{
						uint32_t sample_size = bone_stream.translations.get_sample_size();
						memcpy(animated_track_data, translation_ptr, sample_size);
						animated_track_data += sample_size;
						bit_offset = (animated_track_data - animated_track_data_begin) * 8;
					}
				}

				ACL_ENSURE(animated_track_data <= animated_track_data_end, "Invalid animated track data offset. Wrote too much data.");
			}
		}

		if (bit_offset != 0)
			animated_track_data = animated_track_data_begin + (align_to(bit_offset, 8) / 8);

		ACL_ENSURE(animated_track_data == animated_track_data_end, "Invalid animated track data offset. Wrote too little data.");
	}

	inline void write_format_per_track_data(const BoneStreams* bone_streams, uint16_t num_bones, uint8_t* format_per_track_data, uint32_t format_per_track_data_size)
	{
		const uint8_t* format_per_track_data_end = add_offset_to_ptr<uint8_t>(format_per_track_data, format_per_track_data_size);

		for (uint16_t bone_index = 0; bone_index < num_bones; ++bone_index)
		{
			const BoneStreams& bone_stream = bone_streams[bone_index];

			if (bone_stream.is_rotation_animated() && bone_stream.rotations.is_bit_rate_variable())
			{
				uint8_t bit_rate = bone_stream.rotations.get_bit_rate();
				*format_per_track_data = bit_rate;
				format_per_track_data++;
			}

			if (bone_stream.is_translation_animated() && bone_stream.translations.is_bit_rate_variable())
			{
				uint8_t bit_rate = bone_stream.translations.get_bit_rate();
				*format_per_track_data = bit_rate;
				format_per_track_data++;
			}

			ACL_ENSURE(format_per_track_data <= format_per_track_data_end, "Invalid format per track data offset. Wrote too much data.");
		}

		ACL_ENSURE(format_per_track_data == format_per_track_data_end, "Invalid format per track data offset. Wrote too little data.");
	}
}
