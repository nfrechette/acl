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

namespace acl
{
	template<size_t num_key_frames, class SettingsType, class DecompressionContext>
	inline void skip_rotations(const SettingsType& settings, const ClipHeader& header, DecompressionContext& context)
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
					for (size_t i = 0; i < num_key_frames; ++i)
					{
						uint8_t bit_rate = context.format_per_track_data[i][context.format_per_track_data_offset];
						uint8_t num_bits_at_bit_rate = get_num_bits_at_bit_rate(bit_rate) * 3;	// 3 components

						if (settings.supports_mixed_packing() && context.has_mixed_packing)
							num_bits_at_bit_rate = align_to(num_bits_at_bit_rate, MIXED_PACKING_ALIGNMENT_NUM_BITS);

						context.key_frame_bit_offsets[i] += num_bits_at_bit_rate;

						if (settings.supports_mixed_packing() && context.has_mixed_packing)
							context.key_frame_byte_offsets[i] = context.key_frame_bit_offsets[i] / 8;
					}

					++context.format_per_track_data_offset;
				}
				else
				{
					uint32_t rotation_size = get_packed_rotation_size(rotation_format);

					for (size_t i = 0; i < num_key_frames; ++i)
					{
						context.key_frame_byte_offsets[i] += rotation_size;

						if (settings.supports_mixed_packing() && context.has_mixed_packing)
							context.key_frame_bit_offsets[i] = context.key_frame_byte_offsets[i] * 8;
					}
				}

				const RangeReductionFlags8 clip_range_reduction = settings.get_clip_range_reduction(header.clip_range_reduction);
				if (is_enum_flag_set(clip_range_reduction, RangeReductionFlags8::Rotations))
					context.clip_range_data_offset += context.num_rotation_components * sizeof(float) * 2;

				const RangeReductionFlags8 segment_range_reduction = settings.get_segment_range_reduction(header.segment_range_reduction);
				if (is_enum_flag_set(segment_range_reduction, RangeReductionFlags8::Rotations))
					context.segment_range_data_offset += context.num_rotation_components * ACL_PER_SEGMENT_RANGE_REDUCTION_COMPONENT_BYTE_SIZE * 2;
			}
		}

		++context.default_track_offset;
		++context.constant_track_offset;
	}

	template<class SettingsType, class DecompressionContext>
	inline void skip_rotation(const SettingsType& settings, const ClipHeader& header, DecompressionContext& context) { skip_rotations<1>(settings, header, context); }

	template<class SettingsType, class DecompressionContext>
	inline void skip_rotations_in_two_key_frames(const SettingsType& settings, const ClipHeader& header, DecompressionContext& context) { skip_rotations<2>(settings, header, context); }

	template<class SettingsType, class DecompressionContext>
	inline void skip_rotations_in_four_key_frames(const SettingsType& settings, const ClipHeader& header, DecompressionContext& context) { skip_rotations<4>(settings, header, context); }

	template<size_t num_key_frames, class SettingsAdapterType, class DecompressionContext>
	inline void skip_vectors(const SettingsAdapterType& settings, const ClipHeader& header, DecompressionContext& context)
	{
		const bool is_sample_default = bitset_test(context.default_tracks_bitset, context.bitset_size, context.default_track_offset);
		if (!is_sample_default)
		{
			const bool is_sample_constant = bitset_test(context.constant_tracks_bitset, context.bitset_size, context.constant_track_offset);
			if (is_sample_constant)
			{
				// Constant Vector3 tracks store the remaining sample with full precision
				context.constant_track_data_offset += get_packed_vector_size(VectorFormat8::Vector3_96);
			}
			else
			{
				const VectorFormat8 format = settings.get_vector_format(header);

				if (is_vector_format_variable(format))
				{
					for (size_t i = 0; i < num_key_frames; ++i)
					{
						uint8_t bit_rate = context.format_per_track_data[i][context.format_per_track_data_offset];
						uint8_t num_bits_at_bit_rate = get_num_bits_at_bit_rate(bit_rate) * 3;	// 3 components

						if (settings.supports_mixed_packing() && context.has_mixed_packing)
							num_bits_at_bit_rate = align_to(num_bits_at_bit_rate, MIXED_PACKING_ALIGNMENT_NUM_BITS);

						context.key_frame_bit_offsets[i] += num_bits_at_bit_rate;

						if (settings.supports_mixed_packing() && context.has_mixed_packing)
							context.key_frame_byte_offsets[i] = context.key_frame_bit_offsets[i] / 8;
					}

					++context.format_per_track_data_offset;
				}
				else
				{
					const uint32_t sample_size = get_packed_vector_size(format);

					for (size_t i = 0; i < num_key_frames; ++i)
					{
						context.key_frame_byte_offsets[i] += sample_size;

						if (settings.supports_mixed_packing() && context.has_mixed_packing)
							context.key_frame_bit_offsets[i] = context.key_frame_byte_offsets[i] * 8;
					}
				}

				const RangeReductionFlags8 range_reduction_flag = settings.get_range_reduction_flag();

				const RangeReductionFlags8 clip_range_reduction = settings.get_clip_range_reduction(header.clip_range_reduction);
				if (is_enum_flag_set(clip_range_reduction, range_reduction_flag))
					context.clip_range_data_offset += 3 * sizeof(float) * 2;

				const RangeReductionFlags8 segment_range_reduction = settings.get_segment_range_reduction(header.segment_range_reduction);
				if (is_enum_flag_set(segment_range_reduction, range_reduction_flag))
					context.segment_range_data_offset += 3 * ACL_PER_SEGMENT_RANGE_REDUCTION_COMPONENT_BYTE_SIZE * 2;
			}
		}

		++context.default_track_offset;
		++context.constant_track_offset;
	}

	template<class SettingsAdapterType, class DecompressionContext>
	inline void skip_vector(const SettingsAdapterType& settings, const ClipHeader& header, DecompressionContext& context) { skip_vectors<1>(settings, header, context); }

	template<class SettingsAdapterType, class DecompressionContext>
	inline void skip_vectors_in_two_key_frames(const SettingsAdapterType& settings, const ClipHeader& header, DecompressionContext& context) { skip_vectors<2>(settings, header, context); }

	template<class SettingsAdapterType, class DecompressionContext>
	inline void skip_vectors_in_four_key_frames(const SettingsAdapterType& settings, const ClipHeader& header, DecompressionContext& context) { skip_vectors<4>(settings, header, context); }

	template<size_t num_key_frames, class SettingsType, class DecompressionContext>
	inline void decompress_rotations(const SettingsType& settings, const ClipHeader& header, DecompressionContext& context, Quat_32* out_rotations)
	{
		bool is_rotation_default = bitset_test(context.default_tracks_bitset, context.bitset_size, context.default_track_offset);
		if (is_rotation_default)
		{
			Quat_32 default = quat_identity_32();

			for (size_t i = 0; i < num_key_frames; ++i)
				out_rotations[i] = default;
		}
		else
		{
			const RotationFormat8 rotation_format = settings.get_rotation_format(header.rotation_format);

			bool is_rotation_constant = bitset_test(context.constant_tracks_bitset, context.bitset_size, context.constant_track_offset);
			if (is_rotation_constant)
			{
				const RotationFormat8 packed_format = is_rotation_format_variable(rotation_format) ? get_highest_variant_precision(get_rotation_variant(rotation_format)) : rotation_format;

				Quat_32 rotation;

				if (packed_format == RotationFormat8::Quat_128 && settings.is_rotation_format_supported(RotationFormat8::Quat_128))
					rotation = unpack_quat_128(context.constant_track_data + context.constant_track_data_offset);
				else if (packed_format == RotationFormat8::QuatDropW_96 && settings.is_rotation_format_supported(RotationFormat8::QuatDropW_96))
					rotation = unpack_quat_96(context.constant_track_data + context.constant_track_data_offset);
				else if (packed_format == RotationFormat8::QuatDropW_48 && settings.is_rotation_format_supported(RotationFormat8::QuatDropW_48))
					rotation = unpack_quat_48(context.constant_track_data + context.constant_track_data_offset);
				else if (packed_format == RotationFormat8::QuatDropW_32 && settings.is_rotation_format_supported(RotationFormat8::QuatDropW_32))
					rotation = unpack_quat_32(context.constant_track_data + context.constant_track_data_offset);
				else
				{
					ACL_ENSURE(false, "Unrecognized rotation format");
					rotation = quat_identity_32();
				}

				for (size_t i = 0; i < num_key_frames; ++i)
					out_rotations[i] = rotation;

				context.constant_track_data_offset += get_packed_rotation_size(packed_format);
			}
			else
			{
				const RangeReductionFlags8 clip_range_reduction = settings.get_clip_range_reduction(header.clip_range_reduction);
				const RangeReductionFlags8 segment_range_reduction = settings.get_segment_range_reduction(header.segment_range_reduction);
				const bool are_clip_rotations_normalized = is_enum_flag_set(clip_range_reduction, RangeReductionFlags8::Rotations);
				const bool are_segment_rotations_normalized = is_enum_flag_set(segment_range_reduction, RangeReductionFlags8::Rotations);

				if (rotation_format == RotationFormat8::Quat_128 && settings.is_rotation_format_supported(RotationFormat8::Quat_128))
				{
					Vector4_32 rotations_xyzw[num_key_frames];

					for (size_t i = 0; i < num_key_frames; ++i)
						rotations_xyzw[i] = unpack_vector4_128(context.animated_track_data[i] + context.key_frame_byte_offsets[i]);

					if (are_segment_rotations_normalized)
					{
						for (size_t i = 0; i < num_key_frames; ++i)
						{
							Vector4_32 segment_range_min, segment_range_extent;

#if ACL_PER_SEGMENT_RANGE_REDUCTION_COMPONENT_BIT_SIZE == 8
							segment_range_min = unpack_vector4_32(context.segment_range_data[i] + context.segment_range_data_offset, true);
							segment_range_extent = unpack_vector4_32(context.segment_range_data[i] + context.segment_range_data_offset + (context.num_rotation_components * sizeof(uint8_t)), true);
#else
							segment_range_min = unpack_vector4_64(context.segment_range_data[i] + context.segment_range_data_offset, true);
							segment_range_extent = unpack_vector4_64(context.segment_range_data[i] + context.segment_range_data_offset + (context.num_rotation_components * sizeof(uint16_t)), true);
#endif

							rotations_xyzw[i] = vector_mul_add(rotations_xyzw[i], segment_range_extent, segment_range_min);
						}

						context.segment_range_data_offset += context.num_rotation_components * ACL_PER_SEGMENT_RANGE_REDUCTION_COMPONENT_BYTE_SIZE * 2;
					}

					if (are_clip_rotations_normalized)
					{
						Vector4_32 clip_range_min = vector_unaligned_load(context.clip_range_data + context.clip_range_data_offset);
						Vector4_32 clip_range_extent = vector_unaligned_load(context.clip_range_data + context.clip_range_data_offset + (context.num_rotation_components * sizeof(float)));

						for (size_t i = 0; i < num_key_frames; ++i)
							rotations_xyzw[i] = vector_mul_add(rotations_xyzw[i], clip_range_extent, clip_range_min);

						context.clip_range_data_offset += context.num_rotation_components * sizeof(float) * 2;
					}

					const uint32_t rotation_size = get_packed_rotation_size(rotation_format);

					for (size_t i = 0; i < num_key_frames; ++i)
					{
						out_rotations[i] = vector_to_quat(rotations_xyzw[i]);

						context.key_frame_byte_offsets[i] += rotation_size;

						if (settings.supports_mixed_packing() && context.has_mixed_packing)
							context.key_frame_bit_offsets[i] = context.key_frame_byte_offsets[i] * 8;
					}
				}
				else if (rotation_format == RotationFormat8::QuatDropW_96 && settings.is_rotation_format_supported(RotationFormat8::QuatDropW_96))
				{
					Vector4_32 rotations_xyz[num_key_frames];

					for (size_t i = 0; i < num_key_frames; ++i)
						rotations_xyz[i] = unpack_vector3_96(context.animated_track_data[i] + context.key_frame_byte_offsets[i]);

					if (are_segment_rotations_normalized)
					{
						for (size_t i = 0; i < num_key_frames; ++i)
						{
							Vector4_32 segment_range_min, segment_range_extent;

#if ACL_PER_SEGMENT_RANGE_REDUCTION_COMPONENT_BIT_SIZE == 8
							segment_range_min = unpack_vector3_24(context.segment_range_data[i] + context.segment_range_data_offset, true);
							segment_range_extent = unpack_vector3_24(context.segment_range_data[i] + context.segment_range_data_offset + (context.num_rotation_components * sizeof(uint8_t)), true);
#else
							segment_range_min = unpack_vector3_48(context.segment_range_data[i] + context.segment_range_data_offset, true);
							segment_range_extent = unpack_vector3_48(context.segment_range_data[i] + context.segment_range_data_offset + (context.num_rotation_components * sizeof(uint16_t)), true);
#endif

							rotations_xyz[i] = vector_mul_add(rotations_xyz[i], segment_range_extent, segment_range_min);
						}

						context.segment_range_data_offset += context.num_rotation_components * ACL_PER_SEGMENT_RANGE_REDUCTION_COMPONENT_BYTE_SIZE * 2;
					}

					if (are_clip_rotations_normalized)
					{
						Vector4_32 clip_range_min = vector_unaligned_load(context.clip_range_data + context.clip_range_data_offset);
						Vector4_32 clip_range_extent = vector_unaligned_load(context.clip_range_data + context.clip_range_data_offset + (context.num_rotation_components * sizeof(float)));

						for (size_t i = 0; i < num_key_frames; ++i)
							rotations_xyz[i] = vector_mul_add(rotations_xyz[i], clip_range_extent, clip_range_min);

						context.clip_range_data_offset += context.num_rotation_components * sizeof(float) * 2;
					}

					const uint32_t rotation_size = get_packed_rotation_size(rotation_format);

					for (size_t i = 0; i < num_key_frames; ++i)
					{
						out_rotations[i] = quat_from_positive_w(rotations_xyz[i]);

						context.key_frame_byte_offsets[i] += rotation_size;

						if (settings.supports_mixed_packing() && context.has_mixed_packing)
							context.key_frame_bit_offsets[i] = context.key_frame_byte_offsets[i] * 8;
					}
				}
				else if (rotation_format == RotationFormat8::QuatDropW_48 && settings.is_rotation_format_supported(RotationFormat8::QuatDropW_48))
				{
					Vector4_32 rotations_xyz[num_key_frames];

					for (size_t i = 0; i < num_key_frames; ++i)
						rotations_xyz[i] = unpack_vector3_48(context.animated_track_data[i] + context.key_frame_byte_offsets[i], are_clip_rotations_normalized);

					if (are_segment_rotations_normalized)
					{
						for (size_t i = 0; i < num_key_frames; ++i)
						{
							Vector4_32 segment_range_min, segment_range_extent;

#if ACL_PER_SEGMENT_RANGE_REDUCTION_COMPONENT_BIT_SIZE == 8
							segment_range_min = unpack_vector3_24(context.segment_range_data[i] + context.segment_range_data_offset, true);
							segment_range_extent = unpack_vector3_24(context.segment_range_data[i] + context.segment_range_data_offset + (context.num_rotation_components * sizeof(uint8_t)), true);
#else
							segment_range_min = unpack_vector3_48(context.segment_range_data[i] + context.segment_range_data_offset, true);
							segment_range_extent = unpack_vector3_48(context.segment_range_data[i] + context.segment_range_data_offset + (context.num_rotation_components * sizeof(uint16_t)), true);
#endif

							rotations_xyz[i] = vector_mul_add(rotations_xyz[i], segment_range_extent, segment_range_min);
						}

						context.segment_range_data_offset += context.num_rotation_components * ACL_PER_SEGMENT_RANGE_REDUCTION_COMPONENT_BYTE_SIZE * 2;
					}

					if (are_clip_rotations_normalized)
					{
						Vector4_32 clip_range_min = vector_unaligned_load(context.clip_range_data + context.clip_range_data_offset);
						Vector4_32 clip_range_extent = vector_unaligned_load(context.clip_range_data + context.clip_range_data_offset + (context.num_rotation_components * sizeof(float)));

						for (size_t i = 0; i < num_key_frames; ++i)
							rotations_xyz[i] = vector_mul_add(rotations_xyz[i], clip_range_extent, clip_range_min);

						context.clip_range_data_offset += context.num_rotation_components * sizeof(float) * 2;
					}

					const uint32_t rotation_size = get_packed_rotation_size(rotation_format);

					for (size_t i = 0; i < num_key_frames; ++i)
					{
						out_rotations[i] = quat_from_positive_w(rotations_xyz[i]);

						context.key_frame_byte_offsets[i] += rotation_size;

						if (settings.supports_mixed_packing() && context.has_mixed_packing)
							context.key_frame_bit_offsets[i] = context.key_frame_byte_offsets[i] * 8;
					}
				}
				else if (rotation_format == RotationFormat8::QuatDropW_32 && settings.is_rotation_format_supported(RotationFormat8::QuatDropW_32))
				{
					Vector4_32 rotations_xyz[num_key_frames];

					for (size_t i = 0; i < num_key_frames; ++i)
						rotations_xyz[i] = unpack_vector3_32(11, 11, 10, are_clip_rotations_normalized, context.animated_track_data[i] + context.key_frame_byte_offsets[i]);

					if (are_segment_rotations_normalized)
					{
						for (size_t i = 0; i < num_key_frames; ++i)
						{
							Vector4_32 segment_range_min, segment_range_extent;

#if ACL_PER_SEGMENT_RANGE_REDUCTION_COMPONENT_BIT_SIZE == 8
							segment_range_min = unpack_vector3_24(context.segment_range_data[i] + context.segment_range_data_offset, true);
							segment_range_extent = unpack_vector3_24(context.segment_range_data[i] + context.segment_range_data_offset + (context.num_rotation_components * sizeof(uint8_t)), true);
#else
							segment_range_min = unpack_vector3_48(context.segment_range_data[i] + context.segment_range_data_offset, true);
							segment_range_extent = unpack_vector3_48(context.segment_range_data[i] + context.segment_range_data_offset + (context.num_rotation_components * sizeof(uint16_t)), true);
#endif

							rotations_xyz[i] = vector_mul_add(rotations_xyz[i], segment_range_extent, segment_range_min);
						}

						context.segment_range_data_offset += context.num_rotation_components * ACL_PER_SEGMENT_RANGE_REDUCTION_COMPONENT_BYTE_SIZE * 2;
					}

					if (are_clip_rotations_normalized)
					{
						Vector4_32 clip_range_min = vector_unaligned_load(context.clip_range_data + context.clip_range_data_offset);
						Vector4_32 clip_range_extent = vector_unaligned_load(context.clip_range_data + context.clip_range_data_offset + (context.num_rotation_components * sizeof(float)));

						for (size_t i = 0; i < num_key_frames; ++i)
							rotations_xyz[i] = vector_mul_add(rotations_xyz[i], clip_range_extent, clip_range_min);

						context.clip_range_data_offset += context.num_rotation_components * sizeof(float) * 2;
					}

					const uint32_t rotation_size = get_packed_rotation_size(rotation_format);

					for (size_t i = 0; i < num_key_frames; ++i)
					{
						out_rotations[i] = quat_from_positive_w(rotations_xyz[i]);

						context.key_frame_byte_offsets[i] += rotation_size;

						if (settings.supports_mixed_packing() && context.has_mixed_packing)
							context.key_frame_bit_offsets[i] = context.key_frame_byte_offsets[i] * 8;
					}
				}
				else if (rotation_format == RotationFormat8::QuatDropW_Variable && settings.is_rotation_format_supported(RotationFormat8::QuatDropW_Variable))
				{
					uint8_t bit_rates[num_key_frames];
					uint8_t num_bits_at_bit_rates[num_key_frames];
					Vector4_32 rotations_xyz[num_key_frames];

					for (size_t i = 0; i < num_key_frames; ++i)
					{
						bit_rates[i] = context.format_per_track_data[i][context.format_per_track_data_offset];
						num_bits_at_bit_rates[i] = get_num_bits_at_bit_rate(bit_rates[i]);

						if (is_pack_0_bit_rate(bit_rates[i]))
							(void)bit_rates[i];
						else if (is_pack_72_bit_rate(bit_rates[i]))
							rotations_xyz[i] = unpack_vector3_72(are_clip_rotations_normalized, context.animated_track_data[i], context.key_frame_bit_offsets[i]);
						else if (is_pack_96_bit_rate(bit_rates[i]))
							rotations_xyz[i] = unpack_vector3_96(context.animated_track_data[i], context.key_frame_bit_offsets[i]);
						else
							rotations_xyz[i] = unpack_vector3_n(num_bits_at_bit_rates[i], num_bits_at_bit_rates[i], num_bits_at_bit_rates[i], are_clip_rotations_normalized, context.animated_track_data[i], context.key_frame_bit_offsets[i]);
					}

					++context.format_per_track_data_offset;

					if (are_segment_rotations_normalized)
					{
						for (size_t i = 0; i < num_key_frames; ++i)
						{
#if ACL_PER_SEGMENT_RANGE_REDUCTION_COMPONENT_BIT_SIZE == 8
							if (is_pack_0_bit_rate(bit_rates[i]))
								rotations_xyz[i] = unpack_vector3_48(context.segment_range_data[i] + context.segment_range_data_offset, true);
							else
							{
								Vector4_32 segment_range_min = unpack_vector3_24(context.segment_range_data[i] + context.segment_range_data_offset, true);
								Vector4_32 segment_range_extent = unpack_vector3_24(context.segment_range_data[i] + context.segment_range_data_offset + (context.num_rotation_components * sizeof(uint8_t)), true);

								rotations_xyz[i] = vector_mul_add(rotations_xyz[i], segment_range_extent, segment_range_min);
							}
#else
							if (is_pack_0_bit_rate(bit_rate[i]))
								rotation_xyz[i] = unpack_vector3_96(context.segment_range_data[i] + context.segment_range_data_offset);
							else
							{
								Vector4_32 segment_range_min = unpack_vector3_48(context.segment_range_data[i] + context.segment_range_data_offset, true);
								Vector4_32 segment_range_extent = unpack_vector3_48(context.segment_range_data[i] + context.segment_range_data_offset + (context.num_rotation_components * sizeof(uint16_t)), true);

								rotation_xyz[i] = vector_mul_add(rotation_xyz[i], segment_range_extent, segment_range_min);
							}
#endif
						}

						context.segment_range_data_offset += context.num_rotation_components * ACL_PER_SEGMENT_RANGE_REDUCTION_COMPONENT_BYTE_SIZE * 2;
					}

					if (are_clip_rotations_normalized)
					{
						Vector4_32 clip_range_min = vector_unaligned_load(context.clip_range_data + context.clip_range_data_offset);
						Vector4_32 clip_range_extent = vector_unaligned_load(context.clip_range_data + context.clip_range_data_offset + (context.num_rotation_components * sizeof(float)));

						for (size_t i = 0; i < num_key_frames; ++i)
							rotations_xyz[i] = vector_mul_add(rotations_xyz[i], clip_range_extent, clip_range_min);

						context.clip_range_data_offset += context.num_rotation_components * sizeof(float) * 2;
					}

					for (size_t i = 0; i < num_key_frames; ++i)
					{
						out_rotations[i] = quat_from_positive_w(rotations_xyz[i]);

						uint8_t num_bits_read = num_bits_at_bit_rates[i] * 3;

						if (settings.supports_mixed_packing() && context.has_mixed_packing)
							num_bits_read = align_to(num_bits_read, MIXED_PACKING_ALIGNMENT_NUM_BITS);

						context.key_frame_bit_offsets[i] += num_bits_read;

						if (settings.supports_mixed_packing() && context.has_mixed_packing)
							context.key_frame_byte_offsets[i] = context.key_frame_bit_offsets[i] / 8;
					}
				}
			}
		}

		++context.default_track_offset;
		++context.constant_track_offset;
	}

	template<class SettingsType, class DecompressionContext>
	inline void decompress_rotation(const SettingsType& settings, const ClipHeader& header, DecompressionContext& context, Quat_32* out_rotations) { decompress_rotations<1>(settings, header, context, out_rotations); }

	template<class SettingsType, class DecompressionContext>
	inline void decompress_rotations_in_two_key_frames(const SettingsType& settings, const ClipHeader& header, DecompressionContext& context, Quat_32* out_rotations) { decompress_rotations<2>(settings, header, context, out_rotations); }

	template<class SettingsType, class DecompressionContext>
	inline void decompress_rotations_in_four_key_frames(const SettingsType& settings, const ClipHeader& header, DecompressionContext& context, Quat_32* out_rotations) { decompress_rotations<4>(settings, header, context, out_rotations); }

	template<size_t num_key_frames, class SettingsAdapterType, class DecompressionContext>
	inline void decompress_vectors(const SettingsAdapterType& settings, const ClipHeader& header, DecompressionContext& context, Vector4_32* out_vectors)
	{
		const bool is_sample_default = bitset_test(context.default_tracks_bitset, context.bitset_size, context.default_track_offset);
		if (is_sample_default)
		{
			Vector4_32 default = settings.get_default_value();

			for (size_t i = 0; i < num_key_frames; ++i)
				out_vectors[i] = default;
		}
		else
		{
			const bool is_sample_constant = bitset_test(context.constant_tracks_bitset, context.bitset_size, context.constant_track_offset);
			if (is_sample_constant)
			{
				// Constant translation tracks store the remaining sample with full precision
				Vector4_32 default = unpack_vector3_96(context.constant_track_data + context.constant_track_data_offset);

				for (size_t i = 0; i < num_key_frames; ++i)
					out_vectors[i] = default;

				context.constant_track_data_offset += get_packed_vector_size(VectorFormat8::Vector3_96);
			}
			else
			{
				const VectorFormat8 format = settings.get_vector_format(header);
				const RangeReductionFlags8 clip_range_reduction = settings.get_clip_range_reduction(header.clip_range_reduction);
				const RangeReductionFlags8 segment_range_reduction = settings.get_segment_range_reduction(header.segment_range_reduction);

				uint8_t bit_rates[num_key_frames];

				for (size_t i = 0; i < num_key_frames; ++i)
					bit_rates[i] = INVALID_BIT_RATE;

				if (format == VectorFormat8::Vector3_96 && settings.is_vector_format_supported(VectorFormat8::Vector3_96))
				{
					const uint32_t sample_size = get_packed_vector_size(format);

					for (size_t i = 0; i < num_key_frames; ++i)
					{
						out_vectors[i] = unpack_vector3_96(context.animated_track_data[i] + context.key_frame_byte_offsets[i]);
					
						context.key_frame_byte_offsets[i] += sample_size;

						if (settings.supports_mixed_packing() && context.has_mixed_packing)
							context.key_frame_bit_offsets[i] = context.key_frame_byte_offsets[i] * 8;
					}
				}
				else if (format == VectorFormat8::Vector3_48 && settings.is_vector_format_supported(VectorFormat8::Vector3_48))
				{
					const uint32_t sample_size = get_packed_vector_size(format);

					for (size_t i = 0; i < num_key_frames; ++i)
					{
						out_vectors[i] = unpack_vector3_48(context.animated_track_data[i] + context.key_frame_byte_offsets[i], true);

						context.key_frame_byte_offsets[i] += sample_size;

						if (settings.supports_mixed_packing() && context.has_mixed_packing)
							context.key_frame_bit_offsets[i] = context.key_frame_byte_offsets[i] * 8;
					}
				}
				else if (format == VectorFormat8::Vector3_32 && settings.is_vector_format_supported(VectorFormat8::Vector3_32))
				{
					const uint32_t sample_size = get_packed_vector_size(format);

					for (size_t i = 0; i < num_key_frames; ++i)
					{
						out_vectors[i] = unpack_vector3_32(11, 11, 10, true, context.animated_track_data[i] + context.key_frame_byte_offsets[i]);

						context.key_frame_byte_offsets[i] += sample_size;

						if (settings.supports_mixed_packing() && context.has_mixed_packing)
							context.key_frame_bit_offsets[i] = context.key_frame_byte_offsets[i] * 8;
					}
				}
				else if (format == VectorFormat8::Vector3_Variable && settings.is_vector_format_supported(VectorFormat8::Vector3_Variable))
				{
					for (size_t i = 0; i < num_key_frames; ++i)
					{
						bit_rates[i] = context.format_per_track_data[i][context.format_per_track_data_offset];
						uint8_t num_bits_at_bit_rate = get_num_bits_at_bit_rate(bit_rates[i]);

						if (is_pack_0_bit_rate(bit_rates[i]))
							(void)bit_rates[i];
						else if (is_pack_72_bit_rate(bit_rates[i]))
							out_vectors[i] = unpack_vector3_72(true, context.animated_track_data[i], context.key_frame_bit_offsets[i]);
						else if (is_pack_96_bit_rate(bit_rates[i]))
							out_vectors[i] = unpack_vector3_96(context.animated_track_data[i], context.key_frame_bit_offsets[i]);
						else
							out_vectors[i] = unpack_vector3_n(num_bits_at_bit_rate, num_bits_at_bit_rate, num_bits_at_bit_rate, true, context.animated_track_data[i], context.key_frame_bit_offsets[i]);

						uint8_t num_bits_read = num_bits_at_bit_rate * 3;
						if (settings.supports_mixed_packing() && context.has_mixed_packing)
							num_bits_read = align_to(num_bits_read, MIXED_PACKING_ALIGNMENT_NUM_BITS);

						context.key_frame_bit_offsets[i] += num_bits_read;

						if (settings.supports_mixed_packing() && context.has_mixed_packing)
							context.key_frame_byte_offsets[i] = context.key_frame_bit_offsets[i] / 8;
					}

					++context.format_per_track_data_offset;
				}

				const RangeReductionFlags8 range_reduction_flag = settings.get_range_reduction_flag();
				if (is_enum_flag_set(segment_range_reduction, range_reduction_flag))
				{
					for (size_t i = 0; i < num_key_frames; ++i)
					{
#if ACL_PER_SEGMENT_RANGE_REDUCTION_COMPONENT_BIT_SIZE == 8
						if (format == VectorFormat8::Vector3_Variable && settings.is_vector_format_supported(VectorFormat8::Vector3_Variable) && is_pack_0_bit_rate(bit_rates[i]))
							out_vectors[i] = unpack_vector3_48(context.segment_range_data[i] + context.segment_range_data_offset, true);
						else
						{
							Vector4_32 segment_range_min = unpack_vector3_24(context.segment_range_data[i] + context.segment_range_data_offset, true);
							Vector4_32 segment_range_extent = unpack_vector3_24(context.segment_range_data[i] + context.segment_range_data_offset + (3 * sizeof(uint8_t)), true);

							out_vectors[i] = vector_mul_add(out_vectors[i], segment_range_extent, segment_range_min);
						}
#else
						if (format == VectorFormat8::Vector3_Variable && settings.is_vector_format_supported(VectorFormat8::Vector3_Variable) && is_pack_0_bit_rate(bit_rate0))
							out_vectors[i] = unpack_vector3_96(context.segment_range_data[i] + context.segment_range_data_offset, );
						else
						{
							Vector4_32 segment_range_min = unpack_vector3_48(context.segment_range_data[i] + context.segment_range_data_offset, true);
							Vector4_32 segment_range_extent = unpack_vector3_48(context.segment_range_data[i] + context.segment_range_data_offset + (3 * sizeof(uint16_t)), true);

							out_vectors[i] = vector_mul_add(out_vectors[i], segment_range_extent, segment_range_min);
						}
#endif
					}

					context.segment_range_data_offset += 3 * ACL_PER_SEGMENT_RANGE_REDUCTION_COMPONENT_BYTE_SIZE * 2;
				}

				if (is_enum_flag_set(clip_range_reduction, range_reduction_flag))
				{
					Vector4_32 clip_range_min = unpack_vector3_96(context.clip_range_data + context.clip_range_data_offset);
					Vector4_32 clip_range_extent = unpack_vector3_96(context.clip_range_data + context.clip_range_data_offset + (3 * sizeof(float)));

					for (size_t i = 0; i < num_key_frames; ++i)
						out_vectors[i] = vector_mul_add(out_vectors[i], clip_range_extent, clip_range_min);

					context.clip_range_data_offset += 3 * sizeof(float) * 2;
				}
			}
		}

		context.default_track_offset++;
		context.constant_track_offset++;
	}

	template<class SettingsAdapterType, class DecompressionContext>
	inline void decompress_vector(const SettingsAdapterType& settings, const ClipHeader& header, DecompressionContext& context, Vector4_32* out_vectors) { decompress_vectors<1>(settings, header, context, out_vectors); }

	template<class SettingsAdapterType, class DecompressionContext>
	inline void decompress_vectors_in_two_key_frames(const SettingsAdapterType& settings, const ClipHeader& header, DecompressionContext& context, Vector4_32* out_vectors) { decompress_vectors<2>(settings, header, context, out_vectors); }

	template<class SettingsAdapterType, class DecompressionContext>
	inline void decompress_vectors_in_four_key_frames(const SettingsAdapterType& settings, const ClipHeader& header, DecompressionContext& context, Vector4_32* out_vectors) { decompress_vectors<4>(settings, header, context, out_vectors); }

	template <class SettingsType, class DecompressionContext>
	inline Quat_32 decompress_and_interpolate_rotation(const SettingsType& settings, const ClipHeader& header, DecompressionContext& context)
	{
		Quat_32 rotations[2];

		ACL_ENSURE(array_length(context.key_frame_byte_offsets) == array_length(rotations), "Interpolation requires exactly two keyframes.");

		decompress_rotations_in_two_key_frames(settings, header, context, rotations);

		Quat_32 rotation = quat_lerp(rotations[0], rotations[1], context.interpolation_alpha);

		ACL_ENSURE(quat_is_finite(rotation), "Rotation is not valid!");
		ACL_ENSURE(quat_is_normalized(rotation), "Rotation is not normalized!");

		return rotation;
	}

	template<class SettingsAdapterType, class DecompressionContext>
	inline Vector4_32 decompress_and_interpolate_vector(const SettingsAdapterType& settings, const ClipHeader& header, DecompressionContext& context)
	{
		Vector4_32 vectors[2];

		ACL_ENSURE(array_length(context.key_frame_byte_offsets) == array_length(vectors), "Interpolation requires exactly two keyframes.");

		decompress_vectors_in_two_key_frames(settings, header, context, vectors);

		Vector4_32 vector = vector_lerp(vectors[0], vectors[1], context.interpolation_alpha);

		ACL_ENSURE(vector_is_finite3(vector), "Vector is not valid!");

		return vector;
	}
}