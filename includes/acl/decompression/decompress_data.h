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
#include "acl/core/memory_utils.h"

#include <rtm/quatf.h>
#include <rtm/vector4f.h>
#include <rtm/packing/quatf.h>

ACL_IMPL_FILE_PRAGMA_PUSH

namespace acl
{
	namespace acl_impl
	{
		template<class SettingsType, class DecompressionContextType, class SamplingContextType>
		inline void skip_over_rotation(const SettingsType& settings, const ClipHeader& header, const DecompressionContextType& decomp_context, SamplingContextType& sampling_context)
		{
			const BitSetIndexRef track_index_bit_ref(decomp_context.bitset_desc, sampling_context.track_index);
			const bool is_sample_default = bitset_test(decomp_context.default_tracks_bitset, track_index_bit_ref);
			if (!is_sample_default)
			{
				const RotationFormat8 rotation_format = settings.get_rotation_format(header.rotation_format);

				const bool is_sample_constant = bitset_test(decomp_context.constant_tracks_bitset, track_index_bit_ref);
				if (is_sample_constant)
				{
					const RotationFormat8 packed_format = is_rotation_format_variable(rotation_format) ? get_highest_variant_precision(get_rotation_variant(rotation_format)) : rotation_format;
					sampling_context.constant_track_data_offset += get_packed_rotation_size(packed_format);
				}
				else
				{
					constexpr size_t num_key_frames = SamplingContextType::k_num_samples_to_interpolate;

					if (is_rotation_format_variable(rotation_format))
					{
						for (size_t i = 0; i < num_key_frames; ++i)
						{
							const uint8_t bit_rate = decomp_context.format_per_track_data[i][sampling_context.format_per_track_data_offset];
							uint8_t num_bits_at_bit_rate = get_num_bits_at_bit_rate(bit_rate) * 3;	// 3 components

							if (settings.supports_mixed_packing() && decomp_context.has_mixed_packing)
								num_bits_at_bit_rate = align_to(num_bits_at_bit_rate, k_mixed_packing_alignment_num_bits);

							sampling_context.key_frame_bit_offsets[i] += num_bits_at_bit_rate;

							if (settings.supports_mixed_packing() && decomp_context.has_mixed_packing)
								sampling_context.key_frame_byte_offsets[i] = decomp_context.key_frame_bit_offsets[i] / 8;
						}

						sampling_context.format_per_track_data_offset++;
					}
					else
					{
						const uint32_t rotation_size = get_packed_rotation_size(rotation_format);

						for (size_t i = 0; i < num_key_frames; ++i)
						{
							sampling_context.key_frame_byte_offsets[i] += rotation_size;

							if (settings.supports_mixed_packing() && decomp_context.has_mixed_packing)
								sampling_context.key_frame_bit_offsets[i] = sampling_context.key_frame_byte_offsets[i] * 8;
						}
					}

					const RangeReductionFlags8 clip_range_reduction = settings.get_clip_range_reduction(header.clip_range_reduction);
					if (are_any_enum_flags_set(clip_range_reduction, RangeReductionFlags8::Rotations))
						sampling_context.clip_range_data_offset += decomp_context.num_rotation_components * sizeof(float) * 2;

					const RangeReductionFlags8 segment_range_reduction = settings.get_segment_range_reduction(header.segment_range_reduction);
					if (are_any_enum_flags_set(segment_range_reduction, RangeReductionFlags8::Rotations))
						sampling_context.segment_range_data_offset += decomp_context.num_rotation_components * k_segment_range_reduction_num_bytes_per_component * 2;
				}
			}

			sampling_context.track_index++;
		}

		template<class SettingsAdapterType, class DecompressionContextType, class SamplingContextType>
		inline void skip_over_vector(const SettingsAdapterType& settings, const ClipHeader& header, const DecompressionContextType& decomp_context, SamplingContextType& sampling_context)
		{
			const BitSetIndexRef track_index_bit_ref(decomp_context.bitset_desc, sampling_context.track_index);
			const bool is_sample_default = bitset_test(decomp_context.default_tracks_bitset, track_index_bit_ref);
			if (!is_sample_default)
			{
				const bool is_sample_constant = bitset_test(decomp_context.constant_tracks_bitset, track_index_bit_ref);
				if (is_sample_constant)
				{
					// Constant Vector3 tracks store the remaining sample with full precision
					sampling_context.constant_track_data_offset += get_packed_vector_size(VectorFormat8::Vector3_96);
				}
				else
				{
					constexpr size_t num_key_frames = SamplingContextType::k_num_samples_to_interpolate;
					const VectorFormat8 format = settings.get_vector_format(header);

					if (is_vector_format_variable(format))
					{
						for (size_t i = 0; i < num_key_frames; ++i)
						{
							const uint8_t bit_rate = decomp_context.format_per_track_data[i][sampling_context.format_per_track_data_offset];
							uint8_t num_bits_at_bit_rate = get_num_bits_at_bit_rate(bit_rate) * 3;	// 3 components

							if (settings.supports_mixed_packing() && decomp_context.has_mixed_packing)
								num_bits_at_bit_rate = align_to(num_bits_at_bit_rate, k_mixed_packing_alignment_num_bits);

							sampling_context.key_frame_bit_offsets[i] += num_bits_at_bit_rate;

							if (settings.supports_mixed_packing() && decomp_context.has_mixed_packing)
								sampling_context.key_frame_byte_offsets[i] = sampling_context.key_frame_bit_offsets[i] / 8;
						}

						sampling_context.format_per_track_data_offset++;
					}
					else
					{
						const uint32_t sample_size = get_packed_vector_size(format);

						for (size_t i = 0; i < num_key_frames; ++i)
						{
							sampling_context.key_frame_byte_offsets[i] += sample_size;

							if (settings.supports_mixed_packing() && decomp_context.has_mixed_packing)
								sampling_context.key_frame_bit_offsets[i] = sampling_context.key_frame_byte_offsets[i] * 8;
						}
					}

					const RangeReductionFlags8 range_reduction_flag = settings.get_range_reduction_flag();

					const RangeReductionFlags8 clip_range_reduction = settings.get_clip_range_reduction(header.clip_range_reduction);
					if (are_any_enum_flags_set(clip_range_reduction, range_reduction_flag))
						sampling_context.clip_range_data_offset += k_clip_range_reduction_vector3_range_size;

					const RangeReductionFlags8 segment_range_reduction = settings.get_segment_range_reduction(header.segment_range_reduction);
					if (are_any_enum_flags_set(segment_range_reduction, range_reduction_flag))
						sampling_context.segment_range_data_offset += 3 * k_segment_range_reduction_num_bytes_per_component * 2;
				}
			}

			sampling_context.track_index++;
		}

		template <class SettingsType, class DecompressionContextType, class SamplingContextType>
		inline rtm::quatf RTM_SIMD_CALL decompress_and_interpolate_rotation(const SettingsType& settings, const ClipHeader& header, const DecompressionContextType& decomp_context, SamplingContextType& sampling_context)
		{
			static_assert(SamplingContextType::k_num_samples_to_interpolate == 2 || SamplingContextType::k_num_samples_to_interpolate == 4, "Unsupported number of samples");

			rtm::quatf interpolated_rotation;

			const BitSetIndexRef track_index_bit_ref(decomp_context.bitset_desc, sampling_context.track_index);
			const bool is_sample_default = bitset_test(decomp_context.default_tracks_bitset, track_index_bit_ref);
			if (is_sample_default)
			{
				interpolated_rotation = rtm::quat_identity();
			}
			else
			{
				const RotationFormat8 rotation_format = settings.get_rotation_format(header.rotation_format);
				const bool is_sample_constant = bitset_test(decomp_context.constant_tracks_bitset, track_index_bit_ref);
				if (is_sample_constant)
				{
					if (rotation_format == RotationFormat8::Quat_128 && settings.is_rotation_format_supported(RotationFormat8::Quat_128))
						interpolated_rotation = unpack_quat_128(decomp_context.constant_track_data + sampling_context.constant_track_data_offset);
					else if (rotation_format == RotationFormat8::QuatDropW_96 && settings.is_rotation_format_supported(RotationFormat8::QuatDropW_96))
						interpolated_rotation = unpack_quat_96_unsafe(decomp_context.constant_track_data + sampling_context.constant_track_data_offset);
					else if (rotation_format == RotationFormat8::QuatDropW_48 && settings.is_rotation_format_supported(RotationFormat8::QuatDropW_48))
						interpolated_rotation = unpack_quat_48(decomp_context.constant_track_data + sampling_context.constant_track_data_offset);
					else if (rotation_format == RotationFormat8::QuatDropW_32 && settings.is_rotation_format_supported(RotationFormat8::QuatDropW_32))
						interpolated_rotation = unpack_quat_32(decomp_context.constant_track_data + sampling_context.constant_track_data_offset);
					else if (rotation_format == RotationFormat8::QuatDropW_Variable && settings.is_rotation_format_supported(RotationFormat8::QuatDropW_Variable))
						interpolated_rotation = unpack_quat_96_unsafe(decomp_context.constant_track_data + sampling_context.constant_track_data_offset);
					else
					{
						ACL_ASSERT(false, "Unrecognized rotation format");
						interpolated_rotation = rtm::quat_identity();
					}

					ACL_ASSERT(rtm::quat_is_finite(interpolated_rotation), "Rotation is not valid!");
					ACL_ASSERT(rtm::quat_is_normalized(interpolated_rotation), "Rotation is not normalized!");

					const RotationFormat8 packed_format = is_rotation_format_variable(rotation_format) ? get_highest_variant_precision(get_rotation_variant(rotation_format)) : rotation_format;
					sampling_context.constant_track_data_offset += get_packed_rotation_size(packed_format);
				}
				else
				{
					const RangeReductionFlags8 clip_range_reduction = settings.get_clip_range_reduction(header.clip_range_reduction);
					const RangeReductionFlags8 segment_range_reduction = settings.get_segment_range_reduction(header.segment_range_reduction);
					const bool are_clip_rotations_normalized = are_any_enum_flags_set(clip_range_reduction, RangeReductionFlags8::Rotations);
					const bool are_segment_rotations_normalized = are_any_enum_flags_set(segment_range_reduction, RangeReductionFlags8::Rotations);

					constexpr size_t num_key_frames = SamplingContextType::k_num_samples_to_interpolate;

					// This part is fairly complex, we'll loop and write to the stack (sampling context)
					rtm::vector4f* rotations_as_vec = &sampling_context.vectors[0];

					// Range ignore flags are used to skip range normalization at the clip and/or segment levels
					// Each sample has two bits like so: sample 0 clip, sample 0 segment, sample 1 clip, sample 1 segment, etc
					// By default, we never ignore range reduction
					uint32_t range_ignore_flags = 0;

					if (rotation_format == RotationFormat8::QuatDropW_Variable && settings.is_rotation_format_supported(RotationFormat8::QuatDropW_Variable))
					{
						for (size_t i = 0; i < num_key_frames; ++i)
						{
							range_ignore_flags <<= 2;

							const uint8_t bit_rate = decomp_context.format_per_track_data[i][sampling_context.format_per_track_data_offset];

							// Promote to 32bit to avoid zero extending instructions on x64
							const uint32_t num_bits_at_bit_rate = get_num_bits_at_bit_rate(bit_rate);

							if (is_constant_bit_rate(bit_rate))
							{
								rotations_as_vec[i] = unpack_vector3_u48_unsafe(decomp_context.segment_range_data[i] + sampling_context.segment_range_data_offset);
								range_ignore_flags |= 0x00000001U;	// Skip segment only
							}
							else if (is_raw_bit_rate(bit_rate))
							{
								rotations_as_vec[i] = unpack_vector3_96_unsafe(decomp_context.animated_track_data[i], sampling_context.key_frame_bit_offsets[i]);
								range_ignore_flags |= 0x00000003U;	// Skip clip and segment
							}
							else
							{
								if (are_clip_rotations_normalized)
									rotations_as_vec[i] = unpack_vector3_uXX_unsafe(uint8_t(num_bits_at_bit_rate), decomp_context.animated_track_data[i], sampling_context.key_frame_bit_offsets[i]);
								else
									rotations_as_vec[i] = unpack_vector3_sXX_unsafe(uint8_t(num_bits_at_bit_rate), decomp_context.animated_track_data[i], sampling_context.key_frame_bit_offsets[i]);
							}

							uint32_t num_bits_read = num_bits_at_bit_rate * 3;

							if (settings.supports_mixed_packing() && decomp_context.has_mixed_packing)
								num_bits_read = align_to(num_bits_read, k_mixed_packing_alignment_num_bits);

							sampling_context.key_frame_bit_offsets[i] += num_bits_read;

							if (settings.supports_mixed_packing() && decomp_context.has_mixed_packing)
								sampling_context.key_frame_byte_offsets[i] = sampling_context.key_frame_bit_offsets[i] / 8;
						}

						sampling_context.format_per_track_data_offset++;
					}
					else
					{
						if (rotation_format == RotationFormat8::Quat_128 && settings.is_rotation_format_supported(RotationFormat8::Quat_128))
						{
							for (size_t i = 0; i < num_key_frames; ++i)
								rotations_as_vec[i] = unpack_vector4_128(decomp_context.animated_track_data[i] + sampling_context.key_frame_byte_offsets[i]);
						}
						else if (rotation_format == RotationFormat8::QuatDropW_96 && settings.is_rotation_format_supported(RotationFormat8::QuatDropW_96))
						{
							for (size_t i = 0; i < num_key_frames; ++i)
								rotations_as_vec[i] = unpack_vector3_96_unsafe(decomp_context.animated_track_data[i] + sampling_context.key_frame_byte_offsets[i]);
						}
						else if (rotation_format == RotationFormat8::QuatDropW_48 && settings.is_rotation_format_supported(RotationFormat8::QuatDropW_48))
						{
							for (size_t i = 0; i < num_key_frames; ++i)
							{
								if (are_clip_rotations_normalized)
									rotations_as_vec[i] = unpack_vector3_u48_unsafe(decomp_context.animated_track_data[i] + sampling_context.key_frame_byte_offsets[i]);
								else
									rotations_as_vec[i] = unpack_vector3_s48_unsafe(decomp_context.animated_track_data[i] + sampling_context.key_frame_byte_offsets[i]);
							}
						}
						else if (rotation_format == RotationFormat8::QuatDropW_32 && settings.is_rotation_format_supported(RotationFormat8::QuatDropW_32))
						{
							for (size_t i = 0; i < num_key_frames; ++i)
								rotations_as_vec[i] = unpack_vector3_32(11, 11, 10, are_clip_rotations_normalized, decomp_context.animated_track_data[i] + sampling_context.key_frame_byte_offsets[i]);
						}

						const uint32_t rotation_size = get_packed_rotation_size(rotation_format);

						for (size_t i = 0; i < num_key_frames; ++i)
						{
							sampling_context.key_frame_byte_offsets[i] += rotation_size;

							if (settings.supports_mixed_packing() && decomp_context.has_mixed_packing)
								sampling_context.key_frame_bit_offsets[i] = sampling_context.key_frame_byte_offsets[i] * 8;
						}
					}

					// Load our samples to avoid working with the stack now that things can be unrolled.
					// We unroll because even if we work from the stack, with 2 samples the compiler always
					// unrolls but it fails to keep the values in registers, working from the stack which
					// is inefficient.
					rtm::vector4f rotation_as_vec0 = rotations_as_vec[0];
					rtm::vector4f rotation_as_vec1 = rotations_as_vec[1];
					rtm::vector4f rotation_as_vec2;
					rtm::vector4f rotation_as_vec3;

					if (static_condition<num_key_frames == 4>::test())
					{
						rotation_as_vec2 = rotations_as_vec[2];
						rotation_as_vec3 = rotations_as_vec[3];
					}
					else
					{
						rotation_as_vec2 = rotation_as_vec0;
						rotation_as_vec3 = rotation_as_vec0;
					}

					const uint32_t num_rotation_components = decomp_context.num_rotation_components;

					if (are_segment_rotations_normalized)
					{
						const uint32_t segment_range_min_offset = sampling_context.segment_range_data_offset;
						const uint32_t segment_range_extent_offset = sampling_context.segment_range_data_offset + (num_rotation_components * sizeof(uint8_t));

						if (rotation_format == RotationFormat8::QuatDropW_Variable && settings.is_rotation_format_supported(RotationFormat8::QuatDropW_Variable))
						{
							constexpr uint32_t ignore_mask = 0x00000001U << ((num_key_frames - 1) * 2);
							if ((range_ignore_flags & (ignore_mask >> 0)) == 0)
							{
								const rtm::vector4f segment_range_min = unpack_vector3_u24_unsafe(decomp_context.segment_range_data[0] + segment_range_min_offset);
								const rtm::vector4f segment_range_extent = unpack_vector3_u24_unsafe(decomp_context.segment_range_data[0] + segment_range_extent_offset);

								rotation_as_vec0 = rtm::vector_mul_add(rotation_as_vec0, segment_range_extent, segment_range_min);
							}

							if ((range_ignore_flags & (ignore_mask >> 2)) == 0)
							{
								const rtm::vector4f segment_range_min = unpack_vector3_u24_unsafe(decomp_context.segment_range_data[1] + segment_range_min_offset);
								const rtm::vector4f segment_range_extent = unpack_vector3_u24_unsafe(decomp_context.segment_range_data[1] + segment_range_extent_offset);

								rotation_as_vec1 = rtm::vector_mul_add(rotation_as_vec1, segment_range_extent, segment_range_min);
							}

							if (static_condition<num_key_frames == 4>::test())
							{
								if ((range_ignore_flags & (ignore_mask >> 4)) == 0)
								{
									const rtm::vector4f segment_range_min = unpack_vector3_u24_unsafe(decomp_context.segment_range_data[2] + segment_range_min_offset);
									const rtm::vector4f segment_range_extent = unpack_vector3_u24_unsafe(decomp_context.segment_range_data[2] + segment_range_extent_offset);

									rotation_as_vec2 = rtm::vector_mul_add(rotation_as_vec2, segment_range_extent, segment_range_min);
								}

								if ((range_ignore_flags & (ignore_mask >> 8)) == 0)
								{
									const rtm::vector4f segment_range_min = unpack_vector3_u24_unsafe(decomp_context.segment_range_data[3] + segment_range_min_offset);
									const rtm::vector4f segment_range_extent = unpack_vector3_u24_unsafe(decomp_context.segment_range_data[3] + segment_range_extent_offset);

									rotation_as_vec3 = rtm::vector_mul_add(rotation_as_vec3, segment_range_extent, segment_range_min);
								}
							}
						}
						else
						{
							if (rotation_format == RotationFormat8::Quat_128 && settings.is_rotation_format_supported(RotationFormat8::Quat_128))
							{
								{
									const rtm::vector4f segment_range_min = unpack_vector4_32(decomp_context.segment_range_data[0] + segment_range_min_offset, true);
									const rtm::vector4f segment_range_extent = unpack_vector4_32(decomp_context.segment_range_data[0] + segment_range_extent_offset, true);

									rotation_as_vec0 = rtm::vector_mul_add(rotation_as_vec0, segment_range_extent, segment_range_min);
								}

								{
									const rtm::vector4f segment_range_min = unpack_vector4_32(decomp_context.segment_range_data[1] + segment_range_min_offset, true);
									const rtm::vector4f segment_range_extent = unpack_vector4_32(decomp_context.segment_range_data[1] + segment_range_extent_offset, true);

									rotation_as_vec1 = rtm::vector_mul_add(rotation_as_vec1, segment_range_extent, segment_range_min);
								}

								if (static_condition<num_key_frames == 4>::test())
								{
									{
										const rtm::vector4f segment_range_min = unpack_vector4_32(decomp_context.segment_range_data[2] + segment_range_min_offset, true);
										const rtm::vector4f segment_range_extent = unpack_vector4_32(decomp_context.segment_range_data[2] + segment_range_extent_offset, true);

										rotation_as_vec2 = rtm::vector_mul_add(rotation_as_vec2, segment_range_extent, segment_range_min);
									}

									{
										const rtm::vector4f segment_range_min = unpack_vector4_32(decomp_context.segment_range_data[3] + segment_range_min_offset, true);
										const rtm::vector4f segment_range_extent = unpack_vector4_32(decomp_context.segment_range_data[3] + segment_range_extent_offset, true);

										rotation_as_vec3 = rtm::vector_mul_add(rotation_as_vec3, segment_range_extent, segment_range_min);
									}
								}
							}
							else
							{
								{
									const rtm::vector4f segment_range_min = unpack_vector3_u24_unsafe(decomp_context.segment_range_data[0] + segment_range_min_offset);
									const rtm::vector4f segment_range_extent = unpack_vector3_u24_unsafe(decomp_context.segment_range_data[0] + segment_range_extent_offset);

									rotation_as_vec0 = rtm::vector_mul_add(rotation_as_vec0, segment_range_extent, segment_range_min);
								}

								{
									const rtm::vector4f segment_range_min = unpack_vector3_u24_unsafe(decomp_context.segment_range_data[1] + segment_range_min_offset);
									const rtm::vector4f segment_range_extent = unpack_vector3_u24_unsafe(decomp_context.segment_range_data[1] + segment_range_extent_offset);

									rotation_as_vec1 = rtm::vector_mul_add(rotation_as_vec1, segment_range_extent, segment_range_min);
								}

								if (static_condition<num_key_frames == 4>::test())
								{
									{
										const rtm::vector4f segment_range_min = unpack_vector3_u24_unsafe(decomp_context.segment_range_data[2] + segment_range_min_offset);
										const rtm::vector4f segment_range_extent = unpack_vector3_u24_unsafe(decomp_context.segment_range_data[2] + segment_range_extent_offset);

										rotation_as_vec2 = rtm::vector_mul_add(rotation_as_vec2, segment_range_extent, segment_range_min);
									}

									{
										const rtm::vector4f segment_range_min = unpack_vector3_u24_unsafe(decomp_context.segment_range_data[3] + segment_range_min_offset);
										const rtm::vector4f segment_range_extent = unpack_vector3_u24_unsafe(decomp_context.segment_range_data[3] + segment_range_extent_offset);

										rotation_as_vec3 = rtm::vector_mul_add(rotation_as_vec3, segment_range_extent, segment_range_min);
									}
								}
							}
						}

						sampling_context.segment_range_data_offset += num_rotation_components * k_segment_range_reduction_num_bytes_per_component * 2;
					}

					if (are_clip_rotations_normalized)
					{
						const rtm::vector4f clip_range_min = rtm::vector_load(decomp_context.clip_range_data + sampling_context.clip_range_data_offset);
						const rtm::vector4f clip_range_extent = rtm::vector_load(decomp_context.clip_range_data + sampling_context.clip_range_data_offset + (num_rotation_components * sizeof(float)));

						constexpr uint32_t ignore_mask = 0x00000002U << ((num_key_frames - 1) * 2);
						if ((range_ignore_flags & (ignore_mask >> 0)) == 0)
							rotation_as_vec0 = rtm::vector_mul_add(rotation_as_vec0, clip_range_extent, clip_range_min);

						if ((range_ignore_flags & (ignore_mask >> 2)) == 0)
							rotation_as_vec1 = rtm::vector_mul_add(rotation_as_vec1, clip_range_extent, clip_range_min);

						if (static_condition<num_key_frames == 4>::test())
						{
							if ((range_ignore_flags & (ignore_mask >> 4)) == 0)
								rotation_as_vec2 = rtm::vector_mul_add(rotation_as_vec2, clip_range_extent, clip_range_min);

							if ((range_ignore_flags & (ignore_mask >> 8)) == 0)
								rotation_as_vec3 = rtm::vector_mul_add(rotation_as_vec3, clip_range_extent, clip_range_min);
						}

						sampling_context.clip_range_data_offset += num_rotation_components * sizeof(float) * 2;
					}

					// No-op conversion
					rtm::quatf rotation0 = rtm::vector_to_quat(rotation_as_vec0);
					rtm::quatf rotation1 = rtm::vector_to_quat(rotation_as_vec1);
					rtm::quatf rotation2 = rtm::vector_to_quat(rotation_as_vec2);
					rtm::quatf rotation3 = rtm::vector_to_quat(rotation_as_vec3);

					if (rotation_format != RotationFormat8::Quat_128 || !settings.is_rotation_format_supported(RotationFormat8::Quat_128))
					{
						// We dropped the W component
						rotation0 = rtm::quat_from_positive_w(rotation_as_vec0);
						rotation1 = rtm::quat_from_positive_w(rotation_as_vec1);

						if (static_condition<num_key_frames == 4>::test())
						{
							rotation2 = rtm::quat_from_positive_w(rotation_as_vec2);
							rotation3 = rtm::quat_from_positive_w(rotation_as_vec3);
						}
					}

					if (static_condition<num_key_frames == 4>::test())
						interpolated_rotation = SamplingContextType::interpolate_rotation(rotation0, rotation1, rotation2, rotation3, decomp_context.interpolation_alpha);
					else
						interpolated_rotation = SamplingContextType::interpolate_rotation(rotation0, rotation1, decomp_context.interpolation_alpha);

					ACL_ASSERT(rtm::quat_is_finite(interpolated_rotation), "Rotation is not valid!");
					ACL_ASSERT(rtm::quat_is_normalized(interpolated_rotation), "Rotation is not normalized!");
				}
			}

			sampling_context.track_index++;
			return interpolated_rotation;
		}

		template<class SettingsAdapterType, class DecompressionContextType, class SamplingContextType>
		inline rtm::vector4f RTM_SIMD_CALL decompress_and_interpolate_vector(const SettingsAdapterType& settings, const ClipHeader& header, const DecompressionContextType& decomp_context, SamplingContextType& sampling_context)
		{
			static_assert(SamplingContextType::k_num_samples_to_interpolate == 2 || SamplingContextType::k_num_samples_to_interpolate == 4, "Unsupported number of samples");

			rtm::vector4f interpolated_vector;

			const BitSetIndexRef track_index_bit_ref(decomp_context.bitset_desc, sampling_context.track_index);
			const bool is_sample_default = bitset_test(decomp_context.default_tracks_bitset, track_index_bit_ref);
			if (is_sample_default)
			{
				interpolated_vector = settings.get_default_value();
			}
			else
			{
				const bool is_sample_constant = bitset_test(decomp_context.constant_tracks_bitset, track_index_bit_ref);
				if (is_sample_constant)
				{
					// Constant translation tracks store the remaining sample with full precision
					interpolated_vector = unpack_vector3_96_unsafe(decomp_context.constant_track_data + sampling_context.constant_track_data_offset);
					ACL_ASSERT(rtm::vector_is_finite3(interpolated_vector), "Vector is not valid!");

					sampling_context.constant_track_data_offset += get_packed_vector_size(VectorFormat8::Vector3_96);
				}
				else
				{
					const VectorFormat8 format = settings.get_vector_format(header);
					const RangeReductionFlags8 clip_range_reduction = settings.get_clip_range_reduction(header.clip_range_reduction);
					const RangeReductionFlags8 segment_range_reduction = settings.get_segment_range_reduction(header.segment_range_reduction);

					constexpr size_t num_key_frames = SamplingContextType::k_num_samples_to_interpolate;

					// This part is fairly complex, we'll loop and write to the stack (sampling context)
					rtm::vector4f* vectors = &sampling_context.vectors[0];

					// Range ignore flags are used to skip range normalization at the clip and/or segment levels
					// Each sample has two bits like so: sample 0 clip, sample 0 segment, sample 1 clip, sample 1 segment, etc
					// By default, we never ignore range reduction
					uint32_t range_ignore_flags = 0;

					if (format == VectorFormat8::Vector3_Variable && settings.is_vector_format_supported(VectorFormat8::Vector3_Variable))
					{
						for (size_t i = 0; i < num_key_frames; ++i)
						{
							range_ignore_flags <<= 2;

							const uint8_t bit_rate = decomp_context.format_per_track_data[i][sampling_context.format_per_track_data_offset];

							// Promote to 32bit to avoid zero extending instructions on x64
							const uint32_t num_bits_at_bit_rate = get_num_bits_at_bit_rate(bit_rate);

							if (is_constant_bit_rate(bit_rate))
							{
								vectors[i] = unpack_vector3_u48_unsafe(decomp_context.segment_range_data[i] + sampling_context.segment_range_data_offset);
								range_ignore_flags |= 0x00000001U;	// Skip segment only
							}
							else if (is_raw_bit_rate(bit_rate))
							{
								vectors[i] = unpack_vector3_96_unsafe(decomp_context.animated_track_data[i], sampling_context.key_frame_bit_offsets[i]);
								range_ignore_flags |= 0x00000003U;	// Skip clip and segment
							}
							else
								vectors[i] = unpack_vector3_uXX_unsafe(uint8_t(num_bits_at_bit_rate), decomp_context.animated_track_data[i], sampling_context.key_frame_bit_offsets[i]);

							uint32_t num_bits_read = num_bits_at_bit_rate * 3;

							if (settings.supports_mixed_packing() && decomp_context.has_mixed_packing)
								num_bits_read = align_to(num_bits_read, k_mixed_packing_alignment_num_bits);

							sampling_context.key_frame_bit_offsets[i] += num_bits_read;

							if (settings.supports_mixed_packing() && decomp_context.has_mixed_packing)
								sampling_context.key_frame_byte_offsets[i] = sampling_context.key_frame_bit_offsets[i] / 8;
						}

						sampling_context.format_per_track_data_offset++;
					}
					else
					{
						if (format == VectorFormat8::Vector3_96 && settings.is_vector_format_supported(VectorFormat8::Vector3_96))
						{
							for (size_t i = 0; i < num_key_frames; ++i)
								vectors[i] = unpack_vector3_96_unsafe(decomp_context.animated_track_data[i] + sampling_context.key_frame_byte_offsets[i]);
						}
						else if (format == VectorFormat8::Vector3_48 && settings.is_vector_format_supported(VectorFormat8::Vector3_48))
						{
							for (size_t i = 0; i < num_key_frames; ++i)
								vectors[i] = unpack_vector3_u48_unsafe(decomp_context.animated_track_data[i] + sampling_context.key_frame_byte_offsets[i]);
						}
						else if (format == VectorFormat8::Vector3_32 && settings.is_vector_format_supported(VectorFormat8::Vector3_32))
						{
							for (size_t i = 0; i < num_key_frames; ++i)
								vectors[i] = unpack_vector3_32(11, 11, 10, true, decomp_context.animated_track_data[i] + sampling_context.key_frame_byte_offsets[i]);
						}

						const uint32_t sample_size = get_packed_vector_size(format);

						for (size_t i = 0; i < num_key_frames; ++i)
						{
							sampling_context.key_frame_byte_offsets[i] += sample_size;

							if (settings.supports_mixed_packing() && decomp_context.has_mixed_packing)
								sampling_context.key_frame_bit_offsets[i] = sampling_context.key_frame_byte_offsets[i] * 8;
						}
					}

					// Load our samples to avoid working with the stack now that things can be unrolled.
					// We unroll because even if we work from the stack, with 2 samples the compiler always
					// unrolls but it fails to keep the values in registers, working from the stack which
					// is inefficient.
					rtm::vector4f vector0 = vectors[0];
					rtm::vector4f vector1 = vectors[1];
					rtm::vector4f vector2;
					rtm::vector4f vector3;

					if (static_condition<num_key_frames == 4>::test())
					{
						vector2 = vectors[2];
						vector3 = vectors[3];
					}
					else
					{
						vector2 = vector0;
						vector3 = vector0;
					}

					const RangeReductionFlags8 range_reduction_flag = settings.get_range_reduction_flag();
					if (are_any_enum_flags_set(segment_range_reduction, range_reduction_flag))
					{
						const uint32_t segment_range_min_offset = sampling_context.segment_range_data_offset;
						const uint32_t segment_range_extent_offset = sampling_context.segment_range_data_offset + (3 * sizeof(uint8_t));

						constexpr uint32_t ignore_mask = 0x00000001U << ((num_key_frames - 1) * 2);
						if ((range_ignore_flags & (ignore_mask >> 0)) == 0)
						{
							const rtm::vector4f segment_range_min = unpack_vector3_u24_unsafe(decomp_context.segment_range_data[0] + segment_range_min_offset);
							const rtm::vector4f segment_range_extent = unpack_vector3_u24_unsafe(decomp_context.segment_range_data[0] + segment_range_extent_offset);

							vector0 = rtm::vector_mul_add(vector0, segment_range_extent, segment_range_min);
						}

						if ((range_ignore_flags & (ignore_mask >> 2)) == 0)
						{
							const rtm::vector4f segment_range_min = unpack_vector3_u24_unsafe(decomp_context.segment_range_data[1] + segment_range_min_offset);
							const rtm::vector4f segment_range_extent = unpack_vector3_u24_unsafe(decomp_context.segment_range_data[1] + segment_range_extent_offset);

							vector1 = rtm::vector_mul_add(vector1, segment_range_extent, segment_range_min);
						}

						if (static_condition<num_key_frames == 4>::test())
						{
							if ((range_ignore_flags & (ignore_mask >> 4)) == 0)
							{
								const rtm::vector4f segment_range_min = unpack_vector3_u24_unsafe(decomp_context.segment_range_data[2] + segment_range_min_offset);
								const rtm::vector4f segment_range_extent = unpack_vector3_u24_unsafe(decomp_context.segment_range_data[2] + segment_range_extent_offset);

								vector2 = rtm::vector_mul_add(vector2, segment_range_extent, segment_range_min);
							}

							if ((range_ignore_flags & (ignore_mask >> 8)) == 0)
							{
								const rtm::vector4f segment_range_min = unpack_vector3_u24_unsafe(decomp_context.segment_range_data[3] + segment_range_min_offset);
								const rtm::vector4f segment_range_extent = unpack_vector3_u24_unsafe(decomp_context.segment_range_data[3] + segment_range_extent_offset);

								vector3 = rtm::vector_mul_add(vector3, segment_range_extent, segment_range_min);
							}
						}

						sampling_context.segment_range_data_offset += 3 * k_segment_range_reduction_num_bytes_per_component * 2;
					}

					if (are_any_enum_flags_set(clip_range_reduction, range_reduction_flag))
					{
						const rtm::vector4f clip_range_min = unpack_vector3_96_unsafe(decomp_context.clip_range_data + sampling_context.clip_range_data_offset);
						const rtm::vector4f clip_range_extent = unpack_vector3_96_unsafe(decomp_context.clip_range_data + sampling_context.clip_range_data_offset + (3 * sizeof(float)));

						constexpr uint32_t ignore_mask = 0x00000002U << ((num_key_frames - 1) * 2);
						if ((range_ignore_flags & (ignore_mask >> 0)) == 0)
							vector0 = rtm::vector_mul_add(vector0, clip_range_extent, clip_range_min);

						if ((range_ignore_flags & (ignore_mask >> 2)) == 0)
							vector1 = rtm::vector_mul_add(vector1, clip_range_extent, clip_range_min);

						if (static_condition<num_key_frames == 4>::test())
						{
							if ((range_ignore_flags & (ignore_mask >> 4)) == 0)
								vector2 = rtm::vector_mul_add(vector2, clip_range_extent, clip_range_min);

							if ((range_ignore_flags & (ignore_mask >> 8)) == 0)
								vector3 = rtm::vector_mul_add(vector3, clip_range_extent, clip_range_min);
						}

						sampling_context.clip_range_data_offset += k_clip_range_reduction_vector3_range_size;
					}

					if (static_condition<num_key_frames == 4>::test())
						interpolated_vector = SamplingContextType::interpolate_vector4(vector0, vector1, vector2, vector3, decomp_context.interpolation_alpha);
					else
						interpolated_vector = SamplingContextType::interpolate_vector4(vector0, vector1, decomp_context.interpolation_alpha);

					ACL_ASSERT(rtm::vector_is_finite3(interpolated_vector), "Vector is not valid!");
				}
			}

			sampling_context.track_index++;
			return interpolated_vector;
		}
	}
}

ACL_IMPL_FILE_PRAGMA_POP
