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

#include "acl/core/iallocator.h"
#include "acl/core/compiler_utils.h"
#include "acl/core/error.h"
#include "acl/core/utils.h"
#include "acl/math/quat_32.h"
#include "acl/math/quat_packing.h"
#include "acl/math/vector4_32.h"
#include "acl/math/vector4_packing.h"
#include "acl/compression/impl/track_bit_rate_database.h"
#include "acl/compression/impl/track_database.h"
#include "acl/compression/stream/clip_context.h"
#include "acl/compression/stream/sample_streams.h"
#include "acl/compression/stream/normalize_streams.h"
#include "acl/compression/stream/convert_rotation_streams.h"
#include "acl/compression/skeleton_error_metric.h"
#include "acl/compression/compression_settings.h"

#include <cstddef>
#include <cstdint>

// 0 = no debug info, 1 = basic info, 2 = verbose
#define ACL_IMPL_DEBUG_VARIABLE_QUANTIZATION		0

// 0 = disabled, 1 = enabled
#define ACL_IMPL_USE_DATABASE						1

ACL_IMPL_FILE_PRAGMA_PUSH

namespace acl
{
	namespace impl
	{
		struct QuantizationContext
		{
			IAllocator& allocator;
			ClipContext& clip;
			const ClipContext& raw_clip;
			const ClipContext& additive_base_clip;
			SegmentContext* segment;
			BoneStreams* bone_streams;
			uint16_t num_transforms;
			const RigidSkeleton& skeleton;
			const CompressionSettings& settings;

			acl_impl::track_bit_rate_database database;
			acl_impl::single_track_query local_query;
			acl_impl::hierarchical_track_query object_query;

			uint32_t num_samples;
			uint32_t segment_sample_start_index;
			float sample_rate;
			float clip_duration;
			bool has_scale;
			bool has_additive_base;

			const BoneStreams* raw_bone_streams;

			Transform_32* additive_local_pose;
			Transform_32* raw_local_pose;
			Transform_32* lossy_local_pose;

			BoneBitRate* bit_rate_per_bone;

			QuantizationContext(IAllocator& allocator_, ClipContext& clip_, const ClipContext& raw_clip_, const ClipContext& additive_base_clip_, const CompressionSettings& settings_, const RigidSkeleton& skeleton_)
				: allocator(allocator_)
				, clip(clip_)
				, raw_clip(raw_clip_)
				, additive_base_clip(additive_base_clip_)
				, segment(nullptr)
				, bone_streams(nullptr)
				, num_transforms(clip_.num_bones)
				, skeleton(skeleton_)
				, settings(settings_)
				, database(allocator_, settings_, clip_.segments->bone_streams, raw_clip_.segments->bone_streams, clip_.num_bones, clip_.segments->num_samples)
				, local_query()
				, object_query(allocator_)
				, num_samples(~0u)
				, segment_sample_start_index(~0u)
				, sample_rate(clip_.sample_rate)
				, clip_duration(clip_.duration)
				, has_scale(clip_.has_scale)
				, has_additive_base(clip_.has_additive_base)
				, raw_bone_streams(raw_clip_.segments[0].bone_streams)
			{
				local_query.bind(database);
				object_query.bind(database);

				additive_local_pose = clip_.has_additive_base ? allocate_type_array<Transform_32>(allocator, num_transforms) : nullptr;
				raw_local_pose = allocate_type_array<Transform_32>(allocator, num_transforms);
				lossy_local_pose = allocate_type_array<Transform_32>(allocator, num_transforms);
				bit_rate_per_bone = allocate_type_array<BoneBitRate>(allocator, num_transforms);
			}

			~QuantizationContext()
			{
				deallocate_type_array(allocator, additive_local_pose, num_transforms);
				deallocate_type_array(allocator, raw_local_pose, num_transforms);
				deallocate_type_array(allocator, lossy_local_pose, num_transforms);
				deallocate_type_array(allocator, bit_rate_per_bone, num_transforms);
			}

			void set_segment(SegmentContext& segment_)
			{
				segment = &segment_;
				bone_streams = segment_.bone_streams;
				num_samples = segment_.num_samples;
				segment_sample_start_index = segment_.clip_sample_offset;
				database.set_segment(segment_.bone_streams, segment_.num_bones, segment_.num_samples);
			}

			bool is_valid() const { return segment != nullptr; }
		};

		struct quantization_context
		{
			IAllocator& allocator;

			acl_impl::track_database& mutable_tracks_database;
			const acl_impl::track_database& raw_tracks_database;
			const acl_impl::track_database* additive_base_tracks_database;

			const acl_impl::segment_context& first_segment;
			acl_impl::segment_context* segment;

			const RigidSkeleton& skeleton;
			const CompressionSettings& settings;

			acl_impl::track_bit_rate_database database;
			acl_impl::single_track_query local_query;
			acl_impl::hierarchical_track_query object_query;

			uint32_t num_transforms;

			uint32_t num_samples;
			uint32_t segment_sample_start_index;
			float sample_rate;
			float clip_duration;
			float additive_clip_duration;
			bool has_scale;
			bool has_additive_base;

			Transform_32* additive_local_pose;
			Transform_32* raw_local_pose;
			Transform_32* lossy_local_pose;

			BoneBitRate* bit_rate_per_bone;

			quantization_context(IAllocator& allocator_, acl_impl::track_database& mutable_track_database_, const acl_impl::track_database& raw_track_database_, const acl_impl::track_database* additive_base_track_database_,
				const CompressionSettings& settings_, const RigidSkeleton& skeleton_, const acl_impl::segment_context& first_segment_)
				: allocator(allocator_)
				, mutable_tracks_database(mutable_track_database_)
				, raw_tracks_database(raw_track_database_)
				, additive_base_tracks_database(additive_base_track_database_)
				, first_segment(first_segment_)
				, segment(nullptr)
				, skeleton(skeleton_)
				, settings(settings_)
				, database(allocator_, settings_, mutable_track_database_, raw_track_database_)
				, local_query()
				, object_query(allocator_)
				, num_samples(~0u)
				, segment_sample_start_index(~0u)
				, sample_rate(raw_track_database_.get_sample_rate())
				, clip_duration(raw_track_database_.get_duration())
				, additive_clip_duration(additive_base_track_database_ != nullptr ? additive_base_track_database_->get_duration() : 0.0f)
				, has_scale(mutable_track_database_.has_scale())
				, has_additive_base(additive_base_track_database_ != nullptr)
			{
				local_query.bind(database);
				object_query.bind(database);

				const uint32_t num_transforms_ = mutable_track_database_.get_num_transforms();
				num_transforms = num_transforms_;

				additive_local_pose = additive_base_track_database_ != nullptr ? allocate_type_array<Transform_32>(allocator, num_transforms_) : nullptr;
				raw_local_pose = allocate_type_array<Transform_32>(allocator, num_transforms_);
				lossy_local_pose = allocate_type_array<Transform_32>(allocator, num_transforms_);
				bit_rate_per_bone = allocate_type_array<BoneBitRate>(allocator, num_transforms_);
			}

			~quantization_context()
			{
				deallocate_type_array(allocator, additive_local_pose, num_transforms);
				deallocate_type_array(allocator, raw_local_pose, num_transforms);
				deallocate_type_array(allocator, lossy_local_pose, num_transforms);
				deallocate_type_array(allocator, bit_rate_per_bone, num_transforms);
			}

			void set_segment(acl_impl::segment_context& segment_)
			{
				segment = &segment_;
				num_samples = segment_.num_samples_per_track;
				segment_sample_start_index = segment_.start_offset;
				database.set_segment(segment_);
			}

			bool is_valid() const { return segment != nullptr; }
		};

		inline void quantize_fixed_rotation_stream(IAllocator& allocator, const RotationTrackStream& raw_stream, RotationFormat8 rotation_format, bool are_rotations_normalized, RotationTrackStream& out_quantized_stream)
		{
			// We expect all our samples to have the same width of sizeof(Vector4_32)
			ACL_ASSERT(raw_stream.get_sample_size() == sizeof(Vector4_32), "Unexpected rotation sample size. %u != %u", raw_stream.get_sample_size(), sizeof(Vector4_32));

			const uint32_t num_samples = raw_stream.get_num_samples();
			const uint32_t rotation_sample_size = get_packed_rotation_size(rotation_format);
			const float sample_rate = raw_stream.get_sample_rate();
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
					if (are_rotations_normalized)
						pack_vector3_u48_unsafe(quat_to_vector(rotation), quantized_ptr);
					else
						pack_vector3_s48_unsafe(quat_to_vector(rotation), quantized_ptr);
					break;
				case RotationFormat8::QuatDropW_32:
					pack_vector3_32(quat_to_vector(rotation), 11, 11, 10, are_rotations_normalized, quantized_ptr);
					break;
				case RotationFormat8::QuatDropW_Variable:
				default:
					ACL_ASSERT(false, "Invalid or unsupported rotation format: %s", get_rotation_format_name(rotation_format));
					break;
				}
			}

			out_quantized_stream = std::move(quantized_stream);
		}

		inline void quantize_fixed_rotation_stream(QuantizationContext& context, uint16_t bone_index, RotationFormat8 rotation_format)
		{
			ACL_ASSERT(bone_index < context.num_transforms, "Invalid bone index: %u", bone_index);

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
			ACL_ASSERT(raw_segment_stream.get_sample_size() == sizeof(Vector4_32), "Unexpected rotation sample size. %u != %u", raw_segment_stream.get_sample_size(), sizeof(Vector4_32));

			const uint32_t num_samples = is_constant_bit_rate(bit_rate) ? 1 : raw_segment_stream.get_num_samples();
			const uint32_t sample_size = sizeof(uint64_t) * 2;
			const float sample_rate = raw_segment_stream.get_sample_rate();
			RotationTrackStream quantized_stream(context.allocator, num_samples, sample_size, sample_rate, RotationFormat8::QuatDropW_Variable, bit_rate);

			if (is_constant_bit_rate(bit_rate))
			{
				ACL_ASSERT(are_rotations_normalized, "Cannot drop a constant track if it isn't normalized");

				Vector4_32 rotation = raw_clip_stream.get_raw_sample<Vector4_32>(context.segment_sample_start_index);
				rotation = convert_rotation(rotation, RotationFormat8::Quat_128, RotationFormat8::QuatDropW_Variable);

				const Vector4_32 normalized_rotation = normalize_sample(rotation, clip_range);

				uint8_t* quantized_ptr = quantized_stream.get_raw_sample_ptr(0);
				pack_vector3_u48_unsafe(normalized_rotation, quantized_ptr);
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
						if (are_rotations_normalized)
							pack_vector3_uXX_unsafe(quat_to_vector(rotation), num_bits_at_bit_rate, quantized_ptr);
						else
							pack_vector3_sXX_unsafe(quat_to_vector(rotation), num_bits_at_bit_rate, quantized_ptr);
					}
				}
			}

			out_quantized_stream = std::move(quantized_stream);
		}

		inline void quantize_variable_rotation_stream(QuantizationContext& context, uint16_t bone_index, uint8_t bit_rate)
		{
			ACL_ASSERT(bone_index < context.num_transforms, "Invalid bone index: %u", bone_index);

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

		inline void quantize_fixed_translation_stream(IAllocator& allocator, const TranslationTrackStream& raw_stream, VectorFormat8 translation_format, TranslationTrackStream& out_quantized_stream)
		{
			// We expect all our samples to have the same width of sizeof(Vector4_32)
			ACL_ASSERT(raw_stream.get_sample_size() == sizeof(Vector4_32), "Unexpected translation sample size. %u != %u", raw_stream.get_sample_size(), sizeof(Vector4_32));
			ACL_ASSERT(raw_stream.get_vector_format() == VectorFormat8::Vector3_96, "Expected a Vector3_96 vector format, found: %s", get_vector_format_name(raw_stream.get_vector_format()));

			const uint32_t num_samples = raw_stream.get_num_samples();
			const uint32_t sample_size = get_packed_vector_size(translation_format);
			const float sample_rate = raw_stream.get_sample_rate();
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
					pack_vector3_u48_unsafe(translation, quantized_ptr);
					break;
				case VectorFormat8::Vector3_32:
					pack_vector3_32(translation, 11, 11, 10, true, quantized_ptr);
					break;
				case VectorFormat8::Vector3_Variable:
				default:
					ACL_ASSERT(false, "Invalid or unsupported vector format: %s", get_vector_format_name(translation_format));
					break;
				}
			}

			out_quantized_stream = std::move(quantized_stream);
		}

		inline void quantize_fixed_translation_stream(QuantizationContext& context, uint16_t bone_index, VectorFormat8 translation_format)
		{
			ACL_ASSERT(bone_index < context.num_transforms, "Invalid bone index: %u", bone_index);

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
			ACL_ASSERT(raw_segment_stream.get_sample_size() == sizeof(Vector4_32), "Unexpected translation sample size. %u != %u", raw_segment_stream.get_sample_size(), sizeof(Vector4_32));
			ACL_ASSERT(raw_segment_stream.get_vector_format() == VectorFormat8::Vector3_96, "Expected a Vector3_96 vector format, found: %s", get_vector_format_name(raw_segment_stream.get_vector_format()));

			const uint32_t num_samples = is_constant_bit_rate(bit_rate) ? 1 : raw_segment_stream.get_num_samples();
			const uint32_t sample_size = sizeof(uint64_t) * 2;
			const float sample_rate = raw_segment_stream.get_sample_rate();
			TranslationTrackStream quantized_stream(context.allocator, num_samples, sample_size, sample_rate, VectorFormat8::Vector3_Variable, bit_rate);

			if (is_constant_bit_rate(bit_rate))
			{
				const Vector4_32 translation = raw_clip_stream.get_raw_sample<Vector4_32>(context.segment_sample_start_index);
				const Vector4_32 normalized_translation = normalize_sample(translation, clip_range);

				uint8_t* quantized_ptr = quantized_stream.get_raw_sample_ptr(0);
				pack_vector3_u48_unsafe(normalized_translation, quantized_ptr);
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
						pack_vector3_uXX_unsafe(translation, num_bits_at_bit_rate, quantized_ptr);
					}
				}
			}

			out_quantized_stream = std::move(quantized_stream);
		}

		inline void quantize_variable_translation_stream(QuantizationContext& context, uint16_t bone_index, uint8_t bit_rate)
		{
			ACL_ASSERT(bone_index < context.num_transforms, "Invalid bone index: %u", bone_index);

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

		inline void quantize_fixed_scale_stream(IAllocator& allocator, const ScaleTrackStream& raw_stream, VectorFormat8 scale_format, ScaleTrackStream& out_quantized_stream)
		{
			// We expect all our samples to have the same width of sizeof(Vector4_32)
			ACL_ASSERT(raw_stream.get_sample_size() == sizeof(Vector4_32), "Unexpected scale sample size. %u != %u", raw_stream.get_sample_size(), sizeof(Vector4_32));
			ACL_ASSERT(raw_stream.get_vector_format() == VectorFormat8::Vector3_96, "Expected a Vector3_96 vector format, found: %s", get_vector_format_name(raw_stream.get_vector_format()));

			const uint32_t num_samples = raw_stream.get_num_samples();
			const uint32_t sample_size = get_packed_vector_size(scale_format);
			const float sample_rate = raw_stream.get_sample_rate();
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
					pack_vector3_u48_unsafe(scale, quantized_ptr);
					break;
				case VectorFormat8::Vector3_32:
					pack_vector3_32(scale, 11, 11, 10, true, quantized_ptr);
					break;
				case VectorFormat8::Vector3_Variable:
				default:
					ACL_ASSERT(false, "Invalid or unsupported vector format: %s", get_vector_format_name(scale_format));
					break;
				}
			}

			out_quantized_stream = std::move(quantized_stream);
		}

		inline void quantize_fixed_scale_stream(QuantizationContext& context, uint16_t bone_index, VectorFormat8 scale_format)
		{
			ACL_ASSERT(bone_index < context.num_transforms, "Invalid bone index: %u", bone_index);

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
			ACL_ASSERT(raw_segment_stream.get_sample_size() == sizeof(Vector4_32), "Unexpected scale sample size. %u != %u", raw_segment_stream.get_sample_size(), sizeof(Vector4_32));
			ACL_ASSERT(raw_segment_stream.get_vector_format() == VectorFormat8::Vector3_96, "Expected a Vector3_96 vector format, found: %s", get_vector_format_name(raw_segment_stream.get_vector_format()));

			const uint32_t num_samples = is_constant_bit_rate(bit_rate) ? 1 : raw_segment_stream.get_num_samples();
			const uint32_t sample_size = sizeof(uint64_t) * 2;
			const float sample_rate = raw_segment_stream.get_sample_rate();
			ScaleTrackStream quantized_stream(context.allocator, num_samples, sample_size, sample_rate, VectorFormat8::Vector3_Variable, bit_rate);

			if (is_constant_bit_rate(bit_rate))
			{
				const Vector4_32 scale = raw_clip_stream.get_raw_sample<Vector4_32>(context.segment_sample_start_index);
				const Vector4_32 normalized_scale = normalize_sample(scale, clip_range);

				uint8_t* quantized_ptr = quantized_stream.get_raw_sample_ptr(0);
				pack_vector3_u48_unsafe(normalized_scale, quantized_ptr);
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
						pack_vector3_uXX_unsafe(scale, num_bits_at_bit_rate, quantized_ptr);
					}
				}
			}

			out_quantized_stream = std::move(quantized_stream);
		}

		inline void quantize_variable_scale_stream(QuantizationContext& context, uint16_t bone_index, uint8_t bit_rate)
		{
			ACL_ASSERT(bone_index < context.num_transforms, "Invalid bone index: %u", bone_index);

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

		enum class error_scan_stop_condition { until_error_too_high, until_end_of_segment };

		inline float calculate_max_error_at_bit_rate_local(QuantizationContext& context, uint16_t target_bone_index, error_scan_stop_condition stop_condition)
		{
			const CompressionSettings& settings = context.settings;
			const ISkeletalErrorMetric& error_metric = *settings.error_metric;

			context.local_query.build(target_bone_index, context.bit_rate_per_bone[target_bone_index]);

			float max_error = 0.0f;

			for (uint32_t sample_index = 0; sample_index < context.num_samples; ++sample_index)
			{
				// Sample our streams and calculate the error
				// The sample time is calculated from the full clip duration to be consistent with decompression
				const float sample_time = min(float(context.segment_sample_start_index + sample_index) / context.sample_rate, context.clip_duration);

				sample_stream(context.raw_bone_streams, context.num_transforms, sample_time, target_bone_index, context.raw_local_pose);

#if ACL_IMPL_USE_DATABASE
				context.database.sample(context.local_query, sample_time, context.lossy_local_pose, context.num_transforms);
#else
				sample_stream(context.bone_streams, context.raw_bone_streams, context.num_bones, sample_time, target_bone_index, context.bit_rate_per_bone, settings.rotation_format, settings.translation_format, settings.scale_format, context.lossy_local_pose);
#endif

				if (context.has_additive_base)
				{
					const float normalized_sample_time = context.additive_base_clip.num_samples > 1 ? (sample_time / context.clip_duration) : 0.0f;
					const float additive_sample_time = normalized_sample_time * context.additive_base_clip.duration;
					sample_stream(context.additive_base_clip.segments[0].bone_streams, context.num_transforms, additive_sample_time, target_bone_index, context.additive_local_pose);
				}

				float error;
				if (context.has_scale)
					error = error_metric.calculate_local_bone_error(context.skeleton, context.raw_local_pose, context.additive_local_pose, context.lossy_local_pose, target_bone_index);
				else
					error = error_metric.calculate_local_bone_error_no_scale(context.skeleton, context.raw_local_pose, context.additive_local_pose, context.lossy_local_pose, target_bone_index);

				max_error = max(max_error, error);
				if (stop_condition == error_scan_stop_condition::until_error_too_high && error >= settings.error_threshold)
					break;
			}

			return max_error;
		}

		inline float calculate_max_error_at_bit_rate_local(quantization_context& context, uint32_t target_bone_index, error_scan_stop_condition stop_condition)
		{
			const CompressionSettings& settings = context.settings;
			const ISkeletalErrorMetric& error_metric = *settings.error_metric;
			const uint16_t target_bone_index_ = safe_static_cast<uint16_t>(target_bone_index);

			context.local_query.build(target_bone_index, context.bit_rate_per_bone[target_bone_index]);

			float max_error = 0.0f;

			for (uint32_t sample_index = 0; sample_index < context.num_samples; ++sample_index)
			{
				// Sample our streams and calculate the error
				// The sample time is calculated from the full clip duration to be consistent with decompression
				const float sample_time = min(float(context.segment_sample_start_index + sample_index) / context.sample_rate, context.clip_duration);

				acl_impl::sample_database(context.raw_tracks_database, *context.segment, sample_time, target_bone_index, context.raw_local_pose);

#if ACL_IMPL_USE_DATABASE
				context.database.sample(context.local_query, sample_time, context.lossy_local_pose, context.num_transforms);
#else
				sample_stream(context.bone_streams, context.raw_bone_streams, context.num_bones, sample_time, target_bone_index, context.bit_rate_per_bone, settings.rotation_format, settings.translation_format, settings.scale_format, context.lossy_local_pose);
#endif

				if (context.has_additive_base)
				{
					const float normalized_sample_time = context.additive_base_tracks_database->get_num_samples_per_track() > 1 ? (sample_time / context.clip_duration) : 0.0f;
					const float additive_sample_time = normalized_sample_time * context.additive_clip_duration;
					acl_impl::sample_database(*context.additive_base_tracks_database, *context.segment, additive_sample_time, target_bone_index, context.additive_local_pose);
				}

				float error;
				if (context.has_scale)
					error = error_metric.calculate_local_bone_error(context.skeleton, context.raw_local_pose, context.additive_local_pose, context.lossy_local_pose, target_bone_index_);
				else
					error = error_metric.calculate_local_bone_error_no_scale(context.skeleton, context.raw_local_pose, context.additive_local_pose, context.lossy_local_pose, target_bone_index_);

				max_error = max(max_error, error);
				if (stop_condition == error_scan_stop_condition::until_error_too_high && error >= settings.error_threshold)
					break;
			}

			return max_error;
		}

		inline float calculate_max_error_at_bit_rate_object(QuantizationContext& context, uint16_t target_bone_index, error_scan_stop_condition stop_condition)
		{
			const CompressionSettings& settings = context.settings;
			const ISkeletalErrorMetric& error_metric = *settings.error_metric;

			context.object_query.build(target_bone_index, context.bit_rate_per_bone, context.bone_streams);

			float max_error = 0.0f;

			for (uint32_t sample_index = 0; sample_index < context.num_samples; ++sample_index)
			{
				// Sample our streams and calculate the error
				// The sample time is calculated from the full clip duration to be consistent with decompression
				const float sample_time = min(float(context.segment_sample_start_index + sample_index) / context.sample_rate, context.clip_duration);

				sample_streams_hierarchical(context.raw_bone_streams, context.num_transforms, sample_time, target_bone_index, context.raw_local_pose);

#if ACL_IMPL_USE_DATABASE
				context.database.sample(context.object_query, sample_time, context.lossy_local_pose, context.num_transforms);
#else
				sample_streams_hierarchical(context.bone_streams, context.raw_bone_streams, context.num_transforms, sample_time, target_bone_index, context.bit_rate_per_bone, settings.rotation_format, settings.translation_format, settings.scale_format, context.lossy_local_pose);
#endif

				if (context.has_additive_base)
				{
					const float normalized_sample_time = context.additive_base_clip.num_samples > 1 ? (sample_time / context.clip_duration) : 0.0f;
					const float additive_sample_time = normalized_sample_time * context.additive_base_clip.duration;
					sample_streams_hierarchical(context.additive_base_clip.segments[0].bone_streams, context.num_transforms, additive_sample_time, target_bone_index, context.additive_local_pose);
				}

				float error;
				if (context.has_scale)
					error = error_metric.calculate_object_bone_error(context.skeleton, context.raw_local_pose, context.additive_local_pose, context.lossy_local_pose, target_bone_index);
				else
					error = error_metric.calculate_object_bone_error_no_scale(context.skeleton, context.raw_local_pose, context.additive_local_pose, context.lossy_local_pose, target_bone_index);

				max_error = max(max_error, error);
				if (stop_condition == error_scan_stop_condition::until_error_too_high && error >= settings.error_threshold)
					break;
			}

			return max_error;
		}

		inline float calculate_max_error_at_bit_rate_object(quantization_context& context, uint32_t target_bone_index, error_scan_stop_condition stop_condition)
		{
			const CompressionSettings& settings = context.settings;
			const ISkeletalErrorMetric& error_metric = *settings.error_metric;
			const uint16_t target_bone_index_ = safe_static_cast<uint16_t>(target_bone_index);

			context.object_query.build(target_bone_index, context.bit_rate_per_bone, context.mutable_tracks_database);

			float max_error = 0.0f;

			for (uint32_t sample_index = 0; sample_index < context.num_samples; ++sample_index)
			{
				// Sample our streams and calculate the error
				// The sample time is calculated from the full clip duration to be consistent with decompression
				const float sample_time = min(float(context.segment_sample_start_index + sample_index) / context.sample_rate, context.clip_duration);

				acl_impl::sample_database_hierarchical(context.raw_tracks_database, *context.segment, sample_time, target_bone_index, context.raw_local_pose);

#if ACL_IMPL_USE_DATABASE
				context.database.sample(context.object_query, sample_time, context.lossy_local_pose, context.num_transforms);
#else
				sample_streams_hierarchical(context.bone_streams, context.raw_bone_streams, context.num_transforms, sample_time, target_bone_index, context.bit_rate_per_bone, settings.rotation_format, settings.translation_format, settings.scale_format, context.lossy_local_pose);
#endif

				if (context.has_additive_base)
				{
					const float normalized_sample_time = context.additive_base_tracks_database->get_num_samples_per_track() > 1 ? (sample_time / context.clip_duration) : 0.0f;
					const float additive_sample_time = normalized_sample_time * context.additive_clip_duration;
					acl_impl::sample_database_hierarchical(*context.additive_base_tracks_database, *context.segment, additive_sample_time, target_bone_index, context.additive_local_pose);
				}

				float error;
				if (context.has_scale)
					error = error_metric.calculate_object_bone_error(context.skeleton, context.raw_local_pose, context.additive_local_pose, context.lossy_local_pose, target_bone_index_);
				else
					error = error_metric.calculate_object_bone_error_no_scale(context.skeleton, context.raw_local_pose, context.additive_local_pose, context.lossy_local_pose, target_bone_index_);

				max_error = max(max_error, error);
				if (stop_condition == error_scan_stop_condition::until_error_too_high && error >= settings.error_threshold)
					break;
			}

			return max_error;
		}

		template<typename context_type>
		inline void calculate_local_space_bit_rates(context_type& context)
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

			const CompressionSettings& settings = context.settings;

			for (decltype(context.num_transforms) bone_index = 0; bone_index < context.num_transforms; ++bone_index)
			{
				const BoneBitRate bone_bit_rates = context.bit_rate_per_bone[bone_index];

				if (bone_bit_rates.rotation == k_invalid_bit_rate && bone_bit_rates.translation == k_invalid_bit_rate && bone_bit_rates.scale == k_invalid_bit_rate)
				{
#if ACL_IMPL_DEBUG_VARIABLE_QUANTIZATION
					printf("%u: Best bit rates: %u | %u | %u\n", bone_index, bone_bit_rates.rotation, bone_bit_rates.translation, bone_bit_rates.scale);
#endif
					continue;
				}

				BoneBitRate best_bit_rates = BoneBitRate{ std::max<uint8_t>(bone_bit_rates.rotation, k_highest_bit_rate), std::max<uint8_t>(bone_bit_rates.translation, k_highest_bit_rate), std::max<uint8_t>(bone_bit_rates.scale, k_highest_bit_rate) };
				uint8_t best_size = 0xFF;
				float best_error = settings.error_threshold;

				const uint8_t num_iterations = k_num_bit_rates - 1;
				for (uint8_t iteration = 1; iteration <= num_iterations; ++iteration)
				{
					const uint8_t target_sum = 3 * iteration;

					for (uint8_t rotation_bit_rate = bone_bit_rates.rotation; true; ++rotation_bit_rate)
					{
						for (uint8_t translation_bit_rate = bone_bit_rates.translation; true; ++translation_bit_rate)
						{
							for (uint8_t scale_bit_rate = bone_bit_rates.scale; true; ++scale_bit_rate)
							{
								const uint8_t rotation_increment = rotation_bit_rate - bone_bit_rates.rotation;
								const uint8_t translation_increment = translation_bit_rate - bone_bit_rates.translation;
								const uint8_t scale_increment = scale_bit_rate - bone_bit_rates.scale;
								const uint8_t current_sum = rotation_increment * 3 + translation_increment * 3 + scale_increment * 3;
								if (current_sum != target_sum)
								{
									if (scale_bit_rate >= k_highest_bit_rate)
										break;
									else
										continue;
								}

								context.bit_rate_per_bone[bone_index] = BoneBitRate{ rotation_bit_rate, translation_bit_rate, scale_bit_rate };
								const float error = calculate_max_error_at_bit_rate_local(context, bone_index, error_scan_stop_condition::until_error_too_high);

#if ACL_IMPL_DEBUG_VARIABLE_QUANTIZATION > 1
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
						const uint8_t target_sum = 3 * iteration + (3 * num_iterations);

						for (uint8_t rotation_bit_rate = bone_bit_rates.rotation; true; ++rotation_bit_rate)
						{
							for (uint8_t translation_bit_rate = bone_bit_rates.translation; true; ++translation_bit_rate)
							{
								for (uint8_t scale_bit_rate = bone_bit_rates.scale; true; ++scale_bit_rate)
								{
									const uint8_t rotation_increment = rotation_bit_rate - bone_bit_rates.rotation;
									const uint8_t translation_increment = translation_bit_rate - bone_bit_rates.translation;
									const uint8_t scale_increment = scale_bit_rate - bone_bit_rates.scale;
									const uint8_t current_sum = rotation_increment * 3 + translation_increment * 3 + scale_increment * 3;
									if (current_sum != target_sum)
									{
										if (scale_bit_rate >= k_highest_bit_rate)
											break;
										else
											continue;
									}

									context.bit_rate_per_bone[bone_index] = BoneBitRate{ rotation_bit_rate, translation_bit_rate, scale_bit_rate };
									const float error = calculate_max_error_at_bit_rate_local(context, bone_index, error_scan_stop_condition::until_error_too_high);

#if ACL_IMPL_DEBUG_VARIABLE_QUANTIZATION > 1
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

#if ACL_IMPL_DEBUG_VARIABLE_QUANTIZATION
				printf("%u: Best bit rates: %u | %u | %u (%u) = %f\n", bone_index, best_bit_rates.rotation, best_bit_rates.translation, best_bit_rates.scale, best_size, best_error);
#endif
				context.bit_rate_per_bone[bone_index] = best_bit_rates;
			}
		}

		constexpr uint8_t increment_and_clamp_bit_rate(uint8_t bit_rate, uint8_t increment)
		{
			return bit_rate >= k_highest_bit_rate ? bit_rate : std::min<uint8_t>(bit_rate + increment, k_highest_bit_rate);
		}

		template<typename context_type>
		inline float increase_bone_bit_rate(context_type& context, uint16_t bone_index, uint8_t num_increments, float old_error, BoneBitRate& out_best_bit_rates)
		{
			const BoneBitRate bone_bit_rates = context.bit_rate_per_bone[bone_index];
			const uint8_t num_scale_increments = context.has_scale ? num_increments : 0;

			BoneBitRate best_bit_rates = bone_bit_rates;
			float best_error = old_error;

			for (uint8_t rotation_increment = 0; rotation_increment <= num_increments; ++rotation_increment)
			{
				const uint8_t rotation_bit_rate = increment_and_clamp_bit_rate(bone_bit_rates.rotation, rotation_increment);

				for (uint8_t translation_increment = 0; translation_increment <= num_increments; ++translation_increment)
				{
					const uint8_t translation_bit_rate = increment_and_clamp_bit_rate(bone_bit_rates.translation, translation_increment);

					for (uint8_t scale_increment = 0; scale_increment <= num_scale_increments; ++scale_increment)
					{
						const uint8_t scale_bit_rate = increment_and_clamp_bit_rate(bone_bit_rates.scale, scale_increment);

						if (rotation_increment + translation_increment + scale_increment != num_increments)
						{
							if (scale_bit_rate >= k_highest_bit_rate)
								break;
							else
								continue;
						}

						context.bit_rate_per_bone[bone_index] = BoneBitRate{ rotation_bit_rate, translation_bit_rate, scale_bit_rate };
						float error = calculate_max_error_at_bit_rate_object(context, bone_index, error_scan_stop_condition::until_error_too_high);

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

		template<typename context_type>
		inline float calculate_bone_permutation_error(context_type& context, BoneBitRate* permutation_bit_rates, uint8_t* bone_chain_permutation, const uint16_t* chain_bone_indices, uint16_t num_bones_in_chain, uint16_t bone_index, BoneBitRate* best_bit_rates, float old_error)
		{
			const CompressionSettings& settings = context.settings;

			float best_error = old_error;

			do
			{
				// Copy our current bit rates to the permutation rates
				std::memcpy(permutation_bit_rates, context.bit_rate_per_bone, sizeof(BoneBitRate) * context.num_transforms);

				bool is_permutation_valid = false;
				for (uint16_t chain_link_index = 0; chain_link_index < num_bones_in_chain; ++chain_link_index)
				{
					if (bone_chain_permutation[chain_link_index] != 0)
					{
						// Increase bit rate
						const uint16_t chain_bone_index = chain_bone_indices[chain_link_index];

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
				float permutation_error = calculate_max_error_at_bit_rate_object(context, bone_index, error_scan_stop_condition::until_error_too_high);
				std::swap(context.bit_rate_per_bone, permutation_bit_rates);

				if (permutation_error < best_error)
				{
					best_error = permutation_error;
					std::memcpy(best_bit_rates, permutation_bit_rates, sizeof(BoneBitRate) * context.num_transforms);

					if (permutation_error < settings.error_threshold)
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

		inline void initialize_bone_bit_rates(const QuantizationContext& context, BoneBitRate* out_bit_rate_per_bone)
		{
			const SegmentContext& segment = *context.segment;

			const bool is_rotation_variable = is_rotation_format_variable(context.settings.rotation_format);
			const bool is_translation_variable = is_vector_format_variable(context.settings.translation_format);
			const bool is_scale_variable = segment_context_has_scale(segment) && is_vector_format_variable(context.settings.scale_format);

			for (uint16_t bone_index = 0; bone_index < segment.num_bones; ++bone_index)
			{
				BoneBitRate& bone_bit_rate = out_bit_rate_per_bone[bone_index];

				const bool rotation_supports_constant_tracks = segment.are_rotations_normalized;
				if (is_rotation_variable && !segment.bone_streams[bone_index].is_rotation_constant)
					bone_bit_rate.rotation = rotation_supports_constant_tracks ? 0 : k_lowest_bit_rate;
				else
					bone_bit_rate.rotation = k_invalid_bit_rate;

				const bool translation_supports_constant_tracks = segment.are_translations_normalized;
				if (is_translation_variable && !segment.bone_streams[bone_index].is_translation_constant)
					bone_bit_rate.translation = translation_supports_constant_tracks ? 0 : k_lowest_bit_rate;
				else
					bone_bit_rate.translation = k_invalid_bit_rate;

				const bool scale_supports_constant_tracks = segment.are_scales_normalized;
				if (is_scale_variable && !segment.bone_streams[bone_index].is_scale_constant)
					bone_bit_rate.scale = scale_supports_constant_tracks ? 0 : k_lowest_bit_rate;
				else
					bone_bit_rate.scale = k_invalid_bit_rate;
			}
		}

		inline void initialize_bone_bit_rates(const quantization_context& context, BoneBitRate* out_bit_rate_per_bone)
		{
			const bool is_rotation_variable = is_rotation_format_variable(context.settings.rotation_format);
			const bool is_translation_variable = is_vector_format_variable(context.settings.translation_format);
			const bool is_scale_variable = context.has_scale && is_vector_format_variable(context.settings.scale_format);

			for (uint32_t transform_index = 0; transform_index < context.num_transforms; ++transform_index)
			{
				const acl_impl::qvvf_ranges& transform_range = context.segment->ranges[transform_index];
				BoneBitRate& bone_bit_rate = out_bit_rate_per_bone[transform_index];

				const bool rotation_supports_constant_tracks = transform_range.are_rotations_normalized;
				if (is_rotation_variable && !transform_range.is_rotation_constant)
					bone_bit_rate.rotation = rotation_supports_constant_tracks ? 0 : k_lowest_bit_rate;
				else
					bone_bit_rate.rotation = k_invalid_bit_rate;

				const bool translation_supports_constant_tracks = transform_range.are_translations_normalized;
				if (is_translation_variable && !transform_range.is_translation_constant)
					bone_bit_rate.translation = translation_supports_constant_tracks ? 0 : k_lowest_bit_rate;
				else
					bone_bit_rate.translation = k_invalid_bit_rate;

				const bool scale_supports_constant_tracks = transform_range.are_scales_normalized;
				if (is_scale_variable && !transform_range.is_scale_constant)
					bone_bit_rate.scale = scale_supports_constant_tracks ? 0 : k_lowest_bit_rate;
				else
					bone_bit_rate.scale = k_invalid_bit_rate;
			}
		}

		inline void quantize_all_streams(QuantizationContext& context)
		{
			ACL_ASSERT(context.is_valid(), "QuantizationContext isn't valid");

			const CompressionSettings& settings = context.settings;

			const bool is_rotation_variable = is_rotation_format_variable(settings.rotation_format);
			const bool is_translation_variable = is_vector_format_variable(settings.translation_format);
			const bool is_scale_variable = is_vector_format_variable(settings.scale_format);

			for (uint16_t bone_index = 0; bone_index < context.num_transforms; ++bone_index)
			{
				const BoneBitRate& bone_bit_rate = context.bit_rate_per_bone[bone_index];

				if (is_rotation_variable)
					quantize_variable_rotation_stream(context, bone_index, bone_bit_rate.rotation);
				else
					quantize_fixed_rotation_stream(context, bone_index, settings.rotation_format);

				if (is_translation_variable)
					quantize_variable_translation_stream(context, bone_index, bone_bit_rate.translation);
				else
					quantize_fixed_translation_stream(context, bone_index, settings.translation_format);

				if (context.has_scale)
				{
					if (is_scale_variable)
						quantize_variable_scale_stream(context, bone_index, bone_bit_rate.scale);
					else
						quantize_fixed_scale_stream(context, bone_index, settings.scale_format);
				}
			}
		}

		inline void ACL_SIMD_CALL set_vector4f_track(Vector4_32Arg0 value, uint32_t num_soa_entries, Vector4_32* inputs_x, Vector4_32* inputs_y, Vector4_32* inputs_z, Vector4_32* inputs_w)
		{
			const Vector4_32 xxxx = vector_mix_xxxx(value);
			const Vector4_32 yyyy = vector_mix_yyyy(value);
			const Vector4_32 zzzz = vector_mix_zzzz(value);
			const Vector4_32 wwww = vector_mix_wwww(value);

			// Process two entries at a time to allow the compiler to re-order things to hide instruction latency
			// TODO: Trivial AVX or ISPC conversion
			uint32_t entry_index;
			for (entry_index = 0; entry_index < (num_soa_entries & 0xFFFFFFFEU); ++entry_index)
			{
				inputs_x[entry_index] = xxxx;
				inputs_y[entry_index] = yyyy;
				inputs_z[entry_index] = zzzz;
				inputs_w[entry_index] = wwww;

				entry_index++;
				inputs_x[entry_index] = xxxx;
				inputs_y[entry_index] = yyyy;
				inputs_z[entry_index] = zzzz;
				inputs_w[entry_index] = wwww;
			}

			if (entry_index < num_soa_entries)
			{
				inputs_x[entry_index] = xxxx;
				inputs_y[entry_index] = yyyy;
				inputs_z[entry_index] = zzzz;
				inputs_w[entry_index] = wwww;
			}
		}

		inline void ACL_SIMD_CALL set_vector3f_track(Vector4_32Arg0 value, uint32_t num_soa_entries, Vector4_32* inputs_x, Vector4_32* inputs_y, Vector4_32* inputs_z)
		{
			const Vector4_32 xxxx = vector_mix_xxxx(value);
			const Vector4_32 yyyy = vector_mix_yyyy(value);
			const Vector4_32 zzzz = vector_mix_zzzz(value);

			// Process two entries at a time to allow the compiler to re-order things to hide instruction latency
			// TODO: Trivial AVX or ISPC conversion
			uint32_t entry_index;
			for (entry_index = 0; entry_index < (num_soa_entries & 0xFFFFFFFEU); ++entry_index)
			{
				inputs_x[entry_index] = xxxx;
				inputs_y[entry_index] = yyyy;
				inputs_z[entry_index] = zzzz;

				entry_index++;
				inputs_x[entry_index] = xxxx;
				inputs_y[entry_index] = yyyy;
				inputs_z[entry_index] = zzzz;
			}

			if (entry_index < num_soa_entries)
			{
				inputs_x[entry_index] = xxxx;
				inputs_y[entry_index] = yyyy;
				inputs_z[entry_index] = zzzz;
			}
		}

		inline void copy_vector4f_track(uint32_t num_soa_entries, const Vector4_32* inputs_x, const Vector4_32* inputs_y, const Vector4_32* inputs_z, const Vector4_32* inputs_w, Vector4_32* outputs_x, Vector4_32* outputs_y, Vector4_32* outputs_z, Vector4_32* outputs_w)
		{
			// Process two entries at a time to allow the compiler to re-order things to hide instruction latency
			// TODO: Trivial AVX or ISPC conversion
			uint32_t entry_index;
			for (entry_index = 0; entry_index < (num_soa_entries & 0xFFFFFFFEU); ++entry_index)
			{
				outputs_x[entry_index] = inputs_x[entry_index];
				outputs_y[entry_index] = inputs_y[entry_index];
				outputs_z[entry_index] = inputs_z[entry_index];
				outputs_w[entry_index] = inputs_w[entry_index];

				entry_index++;
				outputs_x[entry_index] = inputs_x[entry_index];
				outputs_y[entry_index] = inputs_y[entry_index];
				outputs_z[entry_index] = inputs_z[entry_index];
				outputs_w[entry_index] = inputs_w[entry_index];
			}

			if (entry_index < num_soa_entries)
			{
				outputs_x[entry_index] = inputs_x[entry_index];
				outputs_y[entry_index] = inputs_y[entry_index];
				outputs_z[entry_index] = inputs_z[entry_index];
				outputs_w[entry_index] = inputs_w[entry_index];
			}
		}

		inline void copy_vector3f_track(uint32_t num_soa_entries, const Vector4_32* inputs_x, const Vector4_32* inputs_y, const Vector4_32* inputs_z, Vector4_32* outputs_x, Vector4_32* outputs_y, Vector4_32* outputs_z)
		{
			// Process two entries at a time to allow the compiler to re-order things to hide instruction latency
			// TODO: Trivial AVX or ISPC conversion
			uint32_t entry_index;
			for (entry_index = 0; entry_index < (num_soa_entries & 0xFFFFFFFEU); ++entry_index)
			{
				outputs_x[entry_index] = inputs_x[entry_index];
				outputs_y[entry_index] = inputs_y[entry_index];
				outputs_z[entry_index] = inputs_z[entry_index];

				entry_index++;
				outputs_x[entry_index] = inputs_x[entry_index];
				outputs_y[entry_index] = inputs_y[entry_index];
				outputs_z[entry_index] = inputs_z[entry_index];
			}

			if (entry_index < num_soa_entries)
			{
				outputs_x[entry_index] = inputs_x[entry_index];
				outputs_y[entry_index] = inputs_y[entry_index];
				outputs_z[entry_index] = inputs_z[entry_index];
			}
		}

		inline void quantize_variable_rotation_track(quantization_context& context, uint32_t transform_index, const acl_impl::qvvf_ranges& transform_range)
		{
			using namespace acl_impl;
			ACL_ASSERT(get_rotation_variant(context.settings.rotation_format) == RotationVariant8::QuatDropW, "Unexpected variant");

			const uint8_t bit_rate = context.bit_rate_per_bone[transform_index].rotation;
			const uint32_t num_soa_entries = context.segment->num_soa_entries;

			Vector4_32* rotations_x;
			Vector4_32* rotations_y;
			Vector4_32* rotations_z;
			Vector4_32* rotations_w;
			context.mutable_tracks_database.get_rotations(*context.segment, transform_index, rotations_x, rotations_y, rotations_z, rotations_w);

			if (is_constant_bit_rate(bit_rate))
			{
				ACL_ASSERT(transform_range.are_rotations_normalized, "Cannot drop a constant track if it isn't normalized");

				// We can't use the values in the mutable track database because they have been normalized to the whole segment
				// and we need them normalized to the clip only.

				const Vector4_32* raw_rotations_x;
				const Vector4_32* raw_rotations_y;
				const Vector4_32* raw_rotations_z;
				const Vector4_32* raw_rotations_w;
				context.raw_tracks_database.get_rotations(*context.segment, transform_index, raw_rotations_x, raw_rotations_y, raw_rotations_z, raw_rotations_w);

				// Copy our raw original values
				copy_vector4f_track(num_soa_entries, raw_rotations_x, raw_rotations_y, raw_rotations_z, raw_rotations_w, rotations_x, rotations_y, rotations_z, rotations_w);

				// Convert our track
				// Drop W, we just ensure it is positive and write it back, the W component can be ignored and trivially reconstructed afterwards
				convert_drop_w_track(rotations_x, rotations_y, rotations_z, rotations_w, num_soa_entries);

				// Normalize to our clip range
				normalize_vector3f_track(rotations_x, rotations_y, rotations_z, num_soa_entries, transform_range.rotation_min, transform_range.rotation_extent);

				// Quantize and pack our values into place on 16 bits per component
				const StaticQuantizationScales<16> scales;

				// Process two entries at a time to allow the compiler to re-order things to hide instruction latency
				// TODO: Trivial AVX or ISPC conversion
				uint32_t entry_index;
				for (entry_index = 0; entry_index < (num_soa_entries & 0xFFFFFFFEU); ++entry_index)
				{
					pack_vector3_u48_soa(rotations_x[entry_index], rotations_y[entry_index], rotations_z[entry_index], scales);

					entry_index++;
					pack_vector3_u48_soa(rotations_x[entry_index], rotations_y[entry_index], rotations_z[entry_index], scales);
				}

				if (entry_index < num_soa_entries)
					pack_vector3_u48_soa(rotations_x[entry_index], rotations_y[entry_index], rotations_z[entry_index], scales);
			}
			else
			{
				if (is_raw_bit_rate(bit_rate))
				{
					const Vector4_32* raw_rotations_x;
					const Vector4_32* raw_rotations_y;
					const Vector4_32* raw_rotations_z;
					const Vector4_32* raw_rotations_w;
					context.raw_tracks_database.get_rotations(*context.segment, transform_index, raw_rotations_x, raw_rotations_y, raw_rotations_z, raw_rotations_w);

					// Copy our raw original values
					copy_vector4f_track(num_soa_entries, raw_rotations_x, raw_rotations_y, raw_rotations_z, raw_rotations_w, rotations_x, rotations_y, rotations_z, rotations_w);

					// Convert our track
					// Drop W, we just ensure it is positive and write it back, the W component can be ignored and trivially reconstructed afterwards
					convert_drop_w_track(rotations_x, rotations_y, rotations_z, rotations_w, num_soa_entries);
				}
				else
				{
					const uint32_t num_bits_at_bit_rate = get_num_bits_at_bit_rate(bit_rate);
					const QuantizationScales scales(num_bits_at_bit_rate);

					if (transform_range.are_rotations_normalized)
					{
						// Process two entries at a time to allow the compiler to re-order things to hide instruction latency
						// TODO: Trivial AVX or ISPC conversion
						uint32_t entry_index;
						for (entry_index = 0; entry_index < (num_soa_entries & 0xFFFFFFFEU); ++entry_index)
						{
							pack_vector3_uXX_soa(rotations_x[entry_index], rotations_y[entry_index], rotations_z[entry_index], scales);

							entry_index++;
							pack_vector3_uXX_soa(rotations_x[entry_index], rotations_y[entry_index], rotations_z[entry_index], scales);
						}

						if (entry_index < num_soa_entries)
							pack_vector3_uXX_soa(rotations_x[entry_index], rotations_y[entry_index], rotations_z[entry_index], scales);
					}
					else
					{
						// Process two entries at a time to allow the compiler to re-order things to hide instruction latency
						// TODO: Trivial AVX or ISPC conversion
						uint32_t entry_index;
						for (entry_index = 0; entry_index < (num_soa_entries & 0xFFFFFFFEU); ++entry_index)
						{
							pack_vector3_sXX_soa(rotations_x[entry_index], rotations_y[entry_index], rotations_z[entry_index], scales);

							entry_index++;
							pack_vector3_sXX_soa(rotations_x[entry_index], rotations_y[entry_index], rotations_z[entry_index], scales);
						}

						if (entry_index < num_soa_entries)
							pack_vector3_sXX_soa(rotations_x[entry_index], rotations_y[entry_index], rotations_z[entry_index], scales);
					}
				}
			}
		}

		inline void quantize_fixed_rotation_track(quantization_context& context, uint32_t transform_index, const acl_impl::qvvf_ranges& transform_range)
		{
			using namespace acl_impl;

			const StaticQuantizationScales<16> scales16;
			const QuantizationScales scales11(11);
			const QuantizationScales scales10(10);

			const uint32_t num_soa_entries = context.segment->num_soa_entries;

			Vector4_32* rotations_x;
			Vector4_32* rotations_y;
			Vector4_32* rotations_z;
			context.mutable_tracks_database.get_rotations(*context.segment, transform_index, rotations_x, rotations_y, rotations_z);

			switch (context.settings.rotation_format)
			{
			case RotationFormat8::Quat_128:
			case RotationFormat8::QuatDropW_96:
			case RotationFormat8::QuatDropW_Variable:
				// Nothing to do, mutable database already contains what we need
				break;
			case RotationFormat8::QuatDropW_48:
				if (transform_range.are_rotations_normalized)
				{
					// Process two entries at a time to allow the compiler to re-order things to hide instruction latency
					// TODO: Trivial AVX or ISPC conversion
					uint32_t entry_index;
					for (entry_index = 0; entry_index < (num_soa_entries & 0xFFFFFFFEU); ++entry_index)
					{
						pack_vector3_u48_soa(rotations_x[entry_index], rotations_y[entry_index], rotations_z[entry_index], scales16);

						entry_index++;
						pack_vector3_u48_soa(rotations_x[entry_index], rotations_y[entry_index], rotations_z[entry_index], scales16);
					}

					if (entry_index < num_soa_entries)
						pack_vector3_u48_soa(rotations_x[entry_index], rotations_y[entry_index], rotations_z[entry_index], scales16);
				}
				else
				{
					// Process two entries at a time to allow the compiler to re-order things to hide instruction latency
					// TODO: Trivial AVX or ISPC conversion
					uint32_t entry_index;
					for (entry_index = 0; entry_index < (num_soa_entries & 0xFFFFFFFEU); ++entry_index)
					{
						pack_vector3_s48_soa(rotations_x[entry_index], rotations_y[entry_index], rotations_z[entry_index], scales16);

						entry_index++;
						pack_vector3_s48_soa(rotations_x[entry_index], rotations_y[entry_index], rotations_z[entry_index], scales16);
					}

					if (entry_index < num_soa_entries)
						pack_vector3_s48_soa(rotations_x[entry_index], rotations_y[entry_index], rotations_z[entry_index], scales16);
				}
				break;
			case RotationFormat8::QuatDropW_32:
				if (transform_range.are_rotations_normalized)
				{
					// Process two entries at a time to allow the compiler to re-order things to hide instruction latency
					// TODO: Trivial AVX or ISPC conversion
					uint32_t entry_index;
					for (entry_index = 0; entry_index < (num_soa_entries & 0xFFFFFFFEU); ++entry_index)
					{
						pack_vector3_u32_soa(rotations_x[entry_index], rotations_y[entry_index], rotations_z[entry_index], scales11, scales11, scales10);

						entry_index++;
						pack_vector3_u32_soa(rotations_x[entry_index], rotations_y[entry_index], rotations_z[entry_index], scales11, scales11, scales10);
					}

					if (entry_index < num_soa_entries)
						pack_vector3_u32_soa(rotations_x[entry_index], rotations_y[entry_index], rotations_z[entry_index], scales11, scales11, scales10);
				}
				else
				{
					// Process two entries at a time to allow the compiler to re-order things to hide instruction latency
					// TODO: Trivial AVX or ISPC conversion
					uint32_t entry_index;
					for (entry_index = 0; entry_index < (num_soa_entries & 0xFFFFFFFEU); ++entry_index)
					{
						pack_vector3_s32_soa(rotations_x[entry_index], rotations_y[entry_index], rotations_z[entry_index], scales11, scales11, scales10);

						entry_index++;
						pack_vector3_s32_soa(rotations_x[entry_index], rotations_y[entry_index], rotations_z[entry_index], scales11, scales11, scales10);
					}

					if (entry_index < num_soa_entries)
						pack_vector3_s32_soa(rotations_x[entry_index], rotations_y[entry_index], rotations_z[entry_index], scales11, scales11, scales10);
				}
				break;
			default:
				ACL_ASSERT(false, "Invalid or unsupported rotation format: %s", get_rotation_format_name(context.settings.rotation_format));
				break;
			}
		}

		struct translation_track_adapter
		{
			static inline bool are_samples_normalized(const acl_impl::qvvf_ranges& transform_range) { return transform_range.are_translations_normalized; }
			static inline uint8_t get_bit_rate(quantization_context& context, uint32_t transform_index) { return context.bit_rate_per_bone[transform_index].translation; }
			static inline void get_mutable_samples(quantization_context& context, uint32_t transform_index, Vector4_32*& out_samples_x, Vector4_32*& out_samples_y, Vector4_32*& out_samples_z)
			{
				context.mutable_tracks_database.get_translations(*context.segment, transform_index, out_samples_x, out_samples_y, out_samples_z);
			}
			static inline void get_raw_samples(quantization_context& context, uint32_t transform_index, const Vector4_32*& out_samples_x, const Vector4_32*& out_samples_y, const Vector4_32*& out_samples_z)
			{
				context.raw_tracks_database.get_translations(*context.segment, transform_index, out_samples_x, out_samples_y, out_samples_z);
			}
			static inline auto get_range_min(const acl_impl::qvvf_ranges& transform_range) { return transform_range.translation_min; }
			static inline auto get_range_extent(const acl_impl::qvvf_ranges& transform_range) { return transform_range.translation_extent; }
			static inline VectorFormat8 get_vector_format(const quantization_context& context) { return context.settings.translation_format; }
		};

		struct scale_track_adapter
		{
			static inline bool are_samples_normalized(const acl_impl::qvvf_ranges& transform_range) { return transform_range.are_scales_normalized; }
			static inline uint8_t get_bit_rate(quantization_context& context, uint32_t transform_index) { return context.bit_rate_per_bone[transform_index].scale; }
			static inline void get_mutable_samples(quantization_context& context, uint32_t transform_index, Vector4_32*& out_samples_x, Vector4_32*& out_samples_y, Vector4_32*& out_samples_z)
			{
				context.mutable_tracks_database.get_scales(*context.segment, transform_index, out_samples_x, out_samples_y, out_samples_z);
			}
			static inline void get_raw_samples(quantization_context& context, uint32_t transform_index, const Vector4_32*& out_samples_x, const Vector4_32*& out_samples_y, const Vector4_32*& out_samples_z)
			{
				context.raw_tracks_database.get_scales(*context.segment, transform_index, out_samples_x, out_samples_y, out_samples_z);
			}
			static inline auto get_range_min(const acl_impl::qvvf_ranges& transform_range) { return transform_range.scale_min; }
			static inline auto get_range_extent(const acl_impl::qvvf_ranges& transform_range) { return transform_range.scale_extent; }
			static inline VectorFormat8 get_vector_format(const quantization_context& context) { return context.settings.scale_format; }
		};

		template<typename adapter_type>
		inline void quantize_variable_vector3f_track(quantization_context& context, uint32_t transform_index, const acl_impl::qvvf_ranges& transform_range)
		{
			using namespace acl_impl;
			ACL_ASSERT(adapter_type::are_samples_normalized(transform_range), "Variable vector3f tracks must be normalized");

			const uint8_t bit_rate = adapter_type::get_bit_rate(context, transform_index);
			const uint32_t num_soa_entries = context.segment->num_soa_entries;

			Vector4_32* samples_x;
			Vector4_32* samples_y;
			Vector4_32* samples_z;
			adapter_type::get_mutable_samples(context, transform_index, samples_x, samples_y, samples_z);

			if (is_constant_bit_rate(bit_rate))
			{
				// We can't use the values in the mutable track database because they have been normalized to the whole segment
				// and we need them normalized to the clip only.

				const Vector4_32* raw_samples_x;
				const Vector4_32* raw_samples_y;
				const Vector4_32* raw_samples_z;
				adapter_type::get_raw_samples(context, transform_index, raw_samples_x, raw_samples_y, raw_samples_z);

				// Copy our raw original values
				copy_vector3f_track(num_soa_entries, raw_samples_x, raw_samples_y, raw_samples_z, samples_x, samples_y, samples_z);

				// Normalize to our clip range
				normalize_vector3f_track(samples_x, samples_y, samples_z, num_soa_entries, adapter_type::get_range_min(transform_range), adapter_type::get_range_extent(transform_range));

				// Quantize and pack our values into place on 16 bits per component
				const StaticQuantizationScales<16> scales;

				// Process two entries at a time to allow the compiler to re-order things to hide instruction latency
				// TODO: Trivial AVX or ISPC conversion
				uint32_t entry_index;
				for (entry_index = 0; entry_index < (num_soa_entries & 0xFFFFFFFEU); ++entry_index)
				{
					pack_vector3_u48_soa(samples_x[entry_index], samples_y[entry_index], samples_z[entry_index], scales);

					entry_index++;
					pack_vector3_u48_soa(samples_x[entry_index], samples_y[entry_index], samples_z[entry_index], scales);
				}

				if (entry_index < num_soa_entries)
					pack_vector3_u48_soa(samples_x[entry_index], samples_y[entry_index], samples_z[entry_index], scales);
			}
			else
			{
				if (is_raw_bit_rate(bit_rate))
				{
					const Vector4_32* raw_samples_x;
					const Vector4_32* raw_samples_y;
					const Vector4_32* raw_samples_z;
					adapter_type::get_raw_samples(context, transform_index, raw_samples_x, raw_samples_y, raw_samples_z);

					// Copy our raw original values
					copy_vector3f_track(num_soa_entries, raw_samples_x, raw_samples_y, raw_samples_z, samples_x, samples_y, samples_z);
				}
				else
				{
					const uint32_t num_bits_at_bit_rate = get_num_bits_at_bit_rate(bit_rate);
					const QuantizationScales scales(num_bits_at_bit_rate);

					// Process two entries at a time to allow the compiler to re-order things to hide instruction latency
					// TODO: Trivial AVX or ISPC conversion
					uint32_t entry_index;
					for (entry_index = 0; entry_index < (num_soa_entries & 0xFFFFFFFEU); ++entry_index)
					{
						pack_vector3_uXX_soa(samples_x[entry_index], samples_y[entry_index], samples_z[entry_index], scales);

						entry_index++;
						pack_vector3_uXX_soa(samples_x[entry_index], samples_y[entry_index], samples_z[entry_index], scales);
					}

					if (entry_index < num_soa_entries)
						pack_vector3_uXX_soa(samples_x[entry_index], samples_y[entry_index], samples_z[entry_index], scales);
				}
			}
		}

		template<typename adapter_type>
		inline void quantize_fixed_vector3f_track(quantization_context& context, uint32_t transform_index, const acl_impl::qvvf_ranges& transform_range)
		{
			using namespace acl_impl;
			(void)transform_range;

			const StaticQuantizationScales<16> scales16;
			const QuantizationScales scales11(11);
			const QuantizationScales scales10(10);

			const uint32_t num_soa_entries = context.segment->num_soa_entries;

			Vector4_32* samples_x;
			Vector4_32* samples_y;
			Vector4_32* samples_z;
			adapter_type::get_mutable_samples(context, transform_index, samples_x, samples_y, samples_z);

			const VectorFormat8 format = adapter_type::get_vector_format(context);
			switch (format)
			{
			case VectorFormat8::Vector3_96:
				// Nothing to do, mutable database already contains what we need
				break;
			case VectorFormat8::Vector3_48:
			{
				ACL_ASSERT(adapter_type::are_samples_normalized(transform_range), "Vector3_48 tracks must be normalized");

				// Process two entries at a time to allow the compiler to re-order things to hide instruction latency
				// TODO: Trivial AVX or ISPC conversion
				uint32_t entry_index;
				for (entry_index = 0; entry_index < (num_soa_entries & 0xFFFFFFFEU); ++entry_index)
				{
					pack_vector3_u48_soa(samples_x[entry_index], samples_y[entry_index], samples_z[entry_index], scales16);

					entry_index++;
					pack_vector3_u48_soa(samples_x[entry_index], samples_y[entry_index], samples_z[entry_index], scales16);
				}

				if (entry_index < num_soa_entries)
					pack_vector3_u48_soa(samples_x[entry_index], samples_y[entry_index], samples_z[entry_index], scales16);
				break;
			}
			case VectorFormat8::Vector3_32:
			{
				ACL_ASSERT(adapter_type::are_samples_normalized(transform_range), "Vector3_32 tracks must be normalized");

				// Process two entries at a time to allow the compiler to re-order things to hide instruction latency
				// TODO: Trivial AVX or ISPC conversion
				uint32_t entry_index;
				for (entry_index = 0; entry_index < (num_soa_entries & 0xFFFFFFFEU); ++entry_index)
				{
					pack_vector3_u32_soa(samples_x[entry_index], samples_y[entry_index], samples_z[entry_index], scales11, scales11, scales10);

					entry_index++;
					pack_vector3_u32_soa(samples_x[entry_index], samples_y[entry_index], samples_z[entry_index], scales11, scales11, scales10);
				}

				if (entry_index < num_soa_entries)
					pack_vector3_u32_soa(samples_x[entry_index], samples_y[entry_index], samples_z[entry_index], scales11, scales11, scales10);
				break;
			}
			case VectorFormat8::Vector3_Variable:
			default:
				ACL_ASSERT(false, "Invalid or unsupported vector format: %s", get_vector_format_name(format));
				break;
			}
		}

		inline void quantize_all_streams(quantization_context& context)
		{
			using namespace acl_impl;
			ACL_ASSERT(context.is_valid(), "quantization_context isn't valid");

			const Vector4_32 default_rotation = quat_identity_32();
			const Vector4_32 default_translation = vector_zero_32();
			const Vector4_32 default_scale = context.mutable_tracks_database.get_default_scale();
			const uint32_t num_soa_entries = context.segment->num_soa_entries;

			// Quantize to the mutable database in-place

			for (uint32_t transform_index = 0; transform_index < context.num_transforms; ++transform_index)
			{
				const qvvf_ranges& transform_range = context.mutable_tracks_database.get_range(transform_index);

				if (transform_range.is_rotation_default)
				{
					Vector4_32* rotations_x;
					Vector4_32* rotations_y;
					Vector4_32* rotations_z;
					Vector4_32* rotations_w;
					context.mutable_tracks_database.get_rotations(*context.segment, transform_index, rotations_x, rotations_y, rotations_z, rotations_w);

					set_vector4f_track(default_rotation, num_soa_entries, rotations_x, rotations_y, rotations_z, rotations_w);
				}
				else if (transform_range.is_rotation_constant)
				{
					Vector4_32* rotations_x;
					Vector4_32* rotations_y;
					Vector4_32* rotations_z;
					Vector4_32* rotations_w;
					context.mutable_tracks_database.get_rotations(*context.segment, transform_index, rotations_x, rotations_y, rotations_z, rotations_w);

					Vector4_32 rotation = context.raw_tracks_database.get_rotation(context.first_segment, transform_index, 0);
					rotation = convert_rotation(rotation, context.raw_tracks_database.get_rotation_format(), context.mutable_tracks_database.get_rotation_format());
					set_vector4f_track(rotation, num_soa_entries, rotations_x, rotations_y, rotations_z, rotations_w);

					// We might need to quantize it
					quantize_fixed_rotation_track(context, transform_index, transform_range);
				}
				else if (is_rotation_format_variable(context.settings.rotation_format))
					quantize_variable_rotation_track(context, transform_index, transform_range);
				else
					quantize_fixed_rotation_track(context, transform_index, transform_range);

				if (transform_range.is_translation_default)
				{
					Vector4_32* translations_x;
					Vector4_32* translations_y;
					Vector4_32* translations_z;
					context.mutable_tracks_database.get_translations(*context.segment, transform_index, translations_x, translations_y, translations_z);

					set_vector3f_track(default_translation, num_soa_entries, translations_x, translations_y, translations_z);
				}
				else if (transform_range.is_translation_constant)
				{
					Vector4_32* translations_x;
					Vector4_32* translations_y;
					Vector4_32* translations_z;
					context.mutable_tracks_database.get_translations(*context.segment, transform_index, translations_x, translations_y, translations_z);

					const Vector4_32 translation = context.raw_tracks_database.get_translation(context.first_segment, transform_index, 0);
					set_vector3f_track(translation, num_soa_entries, translations_x, translations_y, translations_z);
				}
				else if (is_vector_format_variable(context.settings.translation_format))
					quantize_variable_vector3f_track<translation_track_adapter>(context, transform_index, transform_range);
				else
					quantize_fixed_vector3f_track<translation_track_adapter>(context, transform_index, transform_range);

				if (context.has_scale)
				{
					if (transform_range.is_scale_default)
					{
						Vector4_32* scales_x;
						Vector4_32* scales_y;
						Vector4_32* scales_z;
						context.mutable_tracks_database.get_scales(*context.segment, transform_index, scales_x, scales_y, scales_z);

						set_vector3f_track(default_scale, num_soa_entries, scales_x, scales_y, scales_z);
					}
					else if (transform_range.is_scale_constant)
					{
						Vector4_32* scales_x;
						Vector4_32* scales_y;
						Vector4_32* scales_z;
						context.mutable_tracks_database.get_scales(*context.segment, transform_index, scales_x, scales_y, scales_z);

						const Vector4_32 scale = context.raw_tracks_database.get_scale(context.first_segment, transform_index, 0);
						set_vector3f_track(scale, num_soa_entries, scales_x, scales_y, scales_z);
					}
					else if (is_vector_format_variable(context.settings.translation_format))
						quantize_variable_vector3f_track<scale_track_adapter>(context, transform_index, transform_range);
					else
						quantize_fixed_vector3f_track<scale_track_adapter>(context, transform_index, transform_range);
				}

				context.segment->bit_rates[transform_index] = context.bit_rate_per_bone[transform_index];
			}

			context.mutable_tracks_database.set_rotation_format(context.settings.rotation_format);
			context.mutable_tracks_database.set_translation_format(context.settings.translation_format);
			context.mutable_tracks_database.set_scale_format(context.settings.scale_format);
		}

		template<typename context_type>
		inline void find_optimal_bit_rates(context_type& context)
		{
			ACL_ASSERT(context.is_valid(), "QuantizationContext isn't valid");

			const CompressionSettings& settings = context.settings;
			const uint16_t num_transforms = safe_static_cast<uint16_t>(context.num_transforms);

			initialize_bone_bit_rates(context, context.bit_rate_per_bone);

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

			uint8_t* bone_chain_permutation = allocate_type_array<uint8_t>(context.allocator, context.num_transforms);
			uint16_t* chain_bone_indices = allocate_type_array<uint16_t>(context.allocator, context.num_transforms);
			BoneBitRate* permutation_bit_rates = allocate_type_array<BoneBitRate>(context.allocator, context.num_transforms);
			BoneBitRate* best_permutation_bit_rates = allocate_type_array<BoneBitRate>(context.allocator, context.num_transforms);
			BoneBitRate* best_bit_rates = allocate_type_array<BoneBitRate>(context.allocator, context.num_transforms);
			std::memcpy(best_bit_rates, context.bit_rate_per_bone, sizeof(BoneBitRate) * context.num_transforms);

			for (uint16_t bone_index = 0; bone_index < num_transforms; ++bone_index)
			{
				float error = calculate_max_error_at_bit_rate_object(context, bone_index, error_scan_stop_condition::until_error_too_high);
				if (error < settings.error_threshold)
					continue;

				if (context.bit_rate_per_bone[bone_index].rotation >= k_highest_bit_rate && context.bit_rate_per_bone[bone_index].translation >= k_highest_bit_rate && context.bit_rate_per_bone[bone_index].scale >= k_highest_bit_rate)
				{
					// Our bone already has the highest precision possible locally, if the local error already exceeds our threshold,
					// there is nothing we can do, bail out
					const float local_error = calculate_max_error_at_bit_rate_local(context, bone_index, error_scan_stop_condition::until_error_too_high);
					if (local_error >= settings.error_threshold)
						continue;
				}

				const uint16_t num_bones_in_chain = calculate_bone_chain_indices(context.skeleton, bone_index, chain_bone_indices);

				const float initial_error = error;

				while (error >= settings.error_threshold)
				{
					// Generate permutations for up to 3 bit rate increments
					// Perform an exhaustive search of the permutations and pick the best result
					// If our best error is under the threshold, we are done, otherwise we will try again from there
					const float original_error = error;
					float best_error = error;

					// The first permutation increases the bit rate of a single track/bone
					std::fill(bone_chain_permutation, bone_chain_permutation + context.num_transforms, uint8_t(0));
					bone_chain_permutation[num_bones_in_chain - 1] = 1;
					error = calculate_bone_permutation_error(context, permutation_bit_rates, bone_chain_permutation, chain_bone_indices, num_bones_in_chain, bone_index, best_permutation_bit_rates, original_error);
					if (error < best_error)
					{
						best_error = error;
						std::memcpy(best_bit_rates, best_permutation_bit_rates, sizeof(BoneBitRate) * context.num_transforms);

						if (error < settings.error_threshold)
							break;
					}

					if (settings.level >= CompressionLevel8::High)
					{
						// The second permutation increases the bit rate of 2 track/bones
						std::fill(bone_chain_permutation, bone_chain_permutation + context.num_transforms, uint8_t(0));
						bone_chain_permutation[num_bones_in_chain - 1] = 2;
						error = calculate_bone_permutation_error(context, permutation_bit_rates, bone_chain_permutation, chain_bone_indices, num_bones_in_chain, bone_index, best_permutation_bit_rates, original_error);
						if (error < best_error)
						{
							best_error = error;
							std::memcpy(best_bit_rates, best_permutation_bit_rates, sizeof(BoneBitRate) * context.num_transforms);

							if (error < settings.error_threshold)
								break;
						}

						if (num_bones_in_chain > 1)
						{
							std::fill(bone_chain_permutation, bone_chain_permutation + context.num_transforms, uint8_t(0));
							bone_chain_permutation[num_bones_in_chain - 2] = 1;
							bone_chain_permutation[num_bones_in_chain - 1] = 1;
							error = calculate_bone_permutation_error(context, permutation_bit_rates, bone_chain_permutation, chain_bone_indices, num_bones_in_chain, bone_index, best_permutation_bit_rates, original_error);
							if (error < best_error)
							{
								best_error = error;
								std::memcpy(best_bit_rates, best_permutation_bit_rates, sizeof(BoneBitRate) * context.num_transforms);

								if (error < settings.error_threshold)
									break;
							}
						}
					}

					if (settings.level >= CompressionLevel8::Highest)
					{
						// The third permutation increases the bit rate of 3 track/bones
						std::fill(bone_chain_permutation, bone_chain_permutation + context.num_transforms, uint8_t(0));
						bone_chain_permutation[num_bones_in_chain - 1] = 3;
						error = calculate_bone_permutation_error(context, permutation_bit_rates, bone_chain_permutation, chain_bone_indices, num_bones_in_chain, bone_index, best_permutation_bit_rates, original_error);
						if (error < best_error)
						{
							best_error = error;
							std::memcpy(best_bit_rates, best_permutation_bit_rates, sizeof(BoneBitRate) * context.num_transforms);

							if (error < settings.error_threshold)
								break;
						}

						if (num_bones_in_chain > 1)
						{
							std::fill(bone_chain_permutation, bone_chain_permutation + context.num_transforms, uint8_t(0));
							bone_chain_permutation[num_bones_in_chain - 2] = 2;
							bone_chain_permutation[num_bones_in_chain - 1] = 1;
							error = calculate_bone_permutation_error(context, permutation_bit_rates, bone_chain_permutation, chain_bone_indices, num_bones_in_chain, bone_index, best_permutation_bit_rates, original_error);
							if (error < best_error)
							{
								best_error = error;
								std::memcpy(best_bit_rates, best_permutation_bit_rates, sizeof(BoneBitRate) * context.num_transforms);

								if (error < settings.error_threshold)
									break;
							}

							if (num_bones_in_chain > 2)
							{
								std::fill(bone_chain_permutation, bone_chain_permutation + context.num_transforms, uint8_t(0));
								bone_chain_permutation[num_bones_in_chain - 3] = 1;
								bone_chain_permutation[num_bones_in_chain - 2] = 1;
								bone_chain_permutation[num_bones_in_chain - 1] = 1;
								error = calculate_bone_permutation_error(context, permutation_bit_rates, bone_chain_permutation, chain_bone_indices, num_bones_in_chain, bone_index, best_permutation_bit_rates, original_error);
								if (error < best_error)
								{
									best_error = error;
									std::memcpy(best_bit_rates, best_permutation_bit_rates, sizeof(BoneBitRate) * context.num_transforms);

									if (error < settings.error_threshold)
										break;
								}
							}
						}
					}

					if (best_error >= original_error)
						break;	// No progress made

					error = best_error;
					if (error < original_error)
					{
#if ACL_IMPL_DEBUG_VARIABLE_QUANTIZATION
						std::swap(context.bit_rate_per_bone, best_bit_rates);
						float new_error = calculate_max_error_at_bit_rate_object(context, bone_index, error_scan_stop_condition::until_end_of_segment);
						std::swap(context.bit_rate_per_bone, best_bit_rates);

						for (uint16_t i = 0; i < context.num_transforms; ++i)
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

						std::memcpy(context.bit_rate_per_bone, best_bit_rates, sizeof(BoneBitRate) * context.num_transforms);
					}
				}

				if (error < initial_error)
				{
#if ACL_IMPL_DEBUG_VARIABLE_QUANTIZATION
					std::swap(context.bit_rate_per_bone, best_bit_rates);
					float new_error = calculate_max_error_at_bit_rate_object(context, bone_index, error_scan_stop_condition::until_end_of_segment);
					std::swap(context.bit_rate_per_bone, best_bit_rates);

					for (uint16_t i = 0; i < context.num_transforms; ++i)
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

					std::memcpy(context.bit_rate_per_bone, best_bit_rates, sizeof(BoneBitRate) * context.num_transforms);
				}

				// Our error remains too high, this should be rare.
				// Attempt to increase the bit rate as much as we can while still back tracking if it doesn't help.
				error = calculate_max_error_at_bit_rate_object(context, bone_index, error_scan_stop_condition::until_end_of_segment);
				while (error >= settings.error_threshold)
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

						while (error >= settings.error_threshold)
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

							ACL_ASSERT((bone_bit_rate.rotation <= k_highest_bit_rate || bone_bit_rate.rotation == k_invalid_bit_rate) && (bone_bit_rate.translation <= k_highest_bit_rate || bone_bit_rate.translation == k_invalid_bit_rate) && (bone_bit_rate.scale <= k_highest_bit_rate || bone_bit_rate.scale == k_invalid_bit_rate), "Invalid bit rate! [%u, %u, %u]", bone_bit_rate.rotation, bone_bit_rate.translation, bone_bit_rate.scale);

							error = calculate_max_error_at_bit_rate_object(context, bone_index, error_scan_stop_condition::until_end_of_segment);

							if (error < best_bit_rate_error)
							{
								best_bone_bit_rate = bone_bit_rate;
								best_bit_rate_error = error;

#if ACL_IMPL_DEBUG_VARIABLE_QUANTIZATION
								printf("%u: => %u %u %u (%f)\n", chain_bone_index, bone_bit_rate.rotation, bone_bit_rate.translation, bone_bit_rate.scale, error);
								for (uint16_t i = chain_link_index + 1; i < num_bones_in_chain; ++i)
								{
									const uint16_t chain_bone_index2 = chain_bone_indices[chain_link_index];
									float error2 = calculate_max_error_at_bit_rate_object(context, chain_bone_index2, error_scan_stop_condition::until_end_of_segment);
									printf("  %u: => (%f)\n", i, error2);
								}
#endif
							}
						}

						// Only retain the lowest error bit rates
						bone_bit_rate = best_bone_bit_rate;
						error = best_bit_rate_error;

						if (error < settings.error_threshold)
							break;
					}

					if (num_maxed_out == num_bones_in_chain)
						break;

					// TODO: Try to lower the bit rate again in the reverse direction?
				}

				// Despite our best efforts, we failed to meet the threshold with our heuristics.
				// No longer attempt to find what is best for size, max out the bit rates until we meet the threshold.
				// Only do this if the rotation format is full precision quaternions. This last step is not guaranteed
				// to reach the error threshold but it will very likely increase the memory footprint. Even if we do
				// reach the error threshold for the given bone, another sibling bone already processed might now
				// have an error higher than it used to if quantization caused its error to compensate. More often than
				// not, sibling bones will remain fairly close in their error. Some packed rotation formats, namely
				// drop W component can have a high error even with raw values, it is assumed that if such a format
				// is used then a best effort approach to reach the error threshold is entirely fine.
				if (error >= settings.error_threshold && context.settings.rotation_format == RotationFormat8::Quat_128)
				{
					// From child to parent, max out the bit rate
					for (int16_t chain_link_index = num_bones_in_chain - 1; chain_link_index >= 0; --chain_link_index)
					{
						const uint16_t chain_bone_index = chain_bone_indices[chain_link_index];
						BoneBitRate& bone_bit_rate = context.bit_rate_per_bone[chain_bone_index];
						bone_bit_rate.rotation = std::max<uint8_t>(bone_bit_rate.rotation, k_highest_bit_rate);
						bone_bit_rate.translation = std::max<uint8_t>(bone_bit_rate.translation, k_highest_bit_rate);
						bone_bit_rate.scale = std::max<uint8_t>(bone_bit_rate.scale, k_highest_bit_rate);

						error = calculate_max_error_at_bit_rate_object(context, bone_index, error_scan_stop_condition::until_end_of_segment);
						if (error < settings.error_threshold)
							break;
					}
				}
			}

#if ACL_IMPL_DEBUG_VARIABLE_QUANTIZATION
			printf("Variable quantization optimization results:\n");
			for (uint16_t i = 0; i < context.num_transforms; ++i)
			{
				float error = calculate_max_error_at_bit_rate_object(context, i, error_scan_stop_condition::until_end_of_segment);
				const BoneBitRate& bone_bit_rate = context.bit_rate_per_bone[i];
				printf("%u: %u | %u | %u => %f %s\n", i, bone_bit_rate.rotation, bone_bit_rate.translation, bone_bit_rate.scale, error, error >= settings.error_threshold ? "!" : "");
			}
#endif

			deallocate_type_array(context.allocator, bone_chain_permutation, context.num_transforms);
			deallocate_type_array(context.allocator, chain_bone_indices, context.num_transforms);
			deallocate_type_array(context.allocator, permutation_bit_rates, context.num_transforms);
			deallocate_type_array(context.allocator, best_permutation_bit_rates, context.num_transforms);
			deallocate_type_array(context.allocator, best_bit_rates, context.num_transforms);
		}
	}

	inline void quantize_streams(IAllocator& allocator, ClipContext& clip_context, const CompressionSettings& settings, const RigidSkeleton& skeleton, const ClipContext& raw_clip_context, const ClipContext& additive_base_clip_context)
	{
		const bool is_rotation_variable = is_rotation_format_variable(settings.rotation_format);
		const bool is_translation_variable = is_vector_format_variable(settings.translation_format);
		const bool is_scale_variable = is_vector_format_variable(settings.scale_format);
		const bool is_any_variable = is_rotation_variable || is_translation_variable || is_scale_variable;

		impl::QuantizationContext context(allocator, clip_context, raw_clip_context, additive_base_clip_context, settings, skeleton);

		for (SegmentContext& segment : clip_context.segment_iterator())
		{
#if ACL_IMPL_DEBUG_VARIABLE_QUANTIZATION
			printf("Quantizing segment %u...\n", segment.segment_index);
#endif

			context.set_segment(segment);

			if (is_any_variable)
				impl::find_optimal_bit_rates(context);

			// Quantize our streams now that we found the optimal bit rates
			impl::quantize_all_streams(context);
		}
	}

	namespace acl_impl
	{
		inline void quantize_tracks(acl::impl::quantization_context& context, segment_context& segment, const CompressionSettings& settings)
		{
#if ACL_IMPL_DEBUG_VARIABLE_QUANTIZATION
			printf("Quantizing segment %u...\n", segment.index);
#endif

			context.set_segment(segment);

			const bool is_rotation_variable = is_rotation_format_variable(settings.rotation_format);
			const bool is_translation_variable = is_vector_format_variable(settings.translation_format);
			const bool is_scale_variable = is_vector_format_variable(settings.scale_format);
			const bool is_any_variable = is_rotation_variable || is_translation_variable || is_scale_variable;

			if (is_any_variable)
				impl::find_optimal_bit_rates(context);
			else
				std::fill(context.bit_rate_per_bone, context.bit_rate_per_bone + context.num_transforms, BoneBitRate{ k_invalid_bit_rate, k_invalid_bit_rate, k_invalid_bit_rate });

			// Quantize our streams now that we found the optimal bit rates
			impl::quantize_all_streams(context);
		}
	}
}

ACL_IMPL_FILE_PRAGMA_POP
