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
#include "acl/compression/stream/normalize_streams.h"
#include "acl/compression/stream/convert_rotation_streams.h"
#include "acl/compression/skeleton_error_metric.h"
#include "acl/compression/compression_settings.h"

#include <cstddef>
#include <cstdint>

// 0 = no debug info, 1 = basic info, 2 = verbose
#define ACL_DEBUG_VARIABLE_QUANTIZATION		0

namespace acl
{
	namespace impl
	{
		struct QuantizationContext
		{
			Allocator& allocator;
			ClipContext& clip;
			const ClipContext& raw_clip;
			SegmentContext& segment;
			BoneStreams* bone_streams;
			uint16_t num_bones;
			RotationFormat8 rotation_format;
			VectorFormat8 translation_format;
			VectorFormat8 scale_format;
			const RigidSkeleton& skeleton;
			const ISkeletalErrorMetric& error_metric;

			uint32_t num_samples;
			uint32_t segment_sample_start_index;
			float sample_rate;
			float error_threshold;
			float clip_duration;
			float segment_duration;
			bool has_scale;

			const BoneStreams* raw_bone_streams;

			Transform_32* raw_local_pose;
			Transform_32* lossy_local_pose;
			BoneBitRate* bit_rate_per_bone;

			QuantizationContext(Allocator& allocator_, ClipContext& clip_, const ClipContext& raw_clip_, SegmentContext& segment_, const CompressionSettings& settings_, const RigidSkeleton& skeleton_)
				: allocator(allocator_)
				, clip(clip_)
				, raw_clip(raw_clip_)
				, segment(segment_)
				, bone_streams(segment_.bone_streams)
				, num_bones(segment_.num_bones)
				, rotation_format(settings_.rotation_format)
				, translation_format(settings_.translation_format)
				, scale_format(settings_.scale_format)
				, skeleton(skeleton_)
				, error_metric(*settings_.error_metric)
				, raw_bone_streams(raw_clip_.segments[0].bone_streams)
			{
				num_samples = segment_.num_samples;
				segment_sample_start_index = segment_.clip_sample_offset;
				sample_rate = float(segment.bone_streams[0].rotations.get_sample_rate());
				error_threshold = clip_.error_threshold;
				clip_duration = clip_.duration;
				segment_duration = float(num_samples - 1) / sample_rate;
				has_scale = segment_context_has_scale(segment_);

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

		inline void quantize_fixed_rotation_stream(Allocator& allocator, const RotationTrackStream& raw_stream, RotationFormat8 rotation_format, bool are_rotations_normalized, RotationTrackStream& out_quantized_stream)
		{
			// We expect all our samples to have the same width of sizeof(Vector4_32)
			ACL_ENSURE(raw_stream.get_sample_size() == sizeof(Vector4_32), "Unexpected rotation sample size. %u != %u", raw_stream.get_sample_size(), sizeof(Vector4_32));

			const uint32_t num_samples = raw_stream.get_num_samples();
			const uint32_t rotation_sample_size = get_packed_rotation_size(rotation_format);
			const uint32_t sample_rate = raw_stream.get_sample_rate();
			RotationTrackStream quantized_stream(allocator, num_samples, rotation_sample_size, sample_rate, rotation_format);

			for (uint32_t sample_index = 0; sample_index < num_samples; ++sample_index)
			{
				const Quat_32 rotation = raw_stream.get_raw_sample<Quat_32>(sample_index);
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

		inline void quantize_fixed_rotation_stream(QuantizationContext& context, uint16_t bone_index, RotationFormat8 rotation_format)
		{
			ACL_ENSURE(bone_index < context.num_bones, "Invalid bone index: %u", bone_index);

			BoneStreams& bone_stream = context.bone_streams[bone_index];

			// Default tracks aren't quantized
			if (bone_stream.is_rotation_default)
				return;

			const bool are_rotations_normalized = context.clip.are_rotations_normalized && !bone_stream.is_rotation_constant;
			quantize_fixed_rotation_stream(context.allocator, bone_stream.rotations, rotation_format, are_rotations_normalized, bone_stream.rotations);
		}

		inline void quantize_variable_rotation_stream(QuantizationContext& context, const RotationTrackStream& raw_clip_stream, const RotationTrackStream& raw_segment_stream, const TrackStreamRange& clip_range, uint8_t bit_rate, bool are_rotations_normalized, RotationTrackStream& out_quantized_stream)
		{
			// We expect all our samples to have the same width of sizeof(Vector4_32)
			ACL_ENSURE(raw_segment_stream.get_sample_size() == sizeof(Vector4_32), "Unexpected rotation sample size. %u != %u", raw_segment_stream.get_sample_size(), sizeof(Vector4_32));

			const uint32_t num_samples = is_constant_bit_rate(bit_rate) ? 1 : raw_segment_stream.get_num_samples();
			const uint32_t sample_size = sizeof(uint64_t) * 2;
			const uint32_t sample_rate = raw_segment_stream.get_sample_rate();
			RotationTrackStream quantized_stream(context.allocator, num_samples, sample_size, sample_rate, RotationFormat8::QuatDropW_Variable, bit_rate);

			if (is_constant_bit_rate(bit_rate))
			{
				ACL_ENSURE(are_rotations_normalized, "Cannot drop a constant track if it isn't normalized");

				Vector4_32 rotation = raw_clip_stream.get_raw_sample<Vector4_32>(context.segment_sample_start_index);
				rotation = convert_rotation(rotation, RotationFormat8::Quat_128, RotationFormat8::QuatDropW_Variable);

				const Vector4_32 normalized_rotation = normalize_sample(rotation, clip_range);

				uint8_t* quantized_ptr = quantized_stream.get_raw_sample_ptr(0);
				pack_vector3_48(normalized_rotation, true, quantized_ptr);
			}
			else
			{
				const uint8_t num_bits_at_bit_rate = get_num_bits_at_bit_rate(bit_rate);

				for (uint32_t sample_index = 0; sample_index < num_samples; ++sample_index)
				{
					uint8_t* quantized_ptr = quantized_stream.get_raw_sample_ptr(sample_index);

					if (is_raw_bit_rate(bit_rate))
					{
						Vector4_32 rotation = raw_clip_stream.get_raw_sample<Vector4_32>(context.segment_sample_start_index + sample_index);
						rotation = convert_rotation(rotation, RotationFormat8::Quat_128, RotationFormat8::QuatDropW_Variable);
						pack_vector3_96(rotation, quantized_ptr);
					}
					else
					{
						const Quat_32 rotation = raw_segment_stream.get_raw_sample<Quat_32>(sample_index);
						if (is_pack_72_bit_rate(bit_rate))
							pack_vector3_72(quat_to_vector(rotation), are_rotations_normalized, quantized_ptr);
						else
							pack_vector3_n(quat_to_vector(rotation), num_bits_at_bit_rate, num_bits_at_bit_rate, num_bits_at_bit_rate, are_rotations_normalized, quantized_ptr);
					}
				}
			}

			out_quantized_stream = std::move(quantized_stream);
		}

		inline void quantize_variable_rotation_stream(QuantizationContext& context, uint16_t bone_index, uint8_t bit_rate)
		{
			ACL_ENSURE(bone_index < context.num_bones, "Invalid bone index: %u", bone_index);

			BoneStreams& bone_stream = context.bone_streams[bone_index];

			// Default tracks aren't quantized
			if (bone_stream.is_rotation_default)
				return;

			const BoneStreams& raw_bone_stream = context.raw_bone_streams[bone_index];
			const RotationFormat8 highest_bit_rate = get_highest_variant_precision(RotationVariant8::QuatDropW);
			const TrackStreamRange invalid_range;
			const TrackStreamRange& bone_range = context.clip.are_rotations_normalized ? context.clip.ranges[bone_index].rotation : invalid_range;
			const bool are_rotations_normalized = context.clip.are_rotations_normalized && !bone_stream.is_rotation_constant;

			// If our format is variable, we keep them fixed at the highest bit rate in the variant
			if (bone_stream.is_rotation_constant)
				quantize_fixed_rotation_stream(context.allocator, bone_stream.rotations, highest_bit_rate, are_rotations_normalized, bone_stream.rotations);
			else
				quantize_variable_rotation_stream(context, raw_bone_stream.rotations, bone_stream.rotations, bone_range, bit_rate, are_rotations_normalized, bone_stream.rotations);
		}

		inline void quantize_fixed_translation_stream(Allocator& allocator, const TranslationTrackStream& raw_stream, VectorFormat8 translation_format, TranslationTrackStream& out_quantized_stream)
		{
			// We expect all our samples to have the same width of sizeof(Vector4_32)
			ACL_ENSURE(raw_stream.get_sample_size() == sizeof(Vector4_32), "Unexpected translation sample size. %u != %u", raw_stream.get_sample_size(), sizeof(Vector4_32));
			ACL_ENSURE(raw_stream.get_vector_format() == VectorFormat8::Vector3_96, "Expected a Vector3_96 vector format, found: %s", get_vector_format_name(raw_stream.get_vector_format()));

			const uint32_t num_samples = raw_stream.get_num_samples();
			const uint32_t sample_size = get_packed_vector_size(translation_format);
			const uint32_t sample_rate = raw_stream.get_sample_rate();
			TranslationTrackStream quantized_stream(allocator, num_samples, sample_size, sample_rate, translation_format);

			for (uint32_t sample_index = 0; sample_index < num_samples; ++sample_index)
			{
				const Vector4_32 translation = raw_stream.get_raw_sample<Vector4_32>(sample_index);
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

		inline void quantize_fixed_translation_stream(QuantizationContext& context, uint16_t bone_index, VectorFormat8 translation_format)
		{
			ACL_ENSURE(bone_index < context.num_bones, "Invalid bone index: %u", bone_index);

			BoneStreams& bone_stream = context.bone_streams[bone_index];

			// Default tracks aren't quantized
			if (bone_stream.is_translation_default)
				return;

			// Constant translation tracks store the remaining sample with full precision
			const VectorFormat8 format = bone_stream.is_translation_constant ? VectorFormat8::Vector3_96 : translation_format;

			quantize_fixed_translation_stream(context.allocator, bone_stream.translations, format, bone_stream.translations);
		}

		inline void quantize_variable_translation_stream(QuantizationContext& context, const TranslationTrackStream& raw_clip_stream, const TranslationTrackStream& raw_segment_stream, const TrackStreamRange& clip_range, uint8_t bit_rate, TranslationTrackStream& out_quantized_stream)
		{
			// We expect all our samples to have the same width of sizeof(Vector4_32)
			ACL_ENSURE(raw_segment_stream.get_sample_size() == sizeof(Vector4_32), "Unexpected translation sample size. %u != %u", raw_segment_stream.get_sample_size(), sizeof(Vector4_32));
			ACL_ENSURE(raw_segment_stream.get_vector_format() == VectorFormat8::Vector3_96, "Expected a Vector3_96 vector format, found: %s", get_vector_format_name(raw_segment_stream.get_vector_format()));

			const uint32_t num_samples = is_constant_bit_rate(bit_rate) ? 1 : raw_segment_stream.get_num_samples();
			const uint32_t sample_size = sizeof(uint64_t) * 2;
			const uint32_t sample_rate = raw_segment_stream.get_sample_rate();
			TranslationTrackStream quantized_stream(context.allocator, num_samples, sample_size, sample_rate, VectorFormat8::Vector3_Variable, bit_rate);

			if (is_constant_bit_rate(bit_rate))
			{
				const Vector4_32 translation = raw_clip_stream.get_raw_sample<Vector4_32>(context.segment_sample_start_index);
				const Vector4_32 normalized_translation = normalize_sample(translation, clip_range);

				uint8_t* quantized_ptr = quantized_stream.get_raw_sample_ptr(0);
				pack_vector3_48(normalized_translation, true, quantized_ptr);
			}
			else
			{
				const uint8_t num_bits_at_bit_rate = get_num_bits_at_bit_rate(bit_rate);

				for (uint32_t sample_index = 0; sample_index < num_samples; ++sample_index)
				{
					uint8_t* quantized_ptr = quantized_stream.get_raw_sample_ptr(sample_index);

					if (is_raw_bit_rate(bit_rate))
					{
						const Vector4_32 translation = raw_clip_stream.get_raw_sample<Vector4_32>(context.segment_sample_start_index + sample_index);
						pack_vector3_96(translation, quantized_ptr);
					}
					else
					{
						const Vector4_32 translation = raw_segment_stream.get_raw_sample<Vector4_32>(sample_index);
						if (is_pack_72_bit_rate(bit_rate))
							pack_vector3_72(translation, true, quantized_ptr);
						else
							pack_vector3_n(translation, num_bits_at_bit_rate, num_bits_at_bit_rate, num_bits_at_bit_rate, true, quantized_ptr);
					}
				}
			}

			out_quantized_stream = std::move(quantized_stream);
		}

		inline void quantize_variable_translation_stream(QuantizationContext& context, uint16_t bone_index, uint8_t bit_rate)
		{
			ACL_ENSURE(bone_index < context.num_bones, "Invalid bone index: %u", bone_index);

			BoneStreams& bone_stream = context.bone_streams[bone_index];

			// Default tracks aren't quantized
			if (bone_stream.is_translation_default)
				return;

			const TrackStreamRange invalid_range;
			const TrackStreamRange& bone_range = context.clip.are_translations_normalized ? context.clip.ranges[bone_index].translation : invalid_range;
			const BoneStreams& raw_bone_stream = context.raw_bone_streams[bone_index];

			// Constant translation tracks store the remaining sample with full precision
			if (bone_stream.is_translation_constant)
				quantize_fixed_translation_stream(context.allocator, bone_stream.translations, VectorFormat8::Vector3_96, bone_stream.translations);
			else
				quantize_variable_translation_stream(context, raw_bone_stream.translations, bone_stream.translations, bone_range, bit_rate, bone_stream.translations);
		}

		inline void quantize_fixed_scale_stream(Allocator& allocator, const ScaleTrackStream& raw_stream, VectorFormat8 scale_format, ScaleTrackStream& out_quantized_stream)
		{
			// We expect all our samples to have the same width of sizeof(Vector4_32)
			ACL_ENSURE(raw_stream.get_sample_size() == sizeof(Vector4_32), "Unexpected scale sample size. %u != %u", raw_stream.get_sample_size(), sizeof(Vector4_32));
			ACL_ENSURE(raw_stream.get_vector_format() == VectorFormat8::Vector3_96, "Expected a Vector3_96 vector format, found: %s", get_vector_format_name(raw_stream.get_vector_format()));

			const uint32_t num_samples = raw_stream.get_num_samples();
			const uint32_t sample_size = get_packed_vector_size(scale_format);
			const uint32_t sample_rate = raw_stream.get_sample_rate();
			ScaleTrackStream quantized_stream(allocator, num_samples, sample_size, sample_rate, scale_format);

			for (uint32_t sample_index = 0; sample_index < num_samples; ++sample_index)
			{
				const Vector4_32 scale = raw_stream.get_raw_sample<Vector4_32>(sample_index);
				uint8_t* quantized_ptr = quantized_stream.get_raw_sample_ptr(sample_index);

				switch (scale_format)
				{
				case VectorFormat8::Vector3_96:
					pack_vector3_96(scale, quantized_ptr);
					break;
				case VectorFormat8::Vector3_48:
					pack_vector3_48(scale, true, quantized_ptr);
					break;
				case VectorFormat8::Vector3_32:
					pack_vector3_32(scale, 11, 11, 10, true, quantized_ptr);
					break;
				case VectorFormat8::Vector3_Variable:
				default:
					ACL_ENSURE(false, "Invalid or unsupported vector format: %s", get_vector_format_name(scale_format));
					break;
				}
			}

			out_quantized_stream = std::move(quantized_stream);
		}

		inline void quantize_fixed_scale_stream(QuantizationContext& context, uint16_t bone_index, VectorFormat8 scale_format)
		{
			ACL_ENSURE(bone_index < context.num_bones, "Invalid bone index: %u", bone_index);

			BoneStreams& bone_stream = context.bone_streams[bone_index];

			// Default tracks aren't quantized
			if (bone_stream.is_scale_default)
				return;

			// Constant scale tracks store the remaining sample with full precision
			const VectorFormat8 format = bone_stream.is_scale_constant ? VectorFormat8::Vector3_96 : scale_format;

			quantize_fixed_scale_stream(context.allocator, bone_stream.scales, format, bone_stream.scales);
		}

		inline void quantize_variable_scale_stream(QuantizationContext& context, const ScaleTrackStream& raw_clip_stream, const ScaleTrackStream& raw_segment_stream, const TrackStreamRange& clip_range, uint8_t bit_rate, ScaleTrackStream& out_quantized_stream)
		{
			// We expect all our samples to have the same width of sizeof(Vector4_32)
			ACL_ENSURE(raw_segment_stream.get_sample_size() == sizeof(Vector4_32), "Unexpected scale sample size. %u != %u", raw_segment_stream.get_sample_size(), sizeof(Vector4_32));
			ACL_ENSURE(raw_segment_stream.get_vector_format() == VectorFormat8::Vector3_96, "Expected a Vector3_96 vector format, found: %s", get_vector_format_name(raw_segment_stream.get_vector_format()));

			const uint32_t num_samples = is_constant_bit_rate(bit_rate) ? 1 : raw_segment_stream.get_num_samples();
			const uint32_t sample_size = sizeof(uint64_t) * 2;
			const uint32_t sample_rate = raw_segment_stream.get_sample_rate();
			ScaleTrackStream quantized_stream(context.allocator, num_samples, sample_size, sample_rate, VectorFormat8::Vector3_Variable, bit_rate);

			if (is_constant_bit_rate(bit_rate))
			{
				const Vector4_32 scale = raw_clip_stream.get_raw_sample<Vector4_32>(context.segment_sample_start_index);
				const Vector4_32 normalized_scale = normalize_sample(scale, clip_range);

				uint8_t* quantized_ptr = quantized_stream.get_raw_sample_ptr(0);
				pack_vector3_48(normalized_scale, true, quantized_ptr);
			}
			else
			{
				const uint8_t num_bits_at_bit_rate = get_num_bits_at_bit_rate(bit_rate);

				for (uint32_t sample_index = 0; sample_index < num_samples; ++sample_index)
				{
					uint8_t* quantized_ptr = quantized_stream.get_raw_sample_ptr(sample_index);

					if (is_raw_bit_rate(bit_rate))
					{
						const Vector4_32 scale = raw_clip_stream.get_raw_sample<Vector4_32>(context.segment_sample_start_index + sample_index);
						pack_vector3_96(scale, quantized_ptr);
					}
					else
					{
						const Vector4_32 scale = raw_segment_stream.get_raw_sample<Vector4_32>(sample_index);
						if (is_pack_72_bit_rate(bit_rate))
							pack_vector3_72(scale, true, quantized_ptr);
						else
							pack_vector3_n(scale, num_bits_at_bit_rate, num_bits_at_bit_rate, num_bits_at_bit_rate, true, quantized_ptr);
					}
				}
			}

			out_quantized_stream = std::move(quantized_stream);
		}

		inline void quantize_variable_scale_stream(QuantizationContext& context, uint16_t bone_index, uint8_t bit_rate)
		{
			ACL_ENSURE(bone_index < context.num_bones, "Invalid bone index: %u", bone_index);

			BoneStreams& bone_stream = context.bone_streams[bone_index];

			// Default tracks aren't quantized
			if (bone_stream.is_scale_default)
				return;

			const TrackStreamRange invalid_range;
			const TrackStreamRange& bone_range = context.clip.are_scales_normalized ? context.clip.ranges[bone_index].scale : invalid_range;
			const BoneStreams& raw_bone_stream = context.raw_bone_streams[bone_index];

			// Constant scale tracks store the remaining sample with full precision
			if (bone_stream.is_scale_constant)
				quantize_fixed_scale_stream(context.allocator, bone_stream.scales, VectorFormat8::Vector3_96, bone_stream.scales);
			else
				quantize_variable_scale_stream(context, raw_bone_stream.scales, bone_stream.scales, bone_range, bit_rate, bone_stream.scales);
		}

		inline float calculate_max_error_at_bit_rate(QuantizationContext& context, uint16_t target_bone_index, bool use_local_error, bool scan_whole_clip = false)
		{
			float max_error = 0.0f;

			for (uint32_t sample_index = 0; sample_index < context.num_samples; ++sample_index)
			{
				// Sample our streams and calculate the error
				const float sample_time = min(float(sample_index) / context.sample_rate, context.segment_duration);
				const float ref_sample_time = min(float(context.segment_sample_start_index + sample_index) / context.sample_rate, context.clip_duration);

				sample_streams_hierarchical(context.raw_bone_streams, context.num_bones, ref_sample_time, target_bone_index, context.raw_local_pose);
				sample_streams_hierarchical(context.bone_streams, context.raw_bone_streams, context.num_bones, sample_time, target_bone_index, context.bit_rate_per_bone, context.rotation_format, context.translation_format, context.scale_format, context.lossy_local_pose);

				// Constant branch
				float error;
				if (use_local_error)
				{
					if (context.has_scale)
						error = context.error_metric.calculate_local_bone_error(context.skeleton, context.raw_local_pose, context.lossy_local_pose, target_bone_index);
					else
						error = context.error_metric.calculate_local_bone_error_no_scale(context.skeleton, context.raw_local_pose, context.lossy_local_pose, target_bone_index);
				}
				else
				{
					if (context.has_scale)
						error = context.error_metric.calculate_object_bone_error(context.skeleton, context.raw_local_pose, context.lossy_local_pose, target_bone_index);
					else
						error = context.error_metric.calculate_object_bone_error_no_scale(context.skeleton, context.raw_local_pose, context.lossy_local_pose, target_bone_index);
				}

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

				if (bone_bit_rates.rotation == k_invalid_bit_rate && bone_bit_rates.translation == k_invalid_bit_rate && bone_bit_rates.scale == k_invalid_bit_rate)
				{
#if ACL_DEBUG_VARIABLE_QUANTIZATION
					printf("%u: Best bit rates: %u | %u | %u\n", bone_index, bone_bit_rates.rotation, bone_bit_rates.translation, bone_bit_rates.scale);
#endif
					continue;
				}

				BoneBitRate best_bit_rates = BoneBitRate{ std::max<uint8_t>(bone_bit_rates.rotation, k_highest_bit_rate), std::max<uint8_t>(bone_bit_rates.translation, k_highest_bit_rate), std::max<uint8_t>(bone_bit_rates.scale, k_highest_bit_rate) };
				uint8_t best_size = 0xFF;
				float best_error = context.error_threshold;

				uint8_t num_iterations = k_num_bit_rates - 1;
				for (uint8_t iteration = 1; iteration <= num_iterations; ++iteration)
				{
					uint8_t target_sum = 3 * iteration;

					for (uint8_t rotation_bit_rate = bone_bit_rates.rotation; true; ++rotation_bit_rate)
					{
						for (uint8_t translation_bit_rate = bone_bit_rates.translation; true; ++translation_bit_rate)
						{
							for (uint8_t scale_bit_rate = bone_bit_rates.scale; true; ++scale_bit_rate)
							{
								uint8_t rotation_increment = rotation_bit_rate - bone_bit_rates.rotation;
								uint8_t translation_increment = translation_bit_rate - bone_bit_rates.translation;
								uint8_t scale_increment = scale_bit_rate - bone_bit_rates.scale;
								uint8_t current_sum = rotation_increment * 3 + translation_increment * 3 + scale_increment * 3;
								if (current_sum != target_sum)
								{
									if (scale_bit_rate >= k_highest_bit_rate)
										break;
									else
										continue;
								}

								context.bit_rate_per_bone[bone_index] = BoneBitRate{ rotation_bit_rate, translation_bit_rate, scale_bit_rate };
								float error = calculate_max_error_at_bit_rate(context, bone_index, true);

#if ACL_DEBUG_VARIABLE_QUANTIZATION > 1
								printf("%u: %u | %u | %u (%u) = %f\n", bone_index, rotation_bit_rate, translation_bit_rate, scale_bit_rate, target_sum, error);
#endif

								if (error < best_error && target_sum <= best_size)
								{
									best_size = target_sum;
									best_error = error;
									best_bit_rates = context.bit_rate_per_bone[bone_index];
								}

								context.bit_rate_per_bone[bone_index] = bone_bit_rates;

								if (scale_bit_rate >= k_highest_bit_rate)
									break;
							}

							if (translation_bit_rate >= k_highest_bit_rate)
								break;
						}

						if (rotation_bit_rate >= k_highest_bit_rate)
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

						for (uint8_t rotation_bit_rate = bone_bit_rates.rotation; true; ++rotation_bit_rate)
						{
							for (uint8_t translation_bit_rate = bone_bit_rates.translation; true; ++translation_bit_rate)
							{
								for (uint8_t scale_bit_rate = bone_bit_rates.scale; true; ++scale_bit_rate)
								{
									uint8_t rotation_increment = rotation_bit_rate - bone_bit_rates.rotation;
									uint8_t translation_increment = translation_bit_rate - bone_bit_rates.translation;
									uint8_t scale_increment = scale_bit_rate - bone_bit_rates.scale;
									uint8_t current_sum = rotation_increment * 3 + translation_increment * 3 + scale_increment * 3;
									if (current_sum != target_sum)
									{
										if (scale_bit_rate >= k_highest_bit_rate)
											break;
										else
											continue;
									}

									context.bit_rate_per_bone[bone_index] = BoneBitRate{ rotation_bit_rate, translation_bit_rate, scale_bit_rate };
									float error = calculate_max_error_at_bit_rate(context, bone_index, true);

#if ACL_DEBUG_VARIABLE_QUANTIZATION > 1
									printf("%u: %u | %u | %u (%u) = %f\n", bone_index, rotation_bit_rate, translation_bit_rate, scale_bit_rate, target_sum, error);
#endif

									if (error < best_error && target_sum <= best_size)
									{
										best_size = target_sum;
										best_error = error;
										best_bit_rates = context.bit_rate_per_bone[bone_index];
									}

									context.bit_rate_per_bone[bone_index] = bone_bit_rates;

									if (scale_bit_rate >= k_highest_bit_rate)
										break;
								}

								if (translation_bit_rate >= k_highest_bit_rate)
									break;
							}

							if (rotation_bit_rate >= k_highest_bit_rate)
								break;
						}

						if (best_size != 0xFF)
							break;
					}
				}

#if ACL_DEBUG_VARIABLE_QUANTIZATION
				printf("%u: Best bit rates: %u | %u | %u (%u) = %f\n", bone_index, best_bit_rates.rotation, best_bit_rates.translation, best_bit_rates.scale, best_size, best_error);
#endif
				context.bit_rate_per_bone[bone_index] = best_bit_rates;
			}
		}

		constexpr uint8_t increment_and_clamp_bit_rate(uint8_t bit_rate, uint8_t increment)
		{
			return bit_rate >= k_highest_bit_rate ? bit_rate : std::min<uint8_t>(bit_rate + increment, k_highest_bit_rate);
		}

		inline float increase_bone_bit_rate(QuantizationContext& context, uint16_t bone_index, uint8_t num_increments, float old_error, BoneBitRate& out_best_bit_rates)
		{
			const BoneBitRate bone_bit_rates = context.bit_rate_per_bone[bone_index];
			const uint8_t num_scale_increments = context.has_scale ? num_increments : 0;

			BoneBitRate best_bit_rates = bone_bit_rates;
			float best_error = old_error;

			for (uint8_t rotation_increment = 0; rotation_increment <= num_increments; ++rotation_increment)
			{
				uint8_t rotation_bit_rate = increment_and_clamp_bit_rate(bone_bit_rates.rotation, rotation_increment);

				for (uint8_t translation_increment = 0; translation_increment <= num_increments; ++translation_increment)
				{
					uint8_t translation_bit_rate = increment_and_clamp_bit_rate(bone_bit_rates.translation, translation_increment);

					for (uint8_t scale_increment = 0; scale_increment <= num_scale_increments; ++scale_increment)
					{
						uint8_t scale_bit_rate = increment_and_clamp_bit_rate(bone_bit_rates.scale, scale_increment);

						if (rotation_increment + translation_increment + scale_increment != num_increments)
						{
							if (scale_bit_rate >= k_highest_bit_rate)
								break;
							else
								continue;
						}

						context.bit_rate_per_bone[bone_index] = BoneBitRate{ rotation_bit_rate, translation_bit_rate, scale_bit_rate };
						float error = calculate_max_error_at_bit_rate(context, bone_index, false);

						if (error < best_error)
						{
							best_error = error;
							best_bit_rates = context.bit_rate_per_bone[bone_index];
						}

						context.bit_rate_per_bone[bone_index] = bone_bit_rates;

						if (scale_bit_rate >= k_highest_bit_rate)
							break;
					}

					if (translation_bit_rate >= k_highest_bit_rate)
						break;
				}

				if (rotation_bit_rate >= k_highest_bit_rate)
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
						uint16_t chain_bone_index = chain_bone_indices[chain_link_index];
						BoneBitRate chain_bone_best_bit_rates;
						increase_bone_bit_rate(context, chain_bone_index, bone_chain_permutation[chain_link_index], old_error, chain_bone_best_bit_rates);
						is_permutation_valid |= chain_bone_best_bit_rates.rotation != permutation_bit_rates[chain_bone_index].rotation;
						is_permutation_valid |= chain_bone_best_bit_rates.translation != permutation_bit_rates[chain_bone_index].translation;
						is_permutation_valid |= chain_bone_best_bit_rates.scale != permutation_bit_rates[chain_bone_index].scale;
						permutation_bit_rates[chain_bone_index] = chain_bone_best_bit_rates;
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

		inline uint16_t calculate_bone_chain_indices(const RigidSkeleton& skeleton, uint16_t bone_index, uint16_t* out_chain_bone_indices)
		{
			const BoneChain bone_chain = skeleton.get_bone_chain(bone_index);

			uint16_t num_bones_in_chain = 0;
			for (uint16_t chain_bone_index : bone_chain)
				out_chain_bone_indices[num_bones_in_chain++] = chain_bone_index;

			return num_bones_in_chain;
		}

		inline void initialize_bone_bit_rates(const SegmentContext& segment, RotationFormat8 rotation_format, VectorFormat8 translation_format, VectorFormat8 scale_format, BoneBitRate* out_bit_rate_per_bone)
		{
			const bool is_rotation_variable = is_rotation_format_variable(rotation_format);
			const bool is_translation_variable = is_vector_format_variable(translation_format);
			const bool is_scale_variable = is_vector_format_variable(scale_format);
			const bool has_scale = segment_context_has_scale(segment);

			for (uint16_t bone_index = 0; bone_index < segment.num_bones; ++bone_index)
			{
				BoneBitRate& bone_bit_rate = out_bit_rate_per_bone[bone_index];

				const bool rotation_supports_constant_tracks = segment.are_rotations_normalized;
				if (is_rotation_variable && segment.bone_streams[bone_index].is_rotation_animated())
					bone_bit_rate.rotation = rotation_supports_constant_tracks ? 0 : k_lowest_bit_rate;
				else
					bone_bit_rate.rotation = k_invalid_bit_rate;

				const bool translation_supports_constant_tracks = segment.are_translations_normalized;
				if (is_translation_variable && segment.bone_streams[bone_index].is_translation_animated())
					bone_bit_rate.translation = translation_supports_constant_tracks ? 0 : k_lowest_bit_rate;
				else
					bone_bit_rate.translation = k_invalid_bit_rate;

				const bool scale_supports_constant_tracks = segment.are_scales_normalized;
				if (has_scale && is_scale_variable && segment.bone_streams[bone_index].is_scale_animated())
					bone_bit_rate.scale = scale_supports_constant_tracks ? 0 : k_lowest_bit_rate;
				else
					bone_bit_rate.scale = k_invalid_bit_rate;
			}
		}

		inline void quantize_all_streams(QuantizationContext& context)
		{
			const bool is_rotation_variable = is_rotation_format_variable(context.rotation_format);
			const bool is_translation_variable = is_vector_format_variable(context.translation_format);
			const bool is_scale_variable = is_vector_format_variable(context.scale_format);

			for (uint16_t bone_index = 0; bone_index < context.num_bones; ++bone_index)
			{
				const BoneBitRate& bone_bit_rate = context.bit_rate_per_bone[bone_index];

				if (is_rotation_variable)
					quantize_variable_rotation_stream(context, bone_index, bone_bit_rate.rotation);
				else
					quantize_fixed_rotation_stream(context, bone_index, context.rotation_format);

				if (is_translation_variable)
					quantize_variable_translation_stream(context, bone_index, bone_bit_rate.translation);
				else
					quantize_fixed_translation_stream(context, bone_index, context.translation_format);

				if (context.has_scale)
				{
					if (is_scale_variable)
						quantize_variable_scale_stream(context, bone_index, bone_bit_rate.scale);
					else
						quantize_fixed_scale_stream(context, bone_index, context.scale_format);
				}
			}
		}

		inline void quantize_variable_streams(QuantizationContext& context)
		{
			initialize_bone_bit_rates(context.segment, context.rotation_format, context.translation_format, context.scale_format, context.bit_rate_per_bone);

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

			uint8_t* bone_chain_permutation = allocate_type_array<uint8_t>(context.allocator, context.num_bones);
			uint16_t* chain_bone_indices = allocate_type_array<uint16_t>(context.allocator, context.num_bones);
			BoneBitRate* permutation_bit_rates = allocate_type_array<BoneBitRate>(context.allocator, context.num_bones);
			BoneBitRate* best_permutation_bit_rates = allocate_type_array<BoneBitRate>(context.allocator, context.num_bones);
			BoneBitRate* best_bit_rates = allocate_type_array<BoneBitRate>(context.allocator, context.num_bones);
			memcpy(best_bit_rates, context.bit_rate_per_bone, sizeof(BoneBitRate) * context.num_bones);

			for (uint16_t bone_index = 0; bone_index < context.num_bones; ++bone_index)
			{
				float error = calculate_max_error_at_bit_rate(context, bone_index, false);
				if (error < context.error_threshold)
					continue;

				if (context.bit_rate_per_bone[bone_index].rotation >= k_highest_bit_rate && context.bit_rate_per_bone[bone_index].translation >= k_highest_bit_rate && context.bit_rate_per_bone[bone_index].scale >= k_highest_bit_rate)
				{
					// Our bone already has the highest precision possible locally, if the local error already exceeds our threshold,
					// there is nothing we can do, bail out
					const float local_error = calculate_max_error_at_bit_rate(context, bone_index, true);
					if (local_error >= context.error_threshold)
						continue;
				}

				const uint16_t num_bones_in_chain = calculate_bone_chain_indices(context.skeleton, bone_index, chain_bone_indices);

				const float initial_error = error;

				while (error >= context.error_threshold)
				{
					// Generate permutations for up to 3 bit rate increments
					// Perform an exhaustive search of the permutations and pick the best result
					// If our best error is under the threshold, we are done, otherwise we will try again from there
					const float original_error = error;
					float best_error = error;

					// The first permutation increases the bit rate of a single track/bone
					std::fill(bone_chain_permutation, bone_chain_permutation + context.num_bones, uint8_t(0));
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
					std::fill(bone_chain_permutation, bone_chain_permutation + context.num_bones, uint8_t(0));
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
						std::fill(bone_chain_permutation, bone_chain_permutation + context.num_bones, uint8_t(0));
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
					std::fill(bone_chain_permutation, bone_chain_permutation + context.num_bones, uint8_t(0));
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
						std::fill(bone_chain_permutation, bone_chain_permutation + context.num_bones, uint8_t(0));
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
							std::fill(bone_chain_permutation, bone_chain_permutation + context.num_bones, uint8_t(0));
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
							const BoneBitRate& bone_bit_rate = context.bit_rate_per_bone[i];
							const BoneBitRate& best_bone_bit_rate = best_bit_rates[i];
							bool rotation_differs = bone_bit_rate.rotation != best_bone_bit_rate.rotation;
							bool translation_differs = bone_bit_rate.translation != best_bone_bit_rate.translation;
							bool scale_differs = bone_bit_rate.scale != best_bone_bit_rate.scale;
							if (rotation_differs || translation_differs || scale_differs)
								printf("%u: %u | %u | %u => %u  %u %u (%f)\n", i, bone_bit_rate.rotation, bone_bit_rate.translation, bone_bit_rate.scale, best_bone_bit_rate.rotation, best_bone_bit_rate.translation, best_bone_bit_rate.scale, new_error);
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
						const BoneBitRate& bone_bit_rate = context.bit_rate_per_bone[i];
						const BoneBitRate& best_bone_bit_rate = best_bit_rates[i];
						bool rotation_differs = bone_bit_rate.rotation != best_bone_bit_rate.rotation;
						bool translation_differs = bone_bit_rate.translation != best_bone_bit_rate.translation;
						bool scale_differs = bone_bit_rate.scale != best_bone_bit_rate.scale;
						if (rotation_differs || translation_differs || scale_differs)
							printf("%u: %u | %u | %u => %u  %u %u (%f)\n", i, bone_bit_rate.rotation, bone_bit_rate.translation, bone_bit_rate.scale, best_bone_bit_rate.rotation, best_bone_bit_rate.translation, best_bone_bit_rate.scale, new_error);
					}
#endif

					memcpy(context.bit_rate_per_bone, best_bit_rates, sizeof(BoneBitRate) * context.num_bones);
				}

				// Last ditch effort if our error remains too high, this should be rare
				error = calculate_max_error_at_bit_rate(context, bone_index, false, true);
				while (error >= context.error_threshold)
				{
					// From child to parent, increase the bit rate indiscriminately
					uint16_t num_maxed_out = 0;
					for (int16_t chain_link_index = num_bones_in_chain - 1; chain_link_index >= 0; --chain_link_index)
					{
						const uint16_t chain_bone_index = chain_bone_indices[chain_link_index];

						// Work with a copy. We'll increase the bit rate as much as we can and retain the values
						// that yield the smallest error BUT increasing the bit rate does NOT always means
						// that the error will reduce and improve. It could get worse in which case we'll do nothing.

						BoneBitRate& bone_bit_rate = context.bit_rate_per_bone[chain_bone_index];

						// Copy original values
						BoneBitRate best_bone_bit_rate = bone_bit_rate;
						float best_bit_rate_error = error;

						while (error >= context.error_threshold)
						{
							static_assert(offsetof(BoneBitRate, rotation) == 0 && offsetof(BoneBitRate, scale) == sizeof(BoneBitRate) - 1, "Invalid BoneBitRate offsets");
							uint8_t& smallest_bit_rate = *std::min_element<uint8_t*>(&bone_bit_rate.rotation, &bone_bit_rate.scale + 1);

							if (smallest_bit_rate >= k_highest_bit_rate)
							{
								num_maxed_out++;
								break;
							}

							// If rotation == translation and translation has room, bias translation
							// This seems to yield an overall tiny win but it isn't always the case.
							// TODO: Brute force this?
							if (bone_bit_rate.rotation == bone_bit_rate.translation && bone_bit_rate.translation < k_highest_bit_rate && bone_bit_rate.scale >= k_highest_bit_rate)
								bone_bit_rate.translation++;
							else
								smallest_bit_rate++;

							ACL_ENSURE((bone_bit_rate.rotation <= k_highest_bit_rate || bone_bit_rate.rotation == k_invalid_bit_rate) && (bone_bit_rate.translation <= k_highest_bit_rate || bone_bit_rate.translation == k_invalid_bit_rate) && (bone_bit_rate.scale <= k_highest_bit_rate || bone_bit_rate.scale == k_invalid_bit_rate), "Invalid bit rate! [%u, %u, %u]", bone_bit_rate.rotation, bone_bit_rate.translation, bone_bit_rate.scale);

							error = calculate_max_error_at_bit_rate(context, bone_index, false, true);

							if (error < best_bit_rate_error)
							{
								best_bone_bit_rate = bone_bit_rate;
								best_bit_rate_error = error;

#if ACL_DEBUG_VARIABLE_QUANTIZATION
								printf("%u: => %u %u %u (%f)\n", chain_bone_index, bone_bit_rate.rotation, bone_bit_rate.translation, bone_bit_rate.scale, error);
								for (uint16_t i = chain_link_index + 1; i < num_bones_in_chain; ++i)
								{
									const uint16_t chain_bone_index2 = chain_bone_indices[chain_link_index];
									float error2 = calculate_max_error_at_bit_rate(context, chain_bone_index2, false, true);
									printf("  %u: => (%f)\n", i, error2);
								}
#endif
							}
						}

						// Only retain the lowest error bit rates
						bone_bit_rate = best_bone_bit_rate;
						error = best_bit_rate_error;

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
				const BoneBitRate& bone_bit_rate = context.bit_rate_per_bone[i];
				printf("%u: %u | %u | %u => %f %s\n", i, bone_bit_rate.rotation, bone_bit_rate.translation, bone_bit_rate.scale, error, error >= context.error_threshold ? "!" : "");
			}
#endif

			// Quantize our streams now that we found the optimal bit rates
			quantize_all_streams(context);

			deallocate_type_array(context.allocator, bone_chain_permutation, context.num_bones);
			deallocate_type_array(context.allocator, chain_bone_indices, context.num_bones);
			deallocate_type_array(context.allocator, permutation_bit_rates, context.num_bones);
			deallocate_type_array(context.allocator, best_permutation_bit_rates, context.num_bones);
			deallocate_type_array(context.allocator, best_bit_rates, context.num_bones);
		}
	}

	inline void quantize_streams(Allocator& allocator, ClipContext& clip_context, const CompressionSettings& settings, const RigidSkeleton& skeleton, const ClipContext& raw_clip_context)
	{
		const bool is_rotation_variable = is_rotation_format_variable(settings.rotation_format);
		const bool is_translation_variable = is_vector_format_variable(settings.translation_format);
		const bool is_scale_variable = is_vector_format_variable(settings.scale_format);
		const bool is_any_variable = is_rotation_variable || is_translation_variable || is_scale_variable;

		for (SegmentContext& segment : clip_context.segment_iterator())
		{
#if ACL_DEBUG_VARIABLE_QUANTIZATION
			printf("Quantizing segment %u...\n", segment.segment_index);
#endif

			// TODO: Reuse the context if we can and just update the current segment
			impl::QuantizationContext context(allocator, clip_context, raw_clip_context, segment, settings, skeleton);

			if (is_any_variable)
			{
				impl::quantize_variable_streams(context);
			}
			else
			{
				for (uint16_t bone_index = 0; bone_index < segment.num_bones; ++bone_index)
				{
					impl::quantize_fixed_rotation_stream(context, bone_index, settings.rotation_format);
					impl::quantize_fixed_translation_stream(context, bone_index, settings.translation_format);

					if (clip_context.has_scale)
						impl::quantize_fixed_scale_stream(context, bone_index, settings.scale_format);
				}
			}
		}
	}
}
