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
			// TODO: Add a platform define or constant for the cache line size
			static constexpr size_t CONTEXT_ALIGN_AS = 64;

			struct alignas(CONTEXT_ALIGN_AS) DecompressionContext
			{
				// Read-only data
				const SegmentHeader* segment_headers;

				const uint32_t* constant_tracks_bitset;
				const uint8_t* constant_track_data;
				const uint32_t* default_tracks_bitset;

				const uint8_t* format_per_track_data0;
				const uint8_t* format_per_track_data1;
				const uint8_t* range_data;

				const uint8_t* animated_track_data0;
				const uint8_t* animated_track_data1;

				uint32_t bitset_size;
				uint32_t range_rotation_size;
				uint32_t range_translation_size;

				float clip_duration;

				bool has_mixed_packing;

				// Read-write data
				alignas(CONTEXT_ALIGN_AS) uint32_t constant_track_offset;
				uint32_t constant_track_data_offset;
				uint32_t default_track_offset;
				uint32_t format_per_track_data_offset;
				uint32_t range_data_offset;
				uint32_t key_frame_byte_offset0;
				uint32_t key_frame_byte_offset1;
				uint32_t key_frame_bit_offset0;
				uint32_t key_frame_bit_offset1;
				float interpolation_alpha;
			};

			template<class SettingsType>
			inline void initialize_context(const SettingsType& settings, const Header& header, DecompressionContext& context)
			{
				const RotationFormat8 rotation_format = settings.get_rotation_format(header.rotation_format);
				const VectorFormat8 translation_format = settings.get_translation_format(header.translation_format);
				const RangeReductionFlags8 range_reduction = settings.get_range_reduction(header.range_reduction);

#if defined(ACL_USE_ERROR_CHECKS)
				ACL_ENSURE(rotation_format == header.rotation_format, "Statically compiled rotation format (%s) differs from the compressed rotation format (%s)!", get_rotation_format_name(rotation_format), get_rotation_format_name(header.rotation_format));
				ACL_ENSURE(settings.is_rotation_format_supported(rotation_format), "Rotation format (%s) isn't statically supported!", get_rotation_format_name(rotation_format));
				ACL_ENSURE(translation_format == header.translation_format, "Statically compiled translation format (%s) differs from the compressed translation format (%s)!", get_vector_format_name(translation_format), get_vector_format_name(header.translation_format));
				ACL_ENSURE(settings.is_translation_format_supported(translation_format), "Translation format (%s) isn't statically supported!", get_vector_format_name(translation_format));
				ACL_ENSURE(range_reduction == header.range_reduction, "Statically compiled range reduction settings (%u) differ from the compressed settings (%u)!", range_reduction, header.range_reduction);
				ACL_ENSURE(settings.are_range_reduction_flags_supported(range_reduction), "Range reduction settings (%u) aren't statically supported!", range_reduction);
				if (is_rotation_format_variable(rotation_format))
				{
					RotationFormat8 highest_bit_rate_format = get_highest_variant_precision(get_rotation_variant(rotation_format));
					ACL_ENSURE(settings.is_rotation_format_supported(highest_bit_rate_format), "Variable rotation format requires the highest bit rate to be supported: %s", get_rotation_format_name(highest_bit_rate_format));
				}
				if (is_vector_format_variable(translation_format))
				{
					ACL_ENSURE(settings.is_translation_format_supported(VectorFormat8::Vector3_96), "Variable translation format requires the highest bit rate to be supported: %s", get_vector_format_name(VectorFormat8::Vector3_96));
				}
#endif

				const uint32_t range_rotation_size = get_range_reduction_rotation_size(rotation_format);
				const uint32_t range_translation_size = get_range_reduction_vector_size(translation_format);
				const bool has_clip_range_reduction = is_enum_flag_set(range_reduction, RangeReductionFlags8::PerClip);

				context.clip_duration = float(header.num_samples - 1) / float(header.sample_rate);
				context.segment_headers = header.get_segment_headers();
				context.default_tracks_bitset = header.get_default_tracks_bitset();

				context.constant_tracks_bitset = header.get_constant_tracks_bitset();
				context.constant_track_data = header.get_constant_track_data();

				context.format_per_track_data0 = nullptr;
				context.format_per_track_data1 = nullptr;
				context.range_data = header.get_clip_range_data();

				context.animated_track_data0 = nullptr;
				context.animated_track_data1 = nullptr;

				context.bitset_size = get_bitset_size(header.num_bones * Constants::NUM_TRACKS_PER_BONE);
				context.range_rotation_size = has_clip_range_reduction && is_enum_flag_set(range_reduction, RangeReductionFlags8::Rotations) ? range_rotation_size : 0;
				context.range_translation_size = has_clip_range_reduction && is_enum_flag_set(range_reduction, RangeReductionFlags8::Translations) ? range_translation_size : 0;

				// If all tracks are variable, no need for any extra padding except at the very end of the data
				// If our tracks are mixed variable/not variable, we need to add some padding to ensure alignment
				context.has_mixed_packing = is_rotation_format_variable(rotation_format) != is_vector_format_variable(translation_format);
			}

			template<class SettingsType>
			inline void seek(const SettingsType& settings, const Header& header, float sample_time, DecompressionContext& context)
			{
				context.constant_track_offset = 0;
				context.constant_track_data_offset = 0;
				context.default_track_offset = 0;
				context.format_per_track_data_offset = 0;
				context.range_data_offset = 0;

				uint32_t key_frame0;
				uint32_t key_frame1;
				calculate_interpolation_keys(header.num_samples, context.clip_duration, sample_time, key_frame0, key_frame1, context.interpolation_alpha);

				uint32_t segment_key_frame0;
				uint32_t segment_key_frame1;

				// Find segments
				// TODO: Use binary search?
				uint32_t segment_key_frame = 0;
				const SegmentHeader* segment_header0;
				const SegmentHeader* segment_header1;
				for (uint16_t segment_index = 0; segment_index < header.num_segments; ++segment_index)
				{
					const SegmentHeader& segment_header = context.segment_headers[segment_index];

					if (key_frame0 >= segment_key_frame && key_frame0 < segment_key_frame + segment_header.num_samples)
					{
						segment_header0 = &segment_header;
						segment_key_frame0 = key_frame0 - segment_key_frame;

						if (key_frame1 >= segment_key_frame && key_frame1 < segment_key_frame + segment_header.num_samples)
						{
							segment_header1 = &segment_header;
							segment_key_frame1 = key_frame1 - segment_key_frame;
						}
						else
						{
							ACL_ENSURE(segment_index + 1 < header.num_segments, "Invalid segment index: %u", segment_index + 1);
							const SegmentHeader& next_segment_header = context.segment_headers[segment_index + 1];
							segment_header1 = &next_segment_header;
							segment_key_frame1 = key_frame1 - (segment_key_frame + segment_header.num_samples);
						}

						break;
					}

					segment_key_frame += segment_header.num_samples;
				}

				context.format_per_track_data0 = header.get_format_per_track_data(*segment_header0);
				context.format_per_track_data1 = header.get_format_per_track_data(*segment_header1);
				context.animated_track_data0 = header.get_track_data(*segment_header0);
				context.animated_track_data1 = header.get_track_data(*segment_header1);

				context.key_frame_byte_offset0 = (segment_key_frame0 * segment_header0->animated_pose_bit_size) / 8;
				context.key_frame_byte_offset1 = (segment_key_frame1 * segment_header1->animated_pose_bit_size) / 8;
				context.key_frame_bit_offset0 = segment_key_frame0 * segment_header0->animated_pose_bit_size;
				context.key_frame_bit_offset1 = segment_key_frame1 * segment_header1->animated_pose_bit_size;
			}

			template<class SettingsType>
			inline void skip_rotation(const SettingsType& settings, const Header& header, DecompressionContext& context)
			{
				bool is_rotation_default = bitset_test(context.default_tracks_bitset, context.bitset_size, context.default_track_offset);
				if (!is_rotation_default)
				{
					const RotationFormat8 rotation_format = settings.get_rotation_format(header.rotation_format);

					bool is_rotation_constant = bitset_test(context.constant_tracks_bitset, context.bitset_size, context.constant_track_offset);
					if (is_rotation_constant)
					{
						const RotationFormat8 packed_format = is_rotation_format_variable(rotation_format) ? get_highest_variant_precision(get_rotation_variant(rotation_format)) : rotation_format;
						context.constant_track_data_offset += get_packed_rotation_size(packed_format);
					}
					else
					{
						if (is_rotation_format_variable(rotation_format))
						{
							uint8_t bit_rate0 = context.format_per_track_data0[context.format_per_track_data_offset];
							uint8_t bit_rate1 = context.format_per_track_data1[context.format_per_track_data_offset++];
							uint8_t num_bits_at_bit_rate0 = get_num_bits_at_bit_rate(bit_rate0) * 3;	// 3 components
							uint8_t num_bits_at_bit_rate1 = get_num_bits_at_bit_rate(bit_rate1) * 3;	// 3 components

							if (settings.supports_mixed_packing() && context.has_mixed_packing)
							{
								num_bits_at_bit_rate0 = align_to(num_bits_at_bit_rate0, MIXED_PACKING_ALIGNMENT_NUM_BITS);
								num_bits_at_bit_rate1 = align_to(num_bits_at_bit_rate1, MIXED_PACKING_ALIGNMENT_NUM_BITS);
							}

							context.key_frame_bit_offset0 += num_bits_at_bit_rate0;
							context.key_frame_bit_offset1 += num_bits_at_bit_rate1;

							if (settings.supports_mixed_packing() && context.has_mixed_packing)
							{
								context.key_frame_byte_offset0 = context.key_frame_bit_offset0 / 8;
								context.key_frame_byte_offset1 = context.key_frame_bit_offset1 / 8;
							}
						}
						else
						{
							uint32_t rotation_size = get_packed_rotation_size(rotation_format);
							context.key_frame_byte_offset0 += rotation_size;
							context.key_frame_byte_offset1 += rotation_size;

							if (settings.supports_mixed_packing() && context.has_mixed_packing)
							{
								context.key_frame_bit_offset0 = context.key_frame_byte_offset0 * 8;
								context.key_frame_bit_offset1 = context.key_frame_byte_offset1 * 8;
							}
						}

						context.range_data_offset += context.range_rotation_size;
					}
				}

				context.default_track_offset++;
				context.constant_track_offset++;
			}

			template<class SettingsType>
			inline void skip_translation(const SettingsType& settings, const Header& header, DecompressionContext& context)
			{
				bool is_translation_default = bitset_test(context.default_tracks_bitset, context.bitset_size, context.default_track_offset);
				if (!is_translation_default)
				{
					bool is_translation_constant = bitset_test(context.constant_tracks_bitset, context.bitset_size, context.constant_track_offset);
					if (is_translation_constant)
					{
						// Constant translation tracks store the remaining sample with full precision
						context.constant_track_data_offset += get_packed_vector_size(VectorFormat8::Vector3_96);
					}
					else
					{
						const VectorFormat8 translation_format = settings.get_translation_format(header.translation_format);

						if (is_vector_format_variable(translation_format))
						{
							uint8_t bit_rate0 = context.format_per_track_data0[context.format_per_track_data_offset];
							uint8_t bit_rate1 = context.format_per_track_data1[context.format_per_track_data_offset++];
							uint8_t num_bits_at_bit_rate0 = get_num_bits_at_bit_rate(bit_rate0) * 3;	// 3 components
							uint8_t num_bits_at_bit_rate1 = get_num_bits_at_bit_rate(bit_rate1) * 3;	// 3 components

							if (settings.supports_mixed_packing() && context.has_mixed_packing)
							{
								num_bits_at_bit_rate0 = align_to(num_bits_at_bit_rate0, MIXED_PACKING_ALIGNMENT_NUM_BITS);
								num_bits_at_bit_rate1 = align_to(num_bits_at_bit_rate1, MIXED_PACKING_ALIGNMENT_NUM_BITS);
							}

							context.key_frame_bit_offset0 += num_bits_at_bit_rate0;
							context.key_frame_bit_offset1 += num_bits_at_bit_rate1;

							if (settings.supports_mixed_packing() && context.has_mixed_packing)
							{
								context.key_frame_byte_offset0 = context.key_frame_bit_offset0 / 8;
								context.key_frame_byte_offset1 = context.key_frame_bit_offset1 / 8;
							}
						}
						else
						{
							uint32_t translation_size = get_packed_vector_size(translation_format);
							context.key_frame_byte_offset0 += translation_size;
							context.key_frame_byte_offset1 += translation_size;

							if (settings.supports_mixed_packing() && context.has_mixed_packing)
							{
								context.key_frame_bit_offset0 = context.key_frame_byte_offset0 * 8;
								context.key_frame_bit_offset1 = context.key_frame_byte_offset1 * 8;
							}
						}

						context.range_data_offset += context.range_translation_size;
					}
				}

				context.default_track_offset++;
				context.constant_track_offset++;
			}

			template<class SettingsType>
			inline Quat_32 decompress_rotation(const SettingsType& settings, const Header& header, DecompressionContext& context)
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
						const RotationFormat8 packed_format = is_rotation_format_variable(rotation_format) ? get_highest_variant_precision(get_rotation_variant(rotation_format)) : rotation_format;

						if (packed_format == RotationFormat8::Quat_128 && settings.is_rotation_format_supported(RotationFormat8::Quat_128))
							rotation = unpack_quat_128(context.constant_track_data + context.constant_track_data_offset);
						else if (packed_format == RotationFormat8::QuatDropW_96 && settings.is_rotation_format_supported(RotationFormat8::QuatDropW_96))
							rotation = unpack_quat_96(context.constant_track_data + context.constant_track_data_offset);
						else if (packed_format == RotationFormat8::QuatDropW_48 && settings.is_rotation_format_supported(RotationFormat8::QuatDropW_48))
							rotation = unpack_quat_48(context.constant_track_data + context.constant_track_data_offset);
						else if (packed_format == RotationFormat8::QuatDropW_32 && settings.is_rotation_format_supported(RotationFormat8::QuatDropW_32))
							rotation = unpack_quat_32(context.constant_track_data + context.constant_track_data_offset);

						ACL_ENSURE(quat_is_finite(rotation), "Rotation is not valid!");
						ACL_ENSURE(quat_is_normalized(rotation), "Rotation is not normalized!");

						context.constant_track_data_offset += get_packed_rotation_size(packed_format);
					}
					else
					{
						const RangeReductionFlags8 range_reduction = settings.get_range_reduction(header.range_reduction);
						const bool are_rotations_normalized = are_enum_flags_set(range_reduction, RangeReductionFlags8::PerClip | RangeReductionFlags8::Rotations);

						Quat_32 rotation0;
						Quat_32 rotation1;

						if (rotation_format == RotationFormat8::Quat_128 && settings.is_rotation_format_supported(RotationFormat8::Quat_128))
						{
							Vector4_32 rotation0_xyzw = unpack_vector4_128(context.animated_track_data0 + context.key_frame_byte_offset0);
							Vector4_32 rotation1_xyzw = unpack_vector4_128(context.animated_track_data1 + context.key_frame_byte_offset1);

							if (are_enum_flags_set(range_reduction, RangeReductionFlags8::PerClip | RangeReductionFlags8::Rotations))
							{
								Vector4_32 clip_range_min = vector_unaligned_load(context.range_data + context.range_data_offset);
								Vector4_32 clip_range_extent = vector_unaligned_load(context.range_data + context.range_data_offset + (context.range_rotation_size / 2));

								rotation0_xyzw = vector_mul_add(rotation0_xyzw, clip_range_extent, clip_range_min);
								rotation1_xyzw = vector_mul_add(rotation1_xyzw, clip_range_extent, clip_range_min);

								context.range_data_offset += context.range_rotation_size;
							}

							rotation0 = vector_to_quat(rotation0_xyzw);
							rotation1 = vector_to_quat(rotation1_xyzw);

							const uint32_t rotation_size = get_packed_rotation_size(rotation_format);
							context.key_frame_byte_offset0 += rotation_size;
							context.key_frame_byte_offset1 += rotation_size;

							if (settings.supports_mixed_packing() && context.has_mixed_packing)
							{
								context.key_frame_bit_offset0 = context.key_frame_byte_offset0 * 8;
								context.key_frame_bit_offset1 = context.key_frame_byte_offset1 * 8;
							}
						}
						else if (rotation_format == RotationFormat8::QuatDropW_96 && settings.is_rotation_format_supported(RotationFormat8::QuatDropW_96))
						{
							Vector4_32 rotation0_xyz = unpack_vector3_96(context.animated_track_data0 + context.key_frame_byte_offset0);
							Vector4_32 rotation1_xyz = unpack_vector3_96(context.animated_track_data1 + context.key_frame_byte_offset1);

							if (are_enum_flags_set(range_reduction, RangeReductionFlags8::PerClip | RangeReductionFlags8::Rotations))
							{
								Vector4_32 clip_range_min = vector_unaligned_load(context.range_data + context.range_data_offset);
								Vector4_32 clip_range_extent = vector_unaligned_load(context.range_data + context.range_data_offset + (context.range_rotation_size / 2));

								rotation0_xyz = vector_mul_add(rotation0_xyz, clip_range_extent, clip_range_min);
								rotation1_xyz = vector_mul_add(rotation1_xyz, clip_range_extent, clip_range_min);

								context.range_data_offset += context.range_rotation_size;
							}

							rotation0 = quat_from_positive_w(rotation0_xyz);
							rotation1 = quat_from_positive_w(rotation1_xyz);

							const uint32_t rotation_size = get_packed_rotation_size(rotation_format);
							context.key_frame_byte_offset0 += rotation_size;
							context.key_frame_byte_offset1 += rotation_size;

							if (settings.supports_mixed_packing() && context.has_mixed_packing)
							{
								context.key_frame_bit_offset0 = context.key_frame_byte_offset0 * 8;
								context.key_frame_bit_offset1 = context.key_frame_byte_offset1 * 8;
							}
						}
						else if (rotation_format == RotationFormat8::QuatDropW_48 && settings.is_rotation_format_supported(RotationFormat8::QuatDropW_48))
						{
							Vector4_32 rotation0_xyz = unpack_vector3_48(context.animated_track_data0 + context.key_frame_byte_offset0, are_rotations_normalized);
							Vector4_32 rotation1_xyz = unpack_vector3_48(context.animated_track_data1 + context.key_frame_byte_offset1, are_rotations_normalized);

							if (are_enum_flags_set(range_reduction, RangeReductionFlags8::PerClip | RangeReductionFlags8::Rotations))
							{
								Vector4_32 clip_range_min = vector_unaligned_load(context.range_data + context.range_data_offset);
								Vector4_32 clip_range_extent = vector_unaligned_load(context.range_data + context.range_data_offset + (context.range_rotation_size / 2));

								rotation0_xyz = vector_mul_add(rotation0_xyz, clip_range_extent, clip_range_min);
								rotation1_xyz = vector_mul_add(rotation1_xyz, clip_range_extent, clip_range_min);

								context.range_data_offset += context.range_rotation_size;
							}

							rotation0 = quat_from_positive_w(rotation0_xyz);
							rotation1 = quat_from_positive_w(rotation1_xyz);

							const uint32_t rotation_size = get_packed_rotation_size(rotation_format);
							context.key_frame_byte_offset0 += rotation_size;
							context.key_frame_byte_offset1 += rotation_size;

							if (settings.supports_mixed_packing() && context.has_mixed_packing)
							{
								context.key_frame_bit_offset0 = context.key_frame_byte_offset0 * 8;
								context.key_frame_bit_offset1 = context.key_frame_byte_offset1 * 8;
							}
						}
						else if (rotation_format == RotationFormat8::QuatDropW_32 && settings.is_rotation_format_supported(RotationFormat8::QuatDropW_32))
						{
							Vector4_32 rotation0_xyz = unpack_vector3_32(11, 11, 10, are_rotations_normalized, context.animated_track_data0 + context.key_frame_byte_offset0);
							Vector4_32 rotation1_xyz = unpack_vector3_32(11, 11, 10, are_rotations_normalized, context.animated_track_data1 + context.key_frame_byte_offset1);

							if (are_rotations_normalized)
							{
								Vector4_32 clip_range_min = vector_unaligned_load(context.range_data + context.range_data_offset);
								Vector4_32 clip_range_extent = vector_unaligned_load(context.range_data + context.range_data_offset + (context.range_rotation_size / 2));

								rotation0_xyz = vector_mul_add(rotation0_xyz, clip_range_extent, clip_range_min);
								rotation1_xyz = vector_mul_add(rotation1_xyz, clip_range_extent, clip_range_min);

								context.range_data_offset += context.range_rotation_size;
							}

							rotation0 = quat_from_positive_w(rotation0_xyz);
							rotation1 = quat_from_positive_w(rotation1_xyz);

							const uint32_t rotation_size = get_packed_rotation_size(rotation_format);
							context.key_frame_byte_offset0 += rotation_size;
							context.key_frame_byte_offset1 += rotation_size;

							if (settings.supports_mixed_packing() && context.has_mixed_packing)
							{
								context.key_frame_bit_offset0 = context.key_frame_byte_offset0 * 8;
								context.key_frame_bit_offset1 = context.key_frame_byte_offset1 * 8;
							}
						}
						else if (rotation_format == RotationFormat8::QuatDropW_Variable && settings.is_rotation_format_supported(RotationFormat8::QuatDropW_Variable))
						{
							uint8_t bit_rate0 = context.format_per_track_data0[context.format_per_track_data_offset];
							uint8_t bit_rate1 = context.format_per_track_data1[context.format_per_track_data_offset++];
							uint8_t num_bits_at_bit_rate0 = get_num_bits_at_bit_rate(bit_rate0);
							uint8_t num_bits_at_bit_rate1 = get_num_bits_at_bit_rate(bit_rate1);

							Vector4_32 rotation0_xyz;
							Vector4_32 rotation1_xyz;

							if (is_pack_72_bit_rate(bit_rate0))
								rotation0_xyz = unpack_vector3_72(are_rotations_normalized, context.animated_track_data0, context.key_frame_bit_offset0);
							else if (is_pack_96_bit_rate(bit_rate0))
								rotation0_xyz = unpack_vector3_96(context.animated_track_data0, context.key_frame_bit_offset0);
							else
								rotation0_xyz = unpack_vector3_n(num_bits_at_bit_rate0, num_bits_at_bit_rate0, num_bits_at_bit_rate0, are_rotations_normalized, context.animated_track_data0, context.key_frame_bit_offset0);

							if (is_pack_72_bit_rate(bit_rate1))
								rotation1_xyz = unpack_vector3_72(are_rotations_normalized, context.animated_track_data1, context.key_frame_bit_offset1);
							else if (is_pack_96_bit_rate(bit_rate1))
								rotation1_xyz = unpack_vector3_96(context.animated_track_data1, context.key_frame_bit_offset1);
							else
								rotation1_xyz = unpack_vector3_n(num_bits_at_bit_rate1, num_bits_at_bit_rate1, num_bits_at_bit_rate1, are_rotations_normalized, context.animated_track_data1, context.key_frame_bit_offset1);

							if (are_rotations_normalized)
							{
								Vector4_32 clip_range_min = vector_unaligned_load(context.range_data + context.range_data_offset);
								Vector4_32 clip_range_extent = vector_unaligned_load(context.range_data + context.range_data_offset + (context.range_rotation_size / 2));

								rotation0_xyz = vector_mul_add(rotation0_xyz, clip_range_extent, clip_range_min);
								rotation1_xyz = vector_mul_add(rotation1_xyz, clip_range_extent, clip_range_min);

								context.range_data_offset += context.range_rotation_size;
							}

							rotation0 = quat_from_positive_w(rotation0_xyz);
							rotation1 = quat_from_positive_w(rotation1_xyz);

							uint8_t num_bits_read0 = num_bits_at_bit_rate0 * 3;
							uint8_t num_bits_read1 = num_bits_at_bit_rate1 * 3;
							if (settings.supports_mixed_packing() && context.has_mixed_packing)
							{
								num_bits_read0 = align_to(num_bits_read0, MIXED_PACKING_ALIGNMENT_NUM_BITS);
								num_bits_read1 = align_to(num_bits_read1, MIXED_PACKING_ALIGNMENT_NUM_BITS);
							}

							context.key_frame_bit_offset0 += num_bits_read0;
							context.key_frame_bit_offset1 += num_bits_read1;

							if (settings.supports_mixed_packing() && context.has_mixed_packing)
							{
								context.key_frame_byte_offset0 = context.key_frame_bit_offset0 / 8;
								context.key_frame_byte_offset1 = context.key_frame_bit_offset1 / 8;
							}
						}

						rotation = quat_lerp(rotation0, rotation1, context.interpolation_alpha);

						ACL_ENSURE(quat_is_finite(rotation), "Rotation is not valid!");
						ACL_ENSURE(quat_is_normalized(rotation), "Rotation is not normalized!");
					}
				}

				context.default_track_offset++;
				context.constant_track_offset++;
				return rotation;
			}

			template<class SettingsType>
			inline Vector4_32 decompress_translation(const SettingsType& settings, const Header& header, DecompressionContext& context)
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
						// Constant translation tracks store the remaining sample with full precision
						translation = unpack_vector3_96(context.constant_track_data + context.constant_track_data_offset);

						ACL_ENSURE(vector_is_finite3(translation), "Translation is not valid!");

						context.constant_track_data_offset += get_packed_vector_size(VectorFormat8::Vector3_96);
					}
					else
					{
						const VectorFormat8 translation_format = settings.get_translation_format(header.translation_format);
						const RangeReductionFlags8 range_reduction = settings.get_range_reduction(header.range_reduction);

						Vector4_32 translation0;
						Vector4_32 translation1;

						if (translation_format == VectorFormat8::Vector3_96 && settings.is_translation_format_supported(VectorFormat8::Vector3_96))
						{
							translation0 = unpack_vector3_96(context.animated_track_data0 + context.key_frame_byte_offset0);
							translation1 = unpack_vector3_96(context.animated_track_data1 + context.key_frame_byte_offset1);

							const uint32_t translation_size = get_packed_vector_size(translation_format);
							context.key_frame_byte_offset0 += translation_size;
							context.key_frame_byte_offset1 += translation_size;

							if (settings.supports_mixed_packing() && context.has_mixed_packing)
							{
								context.key_frame_bit_offset0 = context.key_frame_byte_offset0 * 8;
								context.key_frame_bit_offset1 = context.key_frame_byte_offset1 * 8;
							}
						}
						else if (translation_format == VectorFormat8::Vector3_48 && settings.is_translation_format_supported(VectorFormat8::Vector3_48))
						{
							translation0 = unpack_vector3_48(context.animated_track_data0 + context.key_frame_byte_offset0, true);
							translation1 = unpack_vector3_48(context.animated_track_data1 + context.key_frame_byte_offset1, true);

							const uint32_t translation_size = get_packed_vector_size(translation_format);
							context.key_frame_byte_offset0 += translation_size;
							context.key_frame_byte_offset1 += translation_size;

							if (settings.supports_mixed_packing() && context.has_mixed_packing)
							{
								context.key_frame_bit_offset0 = context.key_frame_byte_offset0 * 8;
								context.key_frame_bit_offset1 = context.key_frame_byte_offset1 * 8;
							}
						}
						else if (translation_format == VectorFormat8::Vector3_32 && settings.is_translation_format_supported(VectorFormat8::Vector3_32))
						{
							translation0 = unpack_vector3_32(11, 11, 10, true, context.animated_track_data0 + context.key_frame_byte_offset0);
							translation1 = unpack_vector3_32(11, 11, 10, true, context.animated_track_data1 + context.key_frame_byte_offset1);

							const uint32_t translation_size = get_packed_vector_size(translation_format);
							context.key_frame_byte_offset0 += translation_size;
							context.key_frame_byte_offset1 += translation_size;

							if (settings.supports_mixed_packing() && context.has_mixed_packing)
							{
								context.key_frame_bit_offset0 = context.key_frame_byte_offset0 * 8;
								context.key_frame_bit_offset1 = context.key_frame_byte_offset1 * 8;
							}
						}
						else if (translation_format == VectorFormat8::Vector3_Variable && settings.is_translation_format_supported(VectorFormat8::Vector3_Variable))
						{
							uint8_t bit_rate0 = context.format_per_track_data0[context.format_per_track_data_offset];
							uint8_t bit_rate1 = context.format_per_track_data1[context.format_per_track_data_offset++];
							uint8_t num_bits_at_bit_rate0 = get_num_bits_at_bit_rate(bit_rate0);
							uint8_t num_bits_at_bit_rate1 = get_num_bits_at_bit_rate(bit_rate1);

							if (is_pack_72_bit_rate(bit_rate0))
								translation0 = unpack_vector3_72(true, context.animated_track_data0, context.key_frame_bit_offset0);
							else if (is_pack_96_bit_rate(bit_rate0))
								translation0 = unpack_vector3_96(context.animated_track_data0, context.key_frame_bit_offset0);
							else
								translation0 = unpack_vector3_n(num_bits_at_bit_rate0, num_bits_at_bit_rate0, num_bits_at_bit_rate0, true, context.animated_track_data0, context.key_frame_bit_offset0);

							if (is_pack_72_bit_rate(bit_rate1))
								translation1 = unpack_vector3_72(true, context.animated_track_data1, context.key_frame_bit_offset1);
							else if (is_pack_96_bit_rate(bit_rate1))
								translation1 = unpack_vector3_96(context.animated_track_data1, context.key_frame_bit_offset1);
							else
								translation1 = unpack_vector3_n(num_bits_at_bit_rate1, num_bits_at_bit_rate1, num_bits_at_bit_rate1, true, context.animated_track_data1, context.key_frame_bit_offset1);

							uint8_t num_bits_read0 = num_bits_at_bit_rate0 * 3;
							uint8_t num_bits_read1 = num_bits_at_bit_rate1 * 3;
							if (settings.supports_mixed_packing() && context.has_mixed_packing)
							{
								num_bits_read0 = align_to(num_bits_read0, MIXED_PACKING_ALIGNMENT_NUM_BITS);
								num_bits_read1 = align_to(num_bits_read1, MIXED_PACKING_ALIGNMENT_NUM_BITS);
							}

							context.key_frame_bit_offset0 += num_bits_read0;
							context.key_frame_bit_offset1 += num_bits_read1;

							if (settings.supports_mixed_packing() && context.has_mixed_packing)
							{
								context.key_frame_byte_offset0 = context.key_frame_bit_offset0 / 8;
								context.key_frame_byte_offset1 = context.key_frame_bit_offset1 / 8;
							}
						}

						if (are_enum_flags_set(range_reduction, RangeReductionFlags8::PerClip | RangeReductionFlags8::Translations))
						{
							Vector4_32 clip_range_min = vector_unaligned_load(context.range_data + context.range_data_offset);
							Vector4_32 clip_range_extent = vector_unaligned_load(context.range_data + context.range_data_offset + (context.range_translation_size / 2));

							translation0 = vector_mul_add(translation0, clip_range_extent, clip_range_min);
							translation1 = vector_mul_add(translation1, clip_range_extent, clip_range_min);

							context.range_data_offset += context.range_translation_size;
						}

						translation = vector_lerp(translation0, translation1, context.interpolation_alpha);

						ACL_ENSURE(vector_is_finite3(translation), "Translation is not valid!");
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

			constexpr bool are_range_reduction_flags_supported(RangeReductionFlags8 flags) const { return true; }
			constexpr RangeReductionFlags8 get_range_reduction(RangeReductionFlags8 flags) const { return flags; }

			// Whether tracks must all be variable or all fixed width, or if they can be mixed and require padding
			constexpr bool supports_mixed_packing() const { return true; }
		};

		template<class SettingsType>
		inline void* allocate_decompression_context(Allocator& allocator, const SettingsType& settings, const CompressedClip& clip)
		{
			using namespace impl;

			DecompressionContext* context = allocate_type<DecompressionContext>(allocator);

			ACL_ASSERT(is_aligned_to(&context->segment_headers, CONTEXT_ALIGN_AS), "Read-only decompression context is misaligned");
			ACL_ASSERT(is_aligned_to(&context->constant_track_offset, CONTEXT_ALIGN_AS), "Read-write decompression context is misaligned");

			initialize_context(settings, get_header(clip), *context);

			return context;
		}

		inline void deallocate_decompression_context(Allocator& allocator, void* opaque_context)
		{
			using namespace impl;

			DecompressionContext* context = safe_ptr_cast<DecompressionContext>(opaque_context);
			deallocate_type<DecompressionContext>(allocator, context);
		}

		template<class SettingsType, class OutputWriterType>
		inline void decompress_pose(const SettingsType& settings, const CompressedClip& clip, void* opaque_context, float sample_time, OutputWriterType& writer)
		{
			static_assert(std::is_base_of<DecompressionSettings, SettingsType>::value, "SettingsType must derive from DecompressionSettings!");
			static_assert(std::is_base_of<OutputWriter, OutputWriterType>::value, "OutputWriterType must derive from OutputWriter!");

			using namespace impl;

			ACL_ENSURE(clip.get_algorithm_type() == AlgorithmType8::UniformlySampled, "Invalid algorithm type [%s], expected [%s]", get_algorithm_name(clip.get_algorithm_type()), get_algorithm_name(AlgorithmType8::UniformlySampled));
			ACL_ENSURE(clip.is_valid(false), "Clip is invalid");

			const Header& header = get_header(clip);

			DecompressionContext& context = *safe_ptr_cast<DecompressionContext>(opaque_context);

			seek(settings, header, sample_time, context);

			for (uint32_t bone_index = 0; bone_index < header.num_bones; ++bone_index)
			{
				Quat_32 rotation = decompress_rotation(settings, header, context);
				writer.write_bone_rotation(bone_index, rotation);

				Vector4_32 translation = decompress_translation(settings, header, context);
				writer.write_bone_translation(bone_index, translation);
			}
		}

		template<class SettingsType>
		inline void decompress_bone(const SettingsType& settings, const CompressedClip& clip, void* opaque_context, float sample_time, uint16_t sample_bone_index, Quat_32* out_rotation, Vector4_32* out_translation)
		{
			static_assert(std::is_base_of<DecompressionSettings, SettingsType>::value, "SettingsType must derive from DecompressionSettings!");

			using namespace impl;

			ACL_ENSURE(clip.get_algorithm_type() == AlgorithmType8::UniformlySampled, "Invalid algorithm type [%s], expected [%s]", get_algorithm_name(clip.get_algorithm_type()), get_algorithm_name(AlgorithmType8::UniformlySampled));
			ACL_ENSURE(clip.is_valid(false), "Clip is invalid");

			const Header& header = get_header(clip);

			DecompressionContext& context = *safe_ptr_cast<DecompressionContext>(opaque_context);

			seek(settings, header, sample_time, context);

			// TODO: Optimize this by counting the number of bits set, we can use the pop-count instruction on
			// architectures that support it (e.g. xb1/ps4). This would entirely avoid looping here.
			for (uint32_t bone_index = 0; bone_index < header.num_bones; ++bone_index)
			{
				if (bone_index == sample_bone_index)
					break;

				skip_rotation(settings, header, context);
				skip_translation(settings, header, context);
			}

			// TODO: Skip if not interested in return value
			Quat_32 rotation = decompress_rotation(settings, header, context);
			if (out_rotation != nullptr)
				*out_rotation = rotation;

			Vector4_32 translation = decompress_translation(settings, header, context);
			if (out_translation != nullptr)
				*out_translation = translation;
		}
	}
}
