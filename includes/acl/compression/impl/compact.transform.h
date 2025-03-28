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
#include "acl/core/track_formats.h"
#include "acl/core/impl/compiler_utils.h"
#include "acl/core/error.h"
#include "acl/core/scope_profiler.h"
#include "acl/compression/impl/clip_context.h"
#include "acl/compression/impl/compression_stats.h"
#include "acl/compression/impl/rigid_shell_utils.h"
#include "acl/compression/transform_error_metrics.h"

#include <rtm/qvvf.h>

#include <cstdint>

//////////////////////////////////////////////////////////////////////////
// Apply error correction after constant and default tracks are processed.
// Notes:
//     - original code was adapted and cleaned up a bit, but largely as contributed
//     - zero scale isn't properly handled and needs to be guarded against
//     - regression testing over large data sets shows that it is sometimes a win, sometimes not
//     - overall, it seems to be a net loss over the memory footprint and quality does not
//       measurably improve to justify the loss
//     - I tried various tweaks and failed to make the code a consistent win, see https://github.com/nfrechette/acl/issues/353

//#define ACL_IMPL_ENABLE_CONSTANT_ERROR_CORRECTION

// Original algorithm used by ACL 2.1
#define ACL_IMPL_CONSTANT_FOLDING_ALGO_ORIGINAL	0

// Enables a more precise version of constant sub-track folding that evaluates the error
// at each leaf and the dominant transform in object space
#define ACL_IMPL_CONSTANT_FOLDING_ALGO_PRECISE	1

// The currently used algorithm for constant folding
#define ACL_IMPL_CONSTANT_FOLDING_ALGO			ACL_IMPL_CONSTANT_FOLDING_ALGO_ORIGINAL

#ifdef ACL_IMPL_ENABLE_CONSTANT_ERROR_CORRECTION
#include "acl/compression/impl/normalize.transform.h"
#endif

ACL_IMPL_FILE_PRAGMA_PUSH

namespace acl
{
	ACL_IMPL_VERSION_NAMESPACE_BEGIN

	namespace acl_impl
	{

#if ACL_IMPL_CONSTANT_FOLDING_ALGO == ACL_IMPL_CONSTANT_FOLDING_ALGO_ORIGINAL

		// To detect if a sub-track is constant, we grab the first sample as our reference.
		// We then measure the object space error using the qvv error metric and our
		// dominant shell distance. If the error remains within our dominant precision
		// then the sub-track is constant. We perform the same test using the default
		// sub-track value to determine if it is a default sub-track.

		inline bool RTM_SIMD_CALL are_samples_constant(const compression_settings& settings,
			const clip_context& lossy_clip_context, const clip_context& additive_base_clip_context,
			rtm::vector4f_arg0 reference, uint32_t transform_index, animation_track_type8 sub_track_type)
		{
			const segment_context& lossy_segment = lossy_clip_context.segments[0];
			const transform_streams& raw_transform_stream = lossy_segment.bone_streams[transform_index];

			const rigid_shell_metadata_t& shell = lossy_clip_context.clip_shell_metadata[transform_index];

			const itransform_error_metric& error_metric = *settings.error_metric;

			const bool has_additive_base = lossy_clip_context.has_additive_base;
			const bool needs_conversion = error_metric.needs_conversion(lossy_clip_context.has_scale);

			const uint32_t dirty_transform_indices[2] = { 0, 1 };
			rtm::qvvf local_transforms[2];
			rtm::qvvf base_transforms[2];
			alignas(16) uint8_t local_transforms_converted[1024];	// Big enough for 2 transforms for sure
			alignas(16) uint8_t base_transforms_converted[1024];	// Big enough for 2 transforms for sure

			const size_t metric_transform_size = error_metric.get_transform_size(lossy_clip_context.has_scale);
			ACL_ASSERT(metric_transform_size * 2 <= sizeof(local_transforms_converted), "Transform size is too large");

			itransform_error_metric::convert_transforms_args convert_transforms_args_local;
			convert_transforms_args_local.dirty_transform_indices = &dirty_transform_indices[0];
			convert_transforms_args_local.num_dirty_transforms = 1;
			convert_transforms_args_local.num_transforms = 1;
			convert_transforms_args_local.is_additive_base = false;

			itransform_error_metric::convert_transforms_args convert_transforms_args_base;
			convert_transforms_args_base.dirty_transform_indices = &dirty_transform_indices[0];
			convert_transforms_args_base.num_dirty_transforms = 2;
			convert_transforms_args_base.num_transforms = 2;
			convert_transforms_args_base.transforms = &base_transforms[0];
			convert_transforms_args_base.is_additive_base = true;
			convert_transforms_args_base.is_lossy = false;

			itransform_error_metric::apply_additive_to_base_args apply_additive_to_base_args;
			apply_additive_to_base_args.dirty_transform_indices = &dirty_transform_indices[0];
			apply_additive_to_base_args.num_dirty_transforms = 2;
			apply_additive_to_base_args.base_transforms = needs_conversion ? (const void*)&base_transforms_converted[0] : (const void*)&base_transforms[0];
			apply_additive_to_base_args.local_transforms = needs_conversion ? (const void*)&local_transforms_converted[0] : (const void*)&local_transforms[0];
			apply_additive_to_base_args.num_transforms = 2;

			itransform_error_metric::calculate_error_args calculate_error_args;
			calculate_error_args.construct_sphere_shell(shell.local_shell_distance);
			calculate_error_args.transform0 = &local_transforms_converted[metric_transform_size * 0];
			calculate_error_args.transform1 = &local_transforms_converted[metric_transform_size * 1];

			const rtm::scalarf precision_sq = rtm::scalar_set(shell.precision * shell.precision);

			const uint32_t num_samples = lossy_clip_context.num_samples;
			for (uint32_t sample_index = 0; sample_index < num_samples; ++sample_index)
			{
				const rtm::quatf raw_rotation = raw_transform_stream.rotations.get_sample_clamped(sample_index);
				const rtm::vector4f raw_translation = raw_transform_stream.translations.get_sample_clamped(sample_index);
				const rtm::vector4f raw_scale = raw_transform_stream.scales.get_sample_clamped(sample_index);

				rtm::qvvf raw_transform = rtm::qvv_set(raw_rotation, raw_translation, raw_scale);
				rtm::qvvf lossy_transform = raw_transform;	// Copy the raw transform

				// Fix up our lossy transform with the reference value
				switch (sub_track_type)
				{
				case animation_track_type8::rotation:
				default:
					lossy_transform.rotation = rtm::vector_to_quat(reference);
					break;
				case animation_track_type8::translation:
					lossy_transform.translation = reference;
					break;
				case animation_track_type8::scale:
					lossy_transform.scale = reference;
					break;
				}

				local_transforms[0] = raw_transform;
				local_transforms[1] = lossy_transform;

				if (needs_conversion)
				{
					convert_transforms_args_local.sample_index = sample_index;

					convert_transforms_args_local.transforms = &local_transforms[0];
					convert_transforms_args_local.is_lossy = false;

					error_metric.convert_transforms(convert_transforms_args_local, &local_transforms_converted[metric_transform_size * 0]);

					convert_transforms_args_local.transforms = &local_transforms[1];
					convert_transforms_args_local.is_lossy = true;

					error_metric.convert_transforms(convert_transforms_args_local, &local_transforms_converted[metric_transform_size * 1]);
				}
				else
					std::memcpy(&local_transforms_converted[0], &local_transforms[0], metric_transform_size * 2);

				if (has_additive_base)
				{
					const segment_context& additive_base_segment = additive_base_clip_context.segments[0];
					const transform_streams& additive_base_bone_stream = additive_base_segment.bone_streams[transform_index];

					// The sample time is calculated from the full clip duration to be consistent with decompression
					const float sample_time = rtm::scalar_min(float(sample_index) / lossy_clip_context.sample_rate, lossy_clip_context.duration);

					const float normalized_sample_time = additive_base_segment.num_samples > 1 ? (sample_time / lossy_clip_context.duration) : 0.0F;
					const float additive_sample_time = additive_base_segment.num_samples > 1 ? (normalized_sample_time * additive_base_clip_context.duration) : 0.0F;

					// With uniform sample distributions, we do not interpolate.
					const uint32_t base_sample_index = get_uniform_sample_key(additive_base_segment, additive_sample_time);

					const rtm::quatf base_rotation = additive_base_bone_stream.rotations.get_sample_clamped(base_sample_index);
					const rtm::vector4f base_translation = additive_base_bone_stream.translations.get_sample_clamped(base_sample_index);
					const rtm::vector4f base_scale = additive_base_bone_stream.scales.get_sample_clamped(base_sample_index);

					const rtm::qvvf base_transform = rtm::qvv_set(base_rotation, base_translation, base_scale);

					base_transforms[0] = base_transform;
					base_transforms[1] = base_transform;

					if (needs_conversion)
					{
						convert_transforms_args_base.sample_index = base_sample_index;
						error_metric.convert_transforms(convert_transforms_args_base, &base_transforms_converted[0]);
					}
					else
						std::memcpy(&base_transforms_converted[0], &base_transforms[0], metric_transform_size * 2);

					error_metric.apply_additive_to_base(apply_additive_to_base_args, &local_transforms_converted[0]);
				}

				const rtm::scalarf vtx_error_sq = error_metric.calculate_error_squared(calculate_error_args);

				// If our error exceeds the desired precision, we are not constant
				if (rtm::scalar_greater_than(vtx_error_sq, precision_sq))
					return false;
			}

			// All samples were tested against the reference value and the error remained within tolerance
			return true;
		}

		inline bool are_rotations_constant(const compression_settings& settings, const clip_context& lossy_clip_context, const clip_context& additive_base_clip_context, uint32_t transform_index)
		{
			if (lossy_clip_context.num_samples == 0)
				return true;	// We are constant if we have no samples

			// When we are using full precision, we are only constant if range.min == range.max, meaning
			// we have a single unique and repeating sample
			// We want to test if we are binary exact
			// This is used by raw clips, we must preserve the original values
			if (settings.rotation_format == rotation_format8::quatf_full)
				return lossy_clip_context.ranges[transform_index].rotation.is_constant(0.0F);

			const segment_context& segment = lossy_clip_context.segments[0];
			const transform_streams& raw_transform_stream = segment.bone_streams[transform_index];

			// Otherwise check every sample to make sure we fall within the desired tolerance
			return are_samples_constant(settings, lossy_clip_context, additive_base_clip_context, rtm::quat_to_vector(raw_transform_stream.rotations.get_sample(0)), transform_index, animation_track_type8::rotation);
		}

		inline bool are_rotations_default(const compression_settings& settings, const clip_context& lossy_clip_context, const clip_context& additive_base_clip_context, const track_desc_transformf& desc, uint32_t transform_index)
		{
			if (lossy_clip_context.num_samples == 0)
				return true;	// We are default if we have no samples

			const rtm::vector4f default_bind_rotation = rtm::quat_to_vector(desc.default_value.rotation);

			// When we are using full precision, we are only default if (sample 0 == default value), meaning
			// we have a single unique and repeating default sample
			// We want to test if we are binary exact
			// This is used by raw clips, we must preserve the original values
			if (settings.rotation_format == rotation_format8::quatf_full)
			{
				const segment_context& segment = lossy_clip_context.segments[0];
				const transform_streams& raw_transform_stream = segment.bone_streams[transform_index];

				const rtm::vector4f rotation = raw_transform_stream.rotations.get_raw_sample<rtm::vector4f>(0);
				return rtm::vector_all_equal(rotation, default_bind_rotation);
			}

			// Otherwise check every sample to make sure we fall within the desired tolerance
			return are_samples_constant(settings, lossy_clip_context, additive_base_clip_context, default_bind_rotation, transform_index, animation_track_type8::rotation);
		}

		inline bool are_translations_constant(const compression_settings& settings, const clip_context& lossy_clip_context, const clip_context& additive_base_clip_context, uint32_t transform_index)
		{
			if (lossy_clip_context.num_samples == 0)
				return true;	// We are constant if we have no samples

			// When we are using full precision, we are only constant if range.min == range.max, meaning
			// we have a single unique and repeating sample
			// We want to test if we are binary exact
			// This is used by raw clips, we must preserve the original values
			if (settings.translation_format == vector_format8::vector3f_full)
				return lossy_clip_context.ranges[transform_index].translation.is_constant(0.0F);

			const segment_context& segment = lossy_clip_context.segments[0];
			const transform_streams& raw_transform_stream = segment.bone_streams[transform_index];

			// Otherwise check every sample to make sure we fall within the desired tolerance
			return are_samples_constant(settings, lossy_clip_context, additive_base_clip_context, raw_transform_stream.translations.get_sample(0), transform_index, animation_track_type8::translation);
		}

		inline bool are_translations_default(const compression_settings& settings, const clip_context& lossy_clip_context, const clip_context& additive_base_clip_context, const track_desc_transformf& desc, uint32_t transform_index)
		{
			if (lossy_clip_context.num_samples == 0)
				return true;	// We are default if we have no samples

			const rtm::vector4f default_bind_translation = desc.default_value.translation;

			// When we are using full precision, we are only default if (sample 0 == default value), meaning
			// we have a single unique and repeating default sample
			// We want to test if we are binary exact
			// This is used by raw clips, we must preserve the original values
			if (settings.translation_format == vector_format8::vector3f_full)
			{
				const segment_context& segment = lossy_clip_context.segments[0];
				const transform_streams& raw_transform_stream = segment.bone_streams[transform_index];

				const rtm::vector4f translation = raw_transform_stream.translations.get_raw_sample<rtm::vector4f>(0);
				return rtm::vector_all_equal(translation, default_bind_translation);
			}

			// Otherwise check every sample to make sure we fall within the desired tolerance
			return are_samples_constant(settings, lossy_clip_context, additive_base_clip_context, default_bind_translation, transform_index, animation_track_type8::translation);
		}

		inline bool are_scales_constant(const compression_settings& settings, const clip_context& lossy_clip_context, const clip_context& additive_base_clip_context, uint32_t transform_index)
		{
			if (lossy_clip_context.num_samples == 0)
				return true;	// We are constant if we have no samples

			if (!lossy_clip_context.has_scale)
				return true;	// We are constant if we have no scale

			// When we are using full precision, we are only constant if range.min == range.max, meaning
			// we have a single unique and repeating sample
			// We want to test if we are binary exact
			// This is used by raw clips, we must preserve the original values
			if (settings.scale_format == vector_format8::vector3f_full)
				return lossy_clip_context.ranges[transform_index].scale.is_constant(0.0F);

			const segment_context& segment = lossy_clip_context.segments[0];
			const transform_streams& raw_transform_stream = segment.bone_streams[transform_index];

			// Otherwise check every sample to make sure we fall within the desired tolerance
			return are_samples_constant(settings, lossy_clip_context, additive_base_clip_context, raw_transform_stream.scales.get_sample(0), transform_index, animation_track_type8::scale);
		}

		inline bool are_scales_default(const compression_settings& settings, const clip_context& lossy_clip_context, const clip_context& additive_base_clip_context, const track_desc_transformf& desc, uint32_t transform_index)
		{
			if (lossy_clip_context.num_samples == 0)
				return true;	// We are default if we have no samples

			if (!lossy_clip_context.has_scale)
				return true;	// We are default if we have no scale

			const rtm::vector4f default_bind_scale = desc.default_value.scale;

			// When we are using full precision, we are only default if (sample 0 == default value), meaning
			// we have a single unique and repeating default sample
			// We want to test if we are binary exact
			// This is used by raw clips, we must preserve the original values
			if (settings.scale_format == vector_format8::vector3f_full)
			{
				const segment_context& segment = lossy_clip_context.segments[0];
				const transform_streams& raw_transform_stream = segment.bone_streams[transform_index];

				const rtm::vector4f scale = raw_transform_stream.scales.get_raw_sample<rtm::vector4f>(0);
				return rtm::vector_all_equal(scale, default_bind_scale);
			}

			// Otherwise check every sample to make sure we fall within the desired tolerance
			return are_samples_constant(settings, lossy_clip_context, additive_base_clip_context, default_bind_scale, transform_index, animation_track_type8::scale);
		}

		// Compacts constant sub-tracks
		// A sub-track is constant if every sample can be replaced by a single unique sample without exceeding
		// our error threshold.
		// By default, constant sub-tracks will retain the first sample.
		// A constant sub-track is a default sub-track if its unique sample can be replaced by the default value
		// without exceeding our error threshold.
		inline void compact_constant_streams(
			iallocator& allocator,
			clip_context& context,
			const clip_context& additive_base_clip_context,
			const track_array_qvvf& track_list,
			const compression_settings& settings,
			compression_stats_t& compression_stats)
		{
			(void)compression_stats;

#if defined(ACL_USE_SJSON)
			scope_profiler compact_constant_sub_tracks_time;
#endif

			ACL_ASSERT(context.num_segments == 1, "context must contain a single segment!");

			segment_context& segment = context.segments[0];

			const uint32_t num_transforms = context.num_bones;
			const uint32_t num_samples = context.num_samples;

			uint32_t num_default_bone_scales = 0;

#ifdef ACL_IMPL_ENABLE_CONSTANT_ERROR_CORRECTION
			bool has_constant_bone_rotations = false;
			bool has_constant_bone_translations = false;
			bool has_constant_bone_scales = false;
#endif

			// Iterate in any order, doesn't matter
			for (uint32_t transform_index = 0; transform_index < num_transforms; ++transform_index)
			{
				const track_desc_transformf& desc = track_list[transform_index].get_description();

				transform_streams& bone_stream = segment.bone_streams[transform_index];

				transform_range& bone_range = context.ranges[transform_index];

				ACL_ASSERT(bone_stream.rotations.get_num_samples() == num_samples, "Rotation sample mismatch!");
				ACL_ASSERT(bone_stream.translations.get_num_samples() == num_samples, "Translation sample mismatch!");
				ACL_ASSERT(bone_stream.scales.get_num_samples() == num_samples, "Scale sample mismatch!");

				// We expect all our samples to have the same width of sizeof(rtm::vector4f)
				ACL_ASSERT(bone_stream.rotations.get_sample_size() == sizeof(rtm::vector4f), "Unexpected rotation sample size. %u != %zu", bone_stream.rotations.get_sample_size(), sizeof(rtm::vector4f));
				ACL_ASSERT(bone_stream.translations.get_sample_size() == sizeof(rtm::vector4f), "Unexpected translation sample size. %u != %zu", bone_stream.translations.get_sample_size(), sizeof(rtm::vector4f));
				ACL_ASSERT(bone_stream.scales.get_sample_size() == sizeof(rtm::vector4f), "Unexpected scale sample size. %u != %zu", bone_stream.scales.get_sample_size(), sizeof(rtm::vector4f));

				if (are_rotations_constant(settings, context, additive_base_clip_context, transform_index))
				{
					rotation_track_stream constant_stream(allocator, 1, bone_stream.rotations.get_sample_size(), bone_stream.rotations.get_sample_rate(), bone_stream.rotations.get_rotation_format());

					const rtm::vector4f default_bind_rotation = rtm::quat_to_vector(desc.default_value.rotation);

					rtm::vector4f rotation = num_samples != 0 ? bone_stream.rotations.get_raw_sample<rtm::vector4f>(0) : default_bind_rotation;

					bone_stream.is_rotation_constant = true;

					if (are_rotations_default(settings, context, additive_base_clip_context, desc, transform_index))
					{
						bone_stream.is_rotation_default = true;
						rotation = default_bind_rotation;
					}

					constant_stream.set_raw_sample(0, rotation);
					bone_stream.rotations = std::move(constant_stream);

					bone_range.rotation = track_stream_range::from_min_extent(rotation, rtm::vector_zero());

#ifdef ACL_IMPL_ENABLE_CONSTANT_ERROR_CORRECTION
					has_constant_bone_rotations = true;
#endif
				}

				if (are_translations_constant(settings, context, additive_base_clip_context, transform_index))
				{
					translation_track_stream constant_stream(allocator, 1, bone_stream.translations.get_sample_size(), bone_stream.translations.get_sample_rate(), bone_stream.translations.get_vector_format());

					const rtm::vector4f default_bind_translation = desc.default_value.translation;

					rtm::vector4f translation = num_samples != 0 ? bone_stream.translations.get_raw_sample<rtm::vector4f>(0) : default_bind_translation;

					bone_stream.is_translation_constant = true;

					if (are_translations_default(settings, context, additive_base_clip_context, desc, transform_index))
					{
						bone_stream.is_translation_default = true;
						translation = default_bind_translation;
					}

					constant_stream.set_raw_sample(0, translation);
					bone_stream.translations = std::move(constant_stream);

					// Zero out W, could be garbage
					bone_range.translation = track_stream_range::from_min_extent(rtm::vector_set_w(translation, 0.0F), rtm::vector_zero());

#ifdef ACL_IMPL_ENABLE_CONSTANT_ERROR_CORRECTION
					has_constant_bone_translations = true;
#endif
				}

				if (are_scales_constant(settings, context, additive_base_clip_context, transform_index))
				{
					scale_track_stream constant_stream(allocator, 1, bone_stream.scales.get_sample_size(), bone_stream.scales.get_sample_rate(), bone_stream.scales.get_vector_format());

					const rtm::vector4f default_bind_scale = desc.default_value.scale;

					rtm::vector4f scale = (context.has_scale && num_samples != 0) ? bone_stream.scales.get_raw_sample<rtm::vector4f>(0) : default_bind_scale;

					bone_stream.is_scale_constant = true;

					if (are_scales_default(settings, context, additive_base_clip_context, desc, transform_index))
					{
						bone_stream.is_scale_default = true;
						scale = default_bind_scale;
					}

					constant_stream.set_raw_sample(0, scale);
					bone_stream.scales = std::move(constant_stream);

					// Zero out W, could be garbage
					bone_range.scale = track_stream_range::from_min_extent(rtm::vector_set_w(scale, 0.0F), rtm::vector_zero());

					num_default_bone_scales += bone_stream.is_scale_default ? 1 : 0;

#ifdef ACL_IMPL_ENABLE_CONSTANT_ERROR_CORRECTION
					has_constant_bone_scales = true;
#endif
				}
			}

			const bool has_scale = num_default_bone_scales != num_transforms;
			context.has_scale = has_scale;

#ifdef ACL_IMPL_ENABLE_CONSTANT_ERROR_CORRECTION

			// Only perform error compensation if our format isn't raw
			const bool is_raw = settings.rotation_format == rotation_format8::quatf_full || settings.translation_format == vector_format8::vector3f_full || settings.scale_format == vector_format8::vector3f_full;

			// Only perform error compensation if we are lossy due to constant sub-tracks
			// In practice, even if we have no constant sub-tracks, we could be lossy if our rotations drop W
			const bool is_lossy = has_constant_bone_rotations || has_constant_bone_translations || (has_scale && has_constant_bone_scales);

			if (!context.has_additive_base && !is_raw && is_lossy)
			{
				// Apply error correction after constant and default tracks are processed.
				// We use object space of the original data as ground truth, and only deviate for 2 reasons, and as briefly as possible.
				//    -Replace an original local value with a new constant value.
				//    -Correct for the manipulation of an original local value by an ancestor ASAP.
				// We aren't modifying raw data here. We're modifying the raw channels generated from the raw data.
				// The raw data is left alone, and is still used at the end of the process to do regression testing.

				struct dirty_state_t
				{
					bool rotation = false;
					bool translation = false;
					bool scale = false;
				};

				dirty_state_t any_constant_changed;
				dirty_state_t* dirty_states = allocate_type_array<dirty_state_t>(allocator, num_transforms);
				rtm::qvvf* original_object_pose = allocate_type_array<rtm::qvvf>(allocator, num_transforms);
				rtm::qvvf* adjusted_object_pose = allocate_type_array<rtm::qvvf>(allocator, num_transforms);

				for (uint32_t sample_index = 0; sample_index < num_samples; ++sample_index)
				{
					// Iterate over parent transforms first
					for (uint32_t bone_index : context.topology->roots_first_iterator())
					{
						rtm::qvvf& original_object_transform = original_object_pose[bone_index];

						const transform_range& bone_range = context.ranges[bone_index];
						transform_streams& bone_stream = segment.bone_streams[bone_index];
						transform_streams& raw_bone_stream = raw_segment.bone_streams[bone_index];

						const track_desc_transformf& desc = track_list[bone_index].get_description();
						const uint32_t parent_bone_index = desc.parent_index;
						const rtm::qvvf original_local_transform = rtm::qvv_set(
							raw_bone_stream.rotations.get_raw_sample<rtm::quatf>(sample_index),
							raw_bone_stream.translations.get_raw_sample<rtm::vector4f>(sample_index),
							raw_bone_stream.scales.get_raw_sample<rtm::vector4f>(sample_index));

						if (parent_bone_index == k_invalid_track_index)
							original_object_transform = original_local_transform;	// Just copy the root as-is, it has no parent and thus local and object space transforms are equal
						else if (!has_scale)
							original_object_transform = rtm::qvv_normalize(rtm::qvv_mul_no_scale(original_local_transform, original_object_pose[parent_bone_index]));
						else
							original_object_transform = rtm::qvv_normalize(rtm::qvv_mul(original_local_transform, original_object_pose[parent_bone_index]));

						rtm::qvvf adjusted_local_transform = original_local_transform;

						dirty_state_t& constant_changed = dirty_states[bone_index];
						constant_changed.rotation = false;
						constant_changed.translation = false;
						constant_changed.scale = false;

						if (bone_stream.is_rotation_constant)
						{
							const rtm::quatf constant_rotation = rtm::vector_to_quat(bone_range.rotation.get_min());
							if (!rtm::vector_all_near_equal(rtm::quat_to_vector(adjusted_local_transform.rotation), rtm::quat_to_vector(constant_rotation), 0.0F))
							{
								any_constant_changed.rotation = true;
								constant_changed.rotation = true;
								adjusted_local_transform.rotation = constant_rotation;
								raw_bone_stream.rotations.set_raw_sample(sample_index, constant_rotation);
							}
							ACL_ASSERT(bone_stream.rotations.get_num_samples() == 1, "Constant rotation stream mismatch!");
							ACL_ASSERT(rtm::vector_all_near_equal(bone_stream.rotations.get_raw_sample<rtm::vector4f>(0), rtm::quat_to_vector(constant_rotation), 0.0F), "Constant rotation mismatch!");
						}

						if (bone_stream.is_translation_constant)
						{
							const rtm::vector4f constant_translation = bone_range.translation.get_min();
							if (!rtm::vector_all_near_equal3(adjusted_local_transform.translation, constant_translation, 0.0F))
							{
								any_constant_changed.translation = true;
								constant_changed.translation = true;
								adjusted_local_transform.translation = constant_translation;
								raw_bone_stream.translations.set_raw_sample(sample_index, constant_translation);
							}
							ACL_ASSERT(bone_stream.translations.get_num_samples() == 1, "Constant translation stream mismatch!");
							ACL_ASSERT(rtm::vector_all_near_equal3(bone_stream.translations.get_raw_sample<rtm::vector4f>(0), constant_translation, 0.0F), "Constant translation mismatch!");
						}

						if (has_scale && bone_stream.is_scale_constant)
						{
							const rtm::vector4f constant_scale = bone_range.scale.get_min();
							if (!rtm::vector_all_near_equal3(adjusted_local_transform.scale, constant_scale, 0.0F))
							{
								any_constant_changed.scale = true;
								constant_changed.scale = true;
								adjusted_local_transform.scale = constant_scale;
								raw_bone_stream.scales.set_raw_sample(sample_index, constant_scale);
							}
							ACL_ASSERT(bone_stream.scales.get_num_samples() == 1, "Constant scale stream mismatch!");
							ACL_ASSERT(rtm::vector_all_near_equal3(bone_stream.scales.get_raw_sample<rtm::vector4f>(0), constant_scale, 0.0F), "Constant scale mismatch!");
						}

						rtm::qvvf& adjusted_object_transform = adjusted_object_pose[bone_index];
						if (parent_bone_index == k_invalid_track_index)
						{
							adjusted_object_transform = adjusted_local_transform;	// Just copy the root as-is, it has no parent and thus local and object space transforms are equal
						}
						else
						{
							const dirty_state_t& parent_constant_changed = dirty_states[parent_bone_index];
							const rtm::qvvf& parent_adjusted_object_transform = adjusted_object_pose[parent_bone_index];

							if (bone_stream.is_rotation_constant && !constant_changed.rotation)
								constant_changed.rotation = parent_constant_changed.rotation;
							if (bone_stream.is_translation_constant && !constant_changed.translation)
								constant_changed.translation = parent_constant_changed.translation;
							if (has_scale && bone_stream.is_scale_constant && !constant_changed.scale)
								constant_changed.scale = parent_constant_changed.scale;

							// Compensate for the constant changes in your ancestors.
							if (!bone_stream.is_rotation_constant && parent_constant_changed.rotation)
							{
								ACL_ASSERT(any_constant_changed.rotation, "No rotations have changed!");
								adjusted_local_transform.rotation = quat_normalize_stable(rtm::quat_mul(original_object_transform.rotation, rtm::quat_conjugate(parent_adjusted_object_transform.rotation)));
								raw_bone_stream.rotations.set_raw_sample(sample_index, adjusted_local_transform.rotation);
								bone_stream.rotations.set_raw_sample(sample_index, adjusted_local_transform.rotation);
							}

							if (has_scale)
							{
								if (!bone_stream.is_translation_constant && (parent_constant_changed.rotation || parent_constant_changed.translation || parent_constant_changed.scale))
								{
									ACL_ASSERT(any_constant_changed.rotation || any_constant_changed.translation || any_constant_changed.scale, "No channels have changed!");
									const rtm::quatf inv_rotation = rtm::quat_conjugate(parent_adjusted_object_transform.rotation);
									const rtm::vector4f inv_scale = rtm::vector_reciprocal(parent_adjusted_object_transform.scale);
									adjusted_local_transform.translation = rtm::vector_mul(rtm::quat_mul_vector3(rtm::vector_sub(original_object_transform.translation, parent_adjusted_object_transform.translation), inv_rotation), inv_scale);
									raw_bone_stream.translations.set_raw_sample(sample_index, adjusted_local_transform.translation);
									bone_stream.translations.set_raw_sample(sample_index, adjusted_local_transform.translation);
								}

								if (!bone_stream.is_scale_constant && parent_constant_changed.scale)
								{
									ACL_ASSERT(any_constant_changed.scale, "No scales have changed!");
									adjusted_local_transform.scale = rtm::vector_mul(original_object_transform.scale, rtm::vector_reciprocal(parent_adjusted_object_transform.scale));
									raw_bone_stream.scales.set_raw_sample(sample_index, adjusted_local_transform.scale);
									bone_stream.scales.set_raw_sample(sample_index, adjusted_local_transform.scale);
								}

								adjusted_object_transform = rtm::qvv_normalize(rtm::qvv_mul(adjusted_local_transform, parent_adjusted_object_transform));
							}
							else
							{
								if (!bone_stream.is_translation_constant && (parent_constant_changed.rotation || parent_constant_changed.translation))
								{
									ACL_ASSERT(any_constant_changed.rotation || any_constant_changed.translation, "No channels have changed!");
									const rtm::quatf inv_rotation = rtm::quat_conjugate(parent_adjusted_object_transform.rotation);
									adjusted_local_transform.translation = rtm::quat_mul_vector3(rtm::vector_sub(original_object_transform.translation, parent_adjusted_object_transform.translation), inv_rotation);
									raw_bone_stream.translations.set_raw_sample(sample_index, adjusted_local_transform.translation);
									bone_stream.translations.set_raw_sample(sample_index, adjusted_local_transform.translation);
								}

								adjusted_object_transform = rtm::qvv_normalize(rtm::qvv_mul_no_scale(adjusted_local_transform, parent_adjusted_object_transform));
							}
						}
					}
				}
				deallocate_type_array(allocator, adjusted_object_pose, num_transforms);
				deallocate_type_array(allocator, original_object_pose, num_transforms);
				deallocate_type_array(allocator, dirty_states, num_transforms);

				// We need to do these again, to account for error correction.
				if(any_constant_changed.rotation)
				{
					convert_rotation_streams(allocator, context, settings.rotation_format);
				}

				if (any_constant_changed.rotation || any_constant_changed.translation || any_constant_changed.scale)
				{
					deallocate_type_array(allocator, context.ranges, num_transforms);
					extract_clip_bone_ranges(allocator, context);
				}
			}
#endif

#if defined(ACL_USE_SJSON)
			compression_stats.compact_constant_sub_tracks_elapsed_seconds = compact_constant_sub_tracks_time.get_elapsed_seconds();
#endif
		}

#elif ACL_IMPL_CONSTANT_FOLDING_ALGO == ACL_IMPL_CONSTANT_FOLDING_ALGO_PRECISE

		// Idea is to break a chain of object space transforms:
		//     (c2 * c1) * t * (p2 * p1)
		// Where c1, c2 are children and c2 is a leaf and p1, p2 are parents and p1 is the root
		// This maintains the same multiplication order and reduces the memory footprint since we only need to retain 1x transform for
		// (p2 * p1) and 1x transform per leaf for (c2 * c1)
		// We can't cache these and lazily populate them since at this point in time, we have a single segment for the whole clip and it
		// could be quite large. Caching would require duplicating the entire clip which we'd like to avoid at the cost of redoing work.
		//
		// Once we have our root_to_parent and child_to_leaf transforms, we can begin testing if the current transform sub-tracks can
		// be constant or default. Each time we tweak the sub-track, we can recompute the current local space transform ('t' above)
		// and re-compute the object space transform of each leaf cheaply. We then test each leaf's precision.
		//
		// We must also test the dominant transform, not just leaves.
		//
		// We have a list of permutation that we'd like to try for every transform and we try them for every sample.
		// We keep track of which permutations are ruled out as we go along and once all permutations have been tested
		// over every sample, we pick the one with the lowest cost.
		//
		// Despite our transform caching, this algorithm is much slower than the original. This is because while the original works
		// in local space (and is thus O(num_transforms * num_samples)), this new one works in object space which makes it:
		// O(num_transforms * num_transforms * num_samples). Perhaps we can improve on this by using the cummulative transform
		// distance and the cummulative error to cheaply test an approximate solution in local space.
		//

		enum class constant_rotation_value8 : uint8_t
		{
			animated_quatf_full,
			constant_quatf_drop_w_full,
			default_quatf_full,
		};

		static constexpr uint8_t k_constant_rotation_cost[] = { 100, 10, 1 };
		static constexpr size_t k_num_constant_rotation_values = 3;

		enum class constant_translation_value8 : uint8_t
		{
			animated_vector3f_full,
			constant_vector3f_full,
			default_vector3f,
		};

		static constexpr uint8_t k_constant_translation_cost[] = { 100, 10, 1 };
		static constexpr size_t k_num_constant_translation_values = 3;

		enum class constant_scale_value8 : uint8_t
		{
			animated_vector3f_full,
			constant_vector3f_full,
			default_vector3f,
		};

		static constexpr uint8_t k_constant_scale_cost[] = { 100, 10, 1 };
		static constexpr size_t k_num_constant_scale_values = 3;

		struct constant_permutation_t
		{
			constant_rotation_value8 rotation;
			constant_translation_value8 translation;
			constant_scale_value8 scale;

			constexpr uint32_t get_cost() const { return k_constant_rotation_cost[static_cast<uint32_t>(rotation)] + k_constant_translation_cost[static_cast<uint32_t>(translation)] + k_constant_scale_cost[static_cast<uint32_t>(scale)]; }
		};

		static constexpr constant_permutation_t k_constant_permutations[] =
		{
			// We don't include the full animated value since it'll be there by default if all other permutations fail
			//{ constant_rotation_value8::animated_quatf_full, constant_translation_value8::animated_vector3f_full, constant_scale_value8::animated_vector3f_full },
			{ constant_rotation_value8::animated_quatf_full, constant_translation_value8::animated_vector3f_full, constant_scale_value8::constant_vector3f_full },
			{ constant_rotation_value8::animated_quatf_full, constant_translation_value8::animated_vector3f_full, constant_scale_value8::default_vector3f },

			{ constant_rotation_value8::animated_quatf_full, constant_translation_value8::constant_vector3f_full, constant_scale_value8::animated_vector3f_full },
			{ constant_rotation_value8::animated_quatf_full, constant_translation_value8::constant_vector3f_full, constant_scale_value8::constant_vector3f_full },
			{ constant_rotation_value8::animated_quatf_full, constant_translation_value8::constant_vector3f_full, constant_scale_value8::default_vector3f },

			{ constant_rotation_value8::animated_quatf_full, constant_translation_value8::default_vector3f, constant_scale_value8::animated_vector3f_full },
			{ constant_rotation_value8::animated_quatf_full, constant_translation_value8::default_vector3f, constant_scale_value8::constant_vector3f_full },
			{ constant_rotation_value8::animated_quatf_full, constant_translation_value8::default_vector3f, constant_scale_value8::default_vector3f },

			{ constant_rotation_value8::constant_quatf_drop_w_full, constant_translation_value8::animated_vector3f_full, constant_scale_value8::animated_vector3f_full },
			{ constant_rotation_value8::constant_quatf_drop_w_full, constant_translation_value8::animated_vector3f_full, constant_scale_value8::constant_vector3f_full },
			{ constant_rotation_value8::constant_quatf_drop_w_full, constant_translation_value8::animated_vector3f_full, constant_scale_value8::default_vector3f },

			{ constant_rotation_value8::constant_quatf_drop_w_full, constant_translation_value8::constant_vector3f_full, constant_scale_value8::animated_vector3f_full },
			{ constant_rotation_value8::constant_quatf_drop_w_full, constant_translation_value8::constant_vector3f_full, constant_scale_value8::constant_vector3f_full },
			{ constant_rotation_value8::constant_quatf_drop_w_full, constant_translation_value8::constant_vector3f_full, constant_scale_value8::default_vector3f },

			{ constant_rotation_value8::constant_quatf_drop_w_full, constant_translation_value8::default_vector3f, constant_scale_value8::animated_vector3f_full },
			{ constant_rotation_value8::constant_quatf_drop_w_full, constant_translation_value8::default_vector3f, constant_scale_value8::constant_vector3f_full },
			{ constant_rotation_value8::constant_quatf_drop_w_full, constant_translation_value8::default_vector3f, constant_scale_value8::default_vector3f },

			{ constant_rotation_value8::default_quatf_full, constant_translation_value8::animated_vector3f_full, constant_scale_value8::animated_vector3f_full },
			{ constant_rotation_value8::default_quatf_full, constant_translation_value8::animated_vector3f_full, constant_scale_value8::constant_vector3f_full },
			{ constant_rotation_value8::default_quatf_full, constant_translation_value8::animated_vector3f_full, constant_scale_value8::default_vector3f },

			{ constant_rotation_value8::default_quatf_full, constant_translation_value8::constant_vector3f_full, constant_scale_value8::animated_vector3f_full },
			{ constant_rotation_value8::default_quatf_full, constant_translation_value8::constant_vector3f_full, constant_scale_value8::constant_vector3f_full },
			{ constant_rotation_value8::default_quatf_full, constant_translation_value8::constant_vector3f_full, constant_scale_value8::default_vector3f },

			{ constant_rotation_value8::default_quatf_full, constant_translation_value8::default_vector3f, constant_scale_value8::animated_vector3f_full },
			{ constant_rotation_value8::default_quatf_full, constant_translation_value8::default_vector3f, constant_scale_value8::constant_vector3f_full },
			{ constant_rotation_value8::default_quatf_full, constant_translation_value8::default_vector3f, constant_scale_value8::default_vector3f },
		};

		static constexpr size_t k_num_constant_permutations = acl::get_array_size(k_constant_permutations);

		inline void compact_constant_streams(
			iallocator& allocator,
			clip_context& lossy_clip_context,
			const clip_context& additive_base_clip_context,
			const track_array_qvvf& track_list,
			const compression_settings& settings,
			compression_stats_t& compression_stats)
		{
			(void)compression_stats;

			const uint32_t num_transforms = lossy_clip_context.num_bones;
			if (num_transforms == 0)
				return;

			const uint32_t num_samples = lossy_clip_context.num_samples;

#if defined(ACL_USE_SJSON)
			scope_profiler compact_constant_sub_tracks_time;
#endif

			ACL_ASSERT(lossy_clip_context.num_segments == 1, "context must contain a single segment!");

			segment_context& segment = lossy_clip_context.segments[0];
			const clip_topology_t* topology = lossy_clip_context.topology;

			const itransform_error_metric& error_metric = *settings.error_metric;
			if (error_metric.needs_conversion(true))	// We haven't stripped scale yet
				return;	// Not supported

			itransform_error_metric::calculate_error_args calculate_error_args;

			const bool has_additive_base = lossy_clip_context.has_additive_base;
			const float sample_rate = lossy_clip_context.sample_rate;
			const float duration = lossy_clip_context.duration;
			const additive_clip_format8 additive_format = lossy_clip_context.additive_format;

			rtm::quatf rotation_values[k_num_constant_rotation_values];
			rtm::vector4f translation_values[k_num_constant_translation_values];
			rtm::vector4f scale_values[k_num_constant_scale_values];

			bool* needed_transforms = allocate_type_array<bool>(allocator, num_transforms);
			rtm::qvvf* sample_transforms = allocate_type_array<rtm::qvvf>(allocator, num_transforms);
			rtm::qvvf* child_to_leaf_transforms = allocate_type_array<rtm::qvvf>(allocator, topology->num_max_leaves_per_transform);
			rtm::qvvf child_to_dominant_transform = rtm::qvv_identity();
			rtm::qvvf parent_to_root_transform;

			uint32_t num_default_bone_scales = 0;

			// Start compacting at the leaves and work towards the root
			for (uint32_t transform_index_to_test : topology->leaves_first_iterator())
			{
				const track_desc_transformf& transform_desc_to_test = track_list[transform_index_to_test].get_description();

				transform_streams& bone_stream_to_test = segment.bone_streams[transform_index_to_test];
				transform_range& bone_range_to_test = lossy_clip_context.ranges[transform_index_to_test];

				ACL_ASSERT(bone_stream_to_test.rotations.get_num_samples() == num_samples, "Rotation sample mismatch!");
				ACL_ASSERT(bone_stream_to_test.translations.get_num_samples() == num_samples, "Translation sample mismatch!");
				ACL_ASSERT(bone_stream_to_test.scales.get_num_samples() == num_samples, "Scale sample mismatch!");

				// We expect all our samples to have the same width of sizeof(rtm::vector4f)
				ACL_ASSERT(bone_stream_to_test.rotations.get_sample_size() == sizeof(rtm::vector4f), "Unexpected rotation sample size. %u != %zu", bone_stream_to_test.rotations.get_sample_size(), sizeof(rtm::vector4f));
				ACL_ASSERT(bone_stream_to_test.translations.get_sample_size() == sizeof(rtm::vector4f), "Unexpected translation sample size. %u != %zu", bone_stream_to_test.translations.get_sample_size(), sizeof(rtm::vector4f));
				ACL_ASSERT(bone_stream_to_test.scales.get_sample_size() == sizeof(rtm::vector4f), "Unexpected scale sample size. %u != %zu", bone_stream_to_test.scales.get_sample_size(), sizeof(rtm::vector4f));

				// If we have no samples, we consider everything to be default sub-tracks
				if (num_samples == 0)
				{
					bone_stream_to_test.is_rotation_constant = true;
					bone_stream_to_test.is_rotation_default = true;
					bone_stream_to_test.is_translation_constant = true;
					bone_stream_to_test.is_translation_default = true;
					bone_stream_to_test.is_scale_constant = true;
					bone_stream_to_test.is_scale_default = true;
					num_default_bone_scales++;
					continue;
				}

				// Every permutation starts out as valid, we'll rule them out one by one
				bool is_candidate_permutation_valid[k_num_constant_permutations];
				uint32_t num_candidate_permutations = k_num_constant_permutations;
				std::fill_n(is_candidate_permutation_valid, k_num_constant_permutations, true);

				// When we are using full precision, we are only constant if range.min == range.max, meaning
				// we have a single unique and repeating sample
				// When we are using full precision, we are only default if (sample 0 == default value), meaning
				// we have a single unique and repeating default sample
				// We want to test if we are binary exact
				// This is used by raw clips, we must preserve the original values
				{
					if (settings.rotation_format == rotation_format8::quatf_full)
					{
						if (bone_range_to_test.rotation.is_constant(0.0F))
						{
							const rtm::quatf rotation = bone_stream_to_test.rotations.get_raw_sample<rtm::quatf>(0);
							const rtm::quatf default_bind_rotation = transform_desc_to_test.default_value.rotation;
							if (!rtm::quat_are_equal(rotation, default_bind_rotation))
							{
								// We are not default but we are constant, prune out all default permutations
								for (size_t permutation_index = 0; permutation_index < k_num_constant_permutations; ++permutation_index)
								{
									if (k_constant_permutations[permutation_index].rotation == constant_rotation_value8::default_quatf_full)
									{
										is_candidate_permutation_valid[permutation_index] = false;
										num_candidate_permutations--;
									}
								}
							}
						}
						else
						{
							// We are not constant, prune out all constant/default permutations
							for (size_t permutation_index = 0; permutation_index < k_num_constant_permutations; ++permutation_index)
							{
								if (k_constant_permutations[permutation_index].rotation != constant_rotation_value8::animated_quatf_full)
								{
									is_candidate_permutation_valid[permutation_index] = false;
									num_candidate_permutations--;
								}
							}
						}
					}

					if (settings.translation_format == vector_format8::vector3f_full)
					{
						if (bone_range_to_test.translation.is_constant(0.0F))
						{
							const rtm::vector4f translation = bone_stream_to_test.translations.get_raw_sample<rtm::vector4f>(0);
							const rtm::vector4f default_bind_translation = transform_desc_to_test.default_value.translation;
							if (!rtm::vector_all_equal3(translation, default_bind_translation))
							{
								// We are not default but we are constant, prune out all default permutations
								for (size_t permutation_index = 0; permutation_index < k_num_constant_permutations; ++permutation_index)
								{
									if (k_constant_permutations[permutation_index].translation == constant_translation_value8::default_vector3f)
									{
										is_candidate_permutation_valid[permutation_index] = false;
										num_candidate_permutations--;
									}
								}
							}
						}
						else
						{
							// We are not constant, prune out all constant/default permutations
							for (size_t permutation_index = 0; permutation_index < k_num_constant_permutations; ++permutation_index)
							{
								if (k_constant_permutations[permutation_index].translation != constant_translation_value8::animated_vector3f_full)
								{
									is_candidate_permutation_valid[permutation_index] = false;
									num_candidate_permutations--;
								}
							}
						}
					}

					if (settings.scale_format == vector_format8::vector3f_full)
					{
						if (bone_range_to_test.scale.is_constant(0.0F))
						{
							const rtm::vector4f scale = bone_stream_to_test.scales.get_raw_sample<rtm::vector4f>(0);
							const rtm::vector4f default_bind_scale = transform_desc_to_test.default_value.scale;
							if (!rtm::vector_all_equal3(scale, default_bind_scale))
							{
								// We are not default but we are constant, prune out all default permutations
								for (size_t permutation_index = 0; permutation_index < k_num_constant_permutations; ++permutation_index)
								{
									if (k_constant_permutations[permutation_index].scale == constant_scale_value8::default_vector3f)
									{
										is_candidate_permutation_valid[permutation_index] = false;
										num_candidate_permutations--;
									}
								}
							}
						}
						else
						{
							// We are not constant, prune out all constant/default permutations
							for (size_t permutation_index = 0; permutation_index < k_num_constant_permutations; ++permutation_index)
							{
								if (k_constant_permutations[permutation_index].scale != constant_scale_value8::animated_vector3f_full)
								{
									is_candidate_permutation_valid[permutation_index] = false;
									num_candidate_permutations--;
								}
							}
						}
					}
				}

				rotation_values[static_cast<uint32_t>(constant_rotation_value8::constant_quatf_drop_w_full)] = bone_stream_to_test.rotations.get_raw_sample<rtm::quatf>(0);
				rotation_values[static_cast<uint32_t>(constant_rotation_value8::default_quatf_full)] = transform_desc_to_test.default_value.rotation;

				translation_values[static_cast<uint32_t>(constant_translation_value8::constant_vector3f_full)] = bone_stream_to_test.translations.get_raw_sample<rtm::vector4f>(0);
				translation_values[static_cast<uint32_t>(constant_translation_value8::default_vector3f)] = transform_desc_to_test.default_value.translation;

				scale_values[static_cast<uint32_t>(constant_scale_value8::constant_vector3f_full)] = bone_stream_to_test.scales.get_raw_sample<rtm::vector4f>(0);
				scale_values[static_cast<uint32_t>(constant_scale_value8::default_vector3f)] = transform_desc_to_test.default_value.scale;

				// Our dominant transform may or may not be a leaf (we are a leaf if we have no children/leaves)
				const uint32_t dominant_transform_index = lossy_clip_context.clip_shell_metadata[transform_index_to_test].dominant_transform_index;
				bool is_dominant_transform_a_leaf = topology->transforms[transform_index_to_test].num_leaves == 0;

				// Compute which transforms we need to sample (leaves and their parents)
				std::fill_n(needed_transforms, num_transforms, false);

				// We need the transform to test and its parents all the way to the root
				{
					uint32_t parent_transform_index = transform_index_to_test;
					while (parent_transform_index != k_invalid_track_index)
					{
						needed_transforms[parent_transform_index] = true;
						parent_transform_index = topology->transforms[parent_transform_index].parent_index;
					}
				}

				// We need all intermediary transforms between the transform to test and its leaves
				for (uint32_t leaf_transform_index : topology->transforms[transform_index_to_test].leaves_iterator())
				{
					uint32_t parent_transform_index = leaf_transform_index;
					while (parent_transform_index != transform_index_to_test)
					{
						needed_transforms[parent_transform_index] = true;
						parent_transform_index = topology->transforms[parent_transform_index].parent_index;
					}

					if (leaf_transform_index == dominant_transform_index)
						is_dominant_transform_a_leaf = true;
				}

				for (uint32_t sample_index = 0; sample_index < num_samples; ++sample_index)
				{
					for (uint32_t transform_index = 0; transform_index < num_transforms; ++transform_index)
					{
						if (!needed_transforms[transform_index])
							continue;	// We don't need this transform

						const transform_streams& transform_stream = segment.bone_streams[transform_index];

						const rtm::quatf rotation = transform_stream.rotations.get_sample_clamped(sample_index);
						const rtm::vector4f translation = transform_stream.translations.get_sample_clamped(sample_index);
						const rtm::vector4f scale = transform_stream.scales.get_sample_clamped(sample_index);

						sample_transforms[transform_index] = rtm::qvv_set(rotation, translation, scale);
					}

					if (has_additive_base)
					{
						const segment_context& base_segment = additive_base_clip_context.segments[0];

						const uint32_t base_num_samples = additive_base_clip_context.num_samples;
						const float base_duration = additive_base_clip_context.duration;

						// The sample time is calculated from the full clip duration to be consistent with decompression
						const float sample_time = rtm::scalar_min(float(sample_index) / sample_rate, duration);

						const float normalized_sample_time = base_num_samples > 1 ? (sample_time / duration) : 0.0F;
						const float additive_sample_time = base_num_samples > 1 ? (normalized_sample_time * base_duration) : 0.0F;

						// With uniform sample distributions, we do not interpolate.
						const uint32_t base_sample_index = get_uniform_sample_key(base_segment, additive_sample_time);

						for (uint32_t transform_index = 0; transform_index < num_transforms; ++transform_index)
						{
							if (!needed_transforms[transform_index])
								continue;	// We don't need this transform

							const transform_streams& base_transform_stream = base_segment.bone_streams[transform_index];

							const rtm::quatf base_rotation = base_transform_stream.rotations.get_sample_clamped(base_sample_index);
							const rtm::vector4f base_translation = base_transform_stream.translations.get_sample_clamped(base_sample_index);
							const rtm::vector4f base_scale = base_transform_stream.scales.get_sample_clamped(base_sample_index);
							const rtm::qvvf base_transform = rtm::qvv_set(base_rotation, base_translation, base_scale);

							sample_transforms[transform_index] = rtm::qvv_normalize(acl::apply_additive_to_base(additive_format, base_transform, sample_transforms[transform_index]));
						}
					}

					// Compute our root-to-parent transform
					{
						uint32_t parent_transform_index = topology->transforms[transform_index_to_test].parent_index;
						parent_to_root_transform = rtm::qvv_identity();
						while (parent_transform_index != k_invalid_track_index)
						{
							parent_to_root_transform = rtm::qvv_normalize(rtm::qvv_mul(parent_to_root_transform, sample_transforms[parent_transform_index]));
							parent_transform_index = topology->transforms[parent_transform_index].parent_index;
						}
					}

					// Compute our child-to-leaf transforms
					for (uint32_t leaf_index = 0; leaf_index < topology->transforms[transform_index_to_test].num_leaves; ++leaf_index)
					{
						const uint32_t leaf_transform_index = topology->transforms[transform_index_to_test].leaves[leaf_index];

						uint32_t parent_transform_index = topology->transforms[leaf_transform_index].parent_index;
						rtm::qvvf child_to_leaf_transform = sample_transforms[leaf_transform_index];
						while (parent_transform_index != transform_index_to_test)
						{
							child_to_leaf_transform = rtm::qvv_normalize(rtm::qvv_mul(child_to_leaf_transform, sample_transforms[parent_transform_index]));
							parent_transform_index = topology->transforms[parent_transform_index].parent_index;
						}

						child_to_leaf_transforms[leaf_index] = child_to_leaf_transform;
					}

					// If our dominant transform isn't a leaf, we have to consider it as well
					if (!is_dominant_transform_a_leaf)
					{
						uint32_t parent_transform_index = dominant_transform_index;
						child_to_dominant_transform = rtm::qvv_identity();
						while (parent_transform_index != transform_index_to_test)
						{
							child_to_dominant_transform = rtm::qvv_normalize(rtm::qvv_mul(child_to_dominant_transform, sample_transforms[parent_transform_index]));
							parent_transform_index = topology->transforms[parent_transform_index].parent_index;
						}
					}

					const rtm::quatf raw_rotation = bone_stream_to_test.rotations.get_sample_clamped(sample_index);
					const rtm::vector4f raw_translation = bone_stream_to_test.translations.get_sample_clamped(sample_index);
					const rtm::vector4f raw_scale = bone_stream_to_test.scales.get_sample_clamped(sample_index);
					const rtm::qvvf raw_transform = rtm::qvv_set(raw_rotation, raw_translation, raw_scale);

					const rtm::qvvf transform_to_root_raw = rtm::qvv_normalize(rtm::qvv_mul(raw_transform, parent_to_root_transform));

					rotation_values[static_cast<uint32_t>(constant_rotation_value8::animated_quatf_full)] = raw_rotation;
					translation_values[static_cast<uint32_t>(constant_translation_value8::animated_vector3f_full)] = raw_translation;
					scale_values[static_cast<uint32_t>(constant_scale_value8::animated_vector3f_full)] = raw_scale;

					// Iterate over our remaining candidate permutations
					for (size_t permutation_index = 0; permutation_index < k_num_constant_permutations; ++permutation_index)
					{
						if (!is_candidate_permutation_valid[permutation_index])
							continue;	// We've already ruled this permutation out, skip it

						const constant_permutation_t& permutation = k_constant_permutations[permutation_index];

						const rtm::quatf& permutation_rotation = rotation_values[static_cast<uint32_t>(permutation.rotation)];
						const rtm::vector4f& permutation_translation = translation_values[static_cast<uint32_t>(permutation.translation)];
						const rtm::vector4f& permutation_scale = scale_values[static_cast<uint32_t>(permutation.scale)];
						const rtm::qvvf lossy_transform = rtm::qvv_set(permutation_rotation, permutation_translation, permutation_scale);

						const rtm::qvvf transform_to_root_lossy = rtm::qvv_normalize(rtm::qvv_mul(lossy_transform, parent_to_root_transform));

						bool is_permutation_valid = true;

						// First, test our transform in object space, if we fail, none of our leaves can succeed (and we might not have any)
						{
							const transform_metadata& transform_to_test_metadata = lossy_clip_context.metadata[transform_index_to_test];

							calculate_error_args.construct_sphere_shell(transform_to_test_metadata.shell_distance);
							calculate_error_args.transform0 = &transform_to_root_raw;
							calculate_error_args.transform1 = &transform_to_root_lossy;

							const rtm::scalarf precision_sq = rtm::scalar_set(transform_to_test_metadata.precision * transform_to_test_metadata.precision);
							const rtm::scalarf vtx_error_sq = error_metric.calculate_error_squared(calculate_error_args);

							// If our error exceeds the desired precision, this permutation isn't valid
							is_permutation_valid = !rtm::scalar_greater_than(vtx_error_sq, precision_sq);
						}

						// If our dominant transform isn't a leaf, we have to consider it as well
						if (is_permutation_valid && !is_dominant_transform_a_leaf)
						{
							const transform_metadata& dominant_metadata = lossy_clip_context.metadata[dominant_transform_index];

							const rtm::qvvf root_to_dominant_raw = rtm::qvv_normalize(rtm::qvv_mul(child_to_dominant_transform, transform_to_root_raw));
							const rtm::qvvf root_to_dominant_lossy = rtm::qvv_normalize(rtm::qvv_mul(child_to_dominant_transform, transform_to_root_lossy));

							calculate_error_args.construct_sphere_shell(dominant_metadata.shell_distance);
							calculate_error_args.transform0 = &root_to_dominant_raw;
							calculate_error_args.transform1 = &root_to_dominant_lossy;

							const rtm::scalarf precision_sq = rtm::scalar_set(dominant_metadata.precision * dominant_metadata.precision);
							const rtm::scalarf vtx_error_sq = error_metric.calculate_error_squared(calculate_error_args);

							// If our error exceeds the desired precision, this permutation isn't valid
							is_permutation_valid = !rtm::scalar_greater_than(vtx_error_sq, precision_sq);
						}

						// Next, test each leaf in object space if we are valid so far
						if (is_permutation_valid)
						{
							// TODO: we can build the object space transforms lazily on the first permutation that passes
							for (uint32_t leaf_index = 0; leaf_index < topology->transforms[transform_index_to_test].num_leaves; ++leaf_index)
							{
								const uint32_t leaf_transform_index = topology->transforms[transform_index_to_test].leaves[leaf_index];
								const transform_metadata& leaf_metadata = lossy_clip_context.metadata[leaf_transform_index];

								const rtm::qvvf root_to_leaf_raw = rtm::qvv_normalize(rtm::qvv_mul(child_to_leaf_transforms[leaf_index], transform_to_root_raw));
								const rtm::qvvf root_to_leaf_lossy = rtm::qvv_normalize(rtm::qvv_mul(child_to_leaf_transforms[leaf_index], transform_to_root_lossy));

								calculate_error_args.construct_sphere_shell(leaf_metadata.shell_distance);
								calculate_error_args.transform0 = &root_to_leaf_raw;
								calculate_error_args.transform1 = &root_to_leaf_lossy;

								const rtm::scalarf precision_sq = rtm::scalar_set(leaf_metadata.precision * leaf_metadata.precision);
								const rtm::scalarf vtx_error_sq = error_metric.calculate_error_squared(calculate_error_args);

								// If our error exceeds the desired precision, this permutation isn't valid
								if (rtm::scalar_greater_than(vtx_error_sq, precision_sq))
								{
									is_permutation_valid = false;
									break;
								}
							}
						}

						if (!is_permutation_valid)
						{
							is_candidate_permutation_valid[permutation_index] = false;
							num_candidate_permutations--;
						}
					}

					if (num_candidate_permutations == 0)
						break;	// No more valid candidates, we are done
				}

				if (num_candidate_permutations != 0)
				{
					// Scan our valid candidate permutations and pick the one with the lowest cost
					size_t best_permutation_index = k_num_constant_permutations;
					uint32_t best_permutation_cost = ~0U;

					for (size_t permutation_index = 0; permutation_index < k_num_constant_permutations; ++permutation_index)
					{
						if (!is_candidate_permutation_valid[permutation_index])
							continue;	// We've ruled this permutation out, skip it

						const uint32_t permutation_cost = k_constant_permutations[permutation_index].get_cost();
						if (permutation_cost < best_permutation_cost)
						{
							best_permutation_cost = permutation_cost;
							best_permutation_index = permutation_index;
						}
					}

					const constant_permutation_t& best_permutation = k_constant_permutations[best_permutation_index];

					if (best_permutation.rotation != constant_rotation_value8::animated_quatf_full)
					{
						const rtm::quatf& rotation = rotation_values[static_cast<uint32_t>(best_permutation.rotation)];

						rotation_track_stream constant_stream(allocator, 1, bone_stream_to_test.rotations.get_sample_size(), bone_stream_to_test.rotations.get_sample_rate(), bone_stream_to_test.rotations.get_rotation_format());
						constant_stream.set_raw_sample(0, rotation);
						bone_stream_to_test.rotations = std::move(constant_stream);

						bone_stream_to_test.is_rotation_constant = true;
						bone_stream_to_test.is_rotation_default = best_permutation.rotation == constant_rotation_value8::default_quatf_full;

						bone_range_to_test.rotation = track_stream_range::from_min_extent(rotation, rtm::vector_zero());
					}

					if (best_permutation.translation != constant_translation_value8::animated_vector3f_full)
					{
						const rtm::vector4f& translation = translation_values[static_cast<uint32_t>(best_permutation.translation)];

						translation_track_stream constant_stream(allocator, 1, bone_stream_to_test.translations.get_sample_size(), bone_stream_to_test.translations.get_sample_rate(), bone_stream_to_test.translations.get_vector_format());
						constant_stream.set_raw_sample(0, translation);
						bone_stream_to_test.translations = std::move(constant_stream);

						bone_stream_to_test.is_translation_constant = true;
						bone_stream_to_test.is_translation_default = best_permutation.translation == constant_translation_value8::default_vector3f;

						// Zero out W, could be garbage
						bone_range_to_test.translation = track_stream_range::from_min_extent(rtm::vector_set_w(translation, 0.0F), rtm::vector_zero());
					}

					if (best_permutation.scale != constant_scale_value8::animated_vector3f_full)
					{
						const rtm::vector4f& scale = scale_values[static_cast<uint32_t>(best_permutation.scale)];

						scale_track_stream constant_stream(allocator, 1, bone_stream_to_test.scales.get_sample_size(), bone_stream_to_test.scales.get_sample_rate(), bone_stream_to_test.scales.get_vector_format());
						constant_stream.set_raw_sample(0, scale);
						bone_stream_to_test.scales = std::move(constant_stream);

						bone_stream_to_test.is_scale_constant = true;
						bone_stream_to_test.is_scale_default = best_permutation.scale == constant_scale_value8::default_vector3f;

						// Zero out W, could be garbage
						bone_range_to_test.scale = track_stream_range::from_min_extent(rtm::vector_set_w(scale, 0.0F), rtm::vector_zero());

						num_default_bone_scales += best_permutation.scale == constant_scale_value8::default_vector3f ? 1 : 0;
					}
				}
			}

			lossy_clip_context.has_scale = num_default_bone_scales != num_transforms;

			deallocate_type_array(allocator, needed_transforms, num_transforms);
			deallocate_type_array(allocator, sample_transforms, num_transforms);
			deallocate_type_array(allocator, child_to_leaf_transforms, topology->num_max_leaves_per_transform);

#if defined(ACL_USE_SJSON)
			compression_stats.compact_constant_sub_tracks_elapsed_seconds = compact_constant_sub_tracks_time.get_elapsed_seconds();
#endif
		}

#endif

	}

	ACL_IMPL_VERSION_NAMESPACE_END
}

ACL_IMPL_FILE_PRAGMA_POP
