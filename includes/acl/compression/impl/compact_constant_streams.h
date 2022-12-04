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
#include "acl/compression/impl/clip_context.h"
#include "acl/compression/transform_error_metrics.h"

#ifdef ACL_COMPRESSION_OPTIMIZED

#include "acl/compression/impl/normalize_streams.h"

#endif

#include <rtm/quatf.h>
#include <rtm/vector4f.h>

#include <cstdint>

ACL_IMPL_FILE_PRAGMA_PUSH

namespace acl
{
	ACL_IMPL_VERSION_NAMESPACE_BEGIN

	namespace acl_impl
	{
		// Rigid shell information per transform
		struct rigid_shell_metadata_t
		{
			// Dominant local space shell distance (from transform tip)
			float local_shell_distance;

			// Parent space shell distance (from transform root)
			float parent_shell_distance;

			// Precision required on the surface of the rigid shell
			float precision;
		};

		// We use the raw data to compute the rigid shell since rotations might have been converted already
		// We compute the largest value over the whole clip per transform
		// TODO: We could compute a single value per sample and per transform as well but its unclear if this would provide an advantage
		inline rigid_shell_metadata_t* compute_clip_shell_distances(iallocator& allocator, const clip_context& lossy_clip_context, const clip_context& raw_clip_context)
		{
			if (lossy_clip_context.has_additive_base)
				return nullptr;

			const uint32_t num_transforms = raw_clip_context.num_bones;
			if (num_transforms == 0)
				return nullptr;

			const uint32_t num_samples = raw_clip_context.num_samples;
			if (num_samples == 0)
				return nullptr;

			const segment_context& raw_segment = raw_clip_context.segments[0];

			rigid_shell_metadata_t* shell_metadata = allocate_type_array<rigid_shell_metadata_t>(allocator, num_transforms);

			// Initialize everything
			for (uint32_t transform_index = 0; transform_index < num_transforms; ++transform_index)
			{
				const transform_metadata& metadata = raw_clip_context.metadata[transform_index];

				shell_metadata[transform_index].local_shell_distance = metadata.shell_distance;
				shell_metadata[transform_index].precision = metadata.precision;
				shell_metadata[transform_index].parent_shell_distance = 0.0F;
			}

			// Iterate from leaf transforms towards their root, we want to bubble up our shell distance
			for (const uint32_t transform_index : make_reverse_iterator(raw_clip_context.sorted_transforms_parent_first, num_transforms))
			{
				const transform_streams& raw_bone_stream = raw_segment.bone_streams[transform_index];

				rigid_shell_metadata_t& shell = shell_metadata[transform_index];

				// Use the accumulated shell distance so far to see how far it deforms with our local transform
				const rtm::vector4f vtx0 = rtm::vector_set(shell.local_shell_distance, 0.0F, 0.0F);
				const rtm::vector4f vtx1 = rtm::vector_set(0.0F, shell.local_shell_distance, 0.0F);
				const rtm::vector4f vtx2 = rtm::vector_set(0.0F, 0.0F, shell.local_shell_distance);

				// Calculate the shell distance in parent space
				rtm::scalarf parent_shell_distance = rtm::scalar_set(0.0F);
				for (uint32_t sample_index = 0; sample_index < num_samples; ++sample_index)
				{
					const rtm::quatf raw_rotation = raw_bone_stream.rotations.get_sample(sample_index);
					const rtm::vector4f raw_translation = raw_bone_stream.translations.get_sample(sample_index);
					const rtm::vector4f raw_scale = raw_bone_stream.scales.get_sample(sample_index);

					const rtm::qvvf raw_transform = rtm::qvv_set(raw_rotation, raw_translation, raw_scale);

					const rtm::vector4f raw_vtx0 = rtm::qvv_mul_point3(vtx0, raw_transform);
					const rtm::vector4f raw_vtx1 = rtm::qvv_mul_point3(vtx1, raw_transform);
					const rtm::vector4f raw_vtx2 = rtm::qvv_mul_point3(vtx2, raw_transform);

					const rtm::scalarf vtx0_distance = rtm::vector_length3(raw_vtx0);
					const rtm::scalarf vtx1_distance = rtm::vector_length3(raw_vtx1);
					const rtm::scalarf vtx2_distance = rtm::vector_length3(raw_vtx2);

					const rtm::scalarf transform_length = rtm::scalar_max(rtm::scalar_max(vtx0_distance, vtx1_distance), vtx2_distance);
					parent_shell_distance = rtm::scalar_max(parent_shell_distance, transform_length);
				}

				shell.parent_shell_distance = rtm::scalar_cast(parent_shell_distance);

				const transform_metadata& metadata = raw_clip_context.metadata[transform_index];

				// Add precision since we want to make sure to encompass the maximum amount of error allowed
				// Add it only for non-dominant transforms to account for the error they introduce
				// Dominant transforms will use their own precision
				// If our shell distance has changed, we are non-dominant since a dominant child updated it
				if (shell.local_shell_distance != metadata.shell_distance)
					shell.parent_shell_distance += metadata.precision;

				if (metadata.parent_index != k_invalid_track_index)
				{
					// We have a parent, propagate our shell distance if we are a dominant transform
					// We are a dominant transform if our shell distance in parent space is larger
					// than our parent's shell distance in local space. Otherwise, if we are smaller
					// or equal, it means that the full range of motion of our transform fits within
					// the parent's shell distance.

					rigid_shell_metadata_t& parent_shell = shell_metadata[metadata.parent_index];

					if (shell.parent_shell_distance > parent_shell.local_shell_distance)
					{
						// We are the new dominant transform, use our shell distance and precision
						parent_shell.local_shell_distance = shell.parent_shell_distance;
						parent_shell.precision = shell.precision;
					}
				}
			}

			return shell_metadata;
		}

		// To detect if a sub-track is constant, we grab the first sample as our reference.
		// We then measure the object space error using the qvv error metric and our
		// dominant shell distance. If the error remains within our dominant precision
		// then the sub-track is constant. We perform the same test using the default
		// sub-track value to determine if it is a default sub-track.

		inline bool RTM_SIMD_CALL are_rotations_constant(const transform_streams& raw_transform_stream, rtm::quatf_arg0 reference, const rigid_shell_metadata_t& shell)
		{
			const uint32_t num_samples = raw_transform_stream.rotations.get_num_samples();

			qvvf_transform_error_metric::calculate_error_args error_metric_args;
			error_metric_args.construct_sphere_shell(shell.local_shell_distance);

			const qvvf_transform_error_metric error_metric;

			const rtm::scalarf precision = rtm::scalar_set(shell.precision);

			for (uint32_t sample_index = 0; sample_index < num_samples; ++sample_index)
			{
				const rtm::quatf raw_rotation = raw_transform_stream.rotations.get_sample_clamped(sample_index);
				const rtm::vector4f raw_translation = raw_transform_stream.translations.get_sample_clamped(sample_index);
				const rtm::vector4f raw_scale = raw_transform_stream.scales.get_sample_clamped(sample_index);

				const rtm::qvvf raw_transform = rtm::qvv_set(raw_rotation, raw_translation, raw_scale);
				const rtm::qvvf lossy_transform = rtm::qvv_set(reference, raw_translation, raw_scale);

				error_metric_args.transform0 = &raw_transform;
				error_metric_args.transform1 = &lossy_transform;

				const rtm::scalarf vtx_error = error_metric.calculate_error(error_metric_args);

				// If our error exceeds the desired precision, we are not constant
				if (rtm::scalar_greater_than(vtx_error, precision))
					return false;
			}

			// All samples were tested against the reference value and the error remained within tolerance
			return true;
		}

		inline bool are_rotations_constant(const compression_settings& settings, const clip_context& lossy_clip_context, const rigid_shell_metadata_t* shell_metadata, uint32_t transform_index)
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
			return are_rotations_constant(raw_transform_stream, raw_transform_stream.rotations.get_sample(0), shell_metadata[transform_index]);
		}

		inline bool are_rotations_default(const compression_settings& settings, const clip_context& lossy_clip_context, const rigid_shell_metadata_t* shell_metadata, const track_desc_transformf& desc, uint32_t transform_index)
		{
			if (lossy_clip_context.num_samples == 0)
				return true;	// We are default if we have no samples

			const rtm::vector4f default_bind_rotation = rtm::quat_to_vector(desc.default_value.rotation);

			const segment_context& segment = lossy_clip_context.segments[0];
			const transform_streams& raw_transform_stream = segment.bone_streams[transform_index];

			// When we are using full precision, we are only default if (sample 0 == default value), meaning
			// we have a single unique and repeating default sample
			// We want to test if we are binary exact
			// This is used by raw clips, we must preserve the original values
			if (settings.rotation_format == rotation_format8::quatf_full)
			{
				const rtm::vector4f rotation = raw_transform_stream.rotations.get_raw_sample<rtm::vector4f>(0);
				return rtm::vector_all_equal(rotation, default_bind_rotation);
			}

			// Otherwise check every sample to make sure we fall within the desired tolerance
			return are_rotations_constant(raw_transform_stream, default_bind_rotation, shell_metadata[transform_index]);
		}

		inline bool RTM_SIMD_CALL are_translations_constant(const transform_streams& raw_transform_stream, rtm::vector4f_arg0 reference, const rigid_shell_metadata_t& shell)
		{
			const uint32_t num_samples = raw_transform_stream.translations.get_num_samples();

			qvvf_transform_error_metric::calculate_error_args error_metric_args;
			error_metric_args.construct_sphere_shell(shell.local_shell_distance);

			const qvvf_transform_error_metric error_metric;

			const rtm::scalarf precision = rtm::scalar_set(shell.precision);

			for (uint32_t sample_index = 0; sample_index < num_samples; ++sample_index)
			{
				const rtm::quatf raw_rotation = raw_transform_stream.rotations.get_sample_clamped(sample_index);
				const rtm::vector4f raw_translation = raw_transform_stream.translations.get_sample_clamped(sample_index);
				const rtm::vector4f raw_scale = raw_transform_stream.scales.get_sample_clamped(sample_index);

				const rtm::qvvf raw_transform = rtm::qvv_set(raw_rotation, raw_translation, raw_scale);
				const rtm::qvvf lossy_transform = rtm::qvv_set(raw_rotation, reference, raw_scale);

				error_metric_args.transform0 = &raw_transform;
				error_metric_args.transform1 = &lossy_transform;

				const rtm::scalarf vtx_error = error_metric.calculate_error(error_metric_args);

				// If our error exceeds the desired precision, we are not constant
				if (rtm::scalar_greater_than(vtx_error, precision))
					return false;
			}

			// All samples were tested against the reference value and the error remained within tolerance
			return true;
		}

		inline bool are_translations_constant(const compression_settings& settings, const clip_context& lossy_clip_context, const rigid_shell_metadata_t* shell_metadata, uint32_t transform_index)
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
			return are_translations_constant(raw_transform_stream, raw_transform_stream.translations.get_sample(0), shell_metadata[transform_index]);
		}

		inline bool are_translations_default(const compression_settings& settings, const clip_context& lossy_clip_context, const rigid_shell_metadata_t* shell_metadata, const track_desc_transformf& desc, uint32_t transform_index)
		{
			if (lossy_clip_context.num_samples == 0)
				return true;	// We are default if we have no samples

			const rtm::vector4f default_bind_translation = desc.default_value.translation;

			const segment_context& segment = lossy_clip_context.segments[0];
			const transform_streams& raw_transform_stream = segment.bone_streams[transform_index];

			// When we are using full precision, we are only default if (sample 0 == default value), meaning
			// we have a single unique and repeating default sample
			// We want to test if we are binary exact
			// This is used by raw clips, we must preserve the original values
			if (settings.translation_format == vector_format8::vector3f_full)
			{
				const rtm::vector4f translation = raw_transform_stream.translations.get_raw_sample<rtm::vector4f>(0);
				return rtm::vector_all_equal(translation, default_bind_translation);
			}

			// Otherwise check every sample to make sure we fall within the desired tolerance
			return are_translations_constant(raw_transform_stream, default_bind_translation, shell_metadata[transform_index]);
		}

		inline bool RTM_SIMD_CALL are_scales_constant(const transform_streams& raw_transform_stream, rtm::vector4f_arg0 reference, const rigid_shell_metadata_t& shell)
		{
			const uint32_t num_samples = raw_transform_stream.scales.get_num_samples();

			qvvf_transform_error_metric::calculate_error_args error_metric_args;
			error_metric_args.construct_sphere_shell(shell.local_shell_distance);

			const qvvf_transform_error_metric error_metric;

			const rtm::scalarf precision = rtm::scalar_set(shell.precision);

			for (uint32_t sample_index = 0; sample_index < num_samples; ++sample_index)
			{
				const rtm::quatf raw_rotation = raw_transform_stream.rotations.get_sample_clamped(sample_index);
				const rtm::vector4f raw_translation = raw_transform_stream.translations.get_sample_clamped(sample_index);
				const rtm::vector4f raw_scale = raw_transform_stream.scales.get_sample_clamped(sample_index);

				const rtm::qvvf raw_transform = rtm::qvv_set(raw_rotation, raw_translation, raw_scale);
				const rtm::qvvf lossy_transform = rtm::qvv_set(raw_rotation, raw_translation, reference);

				error_metric_args.transform0 = &raw_transform;
				error_metric_args.transform1 = &lossy_transform;

				const rtm::scalarf vtx_error = error_metric.calculate_error(error_metric_args);

				// If our error exceeds the desired precision, we are not constant
				if (rtm::scalar_greater_than(vtx_error, precision))
					return false;
			}

			// All samples were tested against the reference value and the error remained within tolerance
			return true;
		}

		inline bool are_scales_constant(const compression_settings& settings, const clip_context& lossy_clip_context, const rigid_shell_metadata_t* shell_metadata, uint32_t transform_index)
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
			return are_scales_constant(raw_transform_stream, raw_transform_stream.scales.get_sample(0), shell_metadata[transform_index]);
		}

		inline bool are_scales_default(const compression_settings& settings, const clip_context& lossy_clip_context, const rigid_shell_metadata_t* shell_metadata, const track_desc_transformf& desc, uint32_t transform_index)
		{
			if (lossy_clip_context.num_samples == 0)
				return true;	// We are default if we have no samples

			if (!lossy_clip_context.has_scale)
				return true;	// We are default if we have no scale

			const rtm::vector4f default_bind_scale = desc.default_value.scale;

			const segment_context& segment = lossy_clip_context.segments[0];
			const transform_streams& raw_transform_stream = segment.bone_streams[transform_index];

			// When we are using full precision, we are only default if (sample 0 == default value), meaning
			// we have a single unique and repeating default sample
			// We want to test if we are binary exact
			// This is used by raw clips, we must preserve the original values
			if (settings.scale_format == vector_format8::vector3f_full)
			{
				const rtm::vector4f scale = raw_transform_stream.scales.get_raw_sample<rtm::vector4f>(0);
				return rtm::vector_all_equal(scale, default_bind_scale);
			}

			// Otherwise check every sample to make sure we fall within the desired tolerance
			return are_scales_constant(raw_transform_stream, default_bind_scale, shell_metadata[transform_index]);
		}

		inline void compact_constant_streams(iallocator& allocator, clip_context& context, const clip_context& raw_clip_context, const track_array_qvvf& track_list, const compression_settings& settings)
		{
			ACL_ASSERT(context.num_segments == 1, "context must contain a single segment!");
			segment_context& segment = context.segments[0];

			const uint32_t num_bones = context.num_bones;
			const uint32_t num_samples = context.num_samples;

			uint32_t num_default_bone_scales = 0;

#ifdef ACL_COMPRESSION_OPTIMIZED
			bool has_constant_bone_rotations = false;
			bool has_constant_bone_translations = false;
			bool has_constant_bone_scales = false;
#endif

			rigid_shell_metadata_t* shell_metadata = compute_clip_shell_distances(allocator, context, raw_clip_context);

			// When a stream is constant, we only keep the first sample
			for (uint32_t bone_index = 0; bone_index < num_bones; ++bone_index)
			{
				const track_desc_transformf& desc = track_list[bone_index].get_description();

				transform_streams& bone_stream = segment.bone_streams[bone_index];
				transform_range& bone_range = context.ranges[bone_index];

				ACL_ASSERT(bone_stream.rotations.get_num_samples() == num_samples, "Rotation sample mismatch!");
				ACL_ASSERT(bone_stream.translations.get_num_samples() == num_samples, "Translation sample mismatch!");
				ACL_ASSERT(bone_stream.scales.get_num_samples() == num_samples, "Scale sample mismatch!");

				// We expect all our samples to have the same width of sizeof(rtm::vector4f)
				ACL_ASSERT(bone_stream.rotations.get_sample_size() == sizeof(rtm::vector4f), "Unexpected rotation sample size. %u != %zu", bone_stream.rotations.get_sample_size(), sizeof(rtm::vector4f));
				ACL_ASSERT(bone_stream.translations.get_sample_size() == sizeof(rtm::vector4f), "Unexpected translation sample size. %u != %zu", bone_stream.translations.get_sample_size(), sizeof(rtm::vector4f));
				ACL_ASSERT(bone_stream.scales.get_sample_size() == sizeof(rtm::vector4f), "Unexpected scale sample size. %u != %zu", bone_stream.scales.get_sample_size(), sizeof(rtm::vector4f));

				if (are_rotations_constant(settings, context, shell_metadata, bone_index))
				{
					rotation_track_stream constant_stream(allocator, 1, bone_stream.rotations.get_sample_size(), bone_stream.rotations.get_sample_rate(), bone_stream.rotations.get_rotation_format());

					const rtm::vector4f default_bind_rotation = rtm::quat_to_vector(desc.default_value.rotation);

					rtm::vector4f rotation = num_samples != 0 ? bone_stream.rotations.get_raw_sample<rtm::vector4f>(0) : default_bind_rotation;

					bone_stream.is_rotation_constant = true;

					if (are_rotations_default(settings, context, shell_metadata, desc, bone_index))
					{
						bone_stream.is_rotation_default = true;
						rotation = default_bind_rotation;
					}

					constant_stream.set_raw_sample(0, rotation);
					bone_stream.rotations = std::move(constant_stream);

					bone_range.rotation = track_stream_range::from_min_extent(rotation, rtm::vector_zero());

#ifdef ACL_COMPRESSION_OPTIMIZED
					has_constant_bone_rotations = true;
#endif
				}

				if (are_translations_constant(settings, context, shell_metadata, bone_index))
				{
					translation_track_stream constant_stream(allocator, 1, bone_stream.translations.get_sample_size(), bone_stream.translations.get_sample_rate(), bone_stream.translations.get_vector_format());

					const rtm::vector4f default_bind_translation = desc.default_value.translation;

					rtm::vector4f translation = num_samples != 0 ? bone_stream.translations.get_raw_sample<rtm::vector4f>(0) : default_bind_translation;

					bone_stream.is_translation_constant = true;

					if (are_translations_default(settings, context, shell_metadata, desc, bone_index))
					{
						bone_stream.is_translation_default = true;
						translation = default_bind_translation;
					}

					constant_stream.set_raw_sample(0, translation);
					bone_stream.translations = std::move(constant_stream);

					bone_range.translation = track_stream_range::from_min_extent(translation, rtm::vector_zero());

#ifdef ACL_COMPRESSION_OPTIMIZED
					has_constant_bone_translations = true;
#endif
				}

				if (are_scales_constant(settings, context, shell_metadata, bone_index))
				{
					scale_track_stream constant_stream(allocator, 1, bone_stream.scales.get_sample_size(), bone_stream.scales.get_sample_rate(), bone_stream.scales.get_vector_format());

					const rtm::vector4f default_bind_scale = desc.default_value.scale;

					rtm::vector4f scale = (context.has_scale && num_samples != 0) ? bone_stream.scales.get_raw_sample<rtm::vector4f>(0) : default_bind_scale;

					bone_stream.is_scale_constant = true;

					if (are_scales_default(settings, context, shell_metadata, desc, bone_index))
					{
						bone_stream.is_scale_default = true;
						scale = default_bind_scale;
					}

					constant_stream.set_raw_sample(0, scale);
					bone_stream.scales = std::move(constant_stream);

					bone_range.scale = track_stream_range::from_min_extent(scale, rtm::vector_zero());

					num_default_bone_scales += bone_stream.is_scale_default ? 1 : 0;

#ifdef ACL_COMPRESSION_OPTIMIZED
					has_constant_bone_scales = true;
#endif
				}
			}

			context.has_scale = num_default_bone_scales != num_bones;

			deallocate_type_array(allocator, shell_metadata, num_bones);

#ifdef ACL_COMPRESSION_OPTIMIZED

			const bool has_scale = context.has_scale;
			if (!context.has_additive_base &&
				(has_constant_bone_rotations || has_constant_bone_translations || (has_scale && has_constant_bone_scales)))
			{
				// Apply error correction after constant and default tracks are processed.
				// We use object space of the original data as ground truth, and only deviate for 2 reasons, and as briefly as possible.
				//    -Replace an original local value with a new constant value.
				//    -Correct for the manipulation of an original local value by an ancestor ASAP.
				// We aren't modifying raw data here. We're modifying the raw channels generated from the raw data.
				// The raw data is left alone, and is still used at the end of the process to do regression testing.

				segment_context& raw_segment = raw_clip_context.segments[0];

				struct DirtyState
				{
					bool rotation = false;
					bool translation = false;
					bool scale = false;
				};
				DirtyState any_constant_changed;
				DirtyState* dirty_states = allocate_type_array<DirtyState>(allocator, num_bones);
				rtm::qvvf* original_object_pose = allocate_type_array<rtm::qvvf>(allocator, num_bones);
				rtm::qvvf* adjusted_object_pose = allocate_type_array<rtm::qvvf>(allocator, num_bones);
				for (uint32_t sample_index = 0; sample_index < num_samples; ++sample_index)
				{
					for (uint32_t bone_index = 0; bone_index < num_bones; ++bone_index)
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
						{
							original_object_transform = original_local_transform;	// Just copy the root as-is, it has no parent and thus local and object space transforms are equal
						}
						else if (!has_scale)
						{
							original_object_transform = rtm::qvv_normalize(rtm::qvv_mul_no_scale(original_local_transform, original_object_pose[parent_bone_index]));
						}
						else
						{
							original_object_transform = rtm::qvv_normalize(rtm::qvv_mul(original_local_transform, original_object_pose[parent_bone_index]));
						}

						rtm::qvvf adjusted_local_transform = original_local_transform;

						DirtyState& constant_changed = dirty_states[bone_index];
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
							const DirtyState& parent_constant_changed = dirty_states[parent_bone_index];
							const rtm::qvvf& parent_adjusted_object_transform = adjusted_object_pose[parent_bone_index];

							if (bone_stream.is_rotation_constant && !constant_changed.rotation)
							{
								constant_changed.rotation = parent_constant_changed.rotation;
							}
							if (bone_stream.is_translation_constant && !constant_changed.translation)
							{
								constant_changed.translation = parent_constant_changed.translation;
							}
							if (has_scale && bone_stream.is_scale_constant && !constant_changed.scale)
							{
								constant_changed.scale = parent_constant_changed.scale;
							}

							// Compensate for the constant changes in your ancestors.
							if (!bone_stream.is_rotation_constant && parent_constant_changed.rotation)
							{
								ACL_ASSERT(any_constant_changed.rotation, "No rotations have changed!");
								adjusted_local_transform.rotation = rtm::quat_normalize(rtm::quat_mul(original_object_transform.rotation, rtm::quat_conjugate(parent_adjusted_object_transform.rotation)));
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
				deallocate_type_array(allocator, adjusted_object_pose, num_bones);
				deallocate_type_array(allocator, original_object_pose, num_bones);
				deallocate_type_array(allocator, dirty_states, num_bones);

				// We need to do these again, to account for error correction.
				if(any_constant_changed.rotation)
				{
					convert_rotation_streams(allocator, context, settings.rotation_format);
				}
				if (any_constant_changed.rotation || any_constant_changed.translation || any_constant_changed.scale)
				{
					deallocate_type_array(allocator, context.ranges, num_bones);
					extract_clip_bone_ranges(allocator, context);
				}
			}

#endif

		}
	}

	ACL_IMPL_VERSION_NAMESPACE_END
}

ACL_IMPL_FILE_PRAGMA_POP
