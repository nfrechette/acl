#pragma once

////////////////////////////////////////////////////////////////////////////////
// The MIT License (MIT)
//
// Copyright (c) 2020 Nicholas Frechette & Animation Compression Library contributors
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

#include "acl/core/bitset.h"
#include "acl/core/compressed_tracks.h"
#include "acl/core/compressed_tracks_version.h"
#include "acl/core/interpolation_utils.h"
#include "acl/core/range_reduction_types.h"
#include "acl/core/track_writer.h"
#include "acl/core/impl/compiler_utils.h"
#include "acl/math/quatf.h"
#include "acl/math/quat_packing.h"

#include <rtm/scalarf.h>
#include <rtm/vector4f.h>

#include <cstdint>
#include <type_traits>

ACL_IMPL_FILE_PRAGMA_PUSH

namespace acl
{
	namespace acl_impl
	{
		struct alignas(64) persistent_transform_decompression_context_v0
		{
			// Clip related data							//   offsets
			// Only member used to detect if we are initialized, must be first
			const compressed_tracks* tracks;				//   0 |   0

			const uint32_t* constant_tracks_bitset;			//   4 |   8
			const uint8_t* constant_track_data;				//   8 |  16
			const uint32_t* default_tracks_bitset;			//  12 |  24

			const uint8_t* clip_range_data;					//  16 |  32

			float clip_duration;							//  20 |  40

			bitset_description bitset_desc;					//  24 |  44

			uint32_t clip_hash;								//  28 |  48

			rotation_format8 rotation_format;				//  32 |  52
			vector_format8 translation_format;				//  33 |  53
			vector_format8 scale_format;					//  34 |  54
			range_reduction_flags8 range_reduction;			//  35 |  55

			uint8_t num_rotation_components;				//  36 |  56
			uint8_t has_segments;							//  37 |  57

			uint8_t padding0[2];							//  38 |  58

			// Seeking related data
			float sample_time;								//  40 |  60

			const uint8_t* format_per_track_data[2];		//  44 |  64
			const uint8_t* segment_range_data[2];			//  52 |  80
			const uint8_t* animated_track_data[2];			//  60 |  96

			uint32_t key_frame_bit_offsets[2];				//  68 | 112

			float interpolation_alpha;						//  76 | 120

			uint8_t padding1[sizeof(void*) == 4 ? 48 : 4];	//  80 | 124

			//									Total size:	   128 | 128

			//////////////////////////////////////////////////////////////////////////

			inline const compressed_tracks* get_compressed_tracks() const { return tracks; }
			inline compressed_tracks_version16 get_version() const { return tracks->get_version(); }
			inline bool is_initialized() const { return tracks != nullptr; }
			inline void reset() { tracks = nullptr; }
		};

		static_assert(sizeof(persistent_transform_decompression_context_v0) == 128, "Unexpected size");

		struct alignas(64) sampling_context_v0
		{
			//														//   offsets
			uint32_t track_index;									//   0 |   0
			uint32_t constant_track_data_offset;					//   4 |   4
			uint32_t clip_range_data_offset;						//   8 |   8

			uint32_t format_per_track_data_offset;					//  12 |  12
			uint32_t segment_range_data_offset;						//  16 |  16

			uint32_t key_frame_bit_offsets[2];						//  20 |  20

			uint8_t padding[4];										//  28 |  28

			rtm::vector4f vectors[2];								//  32 |  32

			//											Total size:	    64 |  64
		};

		static_assert(sizeof(sampling_context_v0) == 64, "Unexpected size");

		// We use adapters to wrap the decompression_settings
		// This allows us to re-use the code for skipping and decompressing Vector3 samples
		// Code generation will generate specialized code for each specialization
		template<class decompression_settings_type>
		struct translation_decompression_settings_adapter
		{
			// Forward to our decompression settings
			static constexpr range_reduction_flags8 get_range_reduction_flag() { return range_reduction_flags8::translations; }
			static constexpr vector_format8 get_vector_format(const persistent_transform_decompression_context_v0& context) { return context.translation_format; }
			static constexpr bool is_vector_format_supported(vector_format8 format) { return decompression_settings_type::is_translation_format_supported(format); }
		};

		template<class decompression_settings_type>
		struct scale_decompression_settings_adapter
		{
			// Forward to our decompression settings
			static constexpr range_reduction_flags8 get_range_reduction_flag() { return range_reduction_flags8::scales; }
			static constexpr vector_format8 get_vector_format(const persistent_transform_decompression_context_v0& context) { return context.scale_format; }
			static constexpr bool is_vector_format_supported(vector_format8 format) { return decompression_settings_type::is_scale_format_supported(format); }
		};

		// Returns the statically known number of rotation formats supported by the decompression settings
		template<class decompression_settings_type>
		constexpr int32_t num_supported_rotation_formats()
		{
			return decompression_settings_type::is_rotation_format_supported(rotation_format8::quatf_full)
				+ decompression_settings_type::is_rotation_format_supported(rotation_format8::quatf_drop_w_full)
				+ decompression_settings_type::is_rotation_format_supported(rotation_format8::quatf_drop_w_variable);
		}

		// Returns the statically known rotation format supported if we only support one, otherwise we return the input value
		// which might not be known statically
		template<class decompression_settings_type>
		constexpr rotation_format8 get_rotation_format(rotation_format8 format)
		{
			return num_supported_rotation_formats<decompression_settings_type>() > 1
				// More than one format is supported, return the input value, whatever it may be
				? format
				// Only one format is supported, figure out statically which one it is and return it
				: (decompression_settings_type::is_rotation_format_supported(rotation_format8::quatf_full) ? rotation_format8::quatf_full
					: (decompression_settings_type::is_rotation_format_supported(rotation_format8::quatf_drop_w_full) ? rotation_format8::quatf_drop_w_full
						: rotation_format8::quatf_drop_w_variable));
		}

		// Returns the statically known number of vector formats supported by the decompression settings
		template<class decompression_settings_adapter_type>
		constexpr int32_t num_supported_vector_formats()
		{
			return decompression_settings_adapter_type::is_vector_format_supported(vector_format8::vector3f_full)
				+ decompression_settings_adapter_type::is_vector_format_supported(vector_format8::vector3f_variable);
		}

		// Returns the statically known vector format supported if we only support one, otherwise we return the input value
		// which might not be known statically
		template<class decompression_settings_adapter_type>
		constexpr vector_format8 get_vector_format(vector_format8 format)
		{
			return num_supported_vector_formats<decompression_settings_adapter_type>() > 1
				// More than one format is supported, return the input value, whatever it may be
				? format
				// Only one format is supported, figure out statically which one it is and return it
				: (decompression_settings_adapter_type::is_vector_format_supported(vector_format8::vector3f_full) ? vector_format8::vector3f_full
					: vector_format8::vector3f_variable);
		}

		template<class decompression_settings_type>
		inline void skip_over_rotation(const persistent_transform_decompression_context_v0& decomp_context, sampling_context_v0& sampling_context_)
		{
			const bitset_index_ref track_index_bit_ref(decomp_context.bitset_desc, sampling_context_.track_index);
			const bool is_sample_default = bitset_test(decomp_context.default_tracks_bitset, track_index_bit_ref);
			if (!is_sample_default)
			{
				const rotation_format8 rotation_format = get_rotation_format<decompression_settings_type>(decomp_context.rotation_format);

				const bool is_sample_constant = bitset_test(decomp_context.constant_tracks_bitset, track_index_bit_ref);
				if (is_sample_constant)
				{
					const rotation_format8 packed_format = is_rotation_format_variable(rotation_format) ? get_highest_variant_precision(get_rotation_variant(rotation_format)) : rotation_format;
					sampling_context_.constant_track_data_offset += get_packed_rotation_size(packed_format);
				}
				else
				{
					if (is_rotation_format_variable(rotation_format))
					{
						for (uint32_t i = 0; i < 2; ++i)
						{
							const uint8_t bit_rate = decomp_context.format_per_track_data[i][sampling_context_.format_per_track_data_offset];
							const uint32_t num_bits_at_bit_rate = get_num_bits_at_bit_rate(bit_rate) * 3;	// 3 components

							sampling_context_.key_frame_bit_offsets[i] += num_bits_at_bit_rate;
						}

						sampling_context_.format_per_track_data_offset++;
					}
					else
					{
						const uint32_t rotation_size = get_packed_rotation_size(rotation_format);
						const uint32_t num_bits_at_bit_rate = rotation_size == (sizeof(float) * 4) ? 128 : 96;

						for (uint32_t i = 0; i < 2; ++i)
							sampling_context_.key_frame_bit_offsets[i] += num_bits_at_bit_rate;
					}

					if (are_any_enum_flags_set(decomp_context.range_reduction, range_reduction_flags8::rotations))
					{
						sampling_context_.clip_range_data_offset += decomp_context.num_rotation_components * sizeof(float) * 2;

						if (decomp_context.has_segments)
							sampling_context_.segment_range_data_offset += decomp_context.num_rotation_components * k_segment_range_reduction_num_bytes_per_component * 2;
					}
				}
			}

			sampling_context_.track_index++;
		}

		template <class decompression_settings_type>
		inline rtm::quatf RTM_SIMD_CALL decompress_and_interpolate_rotation(const persistent_transform_decompression_context_v0& decomp_context, sampling_context_v0& sampling_context_)
		{
			rtm::quatf interpolated_rotation;

			const bitset_index_ref track_index_bit_ref(decomp_context.bitset_desc, sampling_context_.track_index);
			const bool is_sample_default = bitset_test(decomp_context.default_tracks_bitset, track_index_bit_ref);
			if (is_sample_default)
			{
				interpolated_rotation = rtm::quat_identity();
			}
			else
			{
				const rotation_format8 rotation_format = get_rotation_format<decompression_settings_type>(decomp_context.rotation_format);
				const bool is_sample_constant = bitset_test(decomp_context.constant_tracks_bitset, track_index_bit_ref);
				if (is_sample_constant)
				{
					if (rotation_format == rotation_format8::quatf_full && decompression_settings_type::is_rotation_format_supported(rotation_format8::quatf_full))
						interpolated_rotation = unpack_quat_128(decomp_context.constant_track_data + sampling_context_.constant_track_data_offset);
					else if (rotation_format == rotation_format8::quatf_drop_w_full && decompression_settings_type::is_rotation_format_supported(rotation_format8::quatf_drop_w_full))
						interpolated_rotation = unpack_quat_96_unsafe(decomp_context.constant_track_data + sampling_context_.constant_track_data_offset);
					else if (rotation_format == rotation_format8::quatf_drop_w_variable && decompression_settings_type::is_rotation_format_supported(rotation_format8::quatf_drop_w_variable))
						interpolated_rotation = unpack_quat_96_unsafe(decomp_context.constant_track_data + sampling_context_.constant_track_data_offset);
					else
					{
						ACL_ASSERT(false, "Unrecognized rotation format");
						interpolated_rotation = rtm::quat_identity();
					}

					ACL_ASSERT(rtm::quat_is_finite(interpolated_rotation), "Rotation is not valid!");
					ACL_ASSERT(rtm::quat_is_normalized(interpolated_rotation), "Rotation is not normalized!");

					const rotation_format8 packed_format = is_rotation_format_variable(rotation_format) ? get_highest_variant_precision(get_rotation_variant(rotation_format)) : rotation_format;
					sampling_context_.constant_track_data_offset += get_packed_rotation_size(packed_format);
				}
				else
				{
					// This part is fairly complex, we'll loop and write to the stack (sampling context)
					rtm::vector4f* rotations_as_vec = &sampling_context_.vectors[0];

					// Range ignore flags are used to skip range normalization at the clip and/or segment levels
					// Each sample has two bits like so:
					//    - 0x01 = sample 1 segment
					//    - 0x02 = sample 1 clip
					//    - 0x04 = sample 0 segment
					//    - 0x08 = sample 0 clip
					// By default, we never ignore range reduction
					uint32_t range_ignore_flags = 0;

					if (rotation_format == rotation_format8::quatf_drop_w_variable && decompression_settings_type::is_rotation_format_supported(rotation_format8::quatf_drop_w_variable))
					{
						for (uint32_t i = 0; i < 2; ++i)
						{
							range_ignore_flags <<= 2;

							const uint8_t bit_rate = decomp_context.format_per_track_data[i][sampling_context_.format_per_track_data_offset];
							const uint32_t num_bits_at_bit_rate = get_num_bits_at_bit_rate(bit_rate);

							if (is_constant_bit_rate(bit_rate))
							{
								rotations_as_vec[i] = unpack_vector3_u48_unsafe(decomp_context.segment_range_data[i] + sampling_context_.segment_range_data_offset);
								range_ignore_flags |= 0x00000001U;	// Skip segment only
							}
							else if (is_raw_bit_rate(bit_rate))
							{
								rotations_as_vec[i] = unpack_vector3_96_unsafe(decomp_context.animated_track_data[i], sampling_context_.key_frame_bit_offsets[i]);
								range_ignore_flags |= 0x00000003U;	// Skip clip and segment
							}
							else
								rotations_as_vec[i] = unpack_vector3_uXX_unsafe(uint8_t(num_bits_at_bit_rate), decomp_context.animated_track_data[i], sampling_context_.key_frame_bit_offsets[i]);

							sampling_context_.key_frame_bit_offsets[i] += num_bits_at_bit_rate * 3;
						}

						sampling_context_.format_per_track_data_offset++;
					}
					else
					{
						if (rotation_format == rotation_format8::quatf_full && decompression_settings_type::is_rotation_format_supported(rotation_format8::quatf_full))
						{
							for (uint32_t i = 0; i < 2; ++i)
							{
								rotations_as_vec[i] = unpack_vector4_128_unsafe(decomp_context.animated_track_data[i], sampling_context_.key_frame_bit_offsets[i]);
								sampling_context_.key_frame_bit_offsets[i] += 128;
							}
						}
						else if (rotation_format == rotation_format8::quatf_drop_w_full && decompression_settings_type::is_rotation_format_supported(rotation_format8::quatf_drop_w_full))
						{
							for (uint32_t i = 0; i < 2; ++i)
							{
								rotations_as_vec[i] = unpack_vector3_96_unsafe(decomp_context.animated_track_data[i], sampling_context_.key_frame_bit_offsets[i]);
								sampling_context_.key_frame_bit_offsets[i] += 96;
							}
						}
					}

					// Load our samples to avoid working with the stack now that things can be unrolled.
					// We unroll because even if we work from the stack, with 2 samples the compiler always
					// unrolls but it fails to keep the values in registers, working from the stack which
					// is inefficient.
					rtm::vector4f rotation_as_vec0 = rotations_as_vec[0];
					rtm::vector4f rotation_as_vec1 = rotations_as_vec[1];

					const uint32_t num_rotation_components = decomp_context.num_rotation_components;

					if (are_any_enum_flags_set(decomp_context.range_reduction, range_reduction_flags8::rotations))
					{
						if (decomp_context.has_segments)
						{
							const uint32_t segment_range_min_offset = sampling_context_.segment_range_data_offset;
							const uint32_t segment_range_extent_offset = sampling_context_.segment_range_data_offset + (num_rotation_components * sizeof(uint8_t));

							if (rotation_format == rotation_format8::quatf_drop_w_variable && decompression_settings_type::is_rotation_format_supported(rotation_format8::quatf_drop_w_variable))
							{
								if ((range_ignore_flags & 0x04) == 0)
								{
									const rtm::vector4f segment_range_min = unpack_vector3_u24_unsafe(decomp_context.segment_range_data[0] + segment_range_min_offset);
									const rtm::vector4f segment_range_extent = unpack_vector3_u24_unsafe(decomp_context.segment_range_data[0] + segment_range_extent_offset);

									rotation_as_vec0 = rtm::vector_mul_add(rotation_as_vec0, segment_range_extent, segment_range_min);
								}

								if ((range_ignore_flags & 0x01) == 0)
								{
									const rtm::vector4f segment_range_min = unpack_vector3_u24_unsafe(decomp_context.segment_range_data[1] + segment_range_min_offset);
									const rtm::vector4f segment_range_extent = unpack_vector3_u24_unsafe(decomp_context.segment_range_data[1] + segment_range_extent_offset);

									rotation_as_vec1 = rtm::vector_mul_add(rotation_as_vec1, segment_range_extent, segment_range_min);
								}
							}
							else
							{
								if (rotation_format == rotation_format8::quatf_full && decompression_settings_type::is_rotation_format_supported(rotation_format8::quatf_full))
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
								}
							}

							sampling_context_.segment_range_data_offset += num_rotation_components * k_segment_range_reduction_num_bytes_per_component * 2;
						}

						const rtm::vector4f clip_range_min = rtm::vector_load(decomp_context.clip_range_data + sampling_context_.clip_range_data_offset);
						const rtm::vector4f clip_range_extent = rtm::vector_load(decomp_context.clip_range_data + sampling_context_.clip_range_data_offset + (num_rotation_components * sizeof(float)));

						if ((range_ignore_flags & 0x08) == 0)
							rotation_as_vec0 = rtm::vector_mul_add(rotation_as_vec0, clip_range_extent, clip_range_min);

						if ((range_ignore_flags & 0x02) == 0)
							rotation_as_vec1 = rtm::vector_mul_add(rotation_as_vec1, clip_range_extent, clip_range_min);

						sampling_context_.clip_range_data_offset += num_rotation_components * sizeof(float) * 2;
					}

					// No-op conversion
					rtm::quatf rotation0 = rtm::vector_to_quat(rotation_as_vec0);
					rtm::quatf rotation1 = rtm::vector_to_quat(rotation_as_vec1);

					if (rotation_format != rotation_format8::quatf_full || !decompression_settings_type::is_rotation_format_supported(rotation_format8::quatf_full))
					{
						// We dropped the W component
						rotation0 = rtm::quat_from_positive_w(rotation_as_vec0);
						rotation1 = rtm::quat_from_positive_w(rotation_as_vec1);
					}

					const bool normalize_rotations = decompression_settings_type::normalize_rotations();
					if (normalize_rotations)
						interpolated_rotation = rtm::quat_lerp(rotation0, rotation1, decomp_context.interpolation_alpha);
					else
						interpolated_rotation = quat_lerp_no_normalization(rotation0, rotation1, decomp_context.interpolation_alpha);

					ACL_ASSERT(rtm::quat_is_finite(interpolated_rotation), "Rotation is not valid!");
					ACL_ASSERT(rtm::quat_is_normalized(interpolated_rotation) || !decompression_settings_type::normalize_rotations(), "Rotation is not normalized!");
				}
			}

			sampling_context_.track_index++;
			return interpolated_rotation;
		}

		template<class decompression_settings_adapter_type>
		inline void skip_over_vector(const persistent_transform_decompression_context_v0& decomp_context, sampling_context_v0& sampling_context_)
		{
			const bitset_index_ref track_index_bit_ref(decomp_context.bitset_desc, sampling_context_.track_index);
			const bool is_sample_default = bitset_test(decomp_context.default_tracks_bitset, track_index_bit_ref);
			if (!is_sample_default)
			{
				const bool is_sample_constant = bitset_test(decomp_context.constant_tracks_bitset, track_index_bit_ref);
				if (is_sample_constant)
				{
					// Constant Vector3 tracks store the remaining sample with full precision
					sampling_context_.constant_track_data_offset += get_packed_vector_size(vector_format8::vector3f_full);
				}
				else
				{
					const vector_format8 format = get_vector_format<decompression_settings_adapter_type>(decompression_settings_adapter_type::get_vector_format(decomp_context));

					if (is_vector_format_variable(format))
					{
						for (uint32_t i = 0; i < 2; ++i)
						{
							const uint8_t bit_rate = decomp_context.format_per_track_data[i][sampling_context_.format_per_track_data_offset];
							const uint32_t num_bits_at_bit_rate = get_num_bits_at_bit_rate(bit_rate) * 3;	// 3 components

							sampling_context_.key_frame_bit_offsets[i] += num_bits_at_bit_rate;
						}

						sampling_context_.format_per_track_data_offset++;
					}
					else
					{
						for (uint32_t i = 0; i < 2; ++i)
							sampling_context_.key_frame_bit_offsets[i] += 96;
					}

					const range_reduction_flags8 range_reduction_flag = decompression_settings_adapter_type::get_range_reduction_flag();

					if (are_any_enum_flags_set(decomp_context.range_reduction, range_reduction_flag))
					{
						sampling_context_.clip_range_data_offset += k_clip_range_reduction_vector3_range_size;

						if (decomp_context.has_segments)
							sampling_context_.segment_range_data_offset += 3 * k_segment_range_reduction_num_bytes_per_component * 2;
					}
				}
			}

			sampling_context_.track_index++;
		}

		template<class decompression_settings_adapter_type>
		inline rtm::vector4f RTM_SIMD_CALL decompress_and_interpolate_vector(const persistent_transform_decompression_context_v0& decomp_context, rtm::vector4f_arg0 default_value, sampling_context_v0& sampling_context_)
		{
			rtm::vector4f interpolated_vector;

			const bitset_index_ref track_index_bit_ref(decomp_context.bitset_desc, sampling_context_.track_index);
			const bool is_sample_default = bitset_test(decomp_context.default_tracks_bitset, track_index_bit_ref);
			if (is_sample_default)
			{
				interpolated_vector = default_value;
			}
			else
			{
				const bool is_sample_constant = bitset_test(decomp_context.constant_tracks_bitset, track_index_bit_ref);
				if (is_sample_constant)
				{
					// Constant translation tracks store the remaining sample with full precision
					interpolated_vector = unpack_vector3_96_unsafe(decomp_context.constant_track_data + sampling_context_.constant_track_data_offset);
					ACL_ASSERT(rtm::vector_is_finite3(interpolated_vector), "Vector is not valid!");

					sampling_context_.constant_track_data_offset += get_packed_vector_size(vector_format8::vector3f_full);
				}
				else
				{
					const vector_format8 format = get_vector_format<decompression_settings_adapter_type>(decompression_settings_adapter_type::get_vector_format(decomp_context));

					// This part is fairly complex, we'll loop and write to the stack (sampling context)
					rtm::vector4f* vectors = &sampling_context_.vectors[0];

					// Range ignore flags are used to skip range normalization at the clip and/or segment levels
					// Each sample has two bits like so:
					//    - 0x01 = sample 1 segment
					//    - 0x02 = sample 1 clip
					//    - 0x04 = sample 0 segment
					//    - 0x08 = sample 0 clip
					// By default, we never ignore range reduction
					uint32_t range_ignore_flags = 0;

					if (format == vector_format8::vector3f_variable && decompression_settings_adapter_type::is_vector_format_supported(vector_format8::vector3f_variable))
					{
						for (uint32_t i = 0; i < 2; ++i)
						{
							range_ignore_flags <<= 2;

							const uint8_t bit_rate = decomp_context.format_per_track_data[i][sampling_context_.format_per_track_data_offset];
							const uint32_t num_bits_at_bit_rate = get_num_bits_at_bit_rate(bit_rate);

							if (is_constant_bit_rate(bit_rate))
							{
								vectors[i] = unpack_vector3_u48_unsafe(decomp_context.segment_range_data[i] + sampling_context_.segment_range_data_offset);
								range_ignore_flags |= 0x00000001U;	// Skip segment only
							}
							else if (is_raw_bit_rate(bit_rate))
							{
								vectors[i] = unpack_vector3_96_unsafe(decomp_context.animated_track_data[i], sampling_context_.key_frame_bit_offsets[i]);
								range_ignore_flags |= 0x00000003U;	// Skip clip and segment
							}
							else
								vectors[i] = unpack_vector3_uXX_unsafe(uint8_t(num_bits_at_bit_rate), decomp_context.animated_track_data[i], sampling_context_.key_frame_bit_offsets[i]);

							sampling_context_.key_frame_bit_offsets[i] += num_bits_at_bit_rate * 3;
						}

						sampling_context_.format_per_track_data_offset++;
					}
					else
					{
						if (format == vector_format8::vector3f_full && decompression_settings_adapter_type::is_vector_format_supported(vector_format8::vector3f_full))
						{
							for (uint32_t i = 0; i < 2; ++i)
							{
								vectors[i] = unpack_vector3_96_unsafe(decomp_context.animated_track_data[i], sampling_context_.key_frame_bit_offsets[i]);
								sampling_context_.key_frame_bit_offsets[i] += 96;
							}
						}
					}

					// Load our samples to avoid working with the stack now that things can be unrolled.
					// We unroll because even if we work from the stack, with 2 samples the compiler always
					// unrolls but it fails to keep the values in registers, working from the stack which
					// is inefficient.
					rtm::vector4f vector0 = vectors[0];
					rtm::vector4f vector1 = vectors[1];

					const range_reduction_flags8 range_reduction_flag = decompression_settings_adapter_type::get_range_reduction_flag();
					if (are_any_enum_flags_set(decomp_context.range_reduction, range_reduction_flag))
					{
						if (decomp_context.has_segments)
						{
							const uint32_t segment_range_min_offset = sampling_context_.segment_range_data_offset;
							const uint32_t segment_range_extent_offset = sampling_context_.segment_range_data_offset + (3 * sizeof(uint8_t));

							if ((range_ignore_flags & 0x04) == 0)
							{
								const rtm::vector4f segment_range_min = unpack_vector3_u24_unsafe(decomp_context.segment_range_data[0] + segment_range_min_offset);
								const rtm::vector4f segment_range_extent = unpack_vector3_u24_unsafe(decomp_context.segment_range_data[0] + segment_range_extent_offset);

								vector0 = rtm::vector_mul_add(vector0, segment_range_extent, segment_range_min);
							}

							if ((range_ignore_flags & 0x01) == 0)
							{
								const rtm::vector4f segment_range_min = unpack_vector3_u24_unsafe(decomp_context.segment_range_data[1] + segment_range_min_offset);
								const rtm::vector4f segment_range_extent = unpack_vector3_u24_unsafe(decomp_context.segment_range_data[1] + segment_range_extent_offset);

								vector1 = rtm::vector_mul_add(vector1, segment_range_extent, segment_range_min);
							}

							sampling_context_.segment_range_data_offset += 3 * k_segment_range_reduction_num_bytes_per_component * 2;
						}

						const rtm::vector4f clip_range_min = unpack_vector3_96_unsafe(decomp_context.clip_range_data + sampling_context_.clip_range_data_offset);
						const rtm::vector4f clip_range_extent = unpack_vector3_96_unsafe(decomp_context.clip_range_data + sampling_context_.clip_range_data_offset + (3 * sizeof(float)));

						if ((range_ignore_flags & 0x08) == 0)
							vector0 = rtm::vector_mul_add(vector0, clip_range_extent, clip_range_min);

						if ((range_ignore_flags & 0x02) == 0)
							vector1 = rtm::vector_mul_add(vector1, clip_range_extent, clip_range_min);

						sampling_context_.clip_range_data_offset += k_clip_range_reduction_vector3_range_size;
					}

					interpolated_vector = rtm::vector_lerp(vector0, vector1, decomp_context.interpolation_alpha);

					ACL_ASSERT(rtm::vector_is_finite3(interpolated_vector), "Vector is not valid!");
				}
			}

			sampling_context_.track_index++;
			return interpolated_vector;
		}

		template<class decompression_settings_type>
		inline bool initialize_v0(persistent_transform_decompression_context_v0& context, const compressed_tracks& tracks)
		{
			if (tracks.is_valid(false).any())
				return false;	// Invalid compressed tracks instance

			ACL_ASSERT(tracks.get_algorithm_type() == algorithm_type8::uniformly_sampled, "Invalid algorithm type [%s], expected [%s]", get_algorithm_name(tracks.get_algorithm_type()), get_algorithm_name(algorithm_type8::uniformly_sampled));

			using translation_adapter = acl_impl::translation_decompression_settings_adapter<decompression_settings_type>;
			using scale_adapter = acl_impl::scale_decompression_settings_adapter<decompression_settings_type>;

			const tracks_header& header = get_tracks_header(tracks);
			const transform_tracks_header& transform_header = get_transform_tracks_header(tracks);

			const rotation_format8 packed_rotation_format = header.get_rotation_format();
			const vector_format8 packed_translation_format = header.get_translation_format();
			const vector_format8 packed_scale_format = header.get_scale_format();
			const rotation_format8 rotation_format = get_rotation_format<decompression_settings_type>(packed_rotation_format);
			const vector_format8 translation_format = get_vector_format<translation_adapter>(packed_translation_format);
			const vector_format8 scale_format = get_vector_format<scale_adapter>(packed_scale_format);

			ACL_ASSERT(rotation_format == packed_rotation_format, "Statically compiled rotation format (%s) differs from the compressed rotation format (%s)!", get_rotation_format_name(rotation_format), get_rotation_format_name(packed_rotation_format));
			ACL_ASSERT(translation_format == packed_translation_format, "Statically compiled translation format (%s) differs from the compressed translation format (%s)!", get_vector_format_name(translation_format), get_vector_format_name(packed_translation_format));
			ACL_ASSERT(scale_format == packed_scale_format, "Statically compiled scale format (%s) differs from the compressed scale format (%s)!", get_vector_format_name(scale_format), get_vector_format_name(packed_scale_format));

			context.tracks = &tracks;
			context.clip_hash = tracks.get_hash();
			context.clip_duration = calculate_duration(header.num_samples, header.sample_rate);
			context.sample_time = -1.0F;
			context.default_tracks_bitset = transform_header.get_default_tracks_bitset();

			context.constant_tracks_bitset = transform_header.get_constant_tracks_bitset();
			context.constant_track_data = transform_header.get_constant_track_data();
			context.clip_range_data = transform_header.get_clip_range_data();

			for (uint32_t key_frame_index = 0; key_frame_index < 2; ++key_frame_index)
			{
				context.format_per_track_data[key_frame_index] = nullptr;
				context.segment_range_data[key_frame_index] = nullptr;
				context.animated_track_data[key_frame_index] = nullptr;
			}

			const bool has_scale = header.get_has_scale();
			const uint32_t num_tracks_per_bone = has_scale ? 3 : 2;
			context.bitset_desc = bitset_description::make_from_num_bits(header.num_tracks * num_tracks_per_bone);

			range_reduction_flags8 range_reduction = range_reduction_flags8::none;
			if (is_rotation_format_variable(rotation_format))
				range_reduction |= range_reduction_flags8::rotations;
			if (is_vector_format_variable(translation_format))
				range_reduction |= range_reduction_flags8::translations;
			if (is_vector_format_variable(scale_format))
				range_reduction |= range_reduction_flags8::scales;

			context.rotation_format = rotation_format;
			context.translation_format = translation_format;
			context.scale_format = scale_format;
			context.range_reduction = range_reduction;
			context.num_rotation_components = rotation_format == rotation_format8::quatf_full ? 4 : 3;
			context.has_segments = transform_header.num_segments > 1;

			return true;
		}

		inline bool is_dirty_v0(const persistent_transform_decompression_context_v0& context, const compressed_tracks& tracks)
		{
			if (context.tracks != &tracks)
				return true;

			if (context.clip_hash != tracks.get_hash())
				return true;

			return false;
		}

		template<class decompression_settings_type>
		inline void seek_v0(persistent_transform_decompression_context_v0& context, float sample_time, sample_rounding_policy rounding_policy)
		{
			ACL_ASSERT(context.is_initialized(), "Context is not initialized");
			ACL_ASSERT(rtm::scalar_is_finite(sample_time), "Invalid sample time");

			if (!context.is_initialized())
				return;	// Context is not initialized

			// Clamp for safety, the caller should normally handle this but in practice, it often isn't the case
			if (decompression_settings_type::clamp_sample_time())
				sample_time = rtm::scalar_clamp(sample_time, 0.0F, context.clip_duration);

			if (context.sample_time == sample_time)
				return;

			context.sample_time = sample_time;

			const tracks_header& header = get_tracks_header(*context.tracks);
			const transform_tracks_header& transform_header = get_transform_tracks_header(*context.tracks);

			uint32_t key_frame0;
			uint32_t key_frame1;
			find_linear_interpolation_samples_with_sample_rate(header.num_samples, header.sample_rate, sample_time, rounding_policy, key_frame0, key_frame1, context.interpolation_alpha);

			uint32_t segment_key_frame0;
			uint32_t segment_key_frame1;

			const segment_header* segment_header0;
			const segment_header* segment_header1;

			const segment_header* segment_headers = transform_header.get_segment_headers();
			const uint32_t num_segments = transform_header.num_segments;

			if (num_segments == 1)
			{
				// Key frame 0 and 1 are in the only segment present
				// This is a really common case and when it happens, we don't store the segment start index (zero)
				segment_header0 = segment_headers;
				segment_key_frame0 = key_frame0;

				segment_header1 = segment_headers;
				segment_key_frame1 = key_frame1;
			}
			else
			{
				const uint32_t* segment_start_indices = transform_header.get_segment_start_indices();

				// See segment_streams(..) for implementation details. This implementation is directly tied to it.
				const uint32_t approx_num_samples_per_segment = header.num_samples / num_segments;	// TODO: Store in header?
				const uint32_t approx_segment_index = key_frame0 / approx_num_samples_per_segment;

				uint32_t segment_index0 = 0;
				uint32_t segment_index1 = 0;

				// Our approximate segment guess is just that, a guess. The actual segments we need could be just before or after.
				// We start looking one segment earlier and up to 2 after. If we have too few segments after, we will hit the
				// sentinel value of 0xFFFFFFFF and exit the loop.
				// TODO: Can we do this with SIMD? Load all 4 values, set key_frame0, compare, move mask, count leading zeroes
				const uint32_t start_segment_index = approx_segment_index > 0 ? (approx_segment_index - 1) : 0;
				const uint32_t end_segment_index = start_segment_index + 4;

				for (uint32_t segment_index = start_segment_index; segment_index < end_segment_index; ++segment_index)
				{
					if (key_frame0 < segment_start_indices[segment_index])
					{
						// We went too far, use previous segment
						ACL_ASSERT(segment_index > 0, "Invalid segment index: %u", segment_index);
						segment_index0 = segment_index - 1;
						segment_index1 = key_frame1 < segment_start_indices[segment_index] ? segment_index0 : segment_index;
						break;
					}
				}

				segment_header0 = segment_headers + segment_index0;
				segment_header1 = segment_headers + segment_index1;

				segment_key_frame0 = key_frame0 - segment_start_indices[segment_index0];
				segment_key_frame1 = key_frame1 - segment_start_indices[segment_index1];
			}

			context.format_per_track_data[0] = transform_header.get_format_per_track_data(*segment_header0);
			context.format_per_track_data[1] = transform_header.get_format_per_track_data(*segment_header1);
			context.segment_range_data[0] = transform_header.get_segment_range_data(*segment_header0);
			context.segment_range_data[1] = transform_header.get_segment_range_data(*segment_header1);
			context.animated_track_data[0] = transform_header.get_track_data(*segment_header0);
			context.animated_track_data[1] = transform_header.get_track_data(*segment_header1);

			context.key_frame_bit_offsets[0] = segment_key_frame0 * segment_header0->animated_pose_bit_size;
			context.key_frame_bit_offsets[1] = segment_key_frame1 * segment_header1->animated_pose_bit_size;
		}

		template<class decompression_settings_type, class track_writer_type>
		inline void decompress_tracks_v0(persistent_transform_decompression_context_v0& context, track_writer_type& writer)
		{
			static_assert(std::is_base_of<track_writer, track_writer_type>::value, "track_writer_type must derive from track_writer");
			ACL_ASSERT(context.is_initialized(), "Context is not initialized");
			ACL_ASSERT(context.sample_time >= 0.0f, "Context not set to a valid sample time");

			if (!context.is_initialized())
				return;	// Context is not initialized

			if (context.sample_time < 0.0F)
				return;	// Invalid sample time, we didn't seek yet

			// Due to the SIMD operations, we sometimes overflow in the SIMD lanes not used.
			// Disable floating point exceptions to avoid issues.
			fp_environment fp_env;
			if (decompression_settings_type::disable_fp_exeptions())
				disable_fp_exceptions(fp_env);

			const tracks_header& header = get_tracks_header(*context.tracks);

			using translation_adapter = acl_impl::translation_decompression_settings_adapter<decompression_settings_type>;
			using scale_adapter = acl_impl::scale_decompression_settings_adapter<decompression_settings_type>;

			const rtm::vector4f default_translation = rtm::vector_zero();
			const rtm::vector4f default_scale = rtm::vector_set(float(header.get_default_scale()));
			const bool has_scale = header.get_has_scale();

			sampling_context_v0 sampling_context_;
			sampling_context_.track_index = 0;
			sampling_context_.constant_track_data_offset = 0;
			sampling_context_.clip_range_data_offset = 0;
			sampling_context_.format_per_track_data_offset = 0;
			sampling_context_.segment_range_data_offset = 0;
			sampling_context_.key_frame_bit_offsets[0] = context.key_frame_bit_offsets[0];
			sampling_context_.key_frame_bit_offsets[1] = context.key_frame_bit_offsets[1];

			sampling_context_.vectors[0] = default_translation;	// Init with something to avoid GCC warning
			sampling_context_.vectors[1] = default_translation;	// Init with something to avoid GCC warning

			const uint32_t num_tracks = header.num_tracks;
			for (uint32_t track_index = 0; track_index < num_tracks; ++track_index)
			{
				if (track_writer_type::skip_all_rotations() || writer.skip_track_rotation(track_index))
					skip_over_rotation<decompression_settings_type>(context, sampling_context_);
				else
				{
					const rtm::quatf rotation = decompress_and_interpolate_rotation<decompression_settings_type>(context, sampling_context_);
					writer.write_rotation(track_index, rotation);
				}

				if (track_writer_type::skip_all_translations() || writer.skip_track_translation(track_index))
					skip_over_vector<translation_adapter>(context, sampling_context_);
				else
				{
					const rtm::vector4f translation = decompress_and_interpolate_vector<translation_adapter>(context, default_translation, sampling_context_);
					writer.write_translation(track_index, translation);
				}

				if (track_writer_type::skip_all_scales() || writer.skip_track_scale(track_index))
				{
					if (has_scale)
						skip_over_vector<scale_adapter>(context, sampling_context_);
				}
				else
				{
					const rtm::vector4f scale = has_scale ? decompress_and_interpolate_vector<scale_adapter>(context, default_scale, sampling_context_) : default_scale;
					writer.write_scale(track_index, scale);
				}
			}

			if (decompression_settings_type::disable_fp_exeptions())
				restore_fp_exceptions(fp_env);
		}

		template<class decompression_settings_type, class track_writer_type>
		inline void decompress_track_v0(persistent_transform_decompression_context_v0& context, uint32_t track_index, track_writer_type& writer)
		{
			static_assert(std::is_base_of<track_writer, track_writer_type>::value, "track_writer_type must derive from track_writer");
			ACL_ASSERT(context.is_initialized(), "Context is not initialized");
			ACL_ASSERT(context.sample_time >= 0.0f, "Context not set to a valid sample time");

			if (!context.is_initialized())
				return;	// Context is not initialized

			if (context.sample_time < 0.0F)
				return;	// Invalid sample time, we didn't seek yet

			const tracks_header& tracks_header_ = get_tracks_header(*context.tracks);
			ACL_ASSERT(track_index < tracks_header_.num_tracks, "Invalid track index");

			if (track_index >= tracks_header_.num_tracks)
				return;	// Invalid track index

			// Due to the SIMD operations, we sometimes overflow in the SIMD lanes not used.
			// Disable floating point exceptions to avoid issues.
			fp_environment fp_env;
			if (decompression_settings_type::disable_fp_exeptions())
				disable_fp_exceptions(fp_env);

			using translation_adapter = acl_impl::translation_decompression_settings_adapter<decompression_settings_type>;
			using scale_adapter = acl_impl::scale_decompression_settings_adapter<decompression_settings_type>;

			const rtm::vector4f default_translation = rtm::vector_zero();
			const rtm::vector4f default_scale = rtm::vector_set(float(tracks_header_.get_default_scale()));
			const bool has_scale = tracks_header_.get_has_scale();

			sampling_context_v0 sampling_context_;
			sampling_context_.key_frame_bit_offsets[0] = context.key_frame_bit_offsets[0];
			sampling_context_.key_frame_bit_offsets[1] = context.key_frame_bit_offsets[1];

			const rotation_format8 rotation_format = get_rotation_format<decompression_settings_type>(context.rotation_format);
			const vector_format8 translation_format = get_vector_format<translation_adapter>(context.translation_format);
			const vector_format8 scale_format = get_vector_format<scale_adapter>(context.scale_format);

			const bool are_all_tracks_variable = is_rotation_format_variable(rotation_format) && is_vector_format_variable(translation_format) && is_vector_format_variable(scale_format);
			if (!are_all_tracks_variable)
			{
				// Slow path, not optimized yet because it's more complex and shouldn't be used in production anyway
				sampling_context_.track_index = 0;
				sampling_context_.constant_track_data_offset = 0;
				sampling_context_.clip_range_data_offset = 0;
				sampling_context_.format_per_track_data_offset = 0;
				sampling_context_.segment_range_data_offset = 0;

				for (uint32_t bone_index = 0; bone_index < track_index; ++bone_index)
				{
					skip_over_rotation<decompression_settings_type>(context, sampling_context_);
					skip_over_vector<translation_adapter>(context, sampling_context_);

					if (has_scale)
						skip_over_vector<scale_adapter>(context, sampling_context_);
				}
			}
			else
			{
				const uint32_t num_tracks_per_bone = has_scale ? 3 : 2;
				const uint32_t sub_track_index = track_index * num_tracks_per_bone;
				uint32_t num_default_rotations = 0;
				uint32_t num_default_translations = 0;
				uint32_t num_default_scales = 0;
				uint32_t num_constant_rotations = 0;
				uint32_t num_constant_translations = 0;
				uint32_t num_constant_scales = 0;

				if (has_scale)
				{
					uint32_t rotation_track_bit_mask = 0x92492492;		// b100100100..
					uint32_t translation_track_bit_mask = 0x49249249;	// b010010010..
					uint32_t scale_track_bit_mask = 0x24924924;			// b001001001..

					const uint32_t last_offset = sub_track_index / 32;
					uint32_t offset = 0;
					for (; offset < last_offset; ++offset)
					{
						const uint32_t default_value = context.default_tracks_bitset[offset];
						num_default_rotations += count_set_bits(default_value & rotation_track_bit_mask);
						num_default_translations += count_set_bits(default_value & translation_track_bit_mask);
						num_default_scales += count_set_bits(default_value & scale_track_bit_mask);

						const uint32_t constant_value = context.constant_tracks_bitset[offset];
						num_constant_rotations += count_set_bits(constant_value & rotation_track_bit_mask);
						num_constant_translations += count_set_bits(constant_value & translation_track_bit_mask);
						num_constant_scales += count_set_bits(constant_value & scale_track_bit_mask);

						// Because the number of tracks in a 32bit word isn't a multiple of the number of tracks we have (3),
						// we have to rotate the masks left
						rotation_track_bit_mask = rotate_bits_left(rotation_track_bit_mask, 2);
						translation_track_bit_mask = rotate_bits_left(translation_track_bit_mask, 2);
						scale_track_bit_mask = rotate_bits_left(scale_track_bit_mask, 2);
					}

					const uint32_t remaining_tracks = sub_track_index % 32;
					if (remaining_tracks != 0)
					{
						const uint32_t not_up_to_track_mask = ((1 << (32 - remaining_tracks)) - 1);
						const uint32_t default_value = and_not(not_up_to_track_mask, context.default_tracks_bitset[offset]);
						num_default_rotations += count_set_bits(default_value & rotation_track_bit_mask);
						num_default_translations += count_set_bits(default_value & translation_track_bit_mask);
						num_default_scales += count_set_bits(default_value & scale_track_bit_mask);

						const uint32_t constant_value = and_not(not_up_to_track_mask, context.constant_tracks_bitset[offset]);
						num_constant_rotations += count_set_bits(constant_value & rotation_track_bit_mask);
						num_constant_translations += count_set_bits(constant_value & translation_track_bit_mask);
						num_constant_scales += count_set_bits(constant_value & scale_track_bit_mask);
					}
				}
				else
				{
					const uint32_t rotation_track_bit_mask = 0xAAAAAAAA;		// b10101010..
					const uint32_t translation_track_bit_mask = 0x55555555;		// b01010101..

					const uint32_t last_offset = sub_track_index / 32;
					uint32_t offset = 0;
					for (; offset < last_offset; ++offset)
					{
						const uint32_t default_value = context.default_tracks_bitset[offset];
						num_default_rotations += count_set_bits(default_value & rotation_track_bit_mask);
						num_default_translations += count_set_bits(default_value & translation_track_bit_mask);

						const uint32_t constant_value = context.constant_tracks_bitset[offset];
						num_constant_rotations += count_set_bits(constant_value & rotation_track_bit_mask);
						num_constant_translations += count_set_bits(constant_value & translation_track_bit_mask);
					}

					const uint32_t remaining_tracks = sub_track_index % 32;
					if (remaining_tracks != 0)
					{
						const uint32_t not_up_to_track_mask = ((1 << (32 - remaining_tracks)) - 1);
						const uint32_t default_value = and_not(not_up_to_track_mask, context.default_tracks_bitset[offset]);
						num_default_rotations += count_set_bits(default_value & rotation_track_bit_mask);
						num_default_translations += count_set_bits(default_value & translation_track_bit_mask);

						const uint32_t constant_value = and_not(not_up_to_track_mask, context.constant_tracks_bitset[offset]);
						num_constant_rotations += count_set_bits(constant_value & rotation_track_bit_mask);
						num_constant_translations += count_set_bits(constant_value & translation_track_bit_mask);
					}
				}

				// Tracks that are default are also constant
				const uint32_t num_animated_rotations = track_index - num_constant_rotations;
				const uint32_t num_animated_translations = track_index - num_constant_translations;

				const rotation_format8 packed_rotation_format = is_rotation_format_variable(rotation_format) ? get_highest_variant_precision(get_rotation_variant(rotation_format)) : rotation_format;
				const uint32_t packed_rotation_size = get_packed_rotation_size(packed_rotation_format);

				uint32_t constant_track_data_offset = (num_constant_rotations - num_default_rotations) * packed_rotation_size;
				constant_track_data_offset += (num_constant_translations - num_default_translations) * get_packed_vector_size(vector_format8::vector3f_full);

				uint32_t clip_range_data_offset = 0;
				uint32_t segment_range_data_offset = 0;

				const range_reduction_flags8 range_reduction = context.range_reduction;
				if (are_any_enum_flags_set(range_reduction, range_reduction_flags8::rotations))
				{
					clip_range_data_offset += context.num_rotation_components * sizeof(float) * 2 * num_animated_rotations;

					if (context.has_segments)
						segment_range_data_offset += context.num_rotation_components * k_segment_range_reduction_num_bytes_per_component * 2 * num_animated_rotations;
				}

				if (are_any_enum_flags_set(range_reduction, range_reduction_flags8::translations))
				{
					clip_range_data_offset += k_clip_range_reduction_vector3_range_size * num_animated_translations;

					if (context.has_segments)
						segment_range_data_offset += 3 * k_segment_range_reduction_num_bytes_per_component * 2 * num_animated_translations;
				}

				uint32_t num_animated_tracks = num_animated_rotations + num_animated_translations;
				if (has_scale)
				{
					const uint32_t num_animated_scales = track_index - num_constant_scales;
					num_animated_tracks += num_animated_scales;

					constant_track_data_offset += (num_constant_scales - num_default_scales) * get_packed_vector_size(vector_format8::vector3f_full);

					if (are_any_enum_flags_set(range_reduction, range_reduction_flags8::scales))
					{
						clip_range_data_offset += k_clip_range_reduction_vector3_range_size * num_animated_scales;

						if (context.has_segments)
							segment_range_data_offset += 3 * k_segment_range_reduction_num_bytes_per_component * 2 * num_animated_scales;
					}
				}

				sampling_context_.track_index = sub_track_index;
				sampling_context_.constant_track_data_offset = constant_track_data_offset;
				sampling_context_.clip_range_data_offset = clip_range_data_offset;
				sampling_context_.segment_range_data_offset = segment_range_data_offset;
				sampling_context_.format_per_track_data_offset = num_animated_tracks;

				for (uint32_t animated_track_index = 0; animated_track_index < num_animated_tracks; ++animated_track_index)
				{
					const uint8_t bit_rate0 = context.format_per_track_data[0][animated_track_index];
					const uint32_t num_bits_at_bit_rate0 = get_num_bits_at_bit_rate(bit_rate0) * 3;	// 3 components

					sampling_context_.key_frame_bit_offsets[0] += num_bits_at_bit_rate0;

					const uint8_t bit_rate1 = context.format_per_track_data[1][animated_track_index];
					const uint32_t num_bits_at_bit_rate1 = get_num_bits_at_bit_rate(bit_rate1) * 3;	// 3 components

					sampling_context_.key_frame_bit_offsets[1] += num_bits_at_bit_rate1;
				}
			}

			sampling_context_.vectors[0] = default_translation;	// Init with something to avoid GCC warning
			sampling_context_.vectors[1] = default_translation;	// Init with something to avoid GCC warning

			const rtm::quatf rotation = decompress_and_interpolate_rotation<decompression_settings_type>(context, sampling_context_);
			writer.write_rotation(track_index, rotation);

			const rtm::vector4f translation = decompress_and_interpolate_vector<translation_adapter>(context, default_translation, sampling_context_);
			writer.write_translation(track_index, translation);

			const rtm::vector4f scale = has_scale ? decompress_and_interpolate_vector<scale_adapter>(context, default_scale, sampling_context_) : default_scale;
			writer.write_scale(track_index, scale);

			if (decompression_settings_type::disable_fp_exeptions())
				restore_fp_exceptions(fp_env);
		}
	}
}

ACL_IMPL_FILE_PRAGMA_POP
