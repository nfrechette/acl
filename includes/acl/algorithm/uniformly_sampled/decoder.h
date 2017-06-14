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
#include "acl/math/quat_packing.h"
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

				const uint8_t* range_data;

				const uint8_t* key_frame_data0;
				const uint8_t* key_frame_data1;

				uint32_t bitset_size;
				uint32_t rotation_size;
				uint32_t translation_size;
				uint32_t range_rotation_size;
				uint32_t range_translation_size;

				float interpolation_alpha;

				// Read-write data
				uint32_t default_track_offset;
				uint32_t constant_track_offset;
			};

			template<class SettingsType>
			inline void initialize_context(const SettingsType& settings, const FullPrecisionHeader& header, float sample_time, DecompressionContext& context)
			{
				const RotationFormat8 rotation_format = settings.get_rotation_format(header.rotation_format);
				const VectorFormat8 translation_format = settings.get_translation_format(header.translation_format);

				ACL_ENSURE(rotation_format == header.rotation_format, "Statically compiled rotation format (%s) differs from the compressed rotation format (%s)!", get_rotation_format_name(rotation_format), get_rotation_format_name(header.rotation_format));
				ACL_ENSURE(settings.is_rotation_format_supported(rotation_format), "Rotation format (%s) isn't statically supported!", get_rotation_format_name(rotation_format));
				ACL_ENSURE(translation_format == header.translation_format, "Statically compiled translation format (%s) differs from the compressed translation format (%s)!", get_vector_format_name(translation_format), get_vector_format_name(header.translation_format));
				ACL_ENSURE(settings.is_translation_format_supported(translation_format), "Translation format (%s) isn't statically supported!", get_vector_format_name(translation_format));

				const uint32_t rotation_size = get_packed_rotation_size(rotation_format);
				const uint32_t translation_size = get_packed_vector_size(translation_format);
				const uint32_t range_rotation_size = get_range_reduction_rotation_size(rotation_format);
				const uint32_t range_translation_size = get_range_reduction_vector_size(translation_format);
				const bool has_clip_range_reduction = is_enum_flag_set(header.range_reduction, RangeReductionFlags8::PerClip);

				float clip_duration = float(header.num_samples - 1) / float(header.sample_rate);

				uint32_t key_frame0;
				uint32_t key_frame1;
				float interpolation_alpha;
				calculate_interpolation_keys(header.num_samples, clip_duration, sample_time, key_frame0, key_frame1, interpolation_alpha);

				// TODO: No need to store this, unpack from bitset in context and simplify branching logic below?
				uint32_t animated_pose_size = (rotation_size * header.num_animated_rotation_tracks) + (translation_size * header.num_animated_translation_tracks);

				context.default_tracks_bitset = header.get_default_tracks_bitset();

				context.constant_tracks_bitset = header.get_constant_tracks_bitset();
				context.constant_track_data = header.get_constant_track_data();

				context.range_data = header.get_clip_range_data();

				const uint8_t* animated_track_data = header.get_track_data();
				context.key_frame_data0 = animated_track_data + (key_frame0 * animated_pose_size);
				context.key_frame_data1 = animated_track_data + (key_frame1 * animated_pose_size);

				context.bitset_size = get_bitset_size(header.num_bones * FullPrecisionConstants::NUM_TRACKS_PER_BONE);
				context.rotation_size = rotation_size;
				context.translation_size = translation_size;
				context.range_rotation_size = has_clip_range_reduction && is_enum_flag_set(header.range_reduction, RangeReductionFlags8::Rotations) ? range_rotation_size : 0;
				context.range_translation_size = has_clip_range_reduction && is_enum_flag_set(header.range_reduction, RangeReductionFlags8::Translations) ? range_translation_size : 0;

				context.interpolation_alpha = interpolation_alpha;

				context.constant_track_offset = 0;
				context.default_track_offset = 0;
			}

			inline void skip_rotation(DecompressionContext& context)
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
						context.range_data += context.range_rotation_size;
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
						context.range_data += context.range_translation_size;
					}
				}

				context.default_track_offset++;
				context.constant_track_offset++;
			}

			template<class SettingsType>
			inline Quat_32 decompress_rotation(const SettingsType& settings, const FullPrecisionHeader& header, DecompressionContext& context)
			{
				Quat_32 rotation;
				bool is_rotation_default = bitset_test(context.default_tracks_bitset, context.bitset_size, context.default_track_offset);
				if (is_rotation_default)
				{
					rotation = quat_identity_32();
				}
				else
				{
					const RotationFormat8 rotation_format = settings.get_rotation_format(header.rotation_format);
					bool is_rotation_constant = bitset_test(context.constant_tracks_bitset, context.bitset_size, context.constant_track_offset);
					if (is_rotation_constant)
					{
						if (rotation_format == RotationFormat8::Quat_128 && settings.is_rotation_format_supported(RotationFormat8::Quat_128))
							rotation = unpack_quat_128(context.constant_track_data);
						else if (rotation_format == RotationFormat8::Quat_96 && settings.is_rotation_format_supported(RotationFormat8::Quat_96))
							rotation = unpack_quat_96(context.constant_track_data);
						else if (rotation_format == RotationFormat8::Quat_48 && settings.is_rotation_format_supported(RotationFormat8::Quat_48))
							rotation = unpack_quat_48(context.constant_track_data);
						else if (rotation_format == RotationFormat8::Quat_32 && settings.is_rotation_format_supported(RotationFormat8::Quat_32))
							rotation = unpack_quat_32(context.constant_track_data);

						ACL_ENSURE(quat_is_valid(rotation), "Rotation is not valid!");
						ACL_ENSURE(quat_is_normalized(rotation), "Rotation is not normalized!");

						context.constant_track_data += context.rotation_size;
					}
					else
					{
						Quat_32 rotation0;
						Quat_32 rotation1;

						if (rotation_format == RotationFormat8::Quat_128 && settings.is_rotation_format_supported(RotationFormat8::Quat_128))
						{
							Vector4_32 rotation0_xyzw = unpack_vector4_128(context.key_frame_data0);
							Vector4_32 rotation1_xyzw = unpack_vector4_128(context.key_frame_data1);

							if (is_enum_flag_set(header.range_reduction, RangeReductionFlags8::Rotations))
							{
								if (is_enum_flag_set(header.range_reduction, RangeReductionFlags8::PerClip))
								{
									Vector4_32 clip_range_min = vector_unaligned_load(context.range_data);
									Vector4_32 clip_range_extent = vector_unaligned_load(context.range_data + (context.range_rotation_size / 2));

									rotation0_xyzw = vector_mul_add(rotation0_xyzw, clip_range_extent, clip_range_min);
									rotation1_xyzw = vector_mul_add(rotation1_xyzw, clip_range_extent, clip_range_min);

									context.range_data += context.range_rotation_size;
								}
							}

							rotation0 = vector_to_quat(rotation0_xyzw);
							rotation1 = vector_to_quat(rotation1_xyzw);
						}
						else if (rotation_format == RotationFormat8::Quat_96 && settings.is_rotation_format_supported(RotationFormat8::Quat_96))
						{
							Vector4_32 rotation0_xyz = unpack_vector3_96(context.key_frame_data0);
							Vector4_32 rotation1_xyz = unpack_vector3_96(context.key_frame_data1);

							if (is_enum_flag_set(header.range_reduction, RangeReductionFlags8::Rotations))
							{
								if (is_enum_flag_set(header.range_reduction, RangeReductionFlags8::PerClip))
								{
									Vector4_32 clip_range_min = vector_unaligned_load(context.range_data);
									Vector4_32 clip_range_extent = vector_unaligned_load(context.range_data + (context.range_rotation_size / 2));

									rotation0_xyz = vector_mul_add(rotation0_xyz, clip_range_extent, clip_range_min);
									rotation1_xyz = vector_mul_add(rotation1_xyz, clip_range_extent, clip_range_min);

									context.range_data += context.range_rotation_size;
								}
							}

							rotation0 = quat_from_positive_w(rotation0_xyz);
							rotation1 = quat_from_positive_w(rotation1_xyz);
						}
						else if (rotation_format == RotationFormat8::Quat_48 && settings.is_rotation_format_supported(RotationFormat8::Quat_48))
						{
							Vector4_32 rotation0_xyz = unpack_vector3_48(context.key_frame_data0);
							Vector4_32 rotation1_xyz = unpack_vector3_48(context.key_frame_data1);

							if (is_enum_flag_set(header.range_reduction, RangeReductionFlags8::Rotations))
							{
								if (is_enum_flag_set(header.range_reduction, RangeReductionFlags8::PerClip))
								{
									Vector4_32 clip_range_min = vector_unaligned_load(context.range_data);
									Vector4_32 clip_range_extent = vector_unaligned_load(context.range_data + (context.range_rotation_size / 2));

									rotation0_xyz = vector_mul_add(rotation0_xyz, clip_range_extent, clip_range_min);
									rotation1_xyz = vector_mul_add(rotation1_xyz, clip_range_extent, clip_range_min);

									context.range_data += context.range_rotation_size;
								}
							}

							rotation0 = quat_from_positive_w(rotation0_xyz);
							rotation1 = quat_from_positive_w(rotation1_xyz);
						}
						else if (rotation_format == RotationFormat8::Quat_32 && settings.is_rotation_format_supported(RotationFormat8::Quat_32))
						{
							Vector4_32 rotation0_xyz = unpack_vector3_32<11, 11, 10>(context.key_frame_data0);
							Vector4_32 rotation1_xyz = unpack_vector3_32<11, 11, 10>(context.key_frame_data1);

							if (is_enum_flag_set(header.range_reduction, RangeReductionFlags8::Rotations))
							{
								if (is_enum_flag_set(header.range_reduction, RangeReductionFlags8::PerClip))
								{
									Vector4_32 clip_range_min = vector_unaligned_load(context.range_data);
									Vector4_32 clip_range_extent = vector_unaligned_load(context.range_data + (context.range_rotation_size / 2));

									rotation0_xyz = vector_mul_add(rotation0_xyz, clip_range_extent, clip_range_min);
									rotation1_xyz = vector_mul_add(rotation1_xyz, clip_range_extent, clip_range_min);

									context.range_data += context.range_rotation_size;
								}
							}

							rotation0 = quat_from_positive_w(rotation0_xyz);
							rotation1 = quat_from_positive_w(rotation1_xyz);
						}

						rotation = quat_lerp(rotation0, rotation1, context.interpolation_alpha);

						ACL_ENSURE(quat_is_valid(rotation), "Rotation is not valid!");
						ACL_ENSURE(quat_is_normalized(rotation), "Rotation is not normalized!");

						context.key_frame_data0 += context.rotation_size;
						context.key_frame_data1 += context.rotation_size;
					}
				}

				context.default_track_offset++;
				context.constant_track_offset++;
				return rotation;
			}

			template<class SettingsType>
			inline Vector4_32 decompress_translation(const SettingsType& settings, const FullPrecisionHeader& header, DecompressionContext& context)
			{
				Vector4_32 translation;
				bool is_translation_default = bitset_test(context.default_tracks_bitset, context.bitset_size, context.default_track_offset);
				if (is_translation_default)
				{
					translation = vector_zero_32();
				}
				else
				{
					bool is_translation_constant = bitset_test(context.constant_tracks_bitset, context.bitset_size, context.constant_track_offset);
					if (is_translation_constant)
					{
						translation = vector_unaligned_load3(context.constant_track_data);
						context.constant_track_data += context.translation_size;
					}
					else
					{
						Vector4_32 translation0 = unpack_vector3_96(context.key_frame_data0);
						Vector4_32 translation1 = unpack_vector3_96(context.key_frame_data1);

						if (is_enum_flag_set(header.range_reduction, RangeReductionFlags8::Translations))
						{
							if (is_enum_flag_set(header.range_reduction, RangeReductionFlags8::PerClip))
							{
								Vector4_32 clip_range_min = vector_unaligned_load(context.range_data);
								Vector4_32 clip_range_extent = vector_unaligned_load(context.range_data + (context.range_translation_size / 2));

								translation0 = vector_mul_add(translation0, clip_range_extent, clip_range_min);
								translation1 = vector_mul_add(translation1, clip_range_extent, clip_range_min);

								context.range_data += context.range_translation_size;
							}
						}

						translation = vector_lerp(translation0, translation1, context.interpolation_alpha);

						context.key_frame_data0 += context.translation_size;
						context.key_frame_data1 += context.translation_size;
					}
				}

				context.default_track_offset++;
				context.constant_track_offset++;
				return translation;
			}
		}

		//////////////////////////////////////////////////////////////////////////
		// Deriving from this struct and overriding these constexpr functions
		// allow you to control which code is stripped for maximum performance.
		// With these, you can:
		//    - Support only a subset of the formats and statically strip the rest
		//    - Force a single format and statically strip the rest
		//    - Decide all of this at runtime by not making the overrides constexpr
		//
		// By default, all formats are supported.
		//////////////////////////////////////////////////////////////////////////
		struct DecompressionSettings
		{
			constexpr bool is_rotation_format_supported(RotationFormat8 format) const { return true; }
			constexpr bool is_translation_format_supported(VectorFormat8 format) const { return true; }
			constexpr RotationFormat8 get_rotation_format(RotationFormat8 format) const { return format; }
			constexpr VectorFormat8 get_translation_format(VectorFormat8 format) const { return format; }
		};

		template<class SettingsType, class OutputWriterType>
		inline void decompress_pose(const SettingsType& settings, const CompressedClip& clip, float sample_time, OutputWriterType& writer)
		{
			static_assert(std::is_base_of<DecompressionSettings, SettingsType>::value, "SettingsType must derive from DecompressionSettings!");
			static_assert(std::is_base_of<OutputWriter, OutputWriterType>::value, "OutputWriterType must derive from OutputWriter!");

			using namespace impl;

			ACL_ENSURE(clip.get_algorithm_type() == AlgorithmType8::UniformlySampled, "Invalid algorithm type [%s], expected [%s]", get_algorithm_name(clip.get_algorithm_type()), get_algorithm_name(AlgorithmType8::UniformlySampled));
			ACL_ENSURE(clip.is_valid(false), "Clip is invalid");

			const FullPrecisionHeader& header = get_full_precision_header(clip);

			DecompressionContext context;
			initialize_context(settings, header, sample_time, context);

			for (uint32_t bone_index = 0; bone_index < header.num_bones; ++bone_index)
			{
				Quat_32 rotation = decompress_rotation(settings, header, context);
				writer.write_bone_rotation(bone_index, rotation);

				Vector4_32 translation = decompress_translation(settings, header, context);
				writer.write_bone_translation(bone_index, translation);
			}
		}

		template<class SettingsType>
		inline void decompress_bone(const SettingsType& settings, const CompressedClip& clip, float sample_time, uint16_t sample_bone_index, Quat_32* out_rotation, Vector4_32* out_translation)
		{
			static_assert(std::is_base_of<DecompressionSettings, SettingsType>::value, "SettingsType must derive from DecompressionSettings!");

			using namespace impl;

			ACL_ENSURE(clip.get_algorithm_type() == AlgorithmType8::UniformlySampled, "Invalid algorithm type [%s], expected [%s]", get_algorithm_name(clip.get_algorithm_type()), get_algorithm_name(AlgorithmType8::UniformlySampled));
			ACL_ENSURE(clip.is_valid(false), "Clip is invalid");

			const FullPrecisionHeader& header = get_full_precision_header(clip);

			DecompressionContext context;
			initialize_context(settings, header, sample_time, context);

			// TODO: Optimize this by counting the number of bits set, we can use the pop-count instruction on
			// architectures that support it (e.g. xb1/ps4). This would entirely avoid looping here.
			for (uint32_t bone_index = 0; bone_index < header.num_bones; ++bone_index)
			{
				if (bone_index == sample_bone_index)
					break;

				skip_rotation(context);
				skip_translation(context);
			}

			Quat_32 rotation = decompress_rotation(settings, header, context);
			if (out_rotation != nullptr)
				*out_rotation = rotation;

			Vector4_32 translation = decompress_translation(settings, header, context);
			if (out_translation != nullptr)
				*out_translation = translation;
		}
	}
}
