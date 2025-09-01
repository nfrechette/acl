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

#include "acl/version.h"
#include "acl/core/iallocator.h"
#include "acl/core/impl/compiler_utils.h"
#include "acl/core/error.h"
#include "acl/core/scope_profiler.h"
#include "acl/core/time_utils.h"
#include "acl/core/track_formats.h"
#include "acl/core/impl/variable_bit_rates.h"
#include "acl/math/quat_packing.h"
#include "acl/math/vector4_packing.h"
#include "acl/compression/impl/track_bit_rate_database.h"
#include "acl/compression/impl/transform_bit_rate_permutations.h"
#include "acl/compression/impl/clip_context.h"
#include "acl/compression/impl/compression_stats.h"
#include "acl/compression/impl/sample_streams.h"
#include "acl/compression/impl/normalize.transform.h"
#include "acl/compression/impl/convert_rotation.transform.h"
#include "acl/compression/impl/rigid_shell_utils.h"
#include "acl/compression/impl/topology_metadata.h"
#include "acl/compression/transform_error_metrics.h"
#include "acl/compression/compression_settings.h"

#include <rtm/quatf.h>
#include <rtm/vector4f.h>
#include <rtm/mask4f.h>

#if defined(ACL_USE_SJSON)
#include <sjson/writer.h>
#endif

#include <cstddef>
#include <cstdint>
#include <functional>

#define ACL_IMPL_DEBUG_LEVEL_NONE					0		// No logging whatsoever
#define ACL_IMPL_DEBUG_LEVEL_SUMMARY_ONLY			1		// Logs a summary at the end
#define ACL_IMPL_DEBUG_LEVEL_BASIC_INFO				2		// Logs the best bit rate after each pass
#define ACL_IMPL_DEBUG_LEVEL_VERBOSE_INFO			3		// Logs every attempted bit rate

// Dumps details of quantization optimization process
// Use debug levels above to control debug output
#define ACL_IMPL_DEBUG_VARIABLE_QUANTIZATION		ACL_IMPL_DEBUG_LEVEL_NONE

// 0 = no debug into, 1 = basic info
#define ACL_IMPL_DEBUG_CONTRIBUTING_ERROR			0

// 0 = no profiling, 1 = we perform quantization 10 times in a row for every segment
#define ACL_IMPL_PROFILE_MATH						0

#if ACL_IMPL_PROFILE_MATH && defined(__ANDROID__)
#include <android/log.h>
#endif

ACL_IMPL_FILE_PRAGMA_PUSH

namespace acl
{
	ACL_IMPL_VERSION_NAMESPACE_BEGIN

	namespace acl_impl
	{
		struct quantization_context
		{
			iallocator& allocator;
			clip_context& clip;
			const clip_context& raw_clip;
			const clip_context& additive_base_clip;
			segment_context* segment = nullptr;
			transform_streams* bone_streams = nullptr;
			const transform_metadata* metadata = nullptr;
			uint32_t num_bones = 0;
			const itransform_error_metric* error_metric = nullptr;
			const clip_topology_t* topology = nullptr;

			const compression_settings& settings;
			const compression_segmenting_settings& segmenting_settings;

			track_bit_rate_database bit_rate_database;
			single_track_query local_query;
			every_track_query all_local_query;
			hierarchical_track_query object_query;

			uint32_t num_samples = 0;					// Num samples within our segment
			uint32_t segment_sample_start_index = 0;
			float sample_rate = 0.0F;
			float clip_duration = 0.0F;
			bool has_scale = false;
			bool has_additive_base = false;
			bool needs_conversion = false;

			rotation_format8 rotation_format;
			vector_format8 translation_format;
			vector_format8 scale_format;
			compression_level8 compression_level;

			const transform_streams* raw_bone_streams = nullptr;

			// v2
			rtm::qvvf* additive_base_local_transforms = nullptr;	// 1 per transform, per sample, in segment (only when additive)
			rtm::qvvf* object_transforms_raw = nullptr;				// 1 per transform, per sample, in segment
			rtm::qvvf* cached_transforms_lossy = nullptr;			// 1 per transform, per sample, in segment
			uint32_t* critical_transform_indices = nullptr;			// 1 per transform

			uint32_t* transform_chain_indices = nullptr;			// max_num_transform_chains * max_chain_length
			uint32_t** transform_chains = nullptr;					// max_num_transform_chains
			uint32_t* transform_chain_counts = nullptr;				// max_num_transform_chains

			uint32_t max_num_transform_chains = 0;
			uint32_t max_chain_length = 0;

			// v1
			rigid_shell_metadata_t* shell_metadata_per_transform = nullptr;	// 1 per transform

			rtm::qvvf* additive_local_pose = nullptr;			// 1 per transform
			rtm::qvvf* raw_local_pose = nullptr;				// 1 per transform
			rtm::qvvf* lossy_local_pose = nullptr;				// 1 per transform

			rtm::qvvf* lossy_transforms_start = nullptr;		// 1 per transform, for calculating the contributing error
			rtm::qvvf* lossy_transforms_end = nullptr;			// 1 per transform, for calculating the contributing error

			uint8_t* raw_local_transforms = nullptr;			// 1 per transform per sample in segment
			uint8_t* base_local_transforms = nullptr;			// 1 per transform per sample in segment
			uint8_t* raw_object_transforms = nullptr;			// 1 per transform per sample in segment

			uint8_t* local_transforms_converted = nullptr;		// 1 per transform
			uint8_t* lossy_object_pose = nullptr;				// 1 per transform
			size_t metric_transform_size = 0;

			transform_bit_rates* bit_rate_per_bone = nullptr;	// 1 per transform
			uint32_t* parent_transform_indices = nullptr;		// 1 per transform
			uint32_t* self_transform_indices = nullptr;			// 1 per transform

			uint32_t* chain_bone_indices = nullptr;				// 1 per transform
			uint32_t num_bones_in_chain = 0;
			uint32_t padding1 = 0;								// unused

			quantization_context(iallocator& allocator_, clip_context& clip_, const clip_context& raw_clip_, const clip_context& additive_base_clip_, const compression_settings& settings_, const compression_segmenting_settings& segmenting_settings_)
				: allocator(allocator_)
				, clip(clip_)
				, raw_clip(raw_clip_)
				, additive_base_clip(additive_base_clip_)
				, segment(nullptr)
				, bone_streams(nullptr)
				, metadata(clip_.metadata)
				, num_bones(clip_.num_bones)
				, error_metric(settings_.error_metric)
				, topology(clip_.topology)
				, settings(settings_)
				, segmenting_settings(segmenting_settings_)
				, bit_rate_database(allocator_, settings_.rotation_format, settings_.translation_format, settings_.scale_format, clip_.segments->bone_streams, raw_clip_.segments->bone_streams, clip_.num_bones, clip_.segments->num_samples)
				, local_query()
				, all_local_query(allocator_)
				, object_query(allocator_)
				, num_samples(~0U)
				, segment_sample_start_index(~0U)
				, sample_rate(clip_.sample_rate)
				, clip_duration(clip_.duration)
				, has_scale(clip_.has_scale)
				, has_additive_base(clip_.has_additive_base)
				, rotation_format(settings_.rotation_format)
				, translation_format(settings_.translation_format)
				, scale_format(settings_.scale_format)
				, compression_level(settings_.level)
				, raw_bone_streams(raw_clip_.segments[0].bone_streams)
				, lossy_transforms_start(nullptr)
				, lossy_transforms_end(nullptr)
				, num_bones_in_chain(0)
			{
				local_query.bind(bit_rate_database);
				object_query.bind(bit_rate_database);

				needs_conversion = settings_.error_metric->needs_conversion(clip_.has_scale);
				const size_t metric_transform_size_ = settings_.error_metric->get_transform_size(clip_.has_scale);
				metric_transform_size = metric_transform_size_;

				shell_metadata_per_transform = allocate_type_array<rigid_shell_metadata_t>(allocator, num_bones);
				bit_rate_per_bone = allocate_type_array<transform_bit_rates>(allocator, num_bones);
				chain_bone_indices = allocate_type_array<uint32_t>(allocator, num_bones);
			}

			~quantization_context()
			{
				// v2
				deallocate_type_array(allocator, additive_base_local_transforms, num_bones);
				deallocate_type_array(allocator, object_transforms_raw, num_bones * segmenting_settings.max_num_samples);
				deallocate_type_array(allocator, cached_transforms_lossy, num_bones * segmenting_settings.max_num_samples);
				deallocate_type_array(allocator, critical_transform_indices, num_bones);
				deallocate_type_array(allocator, transform_chain_indices, max_num_transform_chains * max_chain_length);
				deallocate_type_array(allocator, transform_chains, max_num_transform_chains);
				deallocate_type_array(allocator, transform_chain_counts, max_num_transform_chains);

				// v1
				deallocate_type_array(allocator, shell_metadata_per_transform, num_bones);
				deallocate_type_array(allocator, additive_local_pose, num_bones);
				deallocate_type_array(allocator, raw_local_pose, num_bones);
				deallocate_type_array(allocator, lossy_local_pose, num_bones);
				deallocate_type_array(allocator, lossy_transforms_start, num_bones);
				deallocate_type_array(allocator, lossy_transforms_end, num_bones);
				deallocate_type_array(allocator, raw_local_transforms, metric_transform_size * num_bones * clip.segments->num_samples);
				deallocate_type_array(allocator, base_local_transforms, metric_transform_size * num_bones * clip.segments->num_samples);
				deallocate_type_array(allocator, raw_object_transforms, metric_transform_size * num_bones * clip.segments->num_samples);
				deallocate_type_array(allocator, local_transforms_converted, metric_transform_size * num_bones);
				deallocate_type_array(allocator, lossy_object_pose, metric_transform_size * num_bones);
				deallocate_type_array(allocator, bit_rate_per_bone, num_bones);
				deallocate_type_array(allocator, parent_transform_indices, num_bones);
				deallocate_type_array(allocator, self_transform_indices, num_bones);
				deallocate_type_array(allocator, chain_bone_indices, num_bones);
			}

			void set_segment(segment_context& segment_)
			{
				segment = &segment_;
				bone_streams = segment_.bone_streams;
				num_samples = segment_.num_samples;
				segment_sample_start_index = segment_.clip_sample_offset;
				bit_rate_database.set_segment(segment_.bone_streams, segment_.num_bones, segment_.num_samples);
			}

			void initialize_v1()
			{
				if (raw_local_pose != nullptr)
					return;	// Already initialized

				additive_local_pose = clip.has_additive_base ? allocate_type_array<rtm::qvvf>(allocator, num_bones) : nullptr;
				raw_local_pose = allocate_type_array<rtm::qvvf>(allocator, num_bones);
				lossy_local_pose = allocate_type_array<rtm::qvvf>(allocator, num_bones);
				raw_local_transforms = allocate_type_array_aligned<uint8_t>(allocator, metric_transform_size * num_bones * clip.segments->num_samples, 64);
				base_local_transforms = clip.has_additive_base ? allocate_type_array_aligned<uint8_t>(allocator, metric_transform_size * num_bones * clip.segments->num_samples, 64) : nullptr;
				raw_object_transforms = allocate_type_array_aligned<uint8_t>(allocator, metric_transform_size * num_bones * clip.segments->num_samples, 64);
				local_transforms_converted = needs_conversion ? allocate_type_array_aligned<uint8_t>(allocator, metric_transform_size * num_bones, 64) : nullptr;
				lossy_object_pose = allocate_type_array_aligned<uint8_t>(allocator, metric_transform_size * num_bones, 64);
				parent_transform_indices = allocate_type_array<uint32_t>(allocator, num_bones);
				self_transform_indices = allocate_type_array<uint32_t>(allocator, num_bones);

				for (uint32_t transform_index = 0; transform_index < num_bones; ++transform_index)
				{
					const transform_metadata& metadata_ = clip.metadata[transform_index];
					parent_transform_indices[transform_index] = metadata_.parent_index;
					self_transform_indices[transform_index] = transform_index;
				}
			}

			void initialize_v2()
			{
				if (object_transforms_raw != nullptr)
					return;	// Already initialized

				additive_base_local_transforms = has_additive_base ? allocate_type_array<rtm::qvvf>(allocator, num_bones * segmenting_settings.max_num_samples) : nullptr;
				object_transforms_raw = allocate_type_array<rtm::qvvf>(allocator, num_bones * segmenting_settings.max_num_samples);
				cached_transforms_lossy = allocate_type_array<rtm::qvvf>(allocator, num_bones * segmenting_settings.max_num_samples);

				critical_transform_indices = allocate_type_array<uint32_t>(allocator, num_bones);
			}

			void initialize_v2_scale()
			{
				if (transform_chain_indices != nullptr)
					return;	// Already initialized

				max_num_transform_chains = num_bones;
				max_chain_length = topology->max_leaf_depth + 1;						// +1 since depth is 0-based

				transform_chain_indices = allocate_type_array<uint32_t>(allocator, max_num_transform_chains * max_chain_length);
				transform_chains = allocate_type_array<uint32_t*>(allocator, max_num_transform_chains);
				transform_chain_counts = allocate_type_array<uint32_t>(allocator, max_num_transform_chains);

				for (uint32_t chain_index = 0; chain_index < max_num_transform_chains; ++chain_index)
					transform_chains[chain_index] = (chain_index * max_chain_length) + transform_chain_indices;
			}

			bool is_valid() const { return segment != nullptr; }

			quantization_context(const quantization_context&) = delete;
			quantization_context(quantization_context&&) = delete;
			quantization_context& operator=(const quantization_context&) = delete;
			quantization_context& operator=(quantization_context&&) = delete;
		};

		inline void quantize_fixed_rotation_stream(iallocator& allocator, const rotation_track_stream& raw_stream, rotation_format8 rotation_format, rotation_track_stream& out_quantized_stream)
		{
			// We expect all our samples to have the same width of sizeof(rtm::vector4f)
			ACL_ASSERT(raw_stream.get_sample_size() == sizeof(rtm::vector4f), "Unexpected rotation sample size. %u != %zu", raw_stream.get_sample_size(), sizeof(rtm::vector4f));

			const uint32_t num_samples = raw_stream.get_num_samples();
			const uint32_t rotation_sample_size = get_packed_rotation_size(rotation_format);
			const float sample_rate = raw_stream.get_sample_rate();
			rotation_track_stream quantized_stream(allocator, num_samples, rotation_sample_size, sample_rate, rotation_format);

			for (uint32_t sample_index = 0; sample_index < num_samples; ++sample_index)
			{
				const rtm::quatf rotation = raw_stream.get_raw_sample<rtm::quatf>(sample_index);
				uint8_t* quantized_ptr = quantized_stream.get_raw_sample_ptr(sample_index);

				switch (rotation_format)
				{
				case rotation_format8::quatf_full:
					pack_vector4_128(rtm::quat_to_vector(rotation), quantized_ptr);
					break;
				case rotation_format8::quatf_drop_w_full:
					pack_vector3_96(rtm::quat_to_vector(rotation), quantized_ptr);
					break;
				case rotation_format8::quatf_drop_w_variable:
				default:
					ACL_ASSERT(false, "Invalid or unsupported rotation format: " ACL_ASSERT_STRING_FORMAT_SPECIFIER, get_rotation_format_name(rotation_format));
					break;
				}
			}

			out_quantized_stream = std::move(quantized_stream);
		}

		inline void quantize_fixed_rotation_stream(quantization_context& context, uint32_t bone_index, rotation_format8 rotation_format)
		{
			ACL_ASSERT(bone_index < context.num_bones, "Invalid bone index: %u", bone_index);

			transform_streams& bone_stream = context.bone_streams[bone_index];

			// Default tracks aren't quantized
			if (bone_stream.is_rotation_default)
				return;

			quantize_fixed_rotation_stream(context.allocator, bone_stream.rotations, rotation_format, bone_stream.rotations);
		}

		inline void quantize_variable_rotation_stream(quantization_context& context, const transform_streams& raw_track, const transform_streams& lossy_track, uint8_t bit_rate, rotation_track_stream& out_quantized_stream)
		{
			const rotation_track_stream& raw_rotations = raw_track.rotations;
			const rotation_track_stream& lossy_rotations = lossy_track.rotations;

			// We expect all our samples to have the same width of sizeof(rtm::vector4f)
			ACL_ASSERT(lossy_rotations.get_sample_size() == sizeof(rtm::vector4f), "Unexpected rotation sample size. %u != %zu", lossy_rotations.get_sample_size(), sizeof(rtm::vector4f));

			const uint32_t num_samples = is_constant_bit_rate(bit_rate) ? 1 : lossy_rotations.get_num_samples();
			const uint32_t sample_size = sizeof(uint64_t) * 2;
			const float sample_rate = lossy_rotations.get_sample_rate();
			rotation_track_stream quantized_stream(context.allocator, num_samples, sample_size, sample_rate, rotation_format8::quatf_drop_w_variable, bit_rate);

			if (is_constant_bit_rate(bit_rate))
			{
#if defined(ACL_IMPL_ENABLE_WEIGHTED_AVERAGE_CONSTANT_SUB_TRACKS)
				const track_stream_range& bone_range = context.segment->ranges[lossy_track.bone_index].rotation;
				const rtm::vector4f normalized_rotation = clip_range.get_weighted_average();
#else
				const rtm::vector4f normalized_rotation = lossy_track.constant_rotation;
#endif

				uint8_t* quantized_ptr = quantized_stream.get_raw_sample_ptr(0);
				pack_vector3_u48_unsafe(normalized_rotation, quantized_ptr);
			}
			else
			{
				const uint32_t num_bits_at_bit_rate = get_num_bits_at_bit_rate(bit_rate);

				for (uint32_t sample_index = 0; sample_index < num_samples; ++sample_index)
				{
					uint8_t* quantized_ptr = quantized_stream.get_raw_sample_ptr(sample_index);

					if (is_raw_bit_rate(bit_rate))
					{
						rtm::vector4f rotation = raw_rotations.get_raw_sample<rtm::vector4f>(context.segment_sample_start_index + sample_index);
						rotation = convert_rotation(rotation, rotation_format8::quatf_full, rotation_format8::quatf_drop_w_variable);
						pack_vector3_96(rotation, quantized_ptr);
					}
					else
					{
						const rtm::quatf rotation = lossy_rotations.get_raw_sample<rtm::quatf>(sample_index);
						pack_vector3_uXX_unsafe(rtm::quat_to_vector(rotation), num_bits_at_bit_rate, quantized_ptr);
					}
				}
			}

			out_quantized_stream = std::move(quantized_stream);
		}

		inline void quantize_variable_rotation_stream(quantization_context& context, uint32_t bone_index, uint8_t bit_rate)
		{
			ACL_ASSERT(bone_index < context.num_bones, "Invalid bone index: %u", bone_index);

			transform_streams& lossy_track = context.bone_streams[bone_index];

			// Default tracks aren't quantized
			if (lossy_track.is_rotation_default)
				return;

			const transform_streams& raw_track = context.raw_bone_streams[bone_index];
			const rotation_format8 highest_bit_rate = get_highest_variant_precision(rotation_variant8::quat_drop_w);

			// If our format is variable, we keep them fixed at the highest bit rate in the variant
			if (lossy_track.is_rotation_constant)
				quantize_fixed_rotation_stream(context.allocator, lossy_track.rotations, highest_bit_rate, lossy_track.rotations);
			else
				quantize_variable_rotation_stream(context, raw_track, lossy_track, bit_rate, lossy_track.rotations);
		}

		inline void quantize_fixed_translation_stream(iallocator& allocator, const translation_track_stream& raw_stream, vector_format8 translation_format, translation_track_stream& out_quantized_stream)
		{
			// We expect all our samples to have the same width of sizeof(rtm::vector4f)
			ACL_ASSERT(raw_stream.get_sample_size() == sizeof(rtm::vector4f), "Unexpected translation sample size. %u != %zu", raw_stream.get_sample_size(), sizeof(rtm::vector4f));
			ACL_ASSERT(raw_stream.get_vector_format() == vector_format8::vector3f_full, "Expected a vector3f_full vector format, found: " ACL_ASSERT_STRING_FORMAT_SPECIFIER, get_vector_format_name(raw_stream.get_vector_format()));

			const uint32_t num_samples = raw_stream.get_num_samples();
			const uint32_t sample_size = get_packed_vector_size(translation_format);
			const float sample_rate = raw_stream.get_sample_rate();
			translation_track_stream quantized_stream(allocator, num_samples, sample_size, sample_rate, translation_format);

			for (uint32_t sample_index = 0; sample_index < num_samples; ++sample_index)
			{
				const rtm::vector4f translation = raw_stream.get_raw_sample<rtm::vector4f>(sample_index);
				uint8_t* quantized_ptr = quantized_stream.get_raw_sample_ptr(sample_index);

				switch (translation_format)
				{
				case vector_format8::vector3f_full:
					pack_vector3_96(translation, quantized_ptr);
					break;
				case vector_format8::vector3f_variable:
				default:
					ACL_ASSERT(false, "Invalid or unsupported vector format: " ACL_ASSERT_STRING_FORMAT_SPECIFIER, get_vector_format_name(translation_format));
					break;
				}
			}

			out_quantized_stream = std::move(quantized_stream);
		}

		inline void quantize_fixed_translation_stream(quantization_context& context, uint32_t bone_index, vector_format8 translation_format)
		{
			ACL_ASSERT(bone_index < context.num_bones, "Invalid bone index: %u", bone_index);

			transform_streams& bone_stream = context.bone_streams[bone_index];

			// Default tracks aren't quantized
			if (bone_stream.is_translation_default)
				return;

			// Constant translation tracks store the remaining sample with full precision
			const vector_format8 format = bone_stream.is_translation_constant ? vector_format8::vector3f_full : translation_format;

			quantize_fixed_translation_stream(context.allocator, bone_stream.translations, format, bone_stream.translations);
		}

		inline void quantize_variable_translation_stream(quantization_context& context, const transform_streams& raw_track, const transform_streams& lossy_track, uint8_t bit_rate, translation_track_stream& out_quantized_stream)
		{
			const translation_track_stream& raw_translations = raw_track.translations;
			const translation_track_stream& lossy_translations = lossy_track.translations;

			// We expect all our samples to have the same width of sizeof(rtm::vector4f)
			ACL_ASSERT(lossy_translations.get_sample_size() == sizeof(rtm::vector4f), "Unexpected translation sample size. %u != %zu", lossy_translations.get_sample_size(), sizeof(rtm::vector4f));
			ACL_ASSERT(lossy_translations.get_vector_format() == vector_format8::vector3f_full, "Expected a vector3f_full vector format, found: " ACL_ASSERT_STRING_FORMAT_SPECIFIER, get_vector_format_name(lossy_translations.get_vector_format()));

			const uint32_t num_samples = is_constant_bit_rate(bit_rate) ? 1 : lossy_translations.get_num_samples();
			const uint32_t sample_size = sizeof(uint64_t) * 2;
			const float sample_rate = lossy_translations.get_sample_rate();
			translation_track_stream quantized_stream(context.allocator, num_samples, sample_size, sample_rate, vector_format8::vector3f_variable, bit_rate);

			if (is_constant_bit_rate(bit_rate))
			{
#if defined(ACL_IMPL_ENABLE_WEIGHTED_AVERAGE_CONSTANT_SUB_TRACKS)
				const track_stream_range& bone_range = context.segment->ranges[lossy_track.bone_index].translation;
				const rtm::vector4f normalized_translation = clip_range.get_weighted_average();
#else
				const rtm::vector4f normalized_translation = lossy_track.constant_translation;
#endif

				uint8_t* quantized_ptr = quantized_stream.get_raw_sample_ptr(0);
				pack_vector3_u48_unsafe(normalized_translation, quantized_ptr);
			}
			else
			{
				const uint32_t num_bits_at_bit_rate = get_num_bits_at_bit_rate(bit_rate);

				for (uint32_t sample_index = 0; sample_index < num_samples; ++sample_index)
				{
					uint8_t* quantized_ptr = quantized_stream.get_raw_sample_ptr(sample_index);

					if (is_raw_bit_rate(bit_rate))
					{
						const rtm::vector4f translation = raw_translations.get_raw_sample<rtm::vector4f>(context.segment_sample_start_index + sample_index);
						pack_vector3_96(translation, quantized_ptr);
					}
					else
					{
						const rtm::vector4f translation = lossy_translations.get_raw_sample<rtm::vector4f>(sample_index);
						pack_vector3_uXX_unsafe(translation, num_bits_at_bit_rate, quantized_ptr);
					}
				}
			}

			out_quantized_stream = std::move(quantized_stream);
		}

		inline void quantize_variable_translation_stream(quantization_context& context, uint32_t bone_index, uint8_t bit_rate)
		{
			ACL_ASSERT(bone_index < context.num_bones, "Invalid bone index: %u", bone_index);

			transform_streams& lossy_track = context.bone_streams[bone_index];

			// Default tracks aren't quantized
			if (lossy_track.is_translation_default)
				return;

			const transform_streams& raw_track = context.raw_bone_streams[bone_index];

			// Constant translation tracks store the remaining sample with full precision
			if (lossy_track.is_translation_constant)
				quantize_fixed_translation_stream(context.allocator, lossy_track.translations, vector_format8::vector3f_full, lossy_track.translations);
			else
				quantize_variable_translation_stream(context, raw_track, lossy_track, bit_rate, lossy_track.translations);
		}

		inline void quantize_fixed_scale_stream(iallocator& allocator, const scale_track_stream& raw_stream, vector_format8 scale_format, scale_track_stream& out_quantized_stream)
		{
			// We expect all our samples to have the same width of sizeof(rtm::vector4f)
			ACL_ASSERT(raw_stream.get_sample_size() == sizeof(rtm::vector4f), "Unexpected scale sample size. %u != %zu", raw_stream.get_sample_size(), sizeof(rtm::vector4f));
			ACL_ASSERT(raw_stream.get_vector_format() == vector_format8::vector3f_full, "Expected a vector3f_full vector format, found: " ACL_ASSERT_STRING_FORMAT_SPECIFIER, get_vector_format_name(raw_stream.get_vector_format()));

			const uint32_t num_samples = raw_stream.get_num_samples();
			const uint32_t sample_size = get_packed_vector_size(scale_format);
			const float sample_rate = raw_stream.get_sample_rate();
			scale_track_stream quantized_stream(allocator, num_samples, sample_size, sample_rate, scale_format);

			for (uint32_t sample_index = 0; sample_index < num_samples; ++sample_index)
			{
				const rtm::vector4f scale = raw_stream.get_raw_sample<rtm::vector4f>(sample_index);
				uint8_t* quantized_ptr = quantized_stream.get_raw_sample_ptr(sample_index);

				switch (scale_format)
				{
				case vector_format8::vector3f_full:
					pack_vector3_96(scale, quantized_ptr);
					break;
				case vector_format8::vector3f_variable:
				default:
					ACL_ASSERT(false, "Invalid or unsupported vector format: " ACL_ASSERT_STRING_FORMAT_SPECIFIER, get_vector_format_name(scale_format));
					break;
				}
			}

			out_quantized_stream = std::move(quantized_stream);
		}

		inline void quantize_fixed_scale_stream(quantization_context& context, uint32_t bone_index, vector_format8 scale_format)
		{
			ACL_ASSERT(bone_index < context.num_bones, "Invalid bone index: %u", bone_index);

			transform_streams& bone_stream = context.bone_streams[bone_index];

			// Default tracks aren't quantized
			if (bone_stream.is_scale_default)
				return;

			// Constant scale tracks store the remaining sample with full precision
			const vector_format8 format = bone_stream.is_scale_constant ? vector_format8::vector3f_full : scale_format;

			quantize_fixed_scale_stream(context.allocator, bone_stream.scales, format, bone_stream.scales);
		}

		inline void quantize_variable_scale_stream(quantization_context& context, const transform_streams& raw_track, const transform_streams& lossy_track, uint8_t bit_rate, scale_track_stream& out_quantized_stream)
		{
			const scale_track_stream& raw_scales = raw_track.scales;
			const scale_track_stream& lossy_scales = lossy_track.scales;

			// We expect all our samples to have the same width of sizeof(rtm::vector4f)
			ACL_ASSERT(lossy_scales.get_sample_size() == sizeof(rtm::vector4f), "Unexpected scale sample size. %u != %zu", lossy_scales.get_sample_size(), sizeof(rtm::vector4f));
			ACL_ASSERT(lossy_scales.get_vector_format() == vector_format8::vector3f_full, "Expected a vector3f_full vector format, found: " ACL_ASSERT_STRING_FORMAT_SPECIFIER, get_vector_format_name(lossy_scales.get_vector_format()));

			const uint32_t num_samples = is_constant_bit_rate(bit_rate) ? 1 : lossy_scales.get_num_samples();
			const uint32_t sample_size = sizeof(uint64_t) * 2;
			const float sample_rate = lossy_scales.get_sample_rate();
			scale_track_stream quantized_stream(context.allocator, num_samples, sample_size, sample_rate, vector_format8::vector3f_variable, bit_rate);

			if (is_constant_bit_rate(bit_rate))
			{
#if defined(ACL_IMPL_ENABLE_WEIGHTED_AVERAGE_CONSTANT_SUB_TRACKS)
				const track_stream_range& bone_range = context.segment->ranges[lossy_track.bone_index].scale;
				const rtm::vector4f normalized_scale = clip_range.get_weighted_average();
#else
				const rtm::vector4f normalized_scale = lossy_track.constant_scale;
#endif

				uint8_t* quantized_ptr = quantized_stream.get_raw_sample_ptr(0);
				pack_vector3_u48_unsafe(normalized_scale, quantized_ptr);
			}
			else
			{
				const uint32_t num_bits_at_bit_rate = get_num_bits_at_bit_rate(bit_rate);

				for (uint32_t sample_index = 0; sample_index < num_samples; ++sample_index)
				{
					uint8_t* quantized_ptr = quantized_stream.get_raw_sample_ptr(sample_index);

					if (is_raw_bit_rate(bit_rate))
					{
						const rtm::vector4f scale = raw_scales.get_raw_sample<rtm::vector4f>(context.segment_sample_start_index + sample_index);
						pack_vector3_96(scale, quantized_ptr);
					}
					else
					{
						const rtm::vector4f scale = lossy_scales.get_raw_sample<rtm::vector4f>(sample_index);
						pack_vector3_uXX_unsafe(scale, num_bits_at_bit_rate, quantized_ptr);
					}
				}
			}

			out_quantized_stream = std::move(quantized_stream);
		}

		inline void quantize_variable_scale_stream(quantization_context& context, uint32_t bone_index, uint8_t bit_rate)
		{
			ACL_ASSERT(bone_index < context.num_bones, "Invalid bone index: %u", bone_index);

			transform_streams& lossy_track = context.bone_streams[bone_index];

			// Default tracks aren't quantized
			if (lossy_track.is_scale_default)
				return;

			const transform_streams& raw_track = context.raw_bone_streams[bone_index];

			// Constant scale tracks store the remaining sample with full precision
			if (lossy_track.is_scale_constant)
				quantize_fixed_scale_stream(context.allocator, lossy_track.scales, vector_format8::vector3f_full, lossy_track.scales);
			else
				quantize_variable_scale_stream(context, raw_track, lossy_track, bit_rate, lossy_track.scales);
		}

		enum class error_scan_stop_condition { until_error_too_high, until_end_of_segment };

		inline float calculate_max_error_at_bit_rate_local(quantization_context& context, uint32_t target_bone_index, error_scan_stop_condition stop_condition)
		{
			const itransform_error_metric* error_metric = context.error_metric;
			const bool needs_conversion = context.needs_conversion;
			const bool has_additive_base = context.has_additive_base;

			const uint32_t num_transforms = context.num_bones;
			const size_t sample_transform_size = context.metric_transform_size * context.num_bones;
			const float sample_rate = context.sample_rate;
			const float clip_duration = context.clip_duration;

			const rigid_shell_metadata_t& transform_shell = context.shell_metadata_per_transform[target_bone_index];
			const rtm::scalarf error_threshold_sq = rtm::scalar_set(transform_shell.precision * transform_shell.precision);

			const auto convert_transforms_impl = std::mem_fn(context.has_scale ? &itransform_error_metric::convert_transforms : &itransform_error_metric::convert_transforms_no_scale);
			const auto apply_additive_to_base_impl = std::mem_fn(context.has_scale ? &itransform_error_metric::apply_additive_to_base : &itransform_error_metric::apply_additive_to_base_no_scale);
			const auto calculate_error_squared_impl = std::mem_fn(context.has_scale ? &itransform_error_metric::calculate_error_squared : &itransform_error_metric::calculate_error_squared_no_scale);

			itransform_error_metric::convert_transforms_args convert_transforms_args_lossy;
			convert_transforms_args_lossy.dirty_transform_indices = &target_bone_index;
			convert_transforms_args_lossy.num_dirty_transforms = 1;
			convert_transforms_args_lossy.transforms = context.lossy_local_pose;
			convert_transforms_args_lossy.num_transforms = num_transforms;
			convert_transforms_args_lossy.sample_index = 0;
			convert_transforms_args_lossy.is_lossy = true;
			convert_transforms_args_lossy.is_additive_base = false;

			itransform_error_metric::apply_additive_to_base_args apply_additive_to_base_args_lossy;
			apply_additive_to_base_args_lossy.dirty_transform_indices = &target_bone_index;
			apply_additive_to_base_args_lossy.num_dirty_transforms = 1;
			apply_additive_to_base_args_lossy.local_transforms = needs_conversion ? (const void*)context.local_transforms_converted : (const void*)context.lossy_local_pose;
			apply_additive_to_base_args_lossy.base_transforms = nullptr;
			apply_additive_to_base_args_lossy.num_transforms = num_transforms;

			itransform_error_metric::calculate_error_args calculate_error_args;
			calculate_error_args.transform0 = nullptr;
			calculate_error_args.transform1 = needs_conversion ? (const void*)(context.local_transforms_converted + (context.metric_transform_size * target_bone_index)) : (const void*)(context.lossy_local_pose + target_bone_index);
			calculate_error_args.construct_sphere_shell(transform_shell.local_shell_distance);

			const uint8_t* raw_transform = context.raw_local_transforms + (target_bone_index * context.metric_transform_size);
			const uint8_t* base_transforms = context.base_local_transforms;

			context.local_query.build(target_bone_index, context.bit_rate_per_bone[target_bone_index]);

			float sample_indexf = float(context.segment_sample_start_index);
			rtm::scalarf max_error_sq = rtm::scalar_set(0.0F);

			for (uint32_t sample_index = 0; sample_index < context.num_samples; ++sample_index)
			{
				// Sample our streams and calculate the error
				// The sample time is calculated from the full clip duration to be consistent with decompression
				const float sample_time = rtm::scalar_min(sample_indexf / sample_rate, clip_duration);

				context.bit_rate_database.sample(context.local_query, sample_time, context.lossy_local_pose, num_transforms);

				if (needs_conversion)
				{
					convert_transforms_args_lossy.sample_index = sample_index;
					convert_transforms_impl(error_metric, convert_transforms_args_lossy, context.local_transforms_converted);
				}

				if (has_additive_base)
				{
					apply_additive_to_base_args_lossy.base_transforms = base_transforms;
					base_transforms += sample_transform_size;

					// TODO: Is this accurate if we have conversion? Our input is in the converted array for base/local
					//       and we write to the local qvvf buffer? The calculate error below will read from the converted array
					//       if we are converted.
					apply_additive_to_base_impl(error_metric, apply_additive_to_base_args_lossy, context.lossy_local_pose);
				}

				calculate_error_args.transform0 = raw_transform;
				raw_transform += sample_transform_size;

#if defined(RTM_COMPILER_MSVC) && defined(RTM_ARCH_X86) && RTM_COMPILER_MSVC == RTM_COMPILER_MSVC_2015
				// VS2015 fails to generate the right x86 assembly, branch instead
				(void)calculate_error_squared_impl;
				const rtm::scalarf error_sq = context.has_scale ? error_metric->calculate_error_squared(calculate_error_args) : error_metric->calculate_error_squared_no_scale(calculate_error_args);
#else
				const rtm::scalarf error_sq = calculate_error_squared_impl(error_metric, calculate_error_args);
#endif

				max_error_sq = rtm::scalar_max(max_error_sq, error_sq);
				if (stop_condition == error_scan_stop_condition::until_error_too_high && rtm::scalar_greater_equal(error_sq, error_threshold_sq))
					break;

				sample_indexf += 1.0F;
			}

			return rtm::scalar_cast(rtm::scalar_sqrt(max_error_sq));
		}

		inline float calculate_max_error_at_bit_rate_object(quantization_context& context, uint32_t target_bone_index, error_scan_stop_condition stop_condition, bool use_dominance = true)
		{
			const itransform_error_metric* error_metric = context.error_metric;
			const bool needs_conversion = context.needs_conversion;
			const bool has_additive_base = context.has_additive_base;

			const size_t sample_transform_size = context.metric_transform_size * context.num_bones;
			const float sample_rate = context.sample_rate;
			const float clip_duration = context.clip_duration;

			rtm::scalarf error_threshold;
			float shell_distance;
			if (use_dominance)
			{
				const rigid_shell_metadata_t& transform_shell = context.shell_metadata_per_transform[target_bone_index];

				shell_distance = transform_shell.local_shell_distance;
				error_threshold = rtm::scalar_set(transform_shell.precision);
			}
			else
			{
				shell_distance = context.metadata[target_bone_index].shell_distance;
				error_threshold = rtm::scalar_set(context.metadata[target_bone_index].precision);
			}

			const rtm::scalarf error_threshold_sq = rtm::scalar_mul(error_threshold, error_threshold);

			const auto convert_transforms_impl = std::mem_fn(context.has_scale ? &itransform_error_metric::convert_transforms : &itransform_error_metric::convert_transforms_no_scale);
			const auto apply_additive_to_base_impl = std::mem_fn(context.has_scale ? &itransform_error_metric::apply_additive_to_base : &itransform_error_metric::apply_additive_to_base_no_scale);
			const auto local_to_object_space_impl = std::mem_fn(context.has_scale ? &itransform_error_metric::local_to_object_space : &itransform_error_metric::local_to_object_space_no_scale);
			const auto calculate_error_squared_impl = std::mem_fn(context.has_scale ? &itransform_error_metric::calculate_error_squared : &itransform_error_metric::calculate_error_squared_no_scale);

			itransform_error_metric::convert_transforms_args convert_transforms_args_lossy;
			convert_transforms_args_lossy.dirty_transform_indices = context.chain_bone_indices;
			convert_transforms_args_lossy.num_dirty_transforms = context.num_bones_in_chain;
			convert_transforms_args_lossy.transforms = context.lossy_local_pose;
			convert_transforms_args_lossy.num_transforms = context.num_bones;
			convert_transforms_args_lossy.sample_index = 0;
			convert_transforms_args_lossy.is_lossy = true;
			convert_transforms_args_lossy.is_additive_base = false;

			itransform_error_metric::apply_additive_to_base_args apply_additive_to_base_args_lossy;
			apply_additive_to_base_args_lossy.dirty_transform_indices = context.chain_bone_indices;
			apply_additive_to_base_args_lossy.num_dirty_transforms = context.num_bones_in_chain;
			apply_additive_to_base_args_lossy.local_transforms = needs_conversion ? (const void*)(context.local_transforms_converted) : (const void*)context.lossy_local_pose;
			apply_additive_to_base_args_lossy.base_transforms = nullptr;
			apply_additive_to_base_args_lossy.num_transforms = context.num_bones;

			itransform_error_metric::local_to_object_space_args local_to_object_space_args_lossy;
			local_to_object_space_args_lossy.dirty_transform_indices = context.chain_bone_indices;
			local_to_object_space_args_lossy.num_dirty_transforms = context.num_bones_in_chain;
			local_to_object_space_args_lossy.parent_transform_indices = context.parent_transform_indices;
			local_to_object_space_args_lossy.local_transforms = needs_conversion ? (const void*)(context.local_transforms_converted) : (const void*)context.lossy_local_pose;
			local_to_object_space_args_lossy.num_transforms = context.num_bones;

			itransform_error_metric::calculate_error_args calculate_error_args;
			calculate_error_args.transform0 = nullptr;
			calculate_error_args.transform1 = context.lossy_object_pose + (target_bone_index * context.metric_transform_size);
			calculate_error_args.construct_sphere_shell(shell_distance);

			const uint8_t* raw_transform = context.raw_object_transforms + (target_bone_index * context.metric_transform_size);
			const uint8_t* base_transforms = context.base_local_transforms;

			context.object_query.build(target_bone_index, context.bit_rate_per_bone, context.bone_streams);

			float sample_indexf = float(context.segment_sample_start_index);
			rtm::scalarf max_error_sq = rtm::scalar_set(0.0F);

			for (uint32_t sample_index = 0; sample_index < context.num_samples; ++sample_index)
			{
				// Sample our streams and calculate the error
				// The sample time is calculated from the full clip duration to be consistent with decompression
				const float sample_time = rtm::scalar_min(sample_indexf / sample_rate, clip_duration);

				context.bit_rate_database.sample(context.object_query, sample_time, context.lossy_local_pose, context.num_bones);

				if (needs_conversion)
				{
					convert_transforms_args_lossy.sample_index = sample_index;
					convert_transforms_impl(error_metric, convert_transforms_args_lossy, context.local_transforms_converted);
				}

				if (has_additive_base)
				{
					apply_additive_to_base_args_lossy.base_transforms = base_transforms;
					base_transforms += sample_transform_size;

					// TODO: Is this accurate if we have conversion? Our input is in the converted array for base/local
					//       and we write to the local qvvf buffer? The calculate error below will read from the converted array
					//       if we are converted.
					apply_additive_to_base_impl(error_metric, apply_additive_to_base_args_lossy, context.lossy_local_pose);
				}

				local_to_object_space_impl(error_metric, local_to_object_space_args_lossy, context.lossy_object_pose);

				calculate_error_args.transform0 = raw_transform;
				raw_transform += sample_transform_size;

#if defined(RTM_COMPILER_MSVC) && defined(RTM_ARCH_X86) && RTM_COMPILER_MSVC == RTM_COMPILER_MSVC_2015
				// VS2015 fails to generate the right x86 assembly, branch instead
				(void)calculate_error_squared_impl;
				const rtm::scalarf error_sq = context.has_scale ? error_metric->calculate_error_squared(calculate_error_args) : error_metric->calculate_error_squared_no_scale(calculate_error_args);
#else
				const rtm::scalarf error_sq = calculate_error_squared_impl(error_metric, calculate_error_args);
#endif

				max_error_sq = rtm::scalar_max(max_error_sq, error_sq);
				if (stop_condition == error_scan_stop_condition::until_error_too_high && rtm::scalar_greater_equal(error_sq, error_threshold_sq))
					break;

				sample_indexf += 1.0F;
			}

			return rtm::scalar_cast(rtm::scalar_sqrt(max_error_sq));
		}

#if ACL_IMPL_DEBUG_VARIABLE_QUANTIZATION >= ACL_IMPL_DEBUG_LEVEL_SUMMARY_ONLY
		// For algorithm from ACL 2.2 and later (for debugging only)
		inline float calculate_max_error_at_bit_rate_object(
			quantization_context& context, uint32_t transform_index_to_measure,
			const uint32_t* chain_transform_indices, uint32_t num_transforms_in_chain)
		{
			const itransform_error_metric* error_metric = context.error_metric;
			const bool needs_conversion = context.needs_conversion;
			const bool has_additive_base = context.has_additive_base;

			const size_t sample_transform_size = context.metric_transform_size * context.num_bones;
			const float sample_rate = context.sample_rate;
			const float clip_duration = context.clip_duration;

			const float shell_distance = context.metadata[transform_index_to_measure].shell_distance;

			// Because we are measuring for debugging purposes, we assume we have scale
			// This ensures we are consistent in our reporting with the final error measured post-compression
			// which always accounts for scale
			const bool has_scale = true;	// context.has_scale;

			const auto convert_transforms_impl = std::mem_fn(has_scale ? &itransform_error_metric::convert_transforms : &itransform_error_metric::convert_transforms_no_scale);
			const auto apply_additive_to_base_impl = std::mem_fn(has_scale ? &itransform_error_metric::apply_additive_to_base : &itransform_error_metric::apply_additive_to_base_no_scale);
			const auto local_to_object_space_impl = std::mem_fn(has_scale ? &itransform_error_metric::local_to_object_space : &itransform_error_metric::local_to_object_space_no_scale);
			const auto calculate_error_squared_impl = std::mem_fn(has_scale ? &itransform_error_metric::calculate_error_squared : &itransform_error_metric::calculate_error_squared_no_scale);

			itransform_error_metric::convert_transforms_args convert_transforms_args_lossy;
			convert_transforms_args_lossy.dirty_transform_indices = chain_transform_indices;
			convert_transforms_args_lossy.num_dirty_transforms = num_transforms_in_chain;
			convert_transforms_args_lossy.transforms = context.lossy_local_pose;
			convert_transforms_args_lossy.num_transforms = context.num_bones;
			convert_transforms_args_lossy.sample_index = 0;
			convert_transforms_args_lossy.is_lossy = true;
			convert_transforms_args_lossy.is_additive_base = false;

			itransform_error_metric::apply_additive_to_base_args apply_additive_to_base_args_lossy;
			apply_additive_to_base_args_lossy.dirty_transform_indices = chain_transform_indices;
			apply_additive_to_base_args_lossy.num_dirty_transforms = num_transforms_in_chain;
			apply_additive_to_base_args_lossy.local_transforms = needs_conversion ? (const void*)(context.local_transforms_converted) : (const void*)context.lossy_local_pose;
			apply_additive_to_base_args_lossy.base_transforms = nullptr;
			apply_additive_to_base_args_lossy.num_transforms = context.num_bones;

			itransform_error_metric::local_to_object_space_args local_to_object_space_args_lossy;
			local_to_object_space_args_lossy.dirty_transform_indices = chain_transform_indices;
			local_to_object_space_args_lossy.num_dirty_transforms = num_transforms_in_chain;
			local_to_object_space_args_lossy.parent_transform_indices = context.parent_transform_indices;
			local_to_object_space_args_lossy.local_transforms = needs_conversion ? (const void*)(context.local_transforms_converted) : (const void*)context.lossy_local_pose;
			local_to_object_space_args_lossy.num_transforms = context.num_bones;

			itransform_error_metric::calculate_error_args calculate_error_args;
			calculate_error_args.transform0 = nullptr;
			calculate_error_args.transform1 = context.lossy_object_pose + (transform_index_to_measure * context.metric_transform_size);
			calculate_error_args.construct_sphere_shell(shell_distance);

			const uint8_t* raw_transform = context.raw_object_transforms + (transform_index_to_measure * context.metric_transform_size);
			const uint8_t* base_transforms = context.base_local_transforms;

			context.object_query.build(transform_index_to_measure, context.bit_rate_per_bone, context.bone_streams);

			float sample_indexf = float(context.segment_sample_start_index);
			rtm::scalarf max_error_sq = rtm::scalar_set(0.0F);

			for (uint32_t sample_index = 0; sample_index < context.num_samples; ++sample_index)
			{
				// Sample our streams and calculate the error
				// The sample time is calculated from the full clip duration to be consistent with decompression
				const float sample_time = rtm::scalar_min(sample_indexf / sample_rate, clip_duration);

				context.bit_rate_database.sample(context.object_query, sample_time, context.lossy_local_pose, context.num_bones);

				if (needs_conversion)
				{
					convert_transforms_args_lossy.sample_index = sample_index;
					convert_transforms_impl(error_metric, convert_transforms_args_lossy, context.local_transforms_converted);
				}

				if (has_additive_base)
				{
					apply_additive_to_base_args_lossy.base_transforms = base_transforms;
					base_transforms += sample_transform_size;

					// TODO: Is this accurate if we have conversion? Our input is in the converted array for base/local
					//       and we write to the local qvvf buffer? The calculate error below will read from the converted array
					//       if we are converted.
					apply_additive_to_base_impl(error_metric, apply_additive_to_base_args_lossy, context.lossy_local_pose);
				}

				local_to_object_space_impl(error_metric, local_to_object_space_args_lossy, context.lossy_object_pose);

				calculate_error_args.transform0 = raw_transform;
				raw_transform += sample_transform_size;

#if defined(RTM_COMPILER_MSVC) && defined(RTM_ARCH_X86) && RTM_COMPILER_MSVC == RTM_COMPILER_MSVC_2015
				// VS2015 fails to generate the right x86 assembly, branch instead
				(void)calculate_error_squared_impl;
				const rtm::scalarf error_sq = has_scale ? error_metric->calculate_error_squared(calculate_error_args) : error_metric->calculate_error_squared_no_scale(calculate_error_args);
#else
				const rtm::scalarf error_sq = calculate_error_squared_impl(error_metric, calculate_error_args);
#endif

				max_error_sq = rtm::scalar_max(max_error_sq, error_sq);
				sample_indexf += 1.0F;
			}

			return rtm::scalar_cast(rtm::scalar_sqrt(max_error_sq));
		}
#endif

		// For algorithm from ACL 2.2 and later
		// Used when no 3D scale is present
		inline float calculate_max_error_at_bit_rate_object_cached(
			quantization_context& context,
			uint32_t transform_index_being_optimized, uint32_t transform_index_to_measure,
			const rtm::qvvf* additive_base_local_transforms,
			const rtm::qvvf* object_transforms_raw,
			const rtm::qvvf* cached_transforms_lossy,
			float max_allowed_error)
		{
			const itransform_error_metric* error_metric = context.error_metric;
			const bool has_additive_base = context.has_additive_base;
			const float sample_rate = context.sample_rate;
			const float clip_duration = context.clip_duration;
			const uint32_t num_transforms = context.num_bones;
			const uint32_t num_samples = context.num_samples;
			const additive_clip_format8 additive_format = context.clip.additive_format;
			const uint32_t parent_transform_index = context.topology->transforms[transform_index_being_optimized].parent_index;
			const float shell_distance = context.metadata[transform_index_to_measure].shell_distance;

			itransform_error_metric::calculate_error_args calculate_error_args;
			calculate_error_args.transform0 = nullptr;
			calculate_error_args.transform1 = nullptr;
			calculate_error_args.construct_sphere_shell(shell_distance);

			context.local_query.build(transform_index_being_optimized, context.bit_rate_per_bone[transform_index_being_optimized]);

			float sample_indexf = float(context.segment_sample_start_index);
			rtm::scalarf max_error_sq = rtm::scalar_set(0.0F);
			const rtm::scalarf max_allowed_error_sq_f = rtm::scalar_set(max_allowed_error * max_allowed_error);

			for (uint32_t sample_index = 0; sample_index < num_samples; ++sample_index)
			{
				// Sample our streams and calculate the error
				// The sample time is calculated from the full clip duration to be consistent with decompression
				const float sample_time = rtm::scalar_min(sample_indexf / sample_rate, clip_duration);

				rtm::qvvf local_transform_lossy = context.bit_rate_database.sample(context.local_query, sample_time);

				// Apply our additive onto our base
				if (has_additive_base)
					local_transform_lossy = rtm::qvv_normalize(acl::apply_additive_to_base(additive_format, additive_base_local_transforms[(sample_index * num_transforms) + transform_index_being_optimized], local_transform_lossy));

				// Use our local transform being optimized and convert the transform to measure into object space
				// result = measured_to_transform * transform_being_optimized * transform_to_root
				rtm::qvvf parent_transform_to_root_lossy = rtm::qvv_identity();
				if (parent_transform_index != k_invalid_track_index)
					parent_transform_to_root_lossy = cached_transforms_lossy[(sample_index * num_transforms) + parent_transform_index];

				const rtm::qvvf transform_to_root_lossy = rtm::qvv_normalize(rtm::qvv_mul_no_scale(local_transform_lossy, parent_transform_to_root_lossy));

				rtm::qvvf measured_to_transform_lossy = rtm::qvv_identity();
				if (transform_index_being_optimized != transform_index_to_measure)
					measured_to_transform_lossy = cached_transforms_lossy[(sample_index * num_transforms) + transform_index_to_measure];

				const rtm::qvvf measured_to_root_lossy = rtm::qvv_normalize(rtm::qvv_mul_no_scale(measured_to_transform_lossy, transform_to_root_lossy));
				const rtm::qvvf& measured_to_root_raw = object_transforms_raw[(sample_index * num_transforms) + transform_index_to_measure];

				// Measure the error
				calculate_error_args.transform0 = &measured_to_root_raw;
				calculate_error_args.transform1 = &measured_to_root_lossy;

				const rtm::scalarf error_sq = error_metric->calculate_error_squared_no_scale(calculate_error_args);

				max_error_sq = rtm::scalar_max(max_error_sq, error_sq);
				sample_indexf += 1.0F;

				if (rtm::scalar_greater_equal(error_sq, max_allowed_error_sq_f))
					break;	// The error is too high, early out
			}

			return rtm::scalar_cast(rtm::scalar_sqrt(max_error_sq));
		}

		// For algorithm from ACL 2.2 and later
		// Used when we have 3D scale present
		inline float calculate_max_error_at_bit_rate_object_cached_with_scale(
			quantization_context& context,
			uint32_t transform_index_being_optimized, uint32_t transform_index_to_measure,
			const uint32_t* transform_chain_indices, uint32_t transform_chain_length,
			const rtm::qvvf* additive_base_local_transforms,
			const rtm::qvvf* object_transforms_raw,
			const rtm::qvvf* cached_transforms_lossy,
			float max_allowed_error)
		{
			const itransform_error_metric* error_metric = context.error_metric;
			const bool has_additive_base = context.has_additive_base;
			const float sample_rate = context.sample_rate;
			const float clip_duration = context.clip_duration;
			const uint32_t num_transforms = context.num_bones;
			const uint32_t num_samples = context.num_samples;
			const additive_clip_format8 additive_format = context.clip.additive_format;
			const uint32_t parent_transform_index = context.topology->transforms[transform_index_being_optimized].parent_index;
			const float shell_distance = context.metadata[transform_index_to_measure].shell_distance;

			itransform_error_metric::calculate_error_args calculate_error_args;
			calculate_error_args.transform0 = nullptr;
			calculate_error_args.transform1 = nullptr;
			calculate_error_args.construct_sphere_shell(shell_distance);

			context.local_query.build(transform_index_being_optimized, context.bit_rate_per_bone[transform_index_being_optimized]);

			float sample_indexf = float(context.segment_sample_start_index);
			rtm::scalarf max_error_sq = rtm::scalar_set(0.0F);
			const rtm::scalarf max_allowed_error_sq_f = rtm::scalar_set(max_allowed_error * max_allowed_error);

			for (uint32_t sample_index = 0; sample_index < num_samples; ++sample_index)
			{
				// Sample our streams and calculate the error
				// The sample time is calculated from the full clip duration to be consistent with decompression
				const float sample_time = rtm::scalar_min(sample_indexf / sample_rate, clip_duration);

				rtm::qvvf local_transform_lossy = context.bit_rate_database.sample(context.local_query, sample_time);

				// Apply our additive onto our base
				if (has_additive_base)
					local_transform_lossy = rtm::qvv_normalize(acl::apply_additive_to_base(additive_format, additive_base_local_transforms[(sample_index * num_transforms) + transform_index_being_optimized], local_transform_lossy));

				// Use our local transform being optimized and convert the transform to measure into object space
				// result = measured_to_transform * transform_being_optimized * transform_to_root
				rtm::qvvf parent_transform_to_root_lossy = rtm::qvv_identity();
				if (parent_transform_index != k_invalid_track_index)
					parent_transform_to_root_lossy = cached_transforms_lossy[(sample_index * num_transforms) + parent_transform_index];

				const rtm::qvvf transform_to_root_lossy = rtm::qvv_normalize(rtm::qvv_mul(local_transform_lossy, parent_transform_to_root_lossy));

				// When non-uniform 3D scale is present, our cached transforms after optimization contain the local space transform
				// We cannot leverage associativity to speed up computation, do it manually using the transform chain
				rtm::qvvf measured_to_root_lossy = transform_to_root_lossy;

				// Skip the transform currently being optimized
				ACL_ASSERT(transform_chain_indices[transform_chain_length - 1] == transform_index_being_optimized, "Last chain transform index should be the transform being optimized");
				for (const uint32_t chain_transform_index : make_reverse_iterator(transform_chain_indices, transform_chain_length - 1))
				{
					const rtm::qvvf& chain_transform = cached_transforms_lossy[(sample_index * num_transforms) + chain_transform_index];
					measured_to_root_lossy = rtm::qvv_normalize(rtm::qvv_mul(chain_transform, measured_to_root_lossy));
				}

				const rtm::qvvf& measured_to_root_raw = object_transforms_raw[(sample_index * num_transforms) + transform_index_to_measure];

				// Measure the error
				calculate_error_args.transform0 = &measured_to_root_raw;
				calculate_error_args.transform1 = &measured_to_root_lossy;

				const rtm::scalarf error_sq = error_metric->calculate_error_squared(calculate_error_args);

				max_error_sq = rtm::scalar_max(max_error_sq, error_sq);
				sample_indexf += 1.0F;

				if (rtm::scalar_greater_equal(error_sq, max_allowed_error_sq_f))
					break;	// The error is too high, early out
			}

			return rtm::scalar_cast(rtm::scalar_sqrt(max_error_sq));
		}

		// For algorithm from ACL 2.2 and later
		// Used when no 3D scale is present
		inline void update_cached_transforms(
			quantization_context& context,
			uint32_t transform_index_being_optimized, uint32_t transform_index_to_measure,
			const rtm::qvvf* additive_base_local_transforms,
			rtm::qvvf* cached_transforms_lossy)
		{
			const bool has_additive_base = context.has_additive_base;
			const float sample_rate = context.sample_rate;
			const float clip_duration = context.clip_duration;
			const uint32_t num_transforms = context.num_bones;
			const additive_clip_format8 additive_format = context.clip.additive_format;

			context.local_query.build(transform_index_being_optimized, context.bit_rate_per_bone[transform_index_being_optimized]);

			float sample_indexf = float(context.segment_sample_start_index);

			for (uint32_t sample_index = 0; sample_index < context.num_samples; ++sample_index)
			{
				// Sample our streams and calculate the error
				// The sample time is calculated from the full clip duration to be consistent with decompression
				const float sample_time = rtm::scalar_min(sample_indexf / sample_rate, clip_duration);

				rtm::qvvf local_transform_lossy = context.bit_rate_database.sample(context.local_query, sample_time);

				// Apply our additive onto our base
				if (has_additive_base)
					local_transform_lossy = rtm::qvv_normalize(acl::apply_additive_to_base(additive_format, additive_base_local_transforms[(sample_index * num_transforms) + transform_index_being_optimized], local_transform_lossy));

				// Our cached transforms contains the combined measured_to_transform from some parent transform in the chain up to
				// the transform we measure (e.g. leaf)
				rtm::qvvf measured_to_transform_lossy = rtm::qvv_identity();
				if (transform_index_being_optimized != transform_index_to_measure)
					measured_to_transform_lossy = cached_transforms_lossy[(sample_index * num_transforms) + transform_index_to_measure];

				measured_to_transform_lossy = rtm::qvv_normalize(rtm::qvv_mul_no_scale(measured_to_transform_lossy, local_transform_lossy));

				cached_transforms_lossy[(sample_index * num_transforms) + transform_index_to_measure] = measured_to_transform_lossy;

				sample_indexf += 1.0F;
			}
		}

		// For algorithm from ACL 2.2 and later
		// Used when we have 3D scale present
		inline void update_cached_transforms_with_scale(
			quantization_context& context,
			uint32_t transform_index_being_optimized,
			const rtm::qvvf* additive_base_local_transforms,
			rtm::qvvf* cached_transforms_lossy)
		{
			const bool has_additive_base = context.has_additive_base;
			const float sample_rate = context.sample_rate;
			const float clip_duration = context.clip_duration;
			const uint32_t num_transforms = context.num_bones;
			const additive_clip_format8 additive_format = context.clip.additive_format;

			context.local_query.build(transform_index_being_optimized, context.bit_rate_per_bone[transform_index_being_optimized]);

			float sample_indexf = float(context.segment_sample_start_index);

			for (uint32_t sample_index = 0; sample_index < context.num_samples; ++sample_index)
			{
				// Sample our streams and calculate the error
				// The sample time is calculated from the full clip duration to be consistent with decompression
				const float sample_time = rtm::scalar_min(sample_indexf / sample_rate, clip_duration);

				rtm::qvvf local_transform_lossy = context.bit_rate_database.sample(context.local_query, sample_time);

				// Apply our additive onto our base
				if (has_additive_base)
					local_transform_lossy = rtm::qvv_normalize(acl::apply_additive_to_base(additive_format, additive_base_local_transforms[(sample_index * num_transforms) + transform_index_being_optimized], local_transform_lossy));

				// When non-uniform 3D scale is present, our cached transforms after optimization contain the local space transform
				cached_transforms_lossy[(sample_index * num_transforms) + transform_index_being_optimized] = local_transform_lossy;

				sample_indexf += 1.0F;
			}
		}

		inline void calculate_local_space_bit_rates(quantization_context& context)
		{
			// To minimize the bit rate, we first start by trying every permutation in local space
			// until our error is acceptable.
			// We try permutations from the lowest memory footprint to the highest.

			const uint8_t* const bit_rate_permutations_per_dofs[] =
			{
				&acl_impl::k_local_bit_rate_permutations_1_dof[0][0],
				&acl_impl::k_local_bit_rate_permutations_2_dof[0][0],
				&acl_impl::k_local_bit_rate_permutations_3_dof[0][0],
			};
			const size_t num_bit_rate_permutations_per_dofs[] =
			{
				get_array_size(acl_impl::k_local_bit_rate_permutations_1_dof),
				get_array_size(acl_impl::k_local_bit_rate_permutations_2_dof),
				get_array_size(acl_impl::k_local_bit_rate_permutations_3_dof),
			};

			const uint32_t num_bones = context.num_bones;
			for (uint32_t bone_index = 0; bone_index < num_bones; ++bone_index)
			{
				// Update our error threshold
				const float error_threshold = context.shell_metadata_per_transform[bone_index].precision;

				// Bit rates at this point are one of three value:
				// 0: if the segment track is normalized, it can be constant within the segment
				// 1: if the segment track isn't normalized, it starts at the lowest bit rate
				// 255: if the track is constant/default for the whole clip
				const transform_bit_rates bone_bit_rates = context.bit_rate_per_bone[bone_index];

				if (bone_bit_rates.rotation == k_invalid_bit_rate && bone_bit_rates.translation == k_invalid_bit_rate && bone_bit_rates.scale == k_invalid_bit_rate)
				{
#if ACL_IMPL_DEBUG_VARIABLE_QUANTIZATION >= ACL_IMPL_DEBUG_LEVEL_BASIC_INFO
					printf("%8u: Best bit rates: [%3u, %3u, %3u](  0) (all constant)\n", bone_index, bone_bit_rates.rotation, bone_bit_rates.translation, bone_bit_rates.scale);
#endif
					continue;	// Every track bit rate is constant/default, nothing else to do
				}

				transform_bit_rates best_bit_rates = bone_bit_rates;
				float best_error = 1.0E10F;
				uint32_t prev_transform_size = ~0U;
				bool is_error_good_enough = false;

				// Determine how many degrees of freedom we have to optimize our bit rates
				uint32_t num_dof = 0;
				num_dof += bone_bit_rates.rotation != k_invalid_bit_rate ? 1 : 0;
				num_dof += bone_bit_rates.translation != k_invalid_bit_rate ? 1 : 0;
				num_dof += bone_bit_rates.scale != k_invalid_bit_rate ? 1 : 0;

				const uint8_t* bit_rate_permutations_per_dof = bit_rate_permutations_per_dofs[num_dof - 1];
				const size_t num_bit_rate_permutations = num_bit_rate_permutations_per_dofs[num_dof - 1];

				// Our desired bit rates start with the initial value
				transform_bit_rates desired_bit_rates = bone_bit_rates;

				size_t permutation_offset = 0;
				for (size_t permutation_index = 0; permutation_index < num_bit_rate_permutations; ++permutation_index)
				{
					// If a bit rate is variable, grab a permutation for it
					// We'll only consume as many bit rates as we have degrees of freedom

					uint32_t transform_size = 0;	// In bits

					if (desired_bit_rates.rotation != k_invalid_bit_rate)
					{
						desired_bit_rates.rotation = bit_rate_permutations_per_dof[permutation_offset++];
						transform_size += get_num_bits_at_bit_rate(desired_bit_rates.rotation);
					}

					if (desired_bit_rates.translation != k_invalid_bit_rate)
					{
						desired_bit_rates.translation = bit_rate_permutations_per_dof[permutation_offset++];
						transform_size += get_num_bits_at_bit_rate(desired_bit_rates.translation);
					}

					if (desired_bit_rates.scale != k_invalid_bit_rate)
					{
						desired_bit_rates.scale = bit_rate_permutations_per_dof[permutation_offset++];
						transform_size += get_num_bits_at_bit_rate(desired_bit_rates.scale);
					}

					// If our inputs aren't normalized per segment, we can't store them on 0 bits because we'll have no
					// segment range information. This occurs when we have a single segment. Skip those permutations.
					if (bone_bit_rates.rotation == k_lowest_bit_rate && desired_bit_rates.rotation == 0)
						continue;
					else if (bone_bit_rates.translation == k_lowest_bit_rate && desired_bit_rates.translation == 0)
						continue;
					else if (bone_bit_rates.scale == k_lowest_bit_rate && desired_bit_rates.scale == 0)
						continue;

					// If we already found a permutation that is good enough, we test all the others
					// that have the same size. Once the size changes, we stop.
					if (is_error_good_enough && transform_size != prev_transform_size)
						break;

					prev_transform_size = transform_size;

					context.bit_rate_per_bone[bone_index] = desired_bit_rates;

					const float error = calculate_max_error_at_bit_rate_local(context, bone_index, error_scan_stop_condition::until_error_too_high);

#if ACL_IMPL_DEBUG_VARIABLE_QUANTIZATION >= ACL_IMPL_DEBUG_LEVEL_VERBOSE_INFO
					printf("%u: %u | %u | %u (%u) = %f\n", bone_index, desired_bit_rates.rotation, desired_bit_rates.translation, desired_bit_rates.scale, transform_size, error);
#endif

					if (error < best_error)
					{
						best_error = error;
						best_bit_rates = desired_bit_rates;
						is_error_good_enough = error < error_threshold;
					}
				}

#if ACL_IMPL_DEBUG_VARIABLE_QUANTIZATION >= ACL_IMPL_DEBUG_LEVEL_BASIC_INFO
				printf("%8u: Best bit rates: [%3u, %3u, %3u](%3u) @ %.4f%s (local)\n",
					bone_index, best_bit_rates.rotation, best_bit_rates.translation, best_bit_rates.scale,
					best_bit_rates.get_num_bits(), best_error, is_error_good_enough ? "" : " (too high)");
#endif

				context.bit_rate_per_bone[bone_index] = best_bit_rates;
			}
		}

		constexpr uint32_t increment_and_clamp_bit_rate(uint32_t bit_rate, uint32_t increment)
		{
			// If the bit rate is already above highest (e.g 255 if constant), leave it as is otherwise increment and clamp
			return bit_rate >= k_highest_bit_rate ? bit_rate : std::min<uint32_t>(bit_rate + increment, k_highest_bit_rate);
		}

		inline float increase_bone_bit_rate(quantization_context& context, uint32_t bone_index, uint32_t num_increments, float old_error, transform_bit_rates& out_best_bit_rates)
		{
			const transform_bit_rates bone_bit_rates = context.bit_rate_per_bone[bone_index];
			const uint32_t num_scale_increments = context.has_scale ? num_increments : 0;

			transform_bit_rates best_bit_rates = bone_bit_rates;
			float best_error = old_error;

			for (uint32_t rotation_increment = 0; rotation_increment <= num_increments; ++rotation_increment)
			{
				const uint32_t rotation_bit_rate = increment_and_clamp_bit_rate(bone_bit_rates.rotation, rotation_increment);

				for (uint32_t translation_increment = 0; translation_increment <= num_increments; ++translation_increment)
				{
					const uint32_t translation_bit_rate = increment_and_clamp_bit_rate(bone_bit_rates.translation, translation_increment);

					for (uint32_t scale_increment = 0; scale_increment <= num_scale_increments; ++scale_increment)
					{
						const uint32_t scale_bit_rate = increment_and_clamp_bit_rate(bone_bit_rates.scale, scale_increment);

						if (rotation_increment + translation_increment + scale_increment != num_increments)
						{
							if (scale_bit_rate >= k_highest_bit_rate)
								break;
							else
								continue;
						}

						context.bit_rate_per_bone[bone_index] = transform_bit_rates{ (uint8_t)rotation_bit_rate, (uint8_t)translation_bit_rate, (uint8_t)scale_bit_rate };
						const float error = calculate_max_error_at_bit_rate_object(context, bone_index, error_scan_stop_condition::until_error_too_high);

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

		inline float calculate_bone_permutation_error(quantization_context& context, transform_bit_rates* permutation_bit_rates, uint8_t* bone_chain_permutation, uint32_t bone_index, transform_bit_rates* best_bit_rates, float old_error)
		{
			const float error_threshold = context.shell_metadata_per_transform[bone_index].precision;
			float best_error = old_error;

			do
			{
				// Copy our current bit rates to the permutation rates
				std::memcpy(permutation_bit_rates, context.bit_rate_per_bone, sizeof(transform_bit_rates) * context.num_bones);

				bool is_permutation_valid = false;
				const uint32_t num_bones_in_chain = context.num_bones_in_chain;
				for (uint32_t chain_link_index = 0; chain_link_index < num_bones_in_chain; ++chain_link_index)
				{
					if (bone_chain_permutation[chain_link_index] != 0)
					{
						// Increase bit rate
						const uint32_t chain_bone_index = context.chain_bone_indices[chain_link_index];
						transform_bit_rates chain_bone_best_bit_rates;
						increase_bone_bit_rate(context, chain_bone_index, bone_chain_permutation[chain_link_index], old_error, chain_bone_best_bit_rates);
						is_permutation_valid |= chain_bone_best_bit_rates.rotation != permutation_bit_rates[chain_bone_index].rotation;
						is_permutation_valid |= chain_bone_best_bit_rates.translation != permutation_bit_rates[chain_bone_index].translation;
						is_permutation_valid |= chain_bone_best_bit_rates.scale != permutation_bit_rates[chain_bone_index].scale;
						permutation_bit_rates[chain_bone_index] = chain_bone_best_bit_rates;
					}
				}

				if (!is_permutation_valid)
					continue;	// Couldn't increase any bit rate, skip this permutation

				// Measure error
				std::swap(context.bit_rate_per_bone, permutation_bit_rates);
				const float permutation_error = calculate_max_error_at_bit_rate_object(context, bone_index, error_scan_stop_condition::until_error_too_high);
				std::swap(context.bit_rate_per_bone, permutation_bit_rates);

				if (permutation_error < best_error)
				{
					best_error = permutation_error;
					std::memcpy(best_bit_rates, permutation_bit_rates, sizeof(transform_bit_rates) * context.num_bones);

					if (permutation_error < error_threshold)
						break;
				}
			} while (std::next_permutation(bone_chain_permutation, bone_chain_permutation + context.num_bones_in_chain));

			return best_error;
		}

		inline uint32_t calculate_bone_chain_indices(const clip_context& clip, uint32_t bone_index, uint32_t* out_chain_bone_indices)
		{
			const bone_chain bone_chain = clip.get_bone_chain(bone_index);

			uint32_t num_bones_in_chain = 0;
			for (uint32_t chain_bone_index : bone_chain)
				out_chain_bone_indices[num_bones_in_chain++] = chain_bone_index;

			return num_bones_in_chain;
		}

		inline void cache_raw_transforms_v1(quantization_context& context)
		{
			const itransform_error_metric* error_metric_ = context.error_metric;
			const size_t sample_transform_size = context.metric_transform_size * context.num_bones;

			const uint32_t num_bones = context.num_bones;
			const uint32_t num_samples = context.num_samples;
			const float sample_rate = context.sample_rate;
			const float clip_duration = context.clip_duration;
			const uint32_t segment_sample_start_index = context.segment_sample_start_index;
			const bool has_scale = context.has_scale;
			const bool needs_conversion = context.needs_conversion;
			const bool has_additive_base = context.has_additive_base;

			const auto convert_transforms_impl = std::mem_fn(has_scale ? &itransform_error_metric::convert_transforms : &itransform_error_metric::convert_transforms_no_scale);
			const auto apply_additive_to_base_impl = std::mem_fn(has_scale ? &itransform_error_metric::apply_additive_to_base : &itransform_error_metric::apply_additive_to_base_no_scale);
			const auto local_to_object_space_impl = std::mem_fn(has_scale ? &itransform_error_metric::local_to_object_space : &itransform_error_metric::local_to_object_space_no_scale);

			itransform_error_metric::convert_transforms_args convert_transforms_args_raw;
			convert_transforms_args_raw.dirty_transform_indices = context.self_transform_indices;
			convert_transforms_args_raw.num_dirty_transforms = num_bones;
			convert_transforms_args_raw.transforms = context.raw_local_pose;
			convert_transforms_args_raw.num_transforms = num_bones;
			convert_transforms_args_raw.sample_index = 0;
			convert_transforms_args_raw.is_lossy = false;
			convert_transforms_args_raw.is_additive_base = false;

			itransform_error_metric::convert_transforms_args convert_transforms_args_base = convert_transforms_args_raw;
			convert_transforms_args_base.transforms = context.additive_local_pose;
			convert_transforms_args_base.is_additive_base = true;

			itransform_error_metric::apply_additive_to_base_args apply_additive_to_base_args_raw;
			apply_additive_to_base_args_raw.dirty_transform_indices = context.self_transform_indices;
			apply_additive_to_base_args_raw.num_dirty_transforms = num_bones;
			apply_additive_to_base_args_raw.local_transforms = nullptr;
			apply_additive_to_base_args_raw.base_transforms = nullptr;
			apply_additive_to_base_args_raw.num_transforms = num_bones;

			itransform_error_metric::local_to_object_space_args local_to_object_space_args_raw;
			local_to_object_space_args_raw.dirty_transform_indices = context.self_transform_indices;
			local_to_object_space_args_raw.num_dirty_transforms = num_bones;
			local_to_object_space_args_raw.parent_transform_indices = context.parent_transform_indices;
			local_to_object_space_args_raw.local_transforms = nullptr;
			local_to_object_space_args_raw.num_transforms = num_bones;

			for (uint32_t sample_index = 0; sample_index < num_samples; ++sample_index)
			{
				// Sample our streams and calculate the error
				// The sample time is calculated from the full clip duration to be consistent with decompression
				const float sample_time = rtm::scalar_min(float(segment_sample_start_index + sample_index) / sample_rate, clip_duration);

				sample_streams(context.raw_bone_streams, num_bones, sample_time, context.raw_local_pose);

				uint8_t* sample_raw_local_transforms = context.raw_local_transforms + (sample_index * sample_transform_size);

				if (needs_conversion)
				{
					convert_transforms_args_raw.sample_index = sample_index;
					convert_transforms_impl(error_metric_, convert_transforms_args_raw, sample_raw_local_transforms);
				}
				else
					std::memcpy(sample_raw_local_transforms, context.raw_local_pose, sample_transform_size);

				if (has_additive_base)
				{
					const float normalized_sample_time = context.additive_base_clip.num_samples > 1 ? (sample_time / clip_duration) : 0.0F;
					const float additive_sample_time = context.additive_base_clip.num_samples > 1 ? (normalized_sample_time * context.additive_base_clip.duration) : 0.0F;
					sample_streams(context.additive_base_clip.segments[0].bone_streams, num_bones, additive_sample_time, context.additive_local_pose);

					uint8_t* sample_base_local_transforms = context.base_local_transforms + (sample_index * sample_transform_size);

					if (needs_conversion)
					{
						const uint32_t nearest_base_sample_index = static_cast<uint32_t>(rtm::scalar_round_bankers(normalized_sample_time * float(context.additive_base_clip.num_samples)));
						convert_transforms_args_base.sample_index = nearest_base_sample_index;
						convert_transforms_impl(error_metric_, convert_transforms_args_base, sample_base_local_transforms);
					}
					else
						std::memcpy(sample_base_local_transforms, context.additive_local_pose, sample_transform_size);

					apply_additive_to_base_args_raw.local_transforms = sample_raw_local_transforms;
					apply_additive_to_base_args_raw.base_transforms = sample_base_local_transforms;
					apply_additive_to_base_impl(error_metric_, apply_additive_to_base_args_raw, sample_raw_local_transforms);
				}

				local_to_object_space_args_raw.local_transforms = sample_raw_local_transforms;

				uint8_t* sample_raw_object_transforms = context.raw_object_transforms + (sample_index * sample_transform_size);
				local_to_object_space_impl(error_metric_, local_to_object_space_args_raw, sample_raw_object_transforms);
			}
		}

		// For algorithm from ACL 2.1 and earlier
		inline void initialize_bone_bit_rates_v1(const segment_context& segment, rotation_format8 rotation_format, vector_format8 translation_format, vector_format8 scale_format, transform_bit_rates* out_bit_rate_per_bone)
		{
			const bool is_rotation_variable = is_rotation_format_variable(rotation_format);
			const bool is_translation_variable = is_vector_format_variable(translation_format);
			const bool is_scale_variable = segment_context_has_scale(segment) && is_vector_format_variable(scale_format);

			// If our inputs aren't normalized per segment, we can't store them on 0 bits because we'll have no
			// segment range information. This occurs when we have a single segment.
			const uint8_t k_constant_segment_bit_rate = 0;
			const uint8_t k_lowest_non_constant_bit_rate = k_lowest_bit_rate;

			const uint32_t num_bones = segment.num_bones;
			for (uint32_t bone_index = 0; bone_index < num_bones; ++bone_index)
			{
				transform_bit_rates& bone_bit_rate = out_bit_rate_per_bone[bone_index];

				const bool rotation_supports_constant_tracks = segment.are_rotations_normalized;
				if (is_rotation_variable && !segment.bone_streams[bone_index].is_rotation_constant)
					bone_bit_rate.rotation = rotation_supports_constant_tracks ? k_constant_segment_bit_rate : k_lowest_non_constant_bit_rate;
				else
					bone_bit_rate.rotation = k_invalid_bit_rate;

				const bool translation_supports_constant_tracks = segment.are_translations_normalized;
				if (is_translation_variable && !segment.bone_streams[bone_index].is_translation_constant)
					bone_bit_rate.translation = translation_supports_constant_tracks ? k_constant_segment_bit_rate : k_lowest_non_constant_bit_rate;
				else
					bone_bit_rate.translation = k_invalid_bit_rate;

				const bool scale_supports_constant_tracks = segment.are_scales_normalized;
				if (is_scale_variable && !segment.bone_streams[bone_index].is_scale_constant)
					bone_bit_rate.scale = scale_supports_constant_tracks ? k_constant_segment_bit_rate : k_lowest_non_constant_bit_rate;
				else
					bone_bit_rate.scale = k_invalid_bit_rate;
			}
		}

		// For algorithm from ACL 2.2 and later
		inline void initialize_bone_bit_rates_v2(const segment_context& segment, rotation_format8 rotation_format, vector_format8 translation_format, vector_format8 scale_format, transform_bit_rates* out_bit_rate_per_bone)
		{
			const bool is_rotation_variable = is_rotation_format_variable(rotation_format);
			const bool is_translation_variable = is_vector_format_variable(translation_format);
			const bool is_scale_variable = segment_context_has_scale(segment) && is_vector_format_variable(scale_format);

			const uint32_t num_bones = segment.num_bones;
			for (uint32_t bone_index = 0; bone_index < num_bones; ++bone_index)
			{
				transform_bit_rates& bone_bit_rate = out_bit_rate_per_bone[bone_index];

				// We initialize animated bit rates to the highest value possible (3x float32)

				if (is_rotation_variable && !segment.bone_streams[bone_index].is_rotation_constant)
					bone_bit_rate.rotation = k_highest_bit_rate;
				else
					bone_bit_rate.rotation = k_invalid_bit_rate;

				if (is_translation_variable && !segment.bone_streams[bone_index].is_translation_constant)
					bone_bit_rate.translation = k_highest_bit_rate;
				else
					bone_bit_rate.translation = k_invalid_bit_rate;

				if (is_scale_variable && !segment.bone_streams[bone_index].is_scale_constant)
					bone_bit_rate.scale = k_highest_bit_rate;
				else
					bone_bit_rate.scale = k_invalid_bit_rate;
			}
		}

		inline void quantize_all_streams(quantization_context& context)
		{
			ACL_ASSERT(context.is_valid(), "quantization_context isn't valid");

			const bool is_rotation_variable = is_rotation_format_variable(context.rotation_format);
			const bool is_translation_variable = is_vector_format_variable(context.translation_format);
			const bool is_scale_variable = is_vector_format_variable(context.scale_format);

			for (uint32_t bone_index = 0; bone_index < context.num_bones; ++bone_index)
			{
				const transform_bit_rates& bone_bit_rate = context.bit_rate_per_bone[bone_index];

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

		// For algorithm from ACL 2.1 and earlier
		inline void find_optimal_bit_rates_v1(quantization_context& context)
		{
			ACL_ASSERT(context.is_valid(), "quantization_context isn't valid");

			context.initialize_v1();

			// Cache every raw local/object transforms and the base local transforms since they never change
			cache_raw_transforms_v1(context);

			// Update our shell distances
			compute_segment_shell_distances(*context.segment, context.additive_base_clip, context.shell_metadata_per_transform);

			initialize_bone_bit_rates_v1(*context.segment, context.rotation_format, context.translation_format, context.scale_format, context.bit_rate_per_bone);

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
			transform_bit_rates* permutation_bit_rates = allocate_type_array<transform_bit_rates>(context.allocator, context.num_bones);
			transform_bit_rates* best_permutation_bit_rates = allocate_type_array<transform_bit_rates>(context.allocator, context.num_bones);
			transform_bit_rates* best_bit_rates = allocate_type_array<transform_bit_rates>(context.allocator, context.num_bones);
			std::memcpy(best_bit_rates, context.bit_rate_per_bone, sizeof(transform_bit_rates) * context.num_bones);

			// The algorithm complexity is thus as follows: O(T*N!*S)
			// Where:
			//     T: number of transforms
			//     S: number of samples in segment
			//     N: number of transforms in chain length
			// The compression level controls how much of N! to search.

			// Iterate from the root transforms first
			// I attempted to iterate from leaves first and the memory footprint was severely worse
			const uint32_t num_bones = context.num_bones;
			for (const uint32_t bone_index : context.topology->roots_first_iterator())
			{
				// Update our context with the new bone data
				const float error_threshold = context.shell_metadata_per_transform[bone_index].precision;

				const uint32_t num_bones_in_chain = calculate_bone_chain_indices(context.clip, bone_index, context.chain_bone_indices);
				context.num_bones_in_chain = num_bones_in_chain;

				float error = calculate_max_error_at_bit_rate_object(context, bone_index, error_scan_stop_condition::until_error_too_high);

#if ACL_IMPL_DEBUG_VARIABLE_QUANTIZATION >= ACL_IMPL_DEBUG_LEVEL_BASIC_INFO
				printf("%8u: Bit rates: [%3u, %3u, %3u] @ %.4f%s (object)\n",
					bone_index, context.bit_rate_per_bone[bone_index].rotation, context.bit_rate_per_bone[bone_index].translation, context.bit_rate_per_bone[bone_index].scale,
					error, error < error_threshold ? "" : " (too high)");
#endif

				if (error < error_threshold)
					continue;

				const float initial_error = error;

				while (error >= error_threshold)
				{
					// Generate permutations for up to 3 bit rate increments
					// Perform an exhaustive search of the permutations and pick the best result
					// If our best error is under the threshold, we are done, otherwise we will try again from there
					const float original_error = error;
					float best_error = error;

					// The first permutation increases the bit rate of a single track/bone
					std::fill(bone_chain_permutation, bone_chain_permutation + num_bones, uint8_t(0));
					bone_chain_permutation[num_bones_in_chain - 1] = 1;
					error = calculate_bone_permutation_error(context, permutation_bit_rates, bone_chain_permutation, bone_index, best_permutation_bit_rates, original_error);
					if (error < best_error)
					{
						best_error = error;
						std::memcpy(best_bit_rates, best_permutation_bit_rates, sizeof(transform_bit_rates) * num_bones);

						if (error < error_threshold)
							break;
					}

					if (context.compression_level >= compression_level8::high)
					{
						// The second permutation increases the bit rate of 2 track/bones
						std::fill(bone_chain_permutation, bone_chain_permutation + num_bones, uint8_t(0));
						bone_chain_permutation[num_bones_in_chain - 1] = 2;
						error = calculate_bone_permutation_error(context, permutation_bit_rates, bone_chain_permutation, bone_index, best_permutation_bit_rates, original_error);
						if (error < best_error)
						{
							best_error = error;
							std::memcpy(best_bit_rates, best_permutation_bit_rates, sizeof(transform_bit_rates) * num_bones);

							if (error < error_threshold)
								break;
						}

						if (num_bones_in_chain > 1)
						{
							std::fill(bone_chain_permutation, bone_chain_permutation + num_bones, uint8_t(0));
							bone_chain_permutation[num_bones_in_chain - 2] = 1;
							bone_chain_permutation[num_bones_in_chain - 1] = 1;
							error = calculate_bone_permutation_error(context, permutation_bit_rates, bone_chain_permutation, bone_index, best_permutation_bit_rates, original_error);
							if (error < best_error)
							{
								best_error = error;
								std::memcpy(best_bit_rates, best_permutation_bit_rates, sizeof(transform_bit_rates) * num_bones);

								if (error < error_threshold)
									break;
							}
						}
					}

					if (context.compression_level >= compression_level8::highest)
					{
						// The third permutation increases the bit rate of 3 track/bones
						std::fill(bone_chain_permutation, bone_chain_permutation + num_bones, uint8_t(0));
						bone_chain_permutation[num_bones_in_chain - 1] = 3;
						error = calculate_bone_permutation_error(context, permutation_bit_rates, bone_chain_permutation, bone_index, best_permutation_bit_rates, original_error);
						if (error < best_error)
						{
							best_error = error;
							std::memcpy(best_bit_rates, best_permutation_bit_rates, sizeof(transform_bit_rates) * num_bones);

							if (error < error_threshold)
								break;
						}

						if (num_bones_in_chain > 1)
						{
							std::fill(bone_chain_permutation, bone_chain_permutation + num_bones, uint8_t(0));
							bone_chain_permutation[num_bones_in_chain - 2] = 2;
							bone_chain_permutation[num_bones_in_chain - 1] = 1;
							error = calculate_bone_permutation_error(context, permutation_bit_rates, bone_chain_permutation, bone_index, best_permutation_bit_rates, original_error);
							if (error < best_error)
							{
								best_error = error;
								std::memcpy(best_bit_rates, best_permutation_bit_rates, sizeof(transform_bit_rates) * num_bones);

								if (error < error_threshold)
									break;
							}

							if (num_bones_in_chain > 2)
							{
								std::fill(bone_chain_permutation, bone_chain_permutation + num_bones, uint8_t(0));
								bone_chain_permutation[num_bones_in_chain - 3] = 1;
								bone_chain_permutation[num_bones_in_chain - 2] = 1;
								bone_chain_permutation[num_bones_in_chain - 1] = 1;
								error = calculate_bone_permutation_error(context, permutation_bit_rates, bone_chain_permutation, bone_index, best_permutation_bit_rates, original_error);
								if (error < best_error)
								{
									best_error = error;
									std::memcpy(best_bit_rates, best_permutation_bit_rates, sizeof(transform_bit_rates) * num_bones);

									if (error < error_threshold)
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
#if ACL_IMPL_DEBUG_VARIABLE_QUANTIZATION >= ACL_IMPL_DEBUG_LEVEL_BASIC_INFO
						std::swap(context.bit_rate_per_bone, best_bit_rates);
						float new_error = calculate_max_error_at_bit_rate_object(context, bone_index, error_scan_stop_condition::until_end_of_segment);
						std::swap(context.bit_rate_per_bone, best_bit_rates);

						for (uint32_t i = 0; i < num_bones; ++i)
						{
							const transform_bit_rates& bone_bit_rate = context.bit_rate_per_bone[i];
							const transform_bit_rates& best_bone_bit_rate = best_bit_rates[i];
							const bool rotation_differs = bone_bit_rate.rotation != best_bone_bit_rate.rotation;
							const bool translation_differs = bone_bit_rate.translation != best_bone_bit_rate.translation;
							const bool scale_differs = bone_bit_rate.scale != best_bone_bit_rate.scale;
							if (rotation_differs || translation_differs || scale_differs)
							{
								printf("%8u: [%3u, %3u, %3u] @ %.4f => [%3u, %3u, %3u] @ %.4f%s (new permutation)\n",
									i, bone_bit_rate.rotation, bone_bit_rate.translation, bone_bit_rate.scale,
									original_error, best_bone_bit_rate.rotation, best_bone_bit_rate.translation, best_bone_bit_rate.scale,
									new_error, new_error < error_threshold ? "" : " (too high)");
							}
						}
#endif

						std::memcpy(context.bit_rate_per_bone, best_bit_rates, sizeof(transform_bit_rates) * num_bones);
					}
				}

				if (error < initial_error)
				{
#if ACL_IMPL_DEBUG_VARIABLE_QUANTIZATION >= ACL_IMPL_DEBUG_LEVEL_BASIC_INFO
					std::swap(context.bit_rate_per_bone, best_bit_rates);
					float new_error = calculate_max_error_at_bit_rate_object(context, bone_index, error_scan_stop_condition::until_end_of_segment);
					std::swap(context.bit_rate_per_bone, best_bit_rates);

					for (uint32_t i = 0; i < num_bones; ++i)
					{
						const transform_bit_rates& bone_bit_rate = context.bit_rate_per_bone[i];
						const transform_bit_rates& best_bone_bit_rate = best_bit_rates[i];
						const bool rotation_differs = bone_bit_rate.rotation != best_bone_bit_rate.rotation;
						const bool translation_differs = bone_bit_rate.translation != best_bone_bit_rate.translation;
						const bool scale_differs = bone_bit_rate.scale != best_bone_bit_rate.scale;
						if (rotation_differs || translation_differs || scale_differs)
						{
							printf("%8u: [%3u, %3u, %3u] @ %.4f => [%3u, %3u, %3u] @ %.4f%s (alternate permutation)\n",
								i, bone_bit_rate.rotation, bone_bit_rate.translation, bone_bit_rate.scale,
								initial_error, best_bone_bit_rate.rotation, best_bone_bit_rate.translation, best_bone_bit_rate.scale,
								new_error, new_error < error_threshold ? "" : " (too high)");
						}
					}
#endif

					std::memcpy(context.bit_rate_per_bone, best_bit_rates, sizeof(transform_bit_rates) * num_bones);
				}

				// Our error remains too high, this should be rare.
				// Attempt to increase the bit rate as much as we can while still back tracking if it doesn't help.
				error = calculate_max_error_at_bit_rate_object(context, bone_index, error_scan_stop_condition::until_end_of_segment);
				while (error >= error_threshold)
				{
					// From child to parent, increase the bit rate indiscriminately
					uint32_t num_maxed_out = 0;
					for (int32_t chain_link_index = static_cast<int32_t>(num_bones_in_chain) - 1; chain_link_index >= 0; --chain_link_index)
					{
						const uint32_t chain_bone_index = context.chain_bone_indices[chain_link_index];

						// Work with a copy. We'll increase the bit rate as much as we can and retain the values
						// that yield the smallest error BUT increasing the bit rate does NOT always means
						// that the error will reduce and improve. It could get worse in which case we'll do nothing.

						transform_bit_rates& bone_bit_rate = context.bit_rate_per_bone[chain_bone_index];

						// Copy original values
						transform_bit_rates best_bone_bit_rate = bone_bit_rate;
						float best_bit_rate_error = error;

						while (error >= error_threshold)
						{
							static_assert(offsetof(transform_bit_rates, rotation) == 0 && offsetof(transform_bit_rates, scale) == sizeof(transform_bit_rates) - 1, "Invalid BoneBitRate offsets");
							uint8_t& smallest_bit_rate = *std::min_element<uint8_t*>(&bone_bit_rate.rotation, &bone_bit_rate.scale + 1);

							if (smallest_bit_rate >= k_highest_bit_rate)
							{
								num_maxed_out++;
								break;
							}

#if ACL_IMPL_DEBUG_VARIABLE_QUANTIZATION >= ACL_IMPL_DEBUG_LEVEL_BASIC_INFO
							const float old_error = error;
#endif

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
#if ACL_IMPL_DEBUG_VARIABLE_QUANTIZATION >= ACL_IMPL_DEBUG_LEVEL_BASIC_INFO
								printf("%8u: [%3u, %3u, %3u] @ %.4f => [%3u, %3u, %3u] @ %.4f%s (aggressive bumping)\n",
									chain_bone_index, best_bone_bit_rate.rotation, best_bone_bit_rate.translation, best_bone_bit_rate.scale,
									old_error, bone_bit_rate.rotation, bone_bit_rate.translation, bone_bit_rate.scale,
									error, error < error_threshold ? "" : " (too high)");

								for (uint32_t i = chain_link_index + 1; i < num_bones_in_chain; ++i)
								{
									const uint32_t chain_bone_index2 = context.chain_bone_indices[chain_link_index];
									float error2 = calculate_max_error_at_bit_rate_object(context, chain_bone_index2, error_scan_stop_condition::until_end_of_segment);
									printf("  %8u .. @ %.4f (in chain)\n", i, error2);
								}
#endif

								best_bone_bit_rate = bone_bit_rate;
								best_bit_rate_error = error;
							}
						}

						// Only retain the lowest error bit rates
						bone_bit_rate = best_bone_bit_rate;
						error = best_bit_rate_error;

						if (error < error_threshold)
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
				if (error >= error_threshold && context.rotation_format == rotation_format8::quatf_full)
				{
					// From child to parent, max out the bit rate
					for (int32_t chain_link_index = static_cast<int32_t>(num_bones_in_chain) - 1; chain_link_index >= 0; --chain_link_index)
					{
						const uint32_t chain_bone_index = context.chain_bone_indices[chain_link_index];

#if ACL_IMPL_DEBUG_VARIABLE_QUANTIZATION >= ACL_IMPL_DEBUG_LEVEL_BASIC_INFO
						const transform_bit_rates old_bone_bit_rate = context.bit_rate_per_bone[chain_bone_index];
#endif

						transform_bit_rates& bone_bit_rate = context.bit_rate_per_bone[chain_bone_index];
						bone_bit_rate.rotation = std::max<uint8_t>(bone_bit_rate.rotation, k_highest_bit_rate);
						bone_bit_rate.translation = std::max<uint8_t>(bone_bit_rate.translation, k_highest_bit_rate);
						bone_bit_rate.scale = std::max<uint8_t>(bone_bit_rate.scale, k_highest_bit_rate);

						error = calculate_max_error_at_bit_rate_object(context, bone_index, error_scan_stop_condition::until_end_of_segment);

#if ACL_IMPL_DEBUG_VARIABLE_QUANTIZATION >= ACL_IMPL_DEBUG_LEVEL_BASIC_INFO
						printf("%8u: [%3u, %3u, %3u] => [%3u, %3u, %3u] @ %.4f%s (failsafe)\n",
							chain_bone_index, old_bone_bit_rate.rotation, old_bone_bit_rate.translation, old_bone_bit_rate.scale,
							bone_bit_rate.rotation, bone_bit_rate.translation, bone_bit_rate.scale,
							error, error < error_threshold ? "" : " (too high)");
#endif

						if (error < error_threshold)
							break;
					}
				}
			}

#if ACL_IMPL_DEBUG_VARIABLE_QUANTIZATION >= ACL_IMPL_DEBUG_LEVEL_SUMMARY_ONLY
			uint32_t total_num_bits = 0;
			for (uint32_t transform_index = 0; transform_index < num_bones; ++transform_index)
				total_num_bits += context.bit_rate_per_bone[transform_index].get_num_bits();
			printf("Variable quantization optimization results (total size %u bits):\n", total_num_bits);
			for (uint32_t bone_index = 0; bone_index < num_bones; ++bone_index)
			{
				// Update our context with the new bone data
				const float error_threshold = context.shell_metadata_per_transform[bone_index].precision;

				const uint32_t num_bones_in_chain = calculate_bone_chain_indices(context.clip, bone_index, context.chain_bone_indices);
				context.num_bones_in_chain = num_bones_in_chain;

				float error = calculate_max_error_at_bit_rate_object(context, bone_index, error_scan_stop_condition::until_end_of_segment, false);
				const transform_bit_rates& bone_bit_rate = context.bit_rate_per_bone[bone_index];
				printf("%8u: [%3u, %3u, %3u](%3u) @ %.4f%s\n", bone_index,
					bone_bit_rate.rotation, bone_bit_rate.translation, bone_bit_rate.scale, bone_bit_rate.get_num_bits(),
					error, error < error_threshold ? "" : " (too high)");
			}
#endif

			deallocate_type_array(context.allocator, bone_chain_permutation, num_bones);
			deallocate_type_array(context.allocator, permutation_bit_rates, num_bones);
			deallocate_type_array(context.allocator, best_permutation_bit_rates, num_bones);
			deallocate_type_array(context.allocator, best_bit_rates, num_bones);
		}

		//////////////////////////////////////////////////////////////////////////
		// [Bit Rate Optimization Algorithm]
		//
		// In order to properly calculate the error introduced by changing the bit rate, we need
		// to measure the transforms most impacted by the change. These transforms form the
		// critical transform set:
		//     - Dominant transform (the one furthest away from ourself, using 3D Euclidean distance)
		//     - All leaves below ourself (the ones furthest away from ourself, using Manhattan distance)
		//
		// We wish to measure the error in object/world space for each critical transform when testing a
		// new bit rate. To that end, we need to carefully consider the best approach to avoid
		// algorithmic complexity exploding.
		//
		// Let us use a single bone chain as an example:
		//     r = (c2 * c1) * t * (p2 * p1)
		// Where:
		//     c2: a child transform of c1, also a leaf transform
		//     c1: a child transform of t
		//     t: the transform whose bit rate we are changing
		//     p2: parent transform of t
		//     p1: parent transform of p2, also a root transform
		//     r: resulting local to world for c2
		//
		// Our key insight is that when 't' changes, the other transforms do not change. This means
		// that we can pre-calculate (c2 * c1) and (p2 * p1) and re-use them over and over when testing
		// each permutation. This converts our O(N) evaluation where N is the number of transforms
		// into O(1) with just 2 multiplications to compute our final local to world transform for 'c2'.
		//
		// Another key insight is that we wish to start optimizing the leaf transforms first.
		// A transform has a single parent, but it can have many children. By optimizing the children
		// first, we force the parent to retain more precision to accommodate them but each child can
		// use fewer bits. If we went the other way around, a less precise parent may force its children
		// to retain more bits.
		//
		// Our desired algorithm is thus as such:
		// for each transform X, sorted children first ...
		//     for each bit rate permutation, sorted smallest first ...
		//         for each critical transform of X ...
		//             for each sample in segment ...
		//                 Measure the error in object space, retain max error
		//             if max error too high, break
		//         if max error below precision threshold
		//             update bit rate for X, break
		//
		// The algorithm complexity is thus as follows: O(T*P*S*C)
		// Where:
		//     T: number of transforms
		//     P: number of permutations to try
		//     S: number of samples in segment
		//     C: number of critical transforms
		//
		// To facilitate the transform caching described above, we first compute every transform of
		// every sample in object space. These represent the t * (p2 * p1) part above for each transform.
		// This is reasonable because segments are fairly small and have a max of 32 samples.
		// Our algorithm starts at the leaves. The (c2 * c1) portion is thus missing, conceptually we can
		// use the identity instead. Once we find the best bit rate for each transform, we need to update
		// our cached values so that our parent transforms can leverage them. We move on from there.
		//
		// Let us walk through an example for this transform chain (child left, parent right):
		//     A, B, C, D
		// A is a leaf while D is a root. The process here is shown for children, but the treatment of the dominant
		// transform is similar.
		//
		// First we optimize A, it has no children and so only the cached dominant transform needs to be updated: itself.
		// We have no children and so (c2 * c1) is the identity while (p2 * p1) is B's cached transform.
		// We set A's cached transform to A * identity (because it has no children)
		// Next, we move to optimize B. A's cached transform represents (c2 * c1) above while C's cached transform is (p2 * p1).
		// We set A's cached transform to A * B (we roll B's transform into it).
		// Next, we move to optimize C. A's cached transform represents (c2 * c1) while D's cached transform is (p2 * p1).
		// We set A's cached transform to A * B * C.
		// Finally, we move to optimize D. It has no parent and so the (p2 * p1) portion is the identity transform.
		// We update A similarly and the process ends.
		//
		// Only leaf cached transforms and dominant transforms need to be updated. Other intermediary transforms can remain
		// unchanged since they are no longer needed once optimized: their final values are rolled up into the cached critical
		// transforms as we progress through the optimization process.
		//
		// Non-uniform 3D scale throws a wrench in the mix because Realtime Math's qvv type is not associative under multiplication
		// when non-uniform scale is present. As such, the multiplication order is important and we cannot leverage caching
		// as effectively. We have to calculate the object space transforms by traversing the transform chains manually.
		// This is quite a bit slower, but thankfully, non-uniform 3D scale is uncommon. When it is, we take a slower path
		// that does not leverage associativity. Instead, the cached transforms will contain local space values once they
		// have been optimized. We then multiply them one by one using transform chains as needed.
		//
		// POTENTIAL IMPROVEMENTS:
		//
		// The dominant transform is especially important when zero/small scale is present as it will collapse a sub-section.
		// This can bring the leaves beneath under a parent transform which would then become dominant for all others above it.
		// Because things are animated, in practice the dominant transforms can change over time. We would thus need to compute
		// the dominance for each keyframe, not just each transform. We would then test each non-leaf dominant transform at
		// each keyframe instead of including it with the leaves.
		//
		// Track max error per critical transform. We stop iterating once we find a valid permutation that meets our precision
		// thresholds but if we fail to find one, we use the best permutation. To find the best permutation, we should use
		// the remaining optimization room instead of the best/lowest error. The optimization room is: precision - error
		// Ideally, we want our permutation error to be as close as possible to the allowed precision without exceeding it.
		// If we exceed it, we want to minimize the amount of room left.
		//////////////////////////////////////////////////////////////////////////
		inline void find_optimal_bit_rates_v2(quantization_context& context)
		{
			// For algorithm from ACL 2.2 and later
			ACL_ASSERT(context.is_valid(), "quantization_context isn't valid");

			context.initialize_v2();

#if ACL_IMPL_DEBUG_VARIABLE_QUANTIZATION >= ACL_IMPL_DEBUG_LEVEL_SUMMARY_ONLY
			// Still using old v1 code/data
			context.initialize_v1();
			cache_raw_transforms_v1(context);
#endif

			initialize_bone_bit_rates_v2(*context.segment, context.rotation_format, context.translation_format, context.scale_format, context.bit_rate_per_bone);

			const uint32_t num_transforms = context.num_bones;
			const uint32_t num_samples = context.num_samples;
			const float sample_rate = context.sample_rate;
			const float clip_duration = context.clip_duration;
			const bool has_additive_base = context.has_additive_base;
			const additive_clip_format8 additive_format = context.clip.additive_format;

			const bool rotation_supports_constant_tracks = context.segment->are_rotations_normalized;
			const bool translation_supports_constant_tracks = context.segment->are_translations_normalized;
			const bool scale_supports_constant_tracks = context.segment->are_scales_normalized;

			rtm::qvvf* additive_base_local_transforms = context.additive_base_local_transforms;
			rtm::qvvf* object_transforms_raw = context.object_transforms_raw;
			rtm::qvvf* cached_transforms_lossy = context.cached_transforms_lossy;
			uint32_t* critical_transform_indices = context.critical_transform_indices;

			if (!context.all_local_query.is_bound())
				context.all_local_query.bind(context.bit_rate_database);
			context.all_local_query.build(context.bit_rate_per_bone);

			const rtm::vector4f default_scale = rtm::vector_set(1.0F);
			rtm::mask4f has_default_scale = rtm::mask_set(true, true, true, true);

			// Build our cached transforms by sampling everything and converting our segment to object space
			for (uint32_t sample_index = 0; sample_index < num_samples; ++sample_index)
			{
				// The sample time is calculated from the full clip duration to be consistent with decompression
				const float sample_time = rtm::scalar_min(float(context.segment->clip_sample_offset + sample_index) / sample_rate, clip_duration);

				rtm::qvvf* object_pose_raw = object_transforms_raw + (sample_index * num_transforms);
				rtm::qvvf* cached_pose_lossy = cached_transforms_lossy + (sample_index * num_transforms);

				// In local space
				sample_streams(context.raw_bone_streams, num_transforms, sample_time, object_pose_raw);
				context.bit_rate_database.sample(context.all_local_query, sample_time, cached_pose_lossy, num_transforms);

				if (has_additive_base)
				{
					rtm::qvvf* additive_base_pose_transforms = additive_base_local_transforms + (sample_index * num_transforms);

					const float normalized_sample_time = context.additive_base_clip.num_samples > 1 ? (sample_time / clip_duration) : 0.0F;
					const float additive_sample_time = context.additive_base_clip.num_samples > 1 ? (normalized_sample_time * context.additive_base_clip.duration) : 0.0F;
					sample_streams(context.additive_base_clip.segments[0].bone_streams, num_transforms, additive_sample_time, additive_base_pose_transforms);

					// Apply our additives onto our base
					for (uint32_t transform_index = 0; transform_index < num_transforms; ++transform_index)
					{
						object_pose_raw[transform_index] = rtm::qvv_normalize(acl::apply_additive_to_base(additive_format, additive_base_pose_transforms[transform_index], object_pose_raw[transform_index]));
						cached_pose_lossy[transform_index] = rtm::qvv_normalize(acl::apply_additive_to_base(additive_format, additive_base_pose_transforms[transform_index], cached_pose_lossy[transform_index]));
					}
				}

				// Convert our poses to object space
				for (uint32_t transform_index : context.topology->roots_first_iterator())
				{
					// Test if we have scale after applying our base pose since it might introduce scale
					has_default_scale = rtm::mask_and(rtm::vector_equal(default_scale, object_pose_raw[transform_index].scale), has_default_scale);

					const uint32_t parent_transform_index = context.topology->transforms[transform_index].parent_index;
					if (parent_transform_index != k_invalid_track_index)
					{
						object_pose_raw[transform_index] = rtm::qvv_normalize(rtm::qvv_mul(object_pose_raw[transform_index], object_pose_raw[parent_transform_index]));
						cached_pose_lossy[transform_index] = rtm::qvv_normalize(rtm::qvv_mul(cached_pose_lossy[transform_index], cached_pose_lossy[parent_transform_index]));
					}
				}
			}

			// If we have non-uniform 3D scale, we cannot rely on associativity, fall back to the transform chain
			const bool has_scale = rtm::mask_any_true3(rtm::mask_not(has_default_scale));

			if (has_scale)
				context.initialize_v2_scale();

			uint32_t** transform_chains = context.transform_chains;
			uint32_t* transform_chain_counts = context.transform_chain_counts;

			// Compute our dominant transforms
			compute_segment_shell_distances(*context.segment, cached_transforms_lossy, context.shell_metadata_per_transform);

			// We try permutations from the lowest memory footprint to the highest.
			const uint8_t* const bit_rate_permutations_per_dofs[] =
			{
				&acl_impl::k_local_bit_rate_permutations_1_dof[0][0],
				&acl_impl::k_local_bit_rate_permutations_2_dof[0][0],
				&acl_impl::k_local_bit_rate_permutations_3_dof[0][0],
			};
			const size_t num_bit_rate_permutations_per_dofs[] =
			{
				get_array_size(acl_impl::k_local_bit_rate_permutations_1_dof),
				get_array_size(acl_impl::k_local_bit_rate_permutations_2_dof),
				get_array_size(acl_impl::k_local_bit_rate_permutations_3_dof),
			};

			for (const uint32_t transform_index : context.topology->leaves_first_iterator())
			{
				const transform_topology_t& transform_topology = context.topology->transforms[transform_index];

#if ACL_IMPL_DEBUG_VARIABLE_QUANTIZATION >= ACL_IMPL_DEBUG_LEVEL_BASIC_INFO
				printf("%8u: parent: %3u\n", transform_index, transform_topology.parent_index);
#endif

				// Find our critical transform set: ourself + descendants
				// Critical transforms must be sorted leaf first to ensure we update our cached transforms in the correct order
				// Another important property is that if a transform has a descendant as its critical transform, then each
				// parent along the chain must also include the same critical transform: dominance must be transitive (if used).
				uint32_t num_critical_transforms = 0;

				critical_transform_indices[num_critical_transforms++] = transform_index;

				for (const uint32_t descendant_transform_index : transform_topology.descendants_iterator())
					critical_transform_indices[num_critical_transforms++] = descendant_transform_index;

#if defined(ACL_IMPL_DEBUG_ENABLE_DOMINANT_DESCENDANTS) && 0
				// TODO: Can we use dominance to trim down on the number of descendants we measure?
				// Doesn't quite work at the moment as when bit rates change, it impacts dominance
				// and we can't account for it easily if we calculate dominance up-front
				// We'd have to update the dominance map as bit rates change
				for (const uint32_t dominant_transform_index : transform_topology.dominant_descendants_iterator())
				{
					if (context.topology->transforms[dominant_transform_index].is_leaf())
						continue;	// Skip leaf transforms, we'll add them below

					critical_transform_indices[num_critical_transforms++] = dominant_transform_index;
				}

				for (const uint32_t leaf_transform_index : transform_topology.leaves_iterator())
					critical_transform_indices[num_critical_transforms++] = leaf_transform_index;
#endif

				// Sort leaf first
				std::reverse(critical_transform_indices, critical_transform_indices + num_critical_transforms);

				// Non-uniform 3D scale requires slower full chain processing because we can't leverage associativity
				if (has_scale)
				{
					// Build our critical transform chains up to the transform we are optimizing
					for (uint32_t critical_chain_index = 0; critical_chain_index < num_critical_transforms; ++critical_chain_index)
					{
						const uint32_t critical_transform_index = critical_transform_indices[critical_chain_index];
						uint32_t* transform_chain = transform_chains[critical_chain_index];

						uint32_t chain_length = 0;
						uint32_t chain_transform_index = critical_transform_index;

						// Add our critical transform index
						transform_chain[chain_length++] = chain_transform_index;

						while (chain_transform_index != transform_index)
						{
							// If we haven't reached our optimizing transform, add the next one
							chain_transform_index = context.topology->transforms[chain_transform_index].parent_index;

							transform_chain[chain_length++] = chain_transform_index;
						}

						transform_chain_counts[critical_chain_index] = chain_length;
					}
				}

				// Bit rates at this point are one of three value:
				// 0: if the segment track is normalized, it can be constant within the segment
				// 1: if the segment track isn't normalized, it starts at the lowest bit rate
				// 255: if the track is constant/default for the whole clip
				const transform_bit_rates bone_bit_rates = context.bit_rate_per_bone[transform_index];

				if (bone_bit_rates.rotation == k_invalid_bit_rate && bone_bit_rates.translation == k_invalid_bit_rate && bone_bit_rates.scale == k_invalid_bit_rate)
				{
#if ACL_IMPL_DEBUG_VARIABLE_QUANTIZATION >= ACL_IMPL_DEBUG_LEVEL_BASIC_INFO
					const float transform_precision = context.shell_metadata_per_transform[transform_index].precision;
					const float error = calculate_max_error_at_bit_rate_local(context, transform_index, error_scan_stop_condition::until_end_of_segment);
					printf("%8u: Best bit rates: [%3u, %3u, %3u](  0) @ %.4f%s (all constant)\n",
						transform_index, bone_bit_rates.rotation, bone_bit_rates.translation, bone_bit_rates.scale,
						error, error < transform_precision ? "" : " (too high)");
#endif

					// Update our cached transforms using our existing bit rate
					for (const uint32_t critical_transform_index : make_iterator(critical_transform_indices, num_critical_transforms))
					{
						if (has_scale)
							update_cached_transforms_with_scale(context, transform_index, additive_base_local_transforms, cached_transforms_lossy);
						else
							update_cached_transforms(context, transform_index, critical_transform_index, additive_base_local_transforms, cached_transforms_lossy);
					}

					continue;	// Every track bit rate is constant/default, nothing else to do
				}

				transform_bit_rates best_bit_rates = bone_bit_rates;
				float best_error = 1.0E10F;
				uint32_t prev_transform_size = ~0U;
				bool is_error_good_enough = false;

#if ACL_IMPL_DEBUG_VARIABLE_QUANTIZATION >= ACL_IMPL_DEBUG_LEVEL_BASIC_INFO
#if 0
				float best_permutation_group_optimization_room = FLT_MAX;
				size_t best_permutation_group_index = 0;
#endif
				size_t best_permutation_index = 0;

#if ACL_IMPL_DEBUG_VARIABLE_QUANTIZATION >= ACL_IMPL_DEBUG_LEVEL_VERBOSE_INFO
				float best_transform_error = 1.0E10F;
				transform_bit_rates best_transform_bit_rates = bone_bit_rates;
#endif
#endif

				// Determine how many degrees of freedom we have to optimize our bit rates
				uint32_t num_dof = 0;
				num_dof += bone_bit_rates.rotation != k_invalid_bit_rate ? 1 : 0;
				num_dof += bone_bit_rates.translation != k_invalid_bit_rate ? 1 : 0;
				num_dof += bone_bit_rates.scale != k_invalid_bit_rate ? 1 : 0;

				const uint8_t* bit_rate_permutations_per_dof = bit_rate_permutations_per_dofs[num_dof - 1];
				const size_t num_bit_rate_permutations = num_bit_rate_permutations_per_dofs[num_dof - 1];

				// Our desired bit rates start with the initial value
				transform_bit_rates desired_bit_rates = bone_bit_rates;

				size_t permutation_offset = 0;
				for (size_t permutation_index = 0; permutation_index < num_bit_rate_permutations; ++permutation_index)
				{
					// If a bit rate is variable, grab a permutation for it
					// We'll only consume as many bit rates as we have degrees of freedom

					uint32_t transform_size = 0;	// In bits

					if (desired_bit_rates.rotation != k_invalid_bit_rate)
					{
						desired_bit_rates.rotation = bit_rate_permutations_per_dof[permutation_offset++];
						transform_size += get_num_bits_at_bit_rate(desired_bit_rates.rotation);
					}

					if (desired_bit_rates.translation != k_invalid_bit_rate)
					{
						desired_bit_rates.translation = bit_rate_permutations_per_dof[permutation_offset++];
						transform_size += get_num_bits_at_bit_rate(desired_bit_rates.translation);
					}

					if (desired_bit_rates.scale != k_invalid_bit_rate)
					{
						desired_bit_rates.scale = bit_rate_permutations_per_dof[permutation_offset++];
						transform_size += get_num_bits_at_bit_rate(desired_bit_rates.scale);
					}

					// If our inputs aren't normalized per segment, we can't store them on 0 bits because we'll have no
					// segment range information. This occurs when we have a single segment. Skip those permutations.
					if (!rotation_supports_constant_tracks && desired_bit_rates.rotation == 0)
						continue;
					else if (!translation_supports_constant_tracks && desired_bit_rates.translation == 0)
						continue;
					else if (!scale_supports_constant_tracks && desired_bit_rates.scale == 0)
						continue;

					if (transform_size > prev_transform_size)
					{
#if ACL_IMPL_DEBUG_VARIABLE_QUANTIZATION >= ACL_IMPL_DEBUG_LEVEL_VERBOSE_INFO
						printf("%8u: [%3u | %3u | %3u] best with %2u bits @ %.4f\n", transform_index, best_transform_bit_rates.rotation, best_transform_bit_rates.translation, best_transform_bit_rates.scale, prev_transform_size, best_transform_error);

						// Reset
						best_transform_error = 1.0E10F;
#endif

#if ACL_IMPL_DEBUG_VARIABLE_QUANTIZATION >= ACL_IMPL_DEBUG_LEVEL_BASIC_INFO && 0
						best_permutation_group_optimization_room = FLT_MAX;
#endif
					}

					// If we already found a permutation that is good enough, we test all the others
					// that have the same size. Once the size changes, we stop.
					if (is_error_good_enough && transform_size != prev_transform_size)
						break;

					prev_transform_size = transform_size;

					context.bit_rate_per_bone[transform_index] = desired_bit_rates;

#if ACL_IMPL_DEBUG_VARIABLE_QUANTIZATION >= ACL_IMPL_DEBUG_LEVEL_VERBOSE_INFO
					printf("            Measuring [%3u | %3u | %3u] (%2u bits) ...\n", desired_bit_rates.rotation, desired_bit_rates.translation, desired_bit_rates.scale, transform_size);
#endif

					// Calculate the error for each critical transform and find the least precise one
#if 0
					float worst_critical_transform_room = 0.0F;
					uint32_t worst_critical_transform_index = ~0U;
#endif

					float worst_critical_transform_error = 0.0F;
					bool is_permutation_good_enough = true;

					for (uint32_t critical_chain_index = 0; critical_chain_index < num_critical_transforms; ++critical_chain_index)
					{
						const uint32_t critical_transform_index = critical_transform_indices[critical_chain_index];

						float critical_transform_error;
						if (has_scale)
							critical_transform_error = calculate_max_error_at_bit_rate_object_cached_with_scale(
								context,
								transform_index, critical_transform_index,
								transform_chains[critical_chain_index], transform_chain_counts[critical_chain_index],
								additive_base_local_transforms,
								object_transforms_raw,
								cached_transforms_lossy,
								best_error);
						else
							critical_transform_error = calculate_max_error_at_bit_rate_object_cached(
								context,
								transform_index, critical_transform_index,
								additive_base_local_transforms,
								object_transforms_raw,
								cached_transforms_lossy,
								best_error);

						const float critical_transform_precision = context.shell_metadata_per_transform[critical_transform_index].precision;
						is_permutation_good_enough &= critical_transform_error <= critical_transform_precision;

						// TODO: Try this out
#if 0
						// We wish to find the critical transform that has the least room for further optimization
						// This is the least precise critical transform
						// To find the least precise, we compute the remaining optimization room: precision - error
						// This algorithm uses the precision threshold as a target error to reach but not exceed
						// As such, if the error is below our threshold (good), we want our positive room to be as close to zero as possible
						// If the error is below our threshold (bad), we want our negative room to be as close to zero as possible
						// If all our critical transforms are below their precision threshold, their values will be between [0.0, precision]
						// and the least precise is the one with the largest value.
						// If any critical transform is above their precision threshold, its value will be negative
						const float optimization_room = critical_transform_precision - critical_transform_error;

						if (worst_critical_transform_room >= 0.0F)
						{
							if (optimization_room < 0.0F || optimization_room >= worst_critical_transform_room)
							{
								worst_critical_transform_room = optimization_room;
								worst_critical_transform_index = critical_transform_index;
							}
						}
						else
						{
							if (optimization_room < worst_critical_transform_room)
							{
								worst_critical_transform_room = optimization_room;
								worst_critical_transform_index = critical_transform_index;
							}
						}
#endif

#if ACL_IMPL_DEBUG_VARIABLE_QUANTIZATION >= ACL_IMPL_DEBUG_LEVEL_VERBOSE_INFO
						printf("            %8u @ %.4f\n", critical_transform_index, critical_transform_error);
#endif

						// We are only as precise as our least precise critical transform
						worst_critical_transform_error = rtm::scalar_max(critical_transform_error, worst_critical_transform_error);

						if (critical_transform_error >= best_error)
							break;	// The error of this transform is too high, this permutation will not be selected
					}



#if ACL_IMPL_DEBUG_VARIABLE_QUANTIZATION >= ACL_IMPL_DEBUG_LEVEL_BASIC_INFO && 0
					// Now that we found the least precise critical transform, we can use it to find the best permutation
					// The best permutation will have positive optimization room as close to zero as possible
					// If none of the permutations have positive optimization room, then the best one is as close to zero as well

					// Track the best permutation of its group (all permutations with same size)
					if (best_permutation_group_optimization_room == FLT_MAX)
					{
						// First permutation of the group
						best_permutation_group_optimization_room = worst_critical_transform_room;
						best_permutation_group_index = permutation_index;
					}
					else if (worst_critical_transform_room >= 0.0F)
					{
						// This permutation is a good one and it meets our desired precision
						if (best_permutation_group_optimization_room < 0.0F || worst_critical_transform_room < best_permutation_group_optimization_room)
						{
							best_permutation_group_optimization_room = worst_critical_transform_room;
							best_permutation_group_index = permutation_index;
						}
					}
					else
					{
						// This permutation isn't meeting our desired precision
						if (worst_critical_transform_room < best_permutation_group_optimization_room)
						{
							best_permutation_group_optimization_room = worst_critical_transform_room;
							best_permutation_group_index = permutation_index;
						}
					}
#endif

#if ACL_IMPL_DEBUG_VARIABLE_QUANTIZATION >= ACL_IMPL_DEBUG_LEVEL_VERBOSE_INFO
					if (worst_critical_transform_error < best_transform_error)
					{
						best_transform_error = worst_critical_transform_error;
						best_transform_bit_rates = desired_bit_rates;
					}
#endif

					if (worst_critical_transform_error < best_error)
					{
						best_error = worst_critical_transform_error;
						best_bit_rates = desired_bit_rates;
						is_error_good_enough = is_permutation_good_enough;

#if ACL_IMPL_DEBUG_VARIABLE_QUANTIZATION >= ACL_IMPL_DEBUG_LEVEL_BASIC_INFO
						best_permutation_index = permutation_index;
#endif
					}

#if ACL_IMPL_DEBUG_VARIABLE_QUANTIZATION >= ACL_IMPL_DEBUG_LEVEL_VERBOSE_INFO
					if (permutation_index + 1 == num_bit_rate_permutations)
					{
						// Last entry before we exit the loop
						printf("%8u: [%3u | %3u | %3u] best with %2u bits @ %.4f\n", transform_index, best_transform_bit_rates.rotation, best_transform_bit_rates.translation, best_transform_bit_rates.scale, prev_transform_size, best_transform_error);
					}
#endif
				}

#if ACL_IMPL_DEBUG_VARIABLE_QUANTIZATION >= ACL_IMPL_DEBUG_LEVEL_BASIC_INFO
				printf("%8u: Best bit rates: [%3u, %3u, %3u](%2u bits) perm#[%5u] @ %.4f%s (object)\n",
					transform_index, best_bit_rates.rotation, best_bit_rates.translation, best_bit_rates.scale,
					best_bit_rates.get_num_bits(), uint32_t(best_permutation_index), best_error, is_error_good_enough ? "" : " (too high)");
#endif

				context.bit_rate_per_bone[transform_index] = best_bit_rates;

				// Update our cached transforms using our new bit rate
				for (const uint32_t critical_transform_index : make_iterator(critical_transform_indices, num_critical_transforms))
				{
					if (has_scale)
						update_cached_transforms_with_scale(context, transform_index, additive_base_local_transforms, cached_transforms_lossy);
					else
						update_cached_transforms(context, transform_index, critical_transform_index, additive_base_local_transforms, cached_transforms_lossy);
				}
			}

#if ACL_IMPL_DEBUG_VARIABLE_QUANTIZATION >= ACL_IMPL_DEBUG_LEVEL_SUMMARY_ONLY
			uint32_t total_num_bits = 0;
			for (uint32_t transform_index = 0; transform_index < num_transforms; ++transform_index)
				total_num_bits += context.bit_rate_per_bone[transform_index].get_num_bits();
			printf("Variable quantization optimization results (total size %u bits):\n", total_num_bits);
			for (uint32_t transform_index = 0; transform_index < num_transforms; ++transform_index)
			{
				const float transform_precision = context.shell_metadata_per_transform[transform_index].precision;
				const transform_bit_rates& bone_bit_rate = context.bit_rate_per_bone[transform_index];

				const uint32_t num_bones_in_chain = calculate_bone_chain_indices(context.clip, transform_index, context.chain_bone_indices);
				const float error = calculate_max_error_at_bit_rate_object(context, transform_index, context.chain_bone_indices, num_bones_in_chain);

				printf("%8u: [%3u, %3u, %3u][%3u] @ %.4f%s\n", transform_index,
					bone_bit_rate.rotation, bone_bit_rate.translation, bone_bit_rate.scale,
					bone_bit_rate.get_num_bits(), error, error < transform_precision ? "" : " (too high)");
			}
#endif
		}

		inline void find_optimal_bit_rates_dispatch(quantization_context& context)
		{
			// For the time being, we still support the legacy algorithm for error metrics that don't support it
			// ACL 2.2 will deprecate the error metric in the compression settings in favor of an enum to hide
			// it as an implementation detail
			// ACL 2.3 will remove the old stuff entirely
			if (context.error_metric->supports_bit_rate_algorithm_v2())
				find_optimal_bit_rates_v2(context);
			else
				find_optimal_bit_rates_v1(context);
		}

		// Partitioning will be done as follow in two phases: calculating the error contribution for every frame and a global optimization pass.
		//
		// Phase 1:
		// For every clip we compress, when requested, we'll calculate the contributing error of every frame and store it in optional metadata.
		// Later this metadata will be used to create a database.
		// To calculate the contributing error, we iterate over every segment.
		// For every frame we can remove (e.g. not first/last of clip/segment), calculate the resulting error of every frame between the remaining
		// frames. For example, say we have frame 0, 1, 2, 3, and 4. We first try removing frame 1 and calculate the error. We repeat this for
		// frame 2 and 3 individually. The frame with the lowest error is the least important one and can be removed first. Suppose it is frame 1,
		// we'll append in the metadata for that segment that the first frame to remove is frame 1 and the contributing error we just calculated.
		// We then loop again over every frame we can remove. We'll try removing frame 2 by measuring the error at frame 1 and 2 once it is removed
		// (by interpolating between frame 0 and 3). The resulting error is the largest error of every frame we test.
		// And we then try removing frame 3 on its own. Once we have a contributing error for frame 2 and 3, we again pick the lowest and append it
		// to our segment metadata. We repeat this until every frame has been sorted. Each segment is processed individually.
		// This metadata is stored at the end of the clip.
		//
		// Phase 2:
		// A new create database function will be added that takes a list of compressed clips as inputs and outputs a list of compressed clips
		// as well as a new database. Every input clip must contain the metadata from phase 1.
		// Using our tier distribution information, we can calculate how many frames we have that can be moved to the database and how many
		// each tier will contain. We then start with the highest tier (tier 2) which will contain the least important frames. These frames contribute
		// the least to our accuracy (they have the lowest contributing error as calculated in phase 1). For every tier, we iterate over every clip/segment
		// and find the lowest contributing error. One we found it, we assign it to our tier until the tier is full. Repeat for every tier.
		// Once every tier has been populated, we can calculate where our chunk limits are and assign each frame to its chunk.
		// We can then iterate over every clip/segment/frame and copy the compressed bits to its final destination: our new clip, or our database chunk
		// for tier 1 or 2. We copy bit by bit to tightly pack things. Our new clips are mostly unchanged. The header and its metadata is nearly identical
		// except it now contains sample contained information. Each segment has a few frames stripped if they got moved to the database. Finally, the optional
		// database metadata is stripped.
		// Once every clip has been populated, we can finalize them and calculate their final hash value.
		// Once every chunk has been populated, we can finalize them and the database as well.
		//
		// This is thus a globally optimal process. If we wish to retain the 20% most important frames in tier 0 (our compressed clips) and 80% in the database, we can.
		// Some clips might see more than 80% of their frames moved to the database while others might see less.

		inline void find_contributing_error(quantization_context& context)
		{
			ACL_ASSERT(context.num_samples <= 32, "Expected no more than 32 samples per track");

			context.initialize_v1();

			// Still using old v1 code/data
			cache_raw_transforms_v1(context);

			if (context.segment->contributing_error == nullptr)
				context.segment->contributing_error = allocate_type_array<keyframe_stripping_metadata_t>(context.allocator, 32);	// Always no more than 32 frames per segment

			const uint32_t num_frames = context.num_samples;
			const uint32_t num_bones = context.num_bones;
			const uint32_t segment_index = context.segment->segment_index;
			const bitset_description desc = bitset_description::make_from_num_bits<32>();
			constexpr float infinity = std::numeric_limits<float>::infinity();

			keyframe_stripping_metadata_t* contributing_error = context.segment->contributing_error;
			uint32_t frames_retained = ~0U;	// By default, every frame is present

			// First and last frame of the segment cannot be removed and thus contribute infinite error
			// TODO: We could retain only the first/last frames of the clip instead but it would mean interpolating
			// with a frame from the previous/next segments
			contributing_error[0] = keyframe_stripping_metadata_t(0, segment_index, ~0U, infinity, false);
			contributing_error[num_frames - 1] = keyframe_stripping_metadata_t(num_frames - 1, segment_index, ~0U, infinity, false);

			// START OF ERROR METRIC STUFF
			const itransform_error_metric* error_metric = context.error_metric;
			const bool needs_conversion = context.needs_conversion;
			const bool has_additive_base = context.has_additive_base;
			const size_t sample_transform_size = context.metric_transform_size * num_bones;
			const float sample_rate = context.sample_rate;
			const float clip_duration = context.clip_duration;
			const uint32_t segment_sample_start_index = context.segment_sample_start_index;

			const auto convert_transforms_impl = std::mem_fn(context.has_scale ? &itransform_error_metric::convert_transforms : &itransform_error_metric::convert_transforms_no_scale);
			const auto apply_additive_to_base_impl = std::mem_fn(context.has_scale ? &itransform_error_metric::apply_additive_to_base : &itransform_error_metric::apply_additive_to_base_no_scale);
			const auto local_to_object_space_impl = std::mem_fn(context.has_scale ? &itransform_error_metric::local_to_object_space : &itransform_error_metric::local_to_object_space_no_scale);

			itransform_error_metric::convert_transforms_args convert_transforms_args_lossy;
			convert_transforms_args_lossy.dirty_transform_indices = context.self_transform_indices;
			convert_transforms_args_lossy.num_dirty_transforms = num_bones;
			convert_transforms_args_lossy.transforms = context.lossy_local_pose;
			convert_transforms_args_lossy.num_transforms = num_bones;
			convert_transforms_args_lossy.sample_index = 0;
			convert_transforms_args_lossy.is_lossy = true;
			convert_transforms_args_lossy.is_additive_base = false;

			itransform_error_metric::apply_additive_to_base_args apply_additive_to_base_args_lossy;
			apply_additive_to_base_args_lossy.dirty_transform_indices = context.self_transform_indices;
			apply_additive_to_base_args_lossy.num_dirty_transforms = num_bones;
			apply_additive_to_base_args_lossy.local_transforms = needs_conversion ? (const void*)(context.local_transforms_converted) : (const void*)context.lossy_local_pose;
			apply_additive_to_base_args_lossy.base_transforms = nullptr;
			apply_additive_to_base_args_lossy.num_transforms = num_bones;

			itransform_error_metric::local_to_object_space_args local_to_object_space_args_lossy;
			local_to_object_space_args_lossy.dirty_transform_indices = context.self_transform_indices;
			local_to_object_space_args_lossy.num_dirty_transforms = num_bones;
			local_to_object_space_args_lossy.parent_transform_indices = context.parent_transform_indices;
			local_to_object_space_args_lossy.local_transforms = needs_conversion ? (const void*)(context.local_transforms_converted) : (const void*)context.lossy_local_pose;
			local_to_object_space_args_lossy.num_transforms = num_bones;

			const uint8_t* raw_transform = context.raw_object_transforms;
			const uint8_t* base_transforms = context.base_local_transforms;

			if (context.lossy_transforms_start == nullptr)
			{
				// This is the first time we call this, initialize what we'll need and re-use
				if (!context.all_local_query.is_bound())
					context.all_local_query.bind(context.bit_rate_database);

				context.lossy_transforms_start = allocate_type_array<rtm::qvvf>(context.allocator, num_bones);
				context.lossy_transforms_end = allocate_type_array<rtm::qvvf>(context.allocator, num_bones);
			}

			rtm::qvvf* lossy_transforms_start = context.lossy_transforms_start;
			rtm::qvvf* lossy_transforms_end = context.lossy_transforms_end;

			context.all_local_query.build(context.bit_rate_per_bone);
			// END OF ERROR METRIC STUFF

			// We iterate until every frame but the first and last have been removed
			for (uint32_t iteration_count = 1; iteration_count < num_frames - 1; ++iteration_count)
			{
				keyframe_stripping_metadata_t best_error(~0U, segment_index, ~0U, infinity, false);

#if ACL_IMPL_DEBUG_CONTRIBUTING_ERROR
				printf("Contributing error for segment %u (%u frames), iteration %u ...\n", context.segment->segment_index, num_frames, iteration_count);
#endif

				// Calculate how much error each frame contributes as we remove them, skip the first and last
				for (uint32_t frame_index = 1; frame_index < num_frames - 1; ++frame_index)
				{
					// We'll attempt to remove the current frame if it hasn't been removed already
					if (!bitset_test(&frames_retained, desc, frame_index))
						continue;	// This frame has already been removed, skip it

					// Find the first frame before the current one that is retained, it'll be our interpolation start point
					uint32_t interp_start_frame_index = frame_index - 1;
					while (!bitset_test(&frames_retained, desc, interp_start_frame_index))
						interp_start_frame_index--;	// This frame isn't being retained, skip it

					// Find the first frame after the current one that is retained, it'll be out interpolation end point
					uint32_t interp_end_frame_index = frame_index + 1;
					while (!bitset_test(&frames_retained, desc, interp_end_frame_index))
						interp_end_frame_index++;	// This frame isn't being retained, skip it

					// TODO: We could cache the contributing error and only invalidate it when we remove a frame

					// The sample time is calculated from the full clip duration to be consistent with decompression
					const float interp_start_time = rtm::scalar_min(float(interp_start_frame_index + segment_sample_start_index) / sample_rate, clip_duration);
					const float interp_end_time = rtm::scalar_min(float(interp_end_frame_index + segment_sample_start_index) / sample_rate, clip_duration);

					// We'll calculate the resulting error on every frame already removed that lives in between the remaining
					// two interpolation frames.
					context.bit_rate_database.sample(context.all_local_query, interp_start_time, lossy_transforms_start, num_bones);
					context.bit_rate_database.sample(context.all_local_query, interp_end_time, lossy_transforms_end, num_bones);

					// We'll retain the worst error as the current frame's contributing error.
					rtm::scalarf max_contributing_error_sq = rtm::scalar_set(0.0F);
					bool is_keyframe_trivial = true;

					for (uint32_t interp_frame_index = interp_start_frame_index + 1; interp_frame_index < interp_end_frame_index; ++interp_frame_index)
					{
						// Calculate our interpolation alpha
						const float interpolation_alpha = find_linear_interpolation_alpha(float(interp_frame_index), interp_start_frame_index, interp_end_frame_index, sample_rounding_policy::none, sample_looping_policy::clamp);

						// Interpolate our transforms in local space before we convert or apply the additive and transform to object space
						for (uint32_t bone_index = 0; bone_index < num_bones; ++bone_index)
						{
							const rtm::quatf interp_rotation = quat_lerp_stable(lossy_transforms_start[bone_index].rotation, lossy_transforms_end[bone_index].rotation, interpolation_alpha);
							const rtm::vector4f interp_translation = rtm::vector_lerp(lossy_transforms_start[bone_index].translation, lossy_transforms_end[bone_index].translation, interpolation_alpha);
							const rtm::vector4f interp_scale = rtm::vector_lerp(lossy_transforms_start[bone_index].scale, lossy_transforms_end[bone_index].scale, interpolation_alpha);

							context.lossy_local_pose[bone_index] = rtm::qvv_set(interp_rotation, interp_translation, interp_scale);
						}

						// Convert to our object space representation
						if (needs_conversion)
						{
							convert_transforms_args_lossy.sample_index = interp_frame_index;
							convert_transforms_impl(error_metric, convert_transforms_args_lossy, context.local_transforms_converted);
						}

						if (has_additive_base)
						{
							apply_additive_to_base_args_lossy.base_transforms = base_transforms + (interp_frame_index * sample_transform_size);

							apply_additive_to_base_impl(error_metric, apply_additive_to_base_args_lossy, context.lossy_local_pose);
						}

						local_to_object_space_impl(error_metric, local_to_object_space_args_lossy, context.lossy_object_pose);

						// Calculate our error
						const uint8_t* raw_frame_transform = raw_transform + (interp_frame_index * sample_transform_size);

						for (uint32_t bone_index = 0; bone_index < num_bones; ++bone_index)
						{
							itransform_error_metric::calculate_error_args calculate_error_args;
							calculate_error_args.transform0 = raw_frame_transform + (bone_index * context.metric_transform_size);
							calculate_error_args.transform1 = context.lossy_object_pose + (bone_index * context.metric_transform_size);

							const transform_metadata& transform_data = context.metadata[bone_index];
							calculate_error_args.construct_sphere_shell(transform_data.shell_distance);

							// We always include the scale to ensure that when we strip keyframes based on the measured error
							// the resulting clip error remains consistent
							const rtm::scalarf error_sq = error_metric->calculate_error_squared(calculate_error_args);
							const float precision_sq = transform_data.precision * transform_data.precision;

							max_contributing_error_sq = rtm::scalar_max(max_contributing_error_sq, error_sq);
							is_keyframe_trivial &= rtm::scalar_cast(error_sq) <= precision_sq;
						}
					}

					const float max_contributing_errorf = rtm::scalar_cast(rtm::scalar_sqrt(max_contributing_error_sq));

#if ACL_IMPL_DEBUG_CONTRIBUTING_ERROR
					printf("    Error between frame [%u, %u] while testing %u: %f\n", interp_start_frame_index, interp_end_frame_index, frame_index, max_contributing_errorf);
#endif

					// If our current frame's contributing error is lowest, it is the best candidate for removal
					// If our best error is infinite, we assign it anyway in case we cannot remove any more keyframes
					// A later keyframe may end up replacing us but if it isn't the case, the keyframe will be retained
					// as is the case for those with infinite error (such as the first/last of a segment) are never removed
					if (max_contributing_errorf < best_error.stripping_error || best_error.keyframe_index == ~0U)
						best_error = keyframe_stripping_metadata_t(frame_index, segment_index, iteration_count - 1, max_contributing_errorf, is_keyframe_trivial);
				}

				ACL_ASSERT(best_error.keyframe_index != ~0U, "Failed to find the best contributing error");

#if ACL_IMPL_DEBUG_CONTRIBUTING_ERROR
				printf("    Best frame to remove: %u (%f)\n", best_error.keyframe_index, best_error.stripping_error);
#endif

				// We found the best frame to remove, remove it
				contributing_error[best_error.keyframe_index] = best_error;
				bitset_set(&frames_retained, desc, best_error.keyframe_index, false);
			}

			// We found the contributing error for every keyframe, sort them by the order they should be stripped from this segment
			auto sort_predicate = [](const keyframe_stripping_metadata_t& lhs, const keyframe_stripping_metadata_t& rhs) { return lhs.stripping_index < rhs.stripping_index; };
			std::sort(contributing_error, contributing_error + num_frames, sort_predicate);
		}

		inline void sort_contributing_error(iallocator& allocator, clip_context& lossy_clip_context)
		{
			const uint32_t num_samples = lossy_clip_context.num_samples;

			if (lossy_clip_context.contributing_error == nullptr)
				lossy_clip_context.contributing_error = allocate_type_array<keyframe_stripping_metadata_t>(allocator, num_samples);

			constexpr float infinity = std::numeric_limits<float>::infinity();
			const uint32_t num_segments = lossy_clip_context.num_segments;

			uint32_t* num_stripped_in_segment = allocate_type_array<uint32_t>(allocator, num_segments);
			std::fill(num_stripped_in_segment, num_stripped_in_segment + num_segments, 0U);

			// Now that every segment has been populated, the stripping order is relative to each segment
			// We need it to be relative to the whole clip
			for (uint32_t stripping_index = 0; stripping_index < num_samples; ++stripping_index)
			{
				keyframe_stripping_metadata_t best_error(~0U, 0, ~0U, infinity, false);

				for (uint32_t segment_index = 0; segment_index < num_segments; ++segment_index)
				{
					const segment_context& segment = lossy_clip_context.segments[segment_index];
					const uint32_t segment_strip_index = num_stripped_in_segment[segment_index];

					if (segment_strip_index >= segment.num_samples)
						continue;	// We've stripped every keyframe from this segment

					// We use <= because if the stripping error is the same, the order in which we remove keyframes
					// doesn't matter and once all keyframes that can be stripped have been removed, only the boundary
					// keyframes will remain and we'll match here with infinity.
					if (segment.contributing_error[segment_strip_index].stripping_error <= best_error.stripping_error)
						best_error = segment.contributing_error[segment_strip_index];
				}

				ACL_ASSERT(best_error.keyframe_index != ~0U, "Expected to find a valid keyframe to strip");

				// Update our stripping and keyframe indices to make it relative to the clip
				best_error.stripping_index = stripping_index;
				best_error.keyframe_index += lossy_clip_context.segments[best_error.segment_index].clip_sample_offset;

				num_stripped_in_segment[best_error.segment_index]++;
				lossy_clip_context.contributing_error[stripping_index] = best_error;
			}

			deallocate_type_array(allocator, num_stripped_in_segment, num_segments);
		}

		inline void quantize_streams(
			iallocator& allocator,
			clip_context& clip,
			const compression_settings& settings,
			const compression_segmenting_settings& segmenting_settings,
			const clip_context& raw_clip_context,
			const clip_context& additive_base_clip_context,
			const output_stats& out_stats,
			compression_stats_t& compression_stats)
		{
			(void)out_stats;
			(void)compression_stats;

			if (clip.num_bones == 0 || clip.num_samples == 0)
				return;

#if defined(ACL_USE_SJSON)
			scope_profiler bit_rate_optimization_time;
#endif

			const bool is_rotation_variable = is_rotation_format_variable(settings.rotation_format);
			const bool is_translation_variable = is_vector_format_variable(settings.translation_format);
			const bool is_scale_variable = is_vector_format_variable(settings.scale_format);
			const bool is_any_variable = is_rotation_variable || is_translation_variable || is_scale_variable;

			quantization_context context(allocator, clip, raw_clip_context, additive_base_clip_context, settings, segmenting_settings);

			for (segment_context& segment : clip.segment_iterator())
			{
#if ACL_IMPL_DEBUG_VARIABLE_QUANTIZATION >= ACL_IMPL_DEBUG_LEVEL_SUMMARY_ONLY
				printf("Quantizing segment %u...\n", segment.segment_index);
#endif

#if ACL_IMPL_PROFILE_MATH
				{
					scope_profiler timer;

					for (int32_t i = 0; i < 10; ++i)
					{
						context.set_segment(segment);

						if (is_any_variable)
							find_optimal_bit_rates_dispatch(context);
					}

					timer.stop();

#if defined(__ANDROID__)
					__android_log_print(ANDROID_LOG_INFO, "acl", "Quantization optimization for segment %u took: %.4f ms", segment.segment_index, timer.get_elapsed_milliseconds());
#else
					printf("Quantization optimization for segment %u took: %.4f ms\n", segment.segment_index, timer.get_elapsed_milliseconds());
#endif
				}
#endif

				context.set_segment(segment);

				// If we use a variable bit rate, run our optimization algorithm to find the optimal bit rates
				if (is_any_variable)
					find_optimal_bit_rates_dispatch(context);

				// If we need the contributing error of each frame, find it now before we quantize
				if (settings.metadata.include_contributing_error)
					find_contributing_error(context);

				// Quantize our streams now that we found the optimal bit rates
				quantize_all_streams(context);
			}

			// If we need the contributing error of each keyframe, sort them for the whole clip
			if (settings.metadata.include_contributing_error)
				sort_contributing_error(allocator, clip);

#if defined(ACL_USE_SJSON)
			compression_stats.bit_rate_optimization_elapsed_seconds = bit_rate_optimization_time.get_elapsed_seconds();
#endif

#if defined(ACL_USE_SJSON)
			if (are_all_enum_flags_set(out_stats.logging, stat_logging::detailed))
			{
				sjson::ObjectWriter& writer = *out_stats.writer;
				writer["track_bit_rate_database_size"] = static_cast<uint32_t>(context.bit_rate_database.get_allocated_size());

				size_t transform_cache_size = 0;
				transform_cache_size += sizeof(rtm::qvvf) * context.num_bones;	// raw_local_pose
				transform_cache_size += sizeof(rtm::qvvf) * context.num_bones;	// lossy_local_pose
				transform_cache_size += context.metric_transform_size * context.num_bones;	// lossy_object_pose
				transform_cache_size += context.metric_transform_size * context.num_bones * context.clip.segments->num_samples;	// raw_local_transforms
				transform_cache_size += context.metric_transform_size * context.num_bones * context.clip.segments->num_samples;	// raw_object_transforms

				if (context.needs_conversion)
					transform_cache_size += context.metric_transform_size * context.num_bones;	// local_transforms_converted

				if (context.has_additive_base)
				{
					transform_cache_size += sizeof(rtm::qvvf) * context.num_bones;	// additive_local_pose
					transform_cache_size += context.metric_transform_size * context.num_bones * context.clip.segments->num_samples;	// base_local_transforms
					transform_cache_size += context.metric_transform_size * context.num_bones * context.clip.segments->num_samples;	// base_object_transforms
				}

				writer["transform_cache_size"] = static_cast<uint32_t>(transform_cache_size);
			}
#endif
		}
	}

	ACL_IMPL_VERSION_NAMESPACE_END
}

ACL_IMPL_FILE_PRAGMA_POP
