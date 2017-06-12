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

#include "acl/core/compressed_clip.h"
#include "acl/core/utils.h"
#include "acl/math/quat_32.h"
#include "acl/math/vector4_32.h"
#include "acl/algorithm/uniformly_sampled/common.h"
#include "acl/decompression/output_writer.h"

#include <stdint.h>

//////////////////////////////////////////////////////////////////////////
// See encoder for details
//////////////////////////////////////////////////////////////////////////

namespace acl
{
	namespace uniformly_sampled
	{
		// 2 ways to encore a track as default: a bitset or omit the track
		// the second method requires a track id to be present to distinguish the
		// remaining tracks.
		// For a character, about 50-90 tracks are animated.
		// We ideally want to support more than 255 tracks or bones.
		// 50 * 16 bits = 100 bytes
		// 90 * 16 bits = 180 bytes
		// On the other hand, a character has about 140-180 bones, or 280-360 tracks (rotation/translation only)
		// 280 * 1 bit = 35 bytes
		// 360 * 1 bit = 45 bytes
		// It is obvious that storing a bitset is much more compact
		// A bitset also allows us to process and write track values in the order defined when compressed
		// unlike the track id method which makes it impossible to know which values are default until
		// everything has been decompressed (at which point everything else is default).
		// For the track id method to be more compact, an unreasonable small number of tracks would need to be
		// animated or constant compared to the total possible number of tracks. Those are likely to be rare.

		namespace impl
		{
			struct DecompressionContext
			{
				// Read-only data
				const uint32_t* default_tracks_bitset;

				const uint32_t* constant_tracks_bitset;
				const uint8_t* constant_track_data;

				const uint8_t* key_frame_data0;
				const uint8_t* key_frame_data1;

				uint32_t bitset_size;
				uint32_t rotation_size;
				uint32_t translation_size;

				// Read-write data
				uint32_t default_track_offset;
				uint32_t constant_track_offset;
			};

			inline void initialize_context(const FullPrecisionHeader& header, uint32_t key_frame0, uint32_t key_frame1, DecompressionContext& context)
			{
				uint32_t rotation_size = get_rotation_size(header.rotation_format);
				uint32_t translation_size = get_translation_size(header.translation_format);

				// TODO: No need to store this, unpack from bitset in context and simplify branching logic below?
				uint32_t animated_pose_size = (rotation_size * header.num_animated_rotation_tracks) + (translation_size * header.num_animated_translation_tracks);

				context.default_tracks_bitset = header.get_default_tracks_bitset();

				context.constant_tracks_bitset = header.get_constant_tracks_bitset();
				context.constant_track_data = header.get_constant_track_data();

				const uint8_t* animated_track_data = header.get_track_data();
				context.key_frame_data0 = animated_track_data + (key_frame0 * animated_pose_size);
				context.key_frame_data1 = animated_track_data + (key_frame1 * animated_pose_size);

				context.bitset_size = ((header.num_bones * FullPrecisionConstants::NUM_TRACKS_PER_BONE) + FullPrecisionConstants::BITSET_WIDTH - 1) / FullPrecisionConstants::BITSET_WIDTH;
				context.rotation_size = rotation_size;
				context.translation_size = translation_size;

				context.constant_track_offset = 0;
				context.default_track_offset = 0;
			}

			inline void skip_rotation(DecompressionContext& context, RotationFormat8 rotation_format)
			{
				bool is_rotation_default = bitset_test(context.default_tracks_bitset, context.bitset_size, context.default_track_offset);
				if (!is_rotation_default)
				{
					bool is_rotation_constant = bitset_test(context.constant_tracks_bitset, context.bitset_size, context.constant_track_offset);
					if (is_rotation_constant)
					{
						context.constant_track_data += context.rotation_size;
					}
					else
					{
						context.key_frame_data0 += context.rotation_size;
						context.key_frame_data1 += context.rotation_size;
					}
				}

				context.default_track_offset++;
				context.constant_track_offset++;
			}

			inline void skip_translation(DecompressionContext& context)
			{
				bool is_translation_default = bitset_test(context.default_tracks_bitset, context.bitset_size, context.default_track_offset);
				if (!is_translation_default)
				{
					bool is_translation_constant = bitset_test(context.constant_tracks_bitset, context.bitset_size, context.constant_track_offset);
					if (is_translation_constant)
					{
						context.constant_track_data += context.translation_size;
					}
					else
					{
						context.key_frame_data0 += context.translation_size;
						context.key_frame_data1 += context.translation_size;
					}
				}

				context.default_track_offset++;
				context.constant_track_offset++;
			}

			inline Quat_32 decompress_rotation_quat_128(const uint8_t* data_ptr)
			{
				return quat_unaligned_load(data_ptr);
			}

			inline Quat_32 decompress_rotation_quat_96(const uint8_t* data_ptr)
			{
				Vector4_32 rotation_xyz = vector_unaligned_load3(data_ptr);
				return quat_from_positive_w(rotation_xyz);
			}

			inline float dequantize_unsigned_normalized(size_t input, size_t num_bits)
			{
				size_t max_value = (1 << num_bits) - 1;
				ACL_ENSURE(input <= max_value, "Invalue input value: %ull <= 1.0", input);
				return safe_to_float(input) / safe_to_float(max_value);
			}

			inline float dequantize_signed_normalized(size_t input, size_t num_bits)
			{
				return (dequantize_unsigned_normalized(input, num_bits) * 2.0f) - 1.0f;
			}

			inline Quat_32 decompress_rotation_quat_48(const uint8_t* data_ptr)
			{
				const uint16_t* data_ptr_u16 = safe_ptr_cast<const uint16_t>(data_ptr);
				size_t x = data_ptr_u16[0];
				size_t y = data_ptr_u16[1];
				size_t z = data_ptr_u16[2];
				Vector4_32 rotation_xyz = vector_set(dequantize_signed_normalized(x, 16), dequantize_signed_normalized(y, 16), dequantize_signed_normalized(z, 16));
				return quat_from_positive_w(rotation_xyz);
			}

			inline Quat_32 decompress_rotation_quat_32(const uint8_t* data_ptr)
			{
				// Read 2 bytes at a time to ensure safe alignment
				const uint16_t* data_ptr_u16 = safe_ptr_cast<const uint16_t>(data_ptr);
				uint32_t rotation_u32 = (safe_static_cast<uint32_t>(data_ptr_u16[0]) << 16) | safe_static_cast<uint32_t>(data_ptr_u16[1]);
				size_t x = rotation_u32 >> 21;
				size_t y = (rotation_u32 >> 10) & ((1 << 11) - 1);
				size_t z = rotation_u32 & ((1 << 10) - 1);
				Vector4_32 rotation_xyz = vector_set(dequantize_signed_normalized(x, 11), dequantize_signed_normalized(y, 11), dequantize_signed_normalized(z, 10));
				return quat_from_positive_w(rotation_xyz);
			}

			inline Quat_32 decompress_rotation(DecompressionContext& context, RotationFormat8 rotation_format, float interpolation_alpha)
			{
				Quat_32 rotation;
				bool is_rotation_default = bitset_test(context.default_tracks_bitset, context.bitset_size, context.default_track_offset);
				if (is_rotation_default)
				{
					rotation = quat_identity_32();
				}
				else
				{
					bool is_rotation_constant = bitset_test(context.constant_tracks_bitset, context.bitset_size, context.constant_track_offset);
					if (is_rotation_constant)
					{
						// TODO: Use a compile time flag to determine the rotation format and avoid a runtime branch
						if (rotation_format == RotationFormat8::Quat_128)
							rotation = decompress_rotation_quat_128(context.constant_track_data);
						else if (rotation_format == RotationFormat8::Quat_96)
							rotation = decompress_rotation_quat_96(context.constant_track_data);
						else if (rotation_format == RotationFormat8::Quat_48)
							rotation = decompress_rotation_quat_48(context.constant_track_data);
						else if (rotation_format == RotationFormat8::Quat_32)
							rotation = decompress_rotation_quat_32(context.constant_track_data);

						context.constant_track_data += context.rotation_size;
					}
					else
					{
						Quat_32 rotation0;
						Quat_32 rotation1;

						// TODO: Use a compile time flag to determine the rotation format and avoid a runtime branch
						if (rotation_format == RotationFormat8::Quat_128)
						{
							rotation0 = decompress_rotation_quat_128(context.key_frame_data0);
							rotation1 = decompress_rotation_quat_128(context.key_frame_data1);
						}
						else if (rotation_format == RotationFormat8::Quat_96)
						{
							rotation0 = decompress_rotation_quat_96(context.key_frame_data0);
							rotation1 = decompress_rotation_quat_96(context.key_frame_data1);
						}
						else if (rotation_format == RotationFormat8::Quat_48)
						{
							rotation0 = decompress_rotation_quat_48(context.key_frame_data0);
							rotation1 = decompress_rotation_quat_48(context.key_frame_data1);
						}
						else if (rotation_format == RotationFormat8::Quat_32)
						{
							rotation0 = decompress_rotation_quat_32(context.key_frame_data0);
							rotation1 = decompress_rotation_quat_32(context.key_frame_data1);
						}

						rotation = quat_lerp(rotation0, rotation1, interpolation_alpha);

						context.key_frame_data0 += context.rotation_size;
						context.key_frame_data1 += context.rotation_size;
					}
				}

				context.default_track_offset++;
				context.constant_track_offset++;
				return rotation;
			}

			inline Vector4_32 decompress_translation(DecompressionContext& context, RotationFormat8 rotation_format, float interpolation_alpha)
			{
				Vector4_32 translation;
				bool is_translation_default = bitset_test(context.default_tracks_bitset, context.bitset_size, context.default_track_offset);
				if (is_translation_default)
				{
					translation = vector_zero_32();
				}
				else
				{
					bool are_translations_always_aligned = rotation_format != RotationFormat8::Quat_48;
					bool is_translation_constant = bitset_test(context.constant_tracks_bitset, context.bitset_size, context.constant_track_offset);
					if (is_translation_constant)
					{
						translation = vector_unaligned_load3(context.constant_track_data);
						context.constant_track_data += context.translation_size;
					}
					else
					{
						Vector4_32 translation0 = vector_unaligned_load3(context.key_frame_data0);
						Vector4_32 translation1 = vector_unaligned_load3(context.key_frame_data1);

						translation = vector_lerp(translation0, translation1, interpolation_alpha);

						context.key_frame_data0 += context.translation_size;
						context.key_frame_data1 += context.translation_size;
					}
				}

				context.default_track_offset++;
				context.constant_track_offset++;
				return translation;
			}
		}

		template<class OutputWriterType>
		inline void decompress_pose(const CompressedClip& clip, float sample_time, OutputWriterType& writer)
		{
			using namespace impl;

			ACL_ENSURE(clip.get_algorithm_type() == AlgorithmType8::UniformlySampled, "Invalid algorithm type [%s], expected [%s]", get_algorithm_name(clip.get_algorithm_type()), get_algorithm_name(AlgorithmType8::UniformlySampled));
			ACL_ENSURE(clip.is_valid(false), "Clip is invalid");

			const FullPrecisionHeader& header = get_full_precision_header(clip);

			float clip_duration = float(header.num_samples - 1) / float(header.sample_rate);

			uint32_t key_frame0;
			uint32_t key_frame1;
			float interpolation_alpha;
			calculate_interpolation_keys(header.num_samples, clip_duration, sample_time, key_frame0, key_frame1, interpolation_alpha);

			DecompressionContext context;
			initialize_context(header, key_frame0, key_frame1, context);

			for (uint32_t bone_index = 0; bone_index < header.num_bones; ++bone_index)
			{
				Quat_32 rotation = decompress_rotation(context, header.rotation_format, interpolation_alpha);
				writer.write_bone_rotation(bone_index, rotation);

				Vector4_32 translation = decompress_translation(context, header.rotation_format, interpolation_alpha);
				writer.write_bone_translation(bone_index, translation);
			}
		}

		inline void decompress_bone(const CompressedClip& clip, float sample_time, uint16_t sample_bone_index, Quat_32* out_rotation, Vector4_32* out_translation)
		{
			using namespace impl;

			ACL_ENSURE(clip.get_algorithm_type() == AlgorithmType8::UniformlySampled, "Invalid algorithm type [%s], expected [%s]", get_algorithm_name(clip.get_algorithm_type()), get_algorithm_name(AlgorithmType8::UniformlySampled));
			ACL_ENSURE(clip.is_valid(false), "Clip is invalid");

			const FullPrecisionHeader& header = get_full_precision_header(clip);

			float clip_duration = float(header.num_samples - 1) / float(header.sample_rate);

			uint32_t key_frame0;
			uint32_t key_frame1;
			float interpolation_alpha;
			calculate_interpolation_keys(header.num_samples, clip_duration, sample_time, key_frame0, key_frame1, interpolation_alpha);

			DecompressionContext context;
			initialize_context(header, key_frame0, key_frame1, context);

			// TODO: Optimize this by counting the number of bits set, we can use the pop-count instruction on
			// architectures that support it (e.g. xb1/ps4). This would entirely avoid looping here.
			for (uint32_t bone_index = 0; bone_index < header.num_bones; ++bone_index)
			{
				if (bone_index == sample_bone_index)
					break;

				skip_rotation(context, header.rotation_format);
				skip_translation(context);
			}

			Quat_32 rotation = decompress_rotation(context, header.rotation_format, interpolation_alpha);
			if (out_rotation != nullptr)
				*out_rotation = rotation;

			Vector4_32 translation = decompress_translation(context, header.rotation_format, interpolation_alpha);
			if (out_translation != nullptr)
				*out_translation = translation;
		}
	}
}
