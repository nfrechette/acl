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
#include "acl/math/quat_32.h"
#include "acl/math/quat_packing.h"
#include "acl/math/vector4_32.h"
#include "acl/math/vector4_packing.h"
#include "acl/compression/stream/clip_context.h"
#include "acl/compression/stream/sample_streams.h"
#include "acl/compression/skeleton_error_metric.h"

#include <stdint.h>

// 0 = no debug info, 1 = basic info, 2 = verbose
#define ACL_DEBUG_VARIABLE_QUANTIZATION		0

namespace acl
{
	namespace impl
	{
		inline void quantize_fixed_rotation_stream(Allocator& allocator, const RotationTrackStream& raw_stream, RotationFormat8 rotation_format, bool are_rotations_normalized, RotationTrackStream& out_quantized_stream)
		{
			// We expect all our samples to have the same width of sizeof(Vector4_32)
			ACL_ENSURE(raw_stream.get_sample_size() == sizeof(Vector4_32), "Unexpected rotation sample size. %u != %u", raw_stream.get_sample_size(), sizeof(Vector4_32));

			uint32_t num_samples = raw_stream.get_num_samples();
			uint32_t rotation_sample_size = get_packed_rotation_size(rotation_format);
			uint32_t sample_rate = raw_stream.get_sample_rate();
			RotationTrackStream quantized_stream(allocator, num_samples, rotation_sample_size, sample_rate, rotation_format);

			for (uint32_t sample_index = 0; sample_index < num_samples; ++sample_index)
			{
				Quat_32 rotation = raw_stream.get_raw_sample<Quat_32>(sample_index);
				uint8_t* quantized_ptr = quantized_stream.get_raw_sample_ptr(sample_index);

				switch (rotation_format)
				{
				case RotationFormat8::Quat_128:
					pack_vector4_128(quat_to_vector(rotation), quantized_ptr);
					break;
				case RotationFormat8::QuatDropW_96:
					pack_vector3_96(quat_to_vector(rotation), quantized_ptr);
					break;
				case RotationFormat8::QuatDropW_48:
					pack_vector3_48(quat_to_vector(rotation), are_rotations_normalized, quantized_ptr);
					break;
				case RotationFormat8::QuatDropW_32:
					pack_vector3_32(quat_to_vector(rotation), 11, 11, 10, are_rotations_normalized, quantized_ptr);
					break;
				case RotationFormat8::QuatDropW_Variable:
				default:
					ACL_ENSURE(false, "Invalid or unsupported rotation format: %s", get_rotation_format_name(rotation_format));
					break;
				}
			}

			out_quantized_stream = std::move(quantized_stream);
		}

		inline void quantize_fixed_rotation_streams(Allocator& allocator, BoneStreams* bone_streams, uint16_t num_bones, RotationFormat8 rotation_format, bool is_variable_variant)
		{
			const RotationVariant8 rotation_variant = get_rotation_variant(rotation_format);
			const RotationFormat8 highest_bit_rate = get_highest_variant_precision(rotation_variant);

			// By the time we get here, values have been converted to their final format, and normalized if selected
			for (uint16_t bone_index = 0; bone_index < num_bones; ++bone_index)
			{
				BoneStreams& bone_stream = bone_streams[bone_index];

				// Default tracks aren't quantized
				if (bone_stream.is_rotation_default)
					continue;

				const ClipContext* clip_context = bone_stream.segment->clip;
				bool are_rotations_normalized = clip_context->are_rotations_normalized;

				// If our format isn't variable, we allow constant tracks to be quantized to any format
				// If our format is variable, we keep them fixed at the highest bit rate in the variant
				RotationFormat8 format = is_variable_variant && bone_stream.is_rotation_constant ? highest_bit_rate : rotation_format;

				quantize_fixed_rotation_stream(allocator, bone_stream.rotations, format, are_rotations_normalized, bone_stream.rotations);
			}
		}

		inline void quantize_fixed_rotation_stream(Allocator& allocator, const RotationTrackStream& raw_stream, uint8_t bit_rate, bool are_rotations_normalized, RotationTrackStream& out_quantized_stream)
		{
			// We expect all our samples to have the same width of sizeof(Vector4_32)
			ACL_ENSURE(raw_stream.get_sample_size() == sizeof(Vector4_32), "Unexpected rotation sample size. %u != %u", raw_stream.get_sample_size(), sizeof(Vector4_32));

			uint32_t num_samples = raw_stream.get_num_samples();
			uint32_t sample_size = sizeof(uint64_t) * 2;
			uint32_t sample_rate = raw_stream.get_sample_rate();
			RotationTrackStream quantized_stream(allocator, num_samples, sample_size, sample_rate, RotationFormat8::QuatDropW_Variable, bit_rate);

			uint8_t num_bits_at_bit_rate = get_num_bits_at_bit_rate(bit_rate);

			for (uint32_t sample_index = 0; sample_index < num_samples; ++sample_index)
			{
				Quat_32 rotation = raw_stream.get_raw_sample<Quat_32>(sample_index);
				uint8_t* quantized_ptr = quantized_stream.get_raw_sample_ptr(sample_index);
				if (is_pack_72_bit_rate(bit_rate))
					pack_vector3_72(quat_to_vector(rotation), are_rotations_normalized, quantized_ptr);
				else if (is_pack_96_bit_rate(bit_rate))
					pack_vector3_96(quat_to_vector(rotation), quantized_ptr);
				else
					pack_vector3_n(quat_to_vector(rotation), num_bits_at_bit_rate, num_bits_at_bit_rate, num_bits_at_bit_rate, are_rotations_normalized, quantized_ptr);
			}

			out_quantized_stream = std::move(quantized_stream);
		}

		inline void quantize_fixed_rotation_streams(Allocator& allocator, BoneStreams* bone_streams, uint16_t num_bones, uint8_t bit_rate)
		{
			const RotationFormat8 highest_bit_rate = get_highest_variant_precision(RotationVariant8::QuatDropW);

			// By the time we get here, values have been converted to their final format, and normalized if selected
			for (uint16_t bone_index = 0; bone_index < num_bones; ++bone_index)
			{
				BoneStreams& bone_stream = bone_streams[bone_index];

				// Default tracks aren't quantized
				if (bone_stream.is_rotation_default)
					continue;

				const ClipContext* clip_context = bone_stream.segment->clip;
				bool are_rotations_normalized = clip_context->are_rotations_normalized;

				// If our format is variable, we keep them fixed at the highest bit rate in the variant
				if (bone_stream.is_rotation_constant)
					quantize_fixed_rotation_stream(allocator, bone_stream.rotations, highest_bit_rate, are_rotations_normalized, bone_stream.rotations);
				else
					quantize_fixed_rotation_stream(allocator, bone_stream.rotations, bit_rate, are_rotations_normalized, bone_stream.rotations);
			}
		}

		inline void quantize_fixed_translation_stream(Allocator& allocator, const TranslationTrackStream& raw_stream, VectorFormat8 translation_format, TranslationTrackStream& out_quantized_stream)
		{
			// We expect all our samples to have the same width of sizeof(Vector4_32)
			ACL_ENSURE(raw_stream.get_sample_size() == sizeof(Vector4_32), "Unexpected translation sample size. %u != %u", raw_stream.get_sample_size(), sizeof(Vector4_32));
			ACL_ENSURE(raw_stream.get_vector_format() == VectorFormat8::Vector3_96, "Expected a Vector3_96 vector format, found: %s", get_vector_format_name(raw_stream.get_vector_format()));

			uint32_t num_samples = raw_stream.get_num_samples();
			uint32_t sample_size = get_packed_vector_size(translation_format);
			uint32_t sample_rate = raw_stream.get_sample_rate();
			TranslationTrackStream quantized_stream(allocator, num_samples, sample_size, sample_rate, translation_format);

			for (uint32_t sample_index = 0; sample_index < num_samples; ++sample_index)
			{
				Vector4_32 translation = raw_stream.get_raw_sample<Vector4_32>(sample_index);
				uint8_t* quantized_ptr = quantized_stream.get_raw_sample_ptr(sample_index);

				switch (translation_format)
				{
				case VectorFormat8::Vector3_96:
					pack_vector3_96(translation, quantized_ptr);
					break;
				case VectorFormat8::Vector3_48:
					pack_vector3_48(translation, true, quantized_ptr);
					break;
				case VectorFormat8::Vector3_32:
					pack_vector3_32(translation, 11, 11, 10, true, quantized_ptr);
					break;
				case VectorFormat8::Vector3_Variable:
				default:
					ACL_ENSURE(false, "Invalid or unsupported vector format: %s", get_vector_format_name(translation_format));
					break;
				}
			}

			out_quantized_stream = std::move(quantized_stream);
		}

		inline void quantize_fixed_translation_streams(Allocator& allocator, BoneStreams* bone_streams, uint16_t num_bones, VectorFormat8 translation_format)
		{
			// By the time we get here, values have been converted to their final format, and normalized if selected
			for (uint16_t bone_index = 0; bone_index < num_bones; ++bone_index)
			{
				BoneStreams& bone_stream = bone_streams[bone_index];

				// Default tracks aren't quantized
				if (bone_stream.is_translation_default)
					continue;

				// Constant translation tracks store the remaining sample with full precision
				VectorFormat8 format = bone_stream.is_translation_constant ? VectorFormat8::Vector3_96 : translation_format;

				quantize_fixed_translation_stream(allocator, bone_stream.translations, format, bone_stream.translations);
			}
		}

		inline void quantize_fixed_translation_stream(Allocator& allocator, const TranslationTrackStream& raw_stream, uint8_t bit_rate, TranslationTrackStream& out_quantized_stream)
		{
			// We expect all our samples to have the same width of sizeof(Vector4_32)
			ACL_ENSURE(raw_stream.get_sample_size() == sizeof(Vector4_32), "Unexpected translation sample size. %u != %u", raw_stream.get_sample_size(), sizeof(Vector4_32));
			ACL_ENSURE(raw_stream.get_vector_format() == VectorFormat8::Vector3_96, "Expected a Vector3_96 vector format, found: %s", get_vector_format_name(raw_stream.get_vector_format()));

			uint32_t num_samples = raw_stream.get_num_samples();
			uint32_t sample_size = sizeof(uint64_t) * 2;
			uint32_t sample_rate = raw_stream.get_sample_rate();
			TranslationTrackStream quantized_stream(allocator, num_samples, sample_size, sample_rate, VectorFormat8::Vector3_Variable, bit_rate);

			uint8_t num_bits_at_bit_rate = get_num_bits_at_bit_rate(bit_rate);

			for (uint32_t sample_index = 0; sample_index < num_samples; ++sample_index)
			{
				Vector4_32 translation = raw_stream.get_raw_sample<Vector4_32>(sample_index);
				uint8_t* quantized_ptr = quantized_stream.get_raw_sample_ptr(sample_index);
				if (is_pack_72_bit_rate(bit_rate))
					pack_vector3_72(translation, true, quantized_ptr);
				else if (is_pack_96_bit_rate(bit_rate))
					pack_vector3_96(translation, quantized_ptr);
				else
					pack_vector3_n(translation, num_bits_at_bit_rate, num_bits_at_bit_rate, num_bits_at_bit_rate, true, quantized_ptr);
			}

			out_quantized_stream = std::move(quantized_stream);
		}

		inline void quantize_fixed_translation_streams(Allocator& allocator, BoneStreams* bone_streams, uint16_t num_bones, uint8_t bit_rate)
		{
			// By the time we get here, values have been converted to their final format, and normalized if selected
			for (uint16_t bone_index = 0; bone_index < num_bones; ++bone_index)
			{
				BoneStreams& bone_stream = bone_streams[bone_index];

				// Default tracks aren't quantized
				if (bone_stream.is_translation_default)
					continue;

				// Constant translation tracks store the remaining sample with full precision
				if (bone_stream.is_translation_constant)
					quantize_fixed_translation_stream(allocator, bone_stream.translations, VectorFormat8::Vector3_96, bone_stream.translations);
				else
					quantize_fixed_translation_stream(allocator, bone_stream.translations, bit_rate, bone_stream.translations);
			}
		}

		struct QuantizationContext
		{
			Allocator& allocator;
			BoneStreams* bone_streams;
			uint16_t num_bones;
			RotationFormat8 rotation_format;
			VectorFormat8 translation_format;
			const RigidSkeleton& skeleton;

			uint32_t num_samples;
			uint32_t segment_sample_start_index;
			float sample_rate;
			float error_threshold;
			float clip_duration;
			float segment_duration;

			const BoneStreams* raw_bone_streams;

			Transform_32* raw_local_pose;
			Transform_32* lossy_local_pose;
			BoneBitRate* bit_rate_per_bone;

			QuantizationContext(Allocator& allocator_, SegmentContext& segment, RotationFormat8 rotation_format_, VectorFormat8 translation_format_, const AnimationClip& clip_, const RigidSkeleton& skeleton_)
				: allocator(allocator_)
				, bone_streams(segment.bone_streams)
				, num_bones(segment.num_bones)
				, rotation_format(rotation_format_)
				, translation_format(translation_format_)
				, skeleton(skeleton_)
			{
				num_samples = segment.num_samples;
				segment_sample_start_index = segment.clip_sample_offset;
				sample_rate = float(segment.bone_streams[0].rotations.get_sample_rate());
				error_threshold = clip_.get_error_threshold();
				clip_duration = clip_.get_duration();
				segment_duration = float(num_samples - 1) / sample_rate;

				raw_local_pose = allocate_type_array<Transform_32>(allocator, num_bones);
				lossy_local_pose = allocate_type_array<Transform_32>(allocator, num_bones);
				bit_rate_per_bone = allocate_type_array<BoneBitRate>(allocator, num_bones);
			}

			~QuantizationContext()
			{
				deallocate_type_array(allocator, raw_local_pose, num_bones);
				deallocate_type_array(allocator, lossy_local_pose, num_bones);
				deallocate_type_array(allocator, bit_rate_per_bone, num_bones);
			}
		};

		inline float calculate_max_error_at_bit_rate(QuantizationContext& context, uint16_t target_bone_index, bool use_local_error, bool scan_whole_clip = false)
		{
			float max_error = 0.0f;
			constexpr bool use_raw_streams = true;
			float ref_duration = use_raw_streams ? float(get_animated_num_samples(context.raw_bone_streams, context.num_bones) - 1) / context.sample_rate : context.clip_duration;

			for (uint32_t sample_index = 0; sample_index < context.num_samples; ++sample_index)
			{
				// Sample our streams and calculate the error
				float sample_time = min(float(sample_index) / context.sample_rate, context.segment_duration);
				float ref_sample_time = use_raw_streams ? min(float(context.segment_sample_start_index + sample_index) / context.sample_rate, context.clip_duration) : sample_time;

				const BoneStreams* ref_bone_streams = use_raw_streams ? context.raw_bone_streams : context.bone_streams;
				sample_streams(ref_bone_streams, context.num_bones, ref_sample_time, context.raw_local_pose);
				sample_streams(context.bone_streams, context.num_bones, sample_time, context.bit_rate_per_bone, context.rotation_format, context.translation_format, context.lossy_local_pose);

				// Constant branch
				float error;
				if (use_local_error)
					error = calculate_local_bone_error(context.skeleton, context.raw_local_pose, context.lossy_local_pose, target_bone_index);
				else
					error = calculate_object_bone_error(context.skeleton, context.raw_local_pose, context.lossy_local_pose, target_bone_index);

				max_error = max(max_error, error);
				if (!scan_whole_clip && error >= context.error_threshold)
					break;
			}

			return max_error;
		}

		inline void calculate_local_space_bit_rates(QuantizationContext& context)
		{
			// Here is how an exhaustive search to minimize the total bit rate works out for a single bone with 2 tracks
			// rot + 1 trans + 0 ( 3), rot + 0 trans + 1 ( 3)
			// rot + 2 trans + 0 ( 6), rot + 1 trans + 1 ( 6), rot + 0 trans + 2 ( 6)
			// rot + 3 trans + 0 ( 9), rot + 2 trans + 1 ( 9), rot + 1 trans + 2 ( 9), rot + 0 trans + 3 ( 9)
			// rot + 4 trans + 0 (12), rot + 3 trans + 1 (12), rot + 2 trans + 2 (12), rot + 1 trans + 3 (12), rot + 0 trans + 4 (12)
			// rot + 5 trans + 0 (15), rot + 4 trans + 1 (15), rot + 3 trans + 2 (15), rot + 2 trans + 3 (15), rot + 1 trans + 4 (15), rot + 0 trans + 5 (15)

			// rot + 1 trans + 5 (18), rot + 2 trans + 4 (18), rot + 3 trans + 3 (18), rot + 4 trans + 2 (18), rot + 5 trans + 1 (18)
			// rot + 2 trans + 5 (21), rot + 3 trans + 4 (21), rot + 4 trans + 3 (21), rot + 5 trans + 2 (21)
			// rot + 3 trans + 5 (24), rot + 4 trans + 4 (24), rot + 5 trans + 3 (24)
			// rot + 4 trans + 5 (27), rot + 5 trans + 4 (27)
			// rot + 5 trans + 5 (30)
			for (uint16_t bone_index = 0; bone_index < context.num_bones; ++bone_index)
			{
				const BoneBitRate bone_bit_rates = context.bit_rate_per_bone[bone_index];

				if (bone_bit_rates.rotation == INVALID_BIT_RATE && bone_bit_rates.translation == INVALID_BIT_RATE)
				{
#if ACL_DEBUG_VARIABLE_QUANTIZATION
					printf("%u: Best bit rates: %u | %u\n", bone_index, bone_bit_rates.rotation, bone_bit_rates.translation);
#endif
					continue;
				}

				BoneBitRate best_bit_rates = BoneBitRate{ std::max<uint8_t>(bone_bit_rates.rotation, HIGHEST_BIT_RATE), std::max<uint8_t>(bone_bit_rates.translation, HIGHEST_BIT_RATE) };
				uint8_t best_size = 0xFF;
				float best_error = context.error_threshold;

				uint8_t num_iterations = NUM_BIT_RATES - LOWEST_BIT_RATE - 1;
				for (uint8_t iteration = 1; iteration <= num_iterations; ++iteration)
				{
					uint8_t target_sum = 3 * iteration;

					for (uint8_t rotation_bit_rate = bone_bit_rates.rotation; rotation_bit_rate < NUM_BIT_RATES || rotation_bit_rate >= HIGHEST_BIT_RATE; ++rotation_bit_rate)
					{
						for (uint8_t translation_bit_rate = bone_bit_rates.translation; translation_bit_rate < NUM_BIT_RATES || translation_bit_rate >= HIGHEST_BIT_RATE; ++translation_bit_rate)
						{
							uint8_t rotation_increment = rotation_bit_rate - bone_bit_rates.rotation;
							uint8_t translation_increment = translation_bit_rate - bone_bit_rates.translation;
							uint8_t current_sum = rotation_increment * 3 + translation_increment * 3;
							if (current_sum != target_sum)
							{
								if (translation_bit_rate >= HIGHEST_BIT_RATE)
									break;
								else
									continue;
							}

							context.bit_rate_per_bone[bone_index] = BoneBitRate{ rotation_bit_rate, translation_bit_rate };
							float error = calculate_max_error_at_bit_rate(context, bone_index, true);

#if ACL_DEBUG_VARIABLE_QUANTIZATION > 1
							printf("%u: %u | %u (%u) = %f\n", bone_index, rotation_bit_rate, translation_bit_rate, target_sum, error);
#endif

							if (error < best_error && target_sum <= best_size)
							{
								best_size = target_sum;
								best_error = error;
								best_bit_rates = context.bit_rate_per_bone[bone_index];
							}

							context.bit_rate_per_bone[bone_index] = bone_bit_rates;

							if (translation_bit_rate >= HIGHEST_BIT_RATE)
								break;
						}

						if (rotation_bit_rate >= HIGHEST_BIT_RATE)
							break;
					}

					if (best_size != 0xFF)
						break;
				}

				if (best_size == 0xFF)
				{
					for (uint8_t iteration = 1; iteration <= num_iterations; ++iteration)
					{
						uint8_t target_sum = 3 * iteration + (3 * num_iterations);

						for (uint8_t rotation_bit_rate = bone_bit_rates.rotation; rotation_bit_rate < NUM_BIT_RATES || rotation_bit_rate >= HIGHEST_BIT_RATE; ++rotation_bit_rate)
						{
							for (uint8_t translation_bit_rate = bone_bit_rates.translation; translation_bit_rate < NUM_BIT_RATES || translation_bit_rate >= HIGHEST_BIT_RATE; ++translation_bit_rate)
							{
								uint8_t rotation_increment = rotation_bit_rate - bone_bit_rates.rotation;
								uint8_t translation_increment = translation_bit_rate - bone_bit_rates.translation;
								uint8_t current_sum = rotation_increment * 3 + translation_increment * 3;
								if (current_sum != target_sum)
								{
									if (translation_bit_rate >= HIGHEST_BIT_RATE)
										break;
									else
										continue;
								}

								context.bit_rate_per_bone[bone_index] = BoneBitRate{ rotation_bit_rate, translation_bit_rate };
								float error = calculate_max_error_at_bit_rate(context, bone_index, true);

#if ACL_DEBUG_VARIABLE_QUANTIZATION > 1
								printf("%u: %u | %u (%u) = %f\n", bone_index, rotation_bit_rate, translation_bit_rate, target_sum, error);
#endif

								if (error < best_error && target_sum <= best_size)
								{
									best_size = target_sum;
									best_error = error;
									best_bit_rates = context.bit_rate_per_bone[bone_index];
								}

								context.bit_rate_per_bone[bone_index] = bone_bit_rates;

								if (translation_bit_rate >= HIGHEST_BIT_RATE)
									break;
							}

							if (rotation_bit_rate >= HIGHEST_BIT_RATE)
								break;
						}

						if (best_size != 0xFF)
							break;
					}
				}

#if ACL_DEBUG_VARIABLE_QUANTIZATION
				printf("%u: Best bit rates: %u | %u (%u) = %f\n", bone_index, best_bit_rates.rotation, best_bit_rates.translation, best_size, best_error);
#endif
				context.bit_rate_per_bone[bone_index] = best_bit_rates;
			}
		}

		inline uint8_t increment_and_clamp_bit_rate(uint8_t bit_rate, uint8_t increment)
		{
			return bit_rate >= HIGHEST_BIT_RATE ? bit_rate : std::min<uint8_t>(bit_rate + increment, HIGHEST_BIT_RATE);
		}

		inline float increase_bone_bit_rate(QuantizationContext& context, uint16_t bone_index, uint8_t num_increments, float old_error, BoneBitRate& out_best_bit_rates)
		{
			const BoneBitRate bone_bit_rates = context.bit_rate_per_bone[bone_index];

			BoneBitRate best_bit_rates = bone_bit_rates;
			float best_error = old_error;

			for (uint8_t rotation_increment = 0; rotation_increment <= num_increments; ++rotation_increment)
			{
				uint8_t rotation_bit_rate = increment_and_clamp_bit_rate(bone_bit_rates.rotation, rotation_increment);

				for (uint8_t translation_increment = 0; translation_increment <= num_increments; ++translation_increment)
				{
					uint8_t translation_bit_rate = increment_and_clamp_bit_rate(bone_bit_rates.translation, translation_increment);

					if (rotation_increment + translation_increment != num_increments)
					{
						if (translation_bit_rate >= HIGHEST_BIT_RATE)
							break;
						else
							continue;
					}

					context.bit_rate_per_bone[bone_index] = BoneBitRate{ rotation_bit_rate, translation_bit_rate };
					float error = calculate_max_error_at_bit_rate(context, bone_index, false);

					if (error < best_error)
					{
						best_error = error;
						best_bit_rates = context.bit_rate_per_bone[bone_index];
					}

					context.bit_rate_per_bone[bone_index] = bone_bit_rates;

					if (translation_bit_rate >= HIGHEST_BIT_RATE)
						break;
				}

				if (rotation_bit_rate >= HIGHEST_BIT_RATE)
					break;
			}

			out_best_bit_rates = best_bit_rates;
			return best_error;
		}

		inline float calculate_bone_permutation_error(QuantizationContext& context, BoneBitRate* permutation_bit_rates, uint8_t* bone_chain_permutation, const uint16_t* chain_bone_indices, uint16_t num_bones_in_chain, uint16_t bone_index, BoneBitRate* best_bit_rates, float old_error)
		{
			float best_error = old_error;

			do
			{
				// Copy our current bit rates to the permutation rates
				memcpy(permutation_bit_rates, context.bit_rate_per_bone, sizeof(BoneBitRate) * context.num_bones);

				bool is_permutation_valid = false;
				for (uint16_t chain_link_index = 0; chain_link_index < num_bones_in_chain; ++chain_link_index)
				{
					if (bone_chain_permutation[chain_link_index] != 0)
					{
						// Increase bit rate
						uint16_t bone_index = chain_bone_indices[chain_link_index];
						BoneBitRate best_bit_rates;
						increase_bone_bit_rate(context, bone_index, bone_chain_permutation[chain_link_index], old_error, best_bit_rates);
						is_permutation_valid |= best_bit_rates.rotation != permutation_bit_rates[bone_index].rotation;
						is_permutation_valid |= best_bit_rates.translation != permutation_bit_rates[bone_index].translation;
						permutation_bit_rates[bone_index] = best_bit_rates;
					}
				}

				if (!is_permutation_valid)
					continue;

				// Measure error
				std::swap(context.bit_rate_per_bone, permutation_bit_rates);
				float permutation_error = calculate_max_error_at_bit_rate(context, bone_index, false);
				std::swap(context.bit_rate_per_bone, permutation_bit_rates);

				if (permutation_error < best_error)
				{
					best_error = permutation_error;
					memcpy(best_bit_rates, permutation_bit_rates, sizeof(BoneBitRate) * context.num_bones);

					if (permutation_error < context.error_threshold)
						break;
				}
			} while (std::next_permutation(bone_chain_permutation, bone_chain_permutation + num_bones_in_chain));

			return best_error;
		}

		inline void quantize_variable_streams_new(Allocator& allocator, SegmentContext& segment, RotationFormat8 rotation_format, VectorFormat8 translation_format, const AnimationClip& clip, const RigidSkeleton& skeleton, const BoneStreams* raw_bone_streams)
		{
			// Duplicate our streams
			BoneStreams* quantized_streams = allocate_type_array<BoneStreams>(allocator, segment.num_bones);
			for (uint16_t bone_index = 0; bone_index < segment.num_bones; ++bone_index)
				quantized_streams[bone_index] = segment.bone_streams[bone_index].duplicate();

			const bool is_rotation_variable = is_rotation_format_variable(rotation_format);
			const bool is_translation_variable = is_vector_format_variable(translation_format);

			// Quantize everything to the lowest bit rate of the same variant
			if (is_rotation_variable)
				quantize_fixed_rotation_streams(allocator, quantized_streams, segment.num_bones, LOWEST_BIT_RATE);
			else
				quantize_fixed_rotation_streams(allocator, quantized_streams, segment.num_bones, rotation_format, false);

			if (is_translation_variable)
				quantize_fixed_translation_streams(allocator, quantized_streams, segment.num_bones, LOWEST_BIT_RATE);
			else
				quantize_fixed_translation_streams(allocator, quantized_streams, segment.num_bones, translation_format);

			QuantizationContext context(allocator, segment, rotation_format, translation_format, clip, skeleton);
			context.raw_bone_streams = raw_bone_streams;

			for (uint16_t bone_index = 0; bone_index < segment.num_bones; ++bone_index)
				context.bit_rate_per_bone[bone_index] = BoneBitRate{ quantized_streams[bone_index].rotations.get_bit_rate(), quantized_streams[bone_index].translations.get_bit_rate() };

			// First iterate over all bones and find the optimal bit rate for each track using the local space error.
			// We use the local space error to prime the algorithm. If each parent bone has infinite precision,
			// the local space error is equivalent. Since parents are lossy, it is a good approximation. It means
			// that whatever bit rate we find for a bone, it cannot be lower to reach our error threshold since
			// a lossy parent means we need to be equally or more accurate to maintain the threshold.
			//
			// In practice, the error from a child can compensate the error introduced by the parent but
			// this is unlikely to hold true for a whole track at every key. We thus make the assumption
			// that increasing the precision is always good regardless of the hierarchy level.

			calculate_local_space_bit_rates(context);

			// Now that we found an approximate lower bound for the bit rates, we start at the root and perform a brute force search.
			// For each bone, we do the following:
			//    - If object space error meets our error threshold, do nothing
			//    - Iterate over each bone in the chain and increment the bit rate by 1 (rotation or translation, pick lowest error)
			//    - Pick the bone that improved the error the most and increment the bit rate by 1
			//    - Repeat until we meet our error threshold
			//
			// The root is already optimal from the previous step since the local space error is equal to the object space error.
			// Next we'll add one bone to the chain under the root. Performing the above steps, we perform an exhaustive search
			// to find the smallest memory footprint that will meet our error threshold. No combination with a lower memory footprint
			// could yield a smaller error.
			// Next we'll add another bone to the chain. By performing these steps recursively, we can ensure that the accuracy always
			// increases and the memory footprint is always as low as possible.

			// 3 bone chain expansion:
			// 3:	[bone 0] + 1 [bone 1] + 0 [bone 2] + 0 (3)
			//		[bone 0] + 0 [bone 1] + 1 [bone 2] + 0 (3)
			//		[bone 0] + 0 [bone 1] + 0 [bone 2] + 1 (3)
			// 6:	[bone 0] + 2 [bone 1] + 0 [bone 2] + 0 (6)
			//		[bone 0] + 1 [bone 1] + 1 [bone 2] + 0 (6)
			//		[bone 0] + 1 [bone 1] + 0 [bone 2] + 1 (6)
			//		[bone 0] + 0 [bone 1] + 1 [bone 2] + 1 (6)
			//		[bone 0] + 0 [bone 1] + 2 [bone 2] + 0 (6)
			//		[bone 0] + 0 [bone 1] + 0 [bone 2] + 2 (6)
			//10:	[bone 0] + 3 [bone 1] + 0 [bone 2] + 0 (9)
			//		[bone 0] + 2 [bone 1] + 1 [bone 2] + 0 (9)
			//		[bone 0] + 2 [bone 1] + 0 [bone 2] + 1 (9)
			//		[bone 0] + 1 [bone 1] + 2 [bone 2] + 0 (9)
			//		[bone 0] + 1 [bone 1] + 1 [bone 2] + 1 (9)
			//		[bone 0] + 1 [bone 1] + 0 [bone 2] + 2 (9)
			//		[bone 0] + 0 [bone 1] + 3 [bone 2] + 0 (9)
			//		[bone 0] + 0 [bone 1] + 2 [bone 2] + 1 (9)
			//		[bone 0] + 0 [bone 1] + 1 [bone 2] + 2 (9)
			//		[bone 0] + 0 [bone 1] + 0 [bone 2] + 3 (9)

			uint8_t* bone_chain_permutation = allocate_type_array<uint8_t>(allocator, segment.num_bones);
			uint16_t* chain_bone_indices = allocate_type_array<uint16_t>(allocator, segment.num_bones);
			BoneBitRate* permutation_bit_rates = allocate_type_array<BoneBitRate>(allocator, segment.num_bones);
			BoneBitRate* best_permutation_bit_rates = allocate_type_array<BoneBitRate>(allocator, segment.num_bones);
			BoneBitRate* best_bit_rates = allocate_type_array<BoneBitRate>(allocator, segment.num_bones);
			memcpy(best_bit_rates, context.bit_rate_per_bone, sizeof(BoneBitRate) * segment.num_bones);

			for (uint16_t bone_index = 0; bone_index < segment.num_bones; ++bone_index)
			{
				float error = calculate_max_error_at_bit_rate(context, bone_index, false);
				if (error < context.error_threshold)
					continue;

				if (context.bit_rate_per_bone[bone_index].rotation >= HIGHEST_BIT_RATE && context.bit_rate_per_bone[bone_index].translation >= HIGHEST_BIT_RATE)
				{
					// Our bone already has the highest precision possible locally, if the local error already exceeds our threshold,
					// there is nothing we can do, bail out
					float local_error = calculate_max_error_at_bit_rate(context, bone_index, true);
					if (local_error >= context.error_threshold)
						continue;
				}

				uint16_t current_bone_index = bone_index;
				uint16_t num_bones_in_chain = 0;
				while (current_bone_index != INVALID_BONE_INDEX)
				{
					chain_bone_indices[num_bones_in_chain] = current_bone_index;
					num_bones_in_chain++;

					const RigidBone& bone = skeleton.get_bone(current_bone_index);
					current_bone_index = bone.parent_index;
				}

				// Root first
				std::reverse(chain_bone_indices, chain_bone_indices + num_bones_in_chain);

				float initial_error = error;

				while (error >= context.error_threshold)
				{
					// Generate permutations for up to 3 bit rate increments
					// Perform an exhaustive search of the permutations and pick the best result
					// If our best error is under the threshold, we are done, otherwise we will try again from there
					float original_error = error;
					float best_error = error;

					// The first permutation increases the bit rate of a single track/bone
					std::fill(bone_chain_permutation, bone_chain_permutation + segment.num_bones, 0);
					bone_chain_permutation[num_bones_in_chain - 1] = 1;
					error = calculate_bone_permutation_error(context, permutation_bit_rates, bone_chain_permutation, chain_bone_indices, num_bones_in_chain, bone_index, best_permutation_bit_rates, original_error);
					if (error < best_error)
					{
						best_error = error;
						memcpy(best_bit_rates, best_permutation_bit_rates, sizeof(BoneBitRate) * context.num_bones);

						if (error < context.error_threshold)
							break;
					}

					// The second permutation increases the bit rate of 2 track/bones
					std::fill(bone_chain_permutation, bone_chain_permutation + segment.num_bones, 0);
					bone_chain_permutation[num_bones_in_chain - 1] = 2;
					error = calculate_bone_permutation_error(context, permutation_bit_rates, bone_chain_permutation, chain_bone_indices, num_bones_in_chain, bone_index, best_permutation_bit_rates, original_error);
					if (error < best_error)
					{
						best_error = error;
						memcpy(best_bit_rates, best_permutation_bit_rates, sizeof(BoneBitRate) * context.num_bones);

						if (error < context.error_threshold)
							break;
					}

					if (num_bones_in_chain > 1)
					{
						std::fill(bone_chain_permutation, bone_chain_permutation + segment.num_bones, 0);
						bone_chain_permutation[num_bones_in_chain - 2] = 1;
						bone_chain_permutation[num_bones_in_chain - 1] = 1;
						error = calculate_bone_permutation_error(context, permutation_bit_rates, bone_chain_permutation, chain_bone_indices, num_bones_in_chain, bone_index, best_permutation_bit_rates, original_error);
						if (error < best_error)
						{
							best_error = error;
							memcpy(best_bit_rates, best_permutation_bit_rates, sizeof(BoneBitRate) * context.num_bones);

							if (error < context.error_threshold)
								break;
						}
					}

					// The third permutation increases the bit rate of 3 track/bones
					std::fill(bone_chain_permutation, bone_chain_permutation + segment.num_bones, 0);
					bone_chain_permutation[num_bones_in_chain - 1] = 3;
					error = calculate_bone_permutation_error(context, permutation_bit_rates, bone_chain_permutation, chain_bone_indices, num_bones_in_chain, bone_index, best_permutation_bit_rates, original_error);
					if (error < best_error)
					{
						best_error = error;
						memcpy(best_bit_rates, best_permutation_bit_rates, sizeof(BoneBitRate) * context.num_bones);

						if (error < context.error_threshold)
							break;
					}

					if (num_bones_in_chain > 1)
					{
						std::fill(bone_chain_permutation, bone_chain_permutation + segment.num_bones, 0);
						bone_chain_permutation[num_bones_in_chain - 2] = 2;
						bone_chain_permutation[num_bones_in_chain - 1] = 1;
						error = calculate_bone_permutation_error(context, permutation_bit_rates, bone_chain_permutation, chain_bone_indices, num_bones_in_chain, bone_index, best_permutation_bit_rates, original_error);
						if (error < best_error)
						{
							best_error = error;
							memcpy(best_bit_rates, best_permutation_bit_rates, sizeof(BoneBitRate) * context.num_bones);

							if (error < context.error_threshold)
								break;
						}

						if (num_bones_in_chain > 2)
						{
							std::fill(bone_chain_permutation, bone_chain_permutation + segment.num_bones, 0);
							bone_chain_permutation[num_bones_in_chain - 3] = 1;
							bone_chain_permutation[num_bones_in_chain - 2] = 1;
							bone_chain_permutation[num_bones_in_chain - 1] = 1;
							error = calculate_bone_permutation_error(context, permutation_bit_rates, bone_chain_permutation, chain_bone_indices, num_bones_in_chain, bone_index, best_permutation_bit_rates, original_error);
							if (error < best_error)
							{
								best_error = error;
								memcpy(best_bit_rates, best_permutation_bit_rates, sizeof(BoneBitRate) * context.num_bones);

								if (error < context.error_threshold)
									break;
							}
						}
					}

					if (best_error >= original_error)
						break;	// No progress made

					error = best_error;
					if (error < original_error)
					{
#if ACL_DEBUG_VARIABLE_QUANTIZATION
						std::swap(context.bit_rate_per_bone, best_bit_rates);
						float new_error = calculate_max_error_at_bit_rate(context, bone_index, false, true);
						std::swap(context.bit_rate_per_bone, best_bit_rates);

						for (uint16_t i = 0; i < context.num_bones; ++i)
						{
							bool rotation_differs = context.bit_rate_per_bone[i].rotation != best_bit_rates[i].rotation;
							bool translation_differs = context.bit_rate_per_bone[i].translation != best_bit_rates[i].translation;
							if (rotation_differs || translation_differs)
								printf("%u: %u | %u => %u  %u (%f)\n", i, context.bit_rate_per_bone[i].rotation, context.bit_rate_per_bone[i].translation, best_bit_rates[i].rotation, best_bit_rates[i].translation, new_error);
						}
#endif

						memcpy(context.bit_rate_per_bone, best_bit_rates, sizeof(BoneBitRate) * context.num_bones);
					}
				}

				if (error < initial_error)
				{
#if ACL_DEBUG_VARIABLE_QUANTIZATION
					std::swap(context.bit_rate_per_bone, best_bit_rates);
					float new_error = calculate_max_error_at_bit_rate(context, bone_index, false, true);
					std::swap(context.bit_rate_per_bone, best_bit_rates);

					for (uint16_t i = 0; i < context.num_bones; ++i)
					{
						bool rotation_differs = context.bit_rate_per_bone[i].rotation != best_bit_rates[i].rotation;
						bool translation_differs = context.bit_rate_per_bone[i].translation != best_bit_rates[i].translation;
						if (rotation_differs || translation_differs)
							printf("%u: %u | %u => %u  %u (%f)\n", i, context.bit_rate_per_bone[i].rotation, context.bit_rate_per_bone[i].translation, best_bit_rates[i].rotation, best_bit_rates[i].translation, new_error);
					}
#endif

					memcpy(context.bit_rate_per_bone, best_bit_rates, sizeof(BoneBitRate) * context.num_bones);
				}

				// Last ditch effort if our error remains too high, this should be rare
				error = calculate_max_error_at_bit_rate(context, bone_index, false);
				while (error >= context.error_threshold)
				{
					// From child to parent, increase the bit rate indiscriminately
					uint16_t num_maxed_out = 0;
					for (int16_t chain_link_index = num_bones_in_chain - 1; chain_link_index >= 0; --chain_link_index)
					{
						uint16_t chain_bone_index = chain_bone_indices[chain_link_index];
						while (error >= context.error_threshold)
						{
							if (context.bit_rate_per_bone[chain_bone_index].rotation >= HIGHEST_BIT_RATE && context.bit_rate_per_bone[chain_bone_index].translation >= HIGHEST_BIT_RATE)
							{
								num_maxed_out++;
								break;
							}

							if (context.bit_rate_per_bone[chain_bone_index].rotation < context.bit_rate_per_bone[chain_bone_index].translation)
								context.bit_rate_per_bone[chain_bone_index].rotation++;
							else
								context.bit_rate_per_bone[chain_bone_index].translation++;

							error = calculate_max_error_at_bit_rate(context, bone_index, false);

#if ACL_DEBUG_VARIABLE_QUANTIZATION
							printf("%u: => %u  %u (%f)\n", chain_bone_index, context.bit_rate_per_bone[chain_bone_index].rotation, context.bit_rate_per_bone[chain_bone_index].translation, error);
#endif
						}

						if (error < context.error_threshold)
							break;
					}

					if (num_maxed_out == num_bones_in_chain)
						break;

					// TODO: Try to lower the bit rate again in the reverse direction?
				}
			}

#if ACL_DEBUG_VARIABLE_QUANTIZATION
			printf("Variable quantization optimization results:\n");
			for (uint16_t i = 0; i < context.num_bones; ++i)
			{
				float error = calculate_max_error_at_bit_rate(context, i, false, true);
				printf("%u: %u | %u => %f %s\n", i, context.bit_rate_per_bone[i].rotation, context.bit_rate_per_bone[i].translation, error, error >= context.error_threshold ? "!" : "");
			}
#endif

			const ClipContext* clip_context = segment.clip;
			bool are_rotations_normalized = clip_context->are_rotations_normalized;

			// Quantize and swap our streams
			for (uint16_t bone_index = 0; bone_index < segment.num_bones; ++bone_index)
			{
				if (context.bit_rate_per_bone[bone_index].rotation != INVALID_BIT_RATE)
					quantize_fixed_rotation_stream(allocator, segment.bone_streams[bone_index].rotations, context.bit_rate_per_bone[bone_index].rotation, are_rotations_normalized, quantized_streams[bone_index].rotations);

				if (context.bit_rate_per_bone[bone_index].translation != INVALID_BIT_RATE)
					quantize_fixed_translation_stream(allocator, segment.bone_streams[bone_index].translations, context.bit_rate_per_bone[bone_index].translation, quantized_streams[bone_index].translations);

				std::swap(segment.bone_streams[bone_index], quantized_streams[bone_index]);
			}

			deallocate_type_array(allocator, bone_chain_permutation, segment.num_bones);
			deallocate_type_array(allocator, chain_bone_indices, segment.num_bones);
			deallocate_type_array(allocator, permutation_bit_rates, segment.num_bones);
			deallocate_type_array(allocator, best_permutation_bit_rates, segment.num_bones);
			deallocate_type_array(allocator, best_bit_rates, segment.num_bones);
		}

		inline void quantize_variable_streams(Allocator& allocator, BoneStreams* bone_streams, uint16_t num_bones, RotationFormat8 rotation_format, VectorFormat8 translation_format, const AnimationClip& clip, const RigidSkeleton& skeleton)
		{
			// Duplicate our streams
			BoneStreams* quantized_streams = allocate_type_array<BoneStreams>(allocator, num_bones);
			for (uint16_t bone_index = 0; bone_index < num_bones; ++bone_index)
				quantized_streams[bone_index] = bone_streams[bone_index].duplicate();

			const bool is_rotation_variable = is_rotation_format_variable(rotation_format);
			const bool is_translation_variable = is_vector_format_variable(translation_format);
			const bool scan_whole_clip_for_bad_bone = false;

			// Quantize everything to the lowest bit rate of the same variant
			if (is_rotation_variable)
				quantize_fixed_rotation_streams(allocator, quantized_streams, num_bones, LOWEST_BIT_RATE);
			else
				quantize_fixed_rotation_streams(allocator, quantized_streams, num_bones, rotation_format, false);

			if (is_translation_variable)
				quantize_fixed_translation_streams(allocator, quantized_streams, num_bones, LOWEST_BIT_RATE);
			else
				quantize_fixed_translation_streams(allocator, quantized_streams, num_bones, translation_format);

			uint32_t num_samples = 1;//get_animated_num_samples(bone_streams, num_bones);
			float sample_rate = float(bone_streams[0].rotations.get_sample_rate());
			float error_threshold = clip.get_error_threshold();
			float clip_duration = clip.get_duration();
			float error = std::numeric_limits<float>::max();

			// TODO: Use the original un-quantized bone streams?
			// It seems to yield a smaller memory footprint but it could be dangerous since our data diverges from
			// the 64bit original clip and we might also be normalized, adding further loss
			// Basically by using the bone streams, the error we measure is compared to the possibly normalized,
			// converted rotations, etc.
			constexpr bool use_clip_as_ref = true;

			Transform_32* raw_local_pose = allocate_type_array<Transform_32>(allocator, num_bones);
			Transform_32* lossy_local_pose = allocate_type_array<Transform_32>(allocator, num_bones);
			float* error_per_bone = allocate_type_array<float>(allocator, num_bones);
			float* error_per_bone2 = allocate_type_array<float>(allocator, num_bones);
			BoneTrackError* error_per_stream = allocate_type_array<BoneTrackError>(allocator, num_bones);
			BoneTrackError* error_per_stream2 = allocate_type_array<BoneTrackError>(allocator, num_bones);

			struct BoneBitRate { uint8_t rotation; uint8_t translation; };
			BoneBitRate* bit_rate_per_bone = allocate_type_array<BoneBitRate>(allocator, num_bones);

			for (uint16_t bone_index = 0; bone_index < num_bones; ++bone_index)
				bit_rate_per_bone[bone_index] = { quantized_streams[bone_index].rotations.get_bit_rate(), quantized_streams[bone_index].translations.get_bit_rate() };

			uint32_t bitset_size = get_bitset_size(num_bones);
			uint32_t* low_resolution_bones = allocate_type_array<uint32_t>(allocator, bitset_size);

			bitset_reset(low_resolution_bones, bitset_size, false);

			// While we are above our precision threshold, iterate
			while (error > error_threshold)
			{
				error = 0.0f;

				// Scan the whole clip, and find the bone with the worst error across the whole clip
				uint16_t bad_bone_index = INVALID_BONE_INDEX;
				float worst_clip_error = error_threshold;
				float worst_sample_time = 0.0f;
				for (uint32_t sample_index = 0; sample_index < num_samples; ++sample_index)
				{
					// Sample our streams and calculate the error
					float sample_time = min(float(sample_index) / sample_rate, clip_duration);

					if (use_clip_as_ref)
						clip.sample_pose(sample_time, raw_local_pose, num_bones);
					else
						sample_streams(bone_streams, num_bones, sample_time, raw_local_pose);

					sample_streams(quantized_streams, num_bones, sample_time, lossy_local_pose);

					calculate_skeleton_error(allocator, skeleton, raw_local_pose, lossy_local_pose, error_per_bone);

					// Find first bone in the hierarchy that is above our threshold (root first)
					for (uint16_t bone_index = 0; bone_index < num_bones; ++bone_index)
					{
						if (error_per_bone[bone_index] > worst_clip_error && !bitset_test(low_resolution_bones, bitset_size, bone_index))
						{
							worst_clip_error = error_per_bone[bone_index];
							error = error_per_bone[bone_index];
							bad_bone_index = bone_index;
							worst_sample_time = sample_time;
							break;
						}
					}

					if (!scan_whole_clip_for_bad_bone && bad_bone_index != INVALID_BONE_INDEX)
						break;
				}

				if (bad_bone_index == INVALID_BONE_INDEX)
				{
					// We probably have some low resolution bones for some reason, stop nows
					break;
				}

				// Find which bone in the chain contributes the most error that isn't at the highest precision
				calculate_skeleton_error_contribution(skeleton, raw_local_pose, lossy_local_pose, bad_bone_index, error_per_stream);

				uint16_t target_bone_index = INVALID_BONE_INDEX;
				AnimationTrackType8 target_track_type = AnimationTrackType8::Rotation;
				float worst_track_error = 0.0f;

				// We search starting at the root bone, by increasing the precision of a bone higher up, we retain more children
				// with lower precision, and keep the memory footprint lower as a result
				uint16_t current_bone_index = bad_bone_index;
				while (current_bone_index != INVALID_BONE_INDEX)
				{
					// Only select the stream if we can still increase its precision
					uint8_t rotation_bit_rate = quantized_streams[current_bone_index].rotations.get_bit_rate();
					bool can_increase_rotation_precision = is_rotation_variable && rotation_bit_rate < HIGHEST_BIT_RATE;
					if (can_increase_rotation_precision && error_per_stream[current_bone_index].rotation > worst_track_error)
					{
						target_bone_index = current_bone_index;
						worst_track_error = error_per_stream[current_bone_index].rotation;
						target_track_type = AnimationTrackType8::Rotation;
					}

					uint8_t translation_bit_rate = quantized_streams[current_bone_index].translations.get_bit_rate();
					bool can_increase_translation_precision = is_translation_variable && translation_bit_rate < HIGHEST_BIT_RATE;
					if (can_increase_translation_precision && error_per_stream[current_bone_index].translation > worst_track_error)
					{
						target_bone_index = current_bone_index;
						worst_track_error = error_per_stream[current_bone_index].translation;
						target_track_type = AnimationTrackType8::Translation;
					}

					const RigidBone& bone = skeleton.get_bone(current_bone_index);
					current_bone_index = bone.parent_index;
				}

				if (target_bone_index == INVALID_BONE_INDEX)
				{
					// Failed to find a target stream that we could increase its precision
					// This is bad, we have a bone with an error above the error threshold,
					// but every bone in the hierarchy leading up to it is at full precision.
					// Bail out

					// In practice, this should only ever happen if rotations or translations are quantized
					// to a fixed format which yields high loss while the other tracks are variable.
					// They will attempt to keep as much precision as possible but ultimately fail.
					// Variable precision works best if all tracks are variable.

					bitset_set(low_resolution_bones, bitset_size, bad_bone_index, true);
					continue;
				}

				// Increase its bit rate a bit
				if (target_track_type == AnimationTrackType8::Rotation)
				{
					uint8_t new_bit_rate = quantized_streams[target_bone_index].rotations.get_bit_rate() + 1;

#if ACL_DEBUG_VARIABLE_QUANTIZATION
					printf("SELECTED R %u: %u -> %u\n", target_bone_index, quantized_streams[target_bone_index].rotations.get_bit_rate(), new_bit_rate);
#endif

					const ClipContext* clip_context = bone_streams[target_bone_index].segment->clip;
					bool are_rotations_normalized = clip_context->are_rotations_normalized;

					quantize_fixed_rotation_stream(allocator, bone_streams[target_bone_index].rotations, new_bit_rate, are_rotations_normalized, quantized_streams[target_bone_index].rotations);
					bit_rate_per_bone[target_bone_index].rotation = new_bit_rate;
				}
				else
				{
					uint8_t new_bit_rate = quantized_streams[target_bone_index].translations.get_bit_rate() + 1;

#if ACL_DEBUG_VARIABLE_QUANTIZATION
					printf("SELECTED T %u: %u -> %u\n", target_bone_index, quantized_streams[target_bone_index].translations.get_bit_rate(), new_bit_rate);
#endif

					quantize_fixed_translation_stream(allocator, bone_streams[target_bone_index].translations, new_bit_rate, quantized_streams[target_bone_index].translations);
					bit_rate_per_bone[target_bone_index].translation = new_bit_rate;
				}
			}

#if ACL_DEBUG_VARIABLE_QUANTIZATION
			error = 0.0f;
			for (uint16_t bone_index = 0; bone_index < num_bones; ++bone_index)
				error = max(error, error_per_bone[bone_index]);
			printf("DUMPED ERROR: %f\n", error);
			for (uint16_t bone_index = 0; bone_index < num_bones; ++bone_index)
			{
				const BoneStreams& bone_stream = quantized_streams[bone_index];
				if (bone_stream.is_rotation_animated())
					printf("DUMPED R RATE: %u\n", bone_stream.rotations.get_bit_rate());
				if (bone_stream.is_translation_animated())
					printf("DUMPED T RATE: %u\n", bone_stream.translations.get_bit_rate());
			}
#endif

			// Swap our streams
			for (uint16_t bone_index = 0; bone_index < num_bones; ++bone_index)
				std::swap(bone_streams[bone_index], quantized_streams[bone_index]);

			deallocate_type_array(allocator, quantized_streams, num_bones);
			deallocate_type_array(allocator, raw_local_pose, num_bones);
			deallocate_type_array(allocator, lossy_local_pose, num_bones);
			deallocate_type_array(allocator, error_per_bone, num_bones);
			deallocate_type_array(allocator, error_per_stream, num_bones);
			deallocate_type_array(allocator, low_resolution_bones, bitset_size);
		}
	}

	inline void quantize_streams(Allocator& allocator, BoneStreams* bone_streams, uint16_t num_bones, RotationFormat8 rotation_format, VectorFormat8 translation_format, const AnimationClip& clip, const RigidSkeleton& skeleton, const BoneStreams* raw_bone_streams)
	{
		const bool is_rotation_variable = is_rotation_format_variable(rotation_format);
		const bool is_translation_variable = is_vector_format_variable(translation_format);
		constexpr bool use_new_variable_quantization = true;

		if (is_rotation_variable || is_translation_variable)
		{
			SegmentContext segment;
			segment.bone_streams = bone_streams;
			segment.num_bones = num_bones;
			segment.num_samples = clip.get_num_samples();

			if (use_new_variable_quantization)
				impl::quantize_variable_streams_new(allocator, segment, rotation_format, translation_format, clip, skeleton, raw_bone_streams);
			else
				impl::quantize_variable_streams(allocator, bone_streams, num_bones, rotation_format, translation_format, clip, skeleton);
		}
		else
		{
			if (!is_rotation_variable)
				impl::quantize_fixed_rotation_streams(allocator, bone_streams, num_bones, rotation_format, false);

			if (!is_translation_variable)
				impl::quantize_fixed_translation_streams(allocator, bone_streams, num_bones, translation_format);
		}
	}

	inline void quantize_streams(Allocator& allocator, ClipContext& clip_context, RotationFormat8 rotation_format, VectorFormat8 translation_format, const AnimationClip& clip, const RigidSkeleton& skeleton, const ClipContext& raw_clip_context)
	{
		const bool is_rotation_variable = is_rotation_format_variable(rotation_format);
		const bool is_translation_variable = is_vector_format_variable(translation_format);
		constexpr bool use_new_variable_quantization = true;
		const BoneStreams* raw_bone_streams = raw_clip_context.segments[0].bone_streams;

		for (SegmentContext& segment : clip_context.segment_iterator())
		{
			if (is_rotation_variable || is_translation_variable)
			{
				if (use_new_variable_quantization)
					impl::quantize_variable_streams_new(allocator, segment, rotation_format, translation_format, clip, skeleton, raw_bone_streams);
				else
					impl::quantize_variable_streams(allocator, segment.bone_streams, segment.num_bones, rotation_format, translation_format, clip, skeleton);
			}
			else
			{
				if (!is_rotation_variable)
					impl::quantize_fixed_rotation_streams(allocator, segment.bone_streams, segment.num_bones, rotation_format, false);

				if (!is_translation_variable)
					impl::quantize_fixed_translation_streams(allocator, segment.bone_streams, segment.num_bones, translation_format);
			}
		}
	}
}
