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

ACL_IMPL_FILE_PRAGMA_PUSH

namespace acl
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

	template<size_t num_key_frames, class SettingsType, class DecompressionContextType, class SamplingContextType>
	//ACL_DEPRECATED("Use skip_over_rotation instead, to be removed in v2.0")
	inline void skip_rotations(const SettingsType& settings, const ClipHeader& header, const DecompressionContextType& decomp_context, SamplingContextType& sampling_context)
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

	template<class SettingsType, class DecompressionContextType, class SamplingContextType>
	ACL_DEPRECATED("Use skip_over_rotation instead, to be removed in v2.0")
	inline void skip_rotation(const SettingsType& settings, const ClipHeader& header, const DecompressionContextType& decomp_context, SamplingContextType& sampling_context)
	{
		skip_rotations<1>(settings, header, decomp_context, sampling_context);
	}

	template<class SettingsType, class DecompressionContextType, class SamplingContextType>
	ACL_DEPRECATED("Use skip_over_rotation instead, to be removed in v2.0")
	inline void skip_rotations_in_two_key_frames(const SettingsType& settings, const ClipHeader& header, const DecompressionContextType& decomp_context, SamplingContextType& sampling_context)
	{
		skip_rotations<2>(settings, header, decomp_context, sampling_context);
	}

	template<class SettingsType, class DecompressionContextType, class SamplingContextType>
	ACL_DEPRECATED("Use skip_over_rotation instead, to be removed in v2.0")
	inline void skip_rotations_in_four_key_frames(const SettingsType& settings, const ClipHeader& header, const DecompressionContextType& decomp_context, SamplingContextType& sampling_context)
	{
		skip_rotations<4>(settings, header, decomp_context, sampling_context);
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

	template<size_t num_key_frames, class SettingsAdapterType, class DecompressionContextType, class SamplingContextType>
	//ACL_DEPRECATED("Use skip_over_vector instead, to be removed in v2.0")
	inline void skip_vectors(const SettingsAdapterType& settings, const ClipHeader& header, const DecompressionContextType& decomp_context, SamplingContextType& sampling_context)
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

	template<class SettingsAdapterType, class DecompressionContextType, class SamplingContextType>
	ACL_DEPRECATED("Use skip_over_vector instead, to be removed in v2.0")
	inline void skip_vector(const SettingsAdapterType& settings, const ClipHeader& header, const DecompressionContextType& decomp_context, SamplingContextType& sampling_context)
	{
		skip_vectors<1>(settings, header, decomp_context, sampling_context);
	}

	template<class SettingsAdapterType, class DecompressionContextType, class SamplingContextType>
	ACL_DEPRECATED("Use skip_over_vector instead, to be removed in v2.0")
	inline void skip_vectors_in_two_key_frames(const SettingsAdapterType& settings, const ClipHeader& header, const DecompressionContextType& decomp_context, SamplingContextType& sampling_context)
	{
		skip_vectors<2>(settings, header, decomp_context, sampling_context);
	}

	template<class SettingsAdapterType, class DecompressionContextType, class SamplingContextType>
	ACL_DEPRECATED("Use skip_over_vector instead, to be removed in v2.0")
	inline void skip_vectors_in_four_key_frames(const SettingsAdapterType& settings, const ClipHeader& header, const DecompressionContextType& decomp_context, SamplingContextType& sampling_context)
	{
		skip_vectors<4>(settings, header, decomp_context, sampling_context);
	}

	template<size_t num_key_frames, class SettingsType, class DecompressionContextType, class SamplingContextType>
	//ACL_DEPRECATED("Use decompress_and_interpolate_rotation instead, to be removed in v2.0")
	inline TimeSeriesType8 decompress_rotations(const SettingsType& settings, const ClipHeader& header, const DecompressionContextType& decomp_context, SamplingContextType& sampling_context, Quat_32* out_rotations)
	{
		TimeSeriesType8 time_series_type;

		const BitSetIndexRef track_index_bit_ref(decomp_context.bitset_desc, sampling_context.track_index);
		const bool is_sample_default = bitset_test(decomp_context.default_tracks_bitset, track_index_bit_ref);
		if (is_sample_default)
		{
			out_rotations[0] = quat_identity_32();
			time_series_type = TimeSeriesType8::ConstantDefault;
		}
		else
		{
			const RotationFormat8 rotation_format = settings.get_rotation_format(header.rotation_format);
			const bool is_sample_constant = bitset_test(decomp_context.constant_tracks_bitset, track_index_bit_ref);
			if (is_sample_constant)
			{
				Quat_32 rotation;

				if (rotation_format == RotationFormat8::Quat_128 && settings.is_rotation_format_supported(RotationFormat8::Quat_128))
					rotation = unpack_quat_128(decomp_context.constant_track_data + sampling_context.constant_track_data_offset);
				else if (rotation_format == RotationFormat8::QuatDropW_96 && settings.is_rotation_format_supported(RotationFormat8::QuatDropW_96))
					rotation = unpack_quat_96_unsafe(decomp_context.constant_track_data + sampling_context.constant_track_data_offset);
				else if (rotation_format == RotationFormat8::QuatDropW_48 && settings.is_rotation_format_supported(RotationFormat8::QuatDropW_48))
					rotation = unpack_quat_48(decomp_context.constant_track_data + sampling_context.constant_track_data_offset);
				else if (rotation_format == RotationFormat8::QuatDropW_32 && settings.is_rotation_format_supported(RotationFormat8::QuatDropW_32))
					rotation = unpack_quat_32(decomp_context.constant_track_data + sampling_context.constant_track_data_offset);
				else if (rotation_format == RotationFormat8::QuatDropW_Variable && settings.is_rotation_format_supported(RotationFormat8::QuatDropW_Variable))
					rotation = unpack_quat_96_unsafe(decomp_context.constant_track_data + sampling_context.constant_track_data_offset);
				else
				{
					ACL_ASSERT(false, "Unrecognized rotation format");
					rotation = quat_identity_32();
				}

				out_rotations[0] = rotation;
				time_series_type = TimeSeriesType8::Constant;

				const RotationFormat8 packed_format = is_rotation_format_variable(rotation_format) ? get_highest_variant_precision(get_rotation_variant(rotation_format)) : rotation_format;
				sampling_context.constant_track_data_offset += get_packed_rotation_size(packed_format);
			}
			else
			{
				const RangeReductionFlags8 clip_range_reduction = settings.get_clip_range_reduction(header.clip_range_reduction);
				const RangeReductionFlags8 segment_range_reduction = settings.get_segment_range_reduction(header.segment_range_reduction);
				const bool are_clip_rotations_normalized = are_any_enum_flags_set(clip_range_reduction, RangeReductionFlags8::Rotations);
				const bool are_segment_rotations_normalized = are_any_enum_flags_set(segment_range_reduction, RangeReductionFlags8::Rotations);

				Vector4_32 rotations[num_key_frames];
				bool ignore_clip_range[num_key_frames] = { false };
				bool ignore_segment_range[num_key_frames] = { false };

				if (rotation_format == RotationFormat8::QuatDropW_Variable && settings.is_rotation_format_supported(RotationFormat8::QuatDropW_Variable))
				{
					for (size_t i = 0; i < num_key_frames; ++i)
					{
						const uint8_t bit_rate = decomp_context.format_per_track_data[i][sampling_context.format_per_track_data_offset];
						const uint8_t num_bits_at_bit_rate = get_num_bits_at_bit_rate(bit_rate);

						if (is_constant_bit_rate(bit_rate))
						{
							rotations[i] = unpack_vector3_u48_unsafe(decomp_context.segment_range_data[i] + sampling_context.segment_range_data_offset);
							ignore_segment_range[i] = true;
						}
						else if (is_raw_bit_rate(bit_rate))
						{
							rotations[i] = unpack_vector3_96_unsafe(decomp_context.animated_track_data[i], sampling_context.key_frame_bit_offsets[i]);
							ignore_clip_range[i] = true;
							ignore_segment_range[i] = true;
						}
						else
						{
							if (are_clip_rotations_normalized)
								rotations[i] = unpack_vector3_uXX_unsafe(num_bits_at_bit_rate, decomp_context.animated_track_data[i], sampling_context.key_frame_bit_offsets[i]);
							else
								rotations[i] = unpack_vector3_sXX_unsafe(num_bits_at_bit_rate, decomp_context.animated_track_data[i], sampling_context.key_frame_bit_offsets[i]);
						}

						uint8_t num_bits_read = num_bits_at_bit_rate * 3;

						if (settings.supports_mixed_packing() && decomp_context.has_mixed_packing)
							num_bits_read = align_to(num_bits_read, k_mixed_packing_alignment_num_bits);

						sampling_context.key_frame_bit_offsets[i] += num_bits_read;

						if (settings.supports_mixed_packing() && decomp_context.has_mixed_packing)
							sampling_context.key_frame_byte_offsets[i] = sampling_context.key_frame_bit_offsets[i] / 8;
					}

					++sampling_context.format_per_track_data_offset;
				}
				else
				{
					if (rotation_format == RotationFormat8::Quat_128 && settings.is_rotation_format_supported(RotationFormat8::Quat_128))
					{
						for (size_t i = 0; i < num_key_frames; ++i)
							rotations[i] = unpack_vector4_128(decomp_context.animated_track_data[i] + sampling_context.key_frame_byte_offsets[i]);
					}
					else if (rotation_format == RotationFormat8::QuatDropW_96 && settings.is_rotation_format_supported(RotationFormat8::QuatDropW_96))
					{
						for (size_t i = 0; i < num_key_frames; ++i)
							rotations[i] = unpack_vector3_96_unsafe(decomp_context.animated_track_data[i] + sampling_context.key_frame_byte_offsets[i]);
					}
					else if (rotation_format == RotationFormat8::QuatDropW_48 && settings.is_rotation_format_supported(RotationFormat8::QuatDropW_48))
					{
						for (size_t i = 0; i < num_key_frames; ++i)
						{
							if (are_clip_rotations_normalized)
								rotations[i] = unpack_vector3_u48_unsafe(decomp_context.animated_track_data[i] + sampling_context.key_frame_byte_offsets[i]);
							else
								rotations[i] = unpack_vector3_s48_unsafe(decomp_context.animated_track_data[i] + sampling_context.key_frame_byte_offsets[i]);
						}
					}
					else if (rotation_format == RotationFormat8::QuatDropW_32 && settings.is_rotation_format_supported(RotationFormat8::QuatDropW_32))
					{
						for (size_t i = 0; i < num_key_frames; ++i)
							rotations[i] = unpack_vector3_32(11, 11, 10, are_clip_rotations_normalized, decomp_context.animated_track_data[i] + sampling_context.key_frame_byte_offsets[i]);
					}

					const uint32_t rotation_size = get_packed_rotation_size(rotation_format);

					for (size_t i = 0; i < num_key_frames; ++i)
					{
						sampling_context.key_frame_byte_offsets[i] += rotation_size;

						if (settings.supports_mixed_packing() && decomp_context.has_mixed_packing)
							sampling_context.key_frame_bit_offsets[i] = sampling_context.key_frame_byte_offsets[i] * 8;
					}
				}

				if (are_segment_rotations_normalized)
				{
					if (rotation_format == RotationFormat8::QuatDropW_Variable && settings.is_rotation_format_supported(RotationFormat8::QuatDropW_Variable))
					{
						for (size_t i = 0; i < num_key_frames; ++i)
						{
							if (!ignore_segment_range[i])
							{
								const Vector4_32 segment_range_min = unpack_vector3_u24_unsafe(decomp_context.segment_range_data[i] + sampling_context.segment_range_data_offset);
								const Vector4_32 segment_range_extent = unpack_vector3_u24_unsafe(decomp_context.segment_range_data[i] + sampling_context.segment_range_data_offset + (decomp_context.num_rotation_components * sizeof(uint8_t)));

								rotations[i] = vector_mul_add(rotations[i], segment_range_extent, segment_range_min);
							}
						}
					}
					else
					{
						if (rotation_format == RotationFormat8::Quat_128 && settings.is_rotation_format_supported(RotationFormat8::Quat_128))
						{
							for (size_t i = 0; i < num_key_frames; ++i)
							{
								const Vector4_32 segment_range_min = unpack_vector4_32(decomp_context.segment_range_data[i] + sampling_context.segment_range_data_offset, true);
								const Vector4_32 segment_range_extent = unpack_vector4_32(decomp_context.segment_range_data[i] + sampling_context.segment_range_data_offset + (decomp_context.num_rotation_components * sizeof(uint8_t)), true);

								rotations[i] = vector_mul_add(rotations[i], segment_range_extent, segment_range_min);
							}
						}
						else
						{
							for (size_t i = 0; i < num_key_frames; ++i)
							{
								const Vector4_32 segment_range_min = unpack_vector3_u24_unsafe(decomp_context.segment_range_data[i] + sampling_context.segment_range_data_offset);
								const Vector4_32 segment_range_extent = unpack_vector3_u24_unsafe(decomp_context.segment_range_data[i] + sampling_context.segment_range_data_offset + (decomp_context.num_rotation_components * sizeof(uint8_t)));

								rotations[i] = vector_mul_add(rotations[i], segment_range_extent, segment_range_min);
							}
						}
					}

					sampling_context.segment_range_data_offset += decomp_context.num_rotation_components * k_segment_range_reduction_num_bytes_per_component * 2;
				}

				if (are_clip_rotations_normalized)
				{
					const Vector4_32 clip_range_min = vector_unaligned_load_32(decomp_context.clip_range_data + sampling_context.clip_range_data_offset);
					const Vector4_32 clip_range_extent = vector_unaligned_load_32(decomp_context.clip_range_data + sampling_context.clip_range_data_offset + (decomp_context.num_rotation_components * sizeof(float)));

					for (size_t i = 0; i < num_key_frames; ++i)
					{
						if (!ignore_clip_range[i])
							rotations[i] = vector_mul_add(rotations[i], clip_range_extent, clip_range_min);
					}

					sampling_context.clip_range_data_offset += decomp_context.num_rotation_components * sizeof(float) * 2;
				}

				if (rotation_format == RotationFormat8::Quat_128 && settings.is_rotation_format_supported(RotationFormat8::Quat_128))
				{
					for (size_t i = 0; i < num_key_frames; ++i)
						out_rotations[i] = vector_to_quat(rotations[i]);
				}
				else
				{
					for (size_t i = 0; i < num_key_frames; ++i)
						out_rotations[i] = quat_from_positive_w(rotations[i]);
				}

				time_series_type = TimeSeriesType8::Varying;
			}
		}

		sampling_context.track_index++;
		return time_series_type;
	}

	template<class SettingsType, class DecompressionContextType, class SamplingContextType>
	ACL_DEPRECATED("Use decompress_and_interpolate_rotation instead, to be removed in v2.0")
	inline TimeSeriesType8 decompress_rotation(const SettingsType& settings, const ClipHeader& header, const DecompressionContextType& decomp_context, SamplingContextType& sampling_context, Quat_32* out_rotations)
	{
		return decompress_rotations<1>(settings, header, decomp_context, sampling_context, out_rotations);
	}

	template<class SettingsType, class DecompressionContextType, class SamplingContextType>
	ACL_DEPRECATED("Use decompress_and_interpolate_rotation instead, to be removed in v2.0")
	inline TimeSeriesType8 decompress_rotations_in_two_key_frames(const SettingsType& settings, const ClipHeader& header, const DecompressionContextType& decomp_context, SamplingContextType& sampling_context, Quat_32* out_rotations)
	{
		return decompress_rotations<2>(settings, header, decomp_context, sampling_context, out_rotations);
	}

	template<class SettingsType, class DecompressionContextType, class SamplingContextType>
	ACL_DEPRECATED("Use decompress_and_interpolate_rotation instead, to be removed in v2.0")
	inline TimeSeriesType8 decompress_rotations_in_four_key_frames(const SettingsType& settings, const ClipHeader& header, const DecompressionContextType& decomp_context, SamplingContextType& sampling_context, Quat_32* out_rotations)
	{
		return decompress_rotations<4>(settings, header, decomp_context, sampling_context, out_rotations);
	}

	template<size_t num_key_frames, class SettingsAdapterType, class DecompressionContextType, class SamplingContextType>
	//ACL_DEPRECATED("Use decompress_and_interpolate_vector instead, to be removed in v2.0")
	inline TimeSeriesType8 decompress_vectors(const SettingsAdapterType& settings, const ClipHeader& header, const DecompressionContextType& decomp_context, SamplingContextType& sampling_context, Vector4_32* out_vectors)
	{
		TimeSeriesType8 time_series_type;

		const BitSetIndexRef track_index_bit_ref(decomp_context.bitset_desc, sampling_context.track_index);
		const bool is_sample_default = bitset_test(decomp_context.default_tracks_bitset, track_index_bit_ref);
		if (is_sample_default)
		{
			out_vectors[0] = settings.get_default_value();
			time_series_type = TimeSeriesType8::ConstantDefault;
		}
		else
		{
			const bool is_sample_constant = bitset_test(decomp_context.constant_tracks_bitset, track_index_bit_ref);
			if (is_sample_constant)
			{
				// Constant translation tracks store the remaining sample with full precision
				out_vectors[0] = unpack_vector3_96_unsafe(decomp_context.constant_track_data + sampling_context.constant_track_data_offset);
				time_series_type = TimeSeriesType8::Constant;

				sampling_context.constant_track_data_offset += get_packed_vector_size(VectorFormat8::Vector3_96);
			}
			else
			{
				const VectorFormat8 format = settings.get_vector_format(header);
				const RangeReductionFlags8 clip_range_reduction = settings.get_clip_range_reduction(header.clip_range_reduction);
				const RangeReductionFlags8 segment_range_reduction = settings.get_segment_range_reduction(header.segment_range_reduction);

				bool ignore_clip_range[num_key_frames] = { false };
				bool ignore_segment_range[num_key_frames] = { false };

				if (format == VectorFormat8::Vector3_Variable && settings.is_vector_format_supported(VectorFormat8::Vector3_Variable))
				{
					for (size_t i = 0; i < num_key_frames; ++i)
					{
						const uint8_t bit_rate = decomp_context.format_per_track_data[i][sampling_context.format_per_track_data_offset];
						const uint8_t num_bits_at_bit_rate = get_num_bits_at_bit_rate(bit_rate);

						if (is_constant_bit_rate(bit_rate))
						{
							out_vectors[i] = unpack_vector3_u48_unsafe(decomp_context.segment_range_data[i] + sampling_context.segment_range_data_offset);
							ignore_segment_range[i] = true;
						}
						else if (is_raw_bit_rate(bit_rate))
						{
							out_vectors[i] = unpack_vector3_96_unsafe(decomp_context.animated_track_data[i], sampling_context.key_frame_bit_offsets[i]);
							ignore_clip_range[i] = true;
							ignore_segment_range[i] = true;
						}
						else
							out_vectors[i] = unpack_vector3_uXX_unsafe(num_bits_at_bit_rate, decomp_context.animated_track_data[i], sampling_context.key_frame_bit_offsets[i]);

						uint8_t num_bits_read = num_bits_at_bit_rate * 3;

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
							out_vectors[i] = unpack_vector3_96_unsafe(decomp_context.animated_track_data[i] + sampling_context.key_frame_byte_offsets[i]);
					}
					else if (format == VectorFormat8::Vector3_48 && settings.is_vector_format_supported(VectorFormat8::Vector3_48))
					{
						for (size_t i = 0; i < num_key_frames; ++i)
							out_vectors[i] = unpack_vector3_u48_unsafe(decomp_context.animated_track_data[i] + sampling_context.key_frame_byte_offsets[i]);
					}
					else if (format == VectorFormat8::Vector3_32 && settings.is_vector_format_supported(VectorFormat8::Vector3_32))
					{
						for (size_t i = 0; i < num_key_frames; ++i)
							out_vectors[i] = unpack_vector3_32(11, 11, 10, true, decomp_context.animated_track_data[i] + sampling_context.key_frame_byte_offsets[i]);
					}

					const uint32_t sample_size = get_packed_vector_size(format);

					for (size_t i = 0; i < num_key_frames; ++i)
					{
						sampling_context.key_frame_byte_offsets[i] += sample_size;

						if (settings.supports_mixed_packing() && decomp_context.has_mixed_packing)
							sampling_context.key_frame_bit_offsets[i] = sampling_context.key_frame_byte_offsets[i] * 8;
					}
				}

				const RangeReductionFlags8 range_reduction_flag = settings.get_range_reduction_flag();
				if (are_any_enum_flags_set(segment_range_reduction, range_reduction_flag))
				{
					for (size_t i = 0; i < num_key_frames; ++i)
					{
						if (format != VectorFormat8::Vector3_Variable || !settings.is_vector_format_supported(VectorFormat8::Vector3_Variable) || !ignore_segment_range[i])
						{
							const Vector4_32 segment_range_min = unpack_vector3_u24_unsafe(decomp_context.segment_range_data[i] + sampling_context.segment_range_data_offset);
							const Vector4_32 segment_range_extent = unpack_vector3_u24_unsafe(decomp_context.segment_range_data[i] + sampling_context.segment_range_data_offset + (3 * sizeof(uint8_t)));

							out_vectors[i] = vector_mul_add(out_vectors[i], segment_range_extent, segment_range_min);
						}
					}

					sampling_context.segment_range_data_offset += 3 * k_segment_range_reduction_num_bytes_per_component * 2;
				}

				if (are_any_enum_flags_set(clip_range_reduction, range_reduction_flag))
				{
					const Vector4_32 clip_range_min = unpack_vector3_96_unsafe(decomp_context.clip_range_data + sampling_context.clip_range_data_offset);
					const Vector4_32 clip_range_extent = unpack_vector3_96_unsafe(decomp_context.clip_range_data + sampling_context.clip_range_data_offset + (3 * sizeof(float)));

					for (size_t i = 0; i < num_key_frames; ++i)
					{
						if (!ignore_clip_range[i])
							out_vectors[i] = vector_mul_add(out_vectors[i], clip_range_extent, clip_range_min);
					}

					sampling_context.clip_range_data_offset += k_clip_range_reduction_vector3_range_size;
				}

				time_series_type = TimeSeriesType8::Varying;
			}
		}

		sampling_context.track_index++;
		return time_series_type;
	}

	template<class SettingsAdapterType, class DecompressionContextType, class SamplingContextType>
	ACL_DEPRECATED("Use decompress_and_interpolate_vector instead, to be removed in v2.0")
	inline TimeSeriesType8 decompress_vector(const SettingsAdapterType& settings, const ClipHeader& header, const DecompressionContextType& decomp_context, SamplingContextType& sampling_context, Vector4_32* out_vectors)
	{
		return decompress_vectors<1>(settings, header, decomp_context, sampling_context, out_vectors);
	}

	template<class SettingsAdapterType, class DecompressionContextType, class SamplingContextType>
	ACL_DEPRECATED("Use decompress_and_interpolate_vector instead, to be removed in v2.0")
	inline TimeSeriesType8 decompress_vectors_in_two_key_frames(const SettingsAdapterType& settings, const ClipHeader& header, const DecompressionContextType& decomp_context, SamplingContextType& sampling_context, Vector4_32* out_vectors)
	{
		return decompress_vectors<2>(settings, header, decomp_context, sampling_context, out_vectors);
	}

	template<class SettingsAdapterType, class DecompressionContextType, class SamplingContextType>
	ACL_DEPRECATED("Use decompress_and_interpolate_vector instead, to be removed in v2.0")
	inline TimeSeriesType8 decompress_vectors_in_four_key_frames(const SettingsAdapterType& settings, const ClipHeader& header, const DecompressionContextType& decomp_context, SamplingContextType& sampling_context, Vector4_32* out_vectors)
	{
		return decompress_vectors<4>(settings, header, decomp_context, sampling_context, out_vectors);
	}

	template <class SettingsType, class DecompressionContextType, class SamplingContextType>
	inline Quat_32 ACL_SIMD_CALL decompress_and_interpolate_rotation(const SettingsType& settings, const ClipHeader& header, const DecompressionContextType& decomp_context, SamplingContextType& sampling_context)
	{
		Quat_32 interpolated_rotation;

		const BitSetIndexRef track_index_bit_ref(decomp_context.bitset_desc, sampling_context.track_index);
		const bool is_sample_default = bitset_test(decomp_context.default_tracks_bitset, track_index_bit_ref);
		if (is_sample_default)
		{
			interpolated_rotation = quat_identity_32();
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
					interpolated_rotation = quat_identity_32();
				}

				ACL_ASSERT(quat_is_finite(interpolated_rotation), "Rotation is not valid!");
				ACL_ASSERT(quat_is_normalized(interpolated_rotation), "Rotation is not normalized!");

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

				Vector4_32 rotations_as_vec[num_key_frames];

				// Range ignore flags are used to skip range normalization at the clip and/or segment levels
				// Each sample has two bits like so: sample 0 clip, sample 0 segment, sample 1 clip, sample 1 segment, etc
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
							range_ignore_flags |= 0x00000001u;	// Skip segment only
						}
						else if (is_raw_bit_rate(bit_rate))
						{
							rotations_as_vec[i] = unpack_vector3_96_unsafe(decomp_context.animated_track_data[i], sampling_context.key_frame_bit_offsets[i]);
							range_ignore_flags |= 0x00000003u;	// Skip clip and segment
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

				if (are_segment_rotations_normalized)
				{
					if (rotation_format == RotationFormat8::QuatDropW_Variable && settings.is_rotation_format_supported(RotationFormat8::QuatDropW_Variable))
					{
						uint32_t ignore_bit_mask = 0x00000001u << ((num_key_frames - 1) * 2);
						for (size_t i = 0; i < num_key_frames; ++i)
						{
							if ((range_ignore_flags & ignore_bit_mask) == 0)
							{
								const Vector4_32 segment_range_min = unpack_vector3_u24_unsafe(decomp_context.segment_range_data[i] + sampling_context.segment_range_data_offset);
								const Vector4_32 segment_range_extent = unpack_vector3_u24_unsafe(decomp_context.segment_range_data[i] + sampling_context.segment_range_data_offset + (decomp_context.num_rotation_components * sizeof(uint8_t)));

								rotations_as_vec[i] = vector_mul_add(rotations_as_vec[i], segment_range_extent, segment_range_min);
							}

							ignore_bit_mask >>= 2;
						}
					}
					else
					{
						if (rotation_format == RotationFormat8::Quat_128 && settings.is_rotation_format_supported(RotationFormat8::Quat_128))
						{
							for (size_t i = 0; i < num_key_frames; ++i)
							{
								const Vector4_32 segment_range_min = unpack_vector4_32(decomp_context.segment_range_data[i] + sampling_context.segment_range_data_offset, true);
								const Vector4_32 segment_range_extent = unpack_vector4_32(decomp_context.segment_range_data[i] + sampling_context.segment_range_data_offset + (decomp_context.num_rotation_components * sizeof(uint8_t)), true);

								rotations_as_vec[i] = vector_mul_add(rotations_as_vec[i], segment_range_extent, segment_range_min);
							}
						}
						else
						{
							for (size_t i = 0; i < num_key_frames; ++i)
							{
								const Vector4_32 segment_range_min = unpack_vector3_u24_unsafe(decomp_context.segment_range_data[i] + sampling_context.segment_range_data_offset);
								const Vector4_32 segment_range_extent = unpack_vector3_u24_unsafe(decomp_context.segment_range_data[i] + sampling_context.segment_range_data_offset + (decomp_context.num_rotation_components * sizeof(uint8_t)));

								rotations_as_vec[i] = vector_mul_add(rotations_as_vec[i], segment_range_extent, segment_range_min);
							}
						}
					}

					sampling_context.segment_range_data_offset += decomp_context.num_rotation_components * k_segment_range_reduction_num_bytes_per_component * 2;
				}

				if (are_clip_rotations_normalized)
				{
					const Vector4_32 clip_range_min = vector_unaligned_load_32(decomp_context.clip_range_data + sampling_context.clip_range_data_offset);
					const Vector4_32 clip_range_extent = vector_unaligned_load_32(decomp_context.clip_range_data + sampling_context.clip_range_data_offset + (decomp_context.num_rotation_components * sizeof(float)));

					uint32_t ignore_bit_mask = 0x00000002u << ((num_key_frames - 1) * 2);
					for (size_t i = 0; i < num_key_frames; ++i)
					{
						if ((range_ignore_flags & ignore_bit_mask) == 0)
							rotations_as_vec[i] = vector_mul_add(rotations_as_vec[i], clip_range_extent, clip_range_min);

						ignore_bit_mask >>= 2;
					}

					sampling_context.clip_range_data_offset += decomp_context.num_rotation_components * sizeof(float) * 2;
				}

				Quat_32 rotations[num_key_frames];
				if (rotation_format == RotationFormat8::Quat_128 && settings.is_rotation_format_supported(RotationFormat8::Quat_128))
				{
					for (size_t i = 0; i < num_key_frames; ++i)
						rotations[i] = vector_to_quat(rotations_as_vec[i]);
				}
				else
				{
					for (size_t i = 0; i < num_key_frames; ++i)
						rotations[i] = quat_from_positive_w(rotations_as_vec[i]);
				}

				interpolated_rotation = SamplingContextType::interpolate_rotation(rotations, decomp_context.interpolation_alpha);
				ACL_ASSERT(quat_is_finite(interpolated_rotation), "Rotation is not valid!");
				ACL_ASSERT(quat_is_normalized(interpolated_rotation), "Rotation is not normalized!");
			}
		}

		sampling_context.track_index++;
		return interpolated_rotation;
	}

	template<class SettingsAdapterType, class DecompressionContextType, class SamplingContextType>
	inline Vector4_32 ACL_SIMD_CALL decompress_and_interpolate_vector(const SettingsAdapterType& settings, const ClipHeader& header, const DecompressionContextType& decomp_context, SamplingContextType& sampling_context)
	{
		Vector4_32 interpolated_vector;

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
				ACL_ASSERT(vector_is_finite3(interpolated_vector), "Vector is not valid!");

				sampling_context.constant_track_data_offset += get_packed_vector_size(VectorFormat8::Vector3_96);
			}
			else
			{
				const VectorFormat8 format = settings.get_vector_format(header);
				const RangeReductionFlags8 clip_range_reduction = settings.get_clip_range_reduction(header.clip_range_reduction);
				const RangeReductionFlags8 segment_range_reduction = settings.get_segment_range_reduction(header.segment_range_reduction);

				constexpr size_t num_key_frames = SamplingContextType::k_num_samples_to_interpolate;

				Vector4_32 vectors[num_key_frames];

				// Range ignore flags are used to skip range normalization at the clip and/or segment levels
				// Each sample has two bits like so: sample 0 clip, sample 0 segment, sample 1 clip, sample 1 segment, etc
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
							range_ignore_flags |= 0x00000001u;	// Skip segment only
						}
						else if (is_raw_bit_rate(bit_rate))
						{
							vectors[i] = unpack_vector3_96_unsafe(decomp_context.animated_track_data[i], sampling_context.key_frame_bit_offsets[i]);
							range_ignore_flags |= 0x00000003u;	// Skip clip and segment
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

				const RangeReductionFlags8 range_reduction_flag = settings.get_range_reduction_flag();
				if (are_any_enum_flags_set(segment_range_reduction, range_reduction_flag))
				{
					uint32_t ignore_bit_mask = 0x00000001u << ((num_key_frames - 1) * 2);
					for (size_t i = 0; i < num_key_frames; ++i)
					{
						if (format != VectorFormat8::Vector3_Variable || !settings.is_vector_format_supported(VectorFormat8::Vector3_Variable) || (range_ignore_flags & ignore_bit_mask) == 0)
						{
							const Vector4_32 segment_range_min = unpack_vector3_u24_unsafe(decomp_context.segment_range_data[i] + sampling_context.segment_range_data_offset);
							const Vector4_32 segment_range_extent = unpack_vector3_u24_unsafe(decomp_context.segment_range_data[i] + sampling_context.segment_range_data_offset + (3 * sizeof(uint8_t)));

							vectors[i] = vector_mul_add(vectors[i], segment_range_extent, segment_range_min);
						}

						ignore_bit_mask >>= 2;
					}

					sampling_context.segment_range_data_offset += 3 * k_segment_range_reduction_num_bytes_per_component * 2;
				}

				if (are_any_enum_flags_set(clip_range_reduction, range_reduction_flag))
				{
					const Vector4_32 clip_range_min = unpack_vector3_96_unsafe(decomp_context.clip_range_data + sampling_context.clip_range_data_offset);
					const Vector4_32 clip_range_extent = unpack_vector3_96_unsafe(decomp_context.clip_range_data + sampling_context.clip_range_data_offset + (3 * sizeof(float)));

					uint32_t ignore_bit_mask = 0x00000002u << ((num_key_frames - 1) * 2);
					for (size_t i = 0; i < num_key_frames; ++i)
					{
						if ((range_ignore_flags & ignore_bit_mask) == 0)
							vectors[i] = vector_mul_add(vectors[i], clip_range_extent, clip_range_min);

						ignore_bit_mask >>= 2;
					}

					sampling_context.clip_range_data_offset += k_clip_range_reduction_vector3_range_size;
				}

				interpolated_vector = SamplingContextType::interpolate_vector4(vectors, decomp_context.interpolation_alpha);
				ACL_ASSERT(vector_is_finite3(interpolated_vector), "Vector is not valid!");
			}
		}

		sampling_context.track_index++;
		return interpolated_vector;
	}
}

ACL_IMPL_FILE_PRAGMA_POP
