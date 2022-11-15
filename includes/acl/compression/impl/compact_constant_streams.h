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
#if defined(ACL_IMPL_ENABLE_WEIGHTED_AVERAGE_CONSTANT_SUB_TRACKS)
		inline bool is_rotation_track_constant(const rotation_track_stream& rotations, float threshold_angle, rtm::quatf_arg0 ref_rotation)
#else
		inline bool is_rotation_track_constant(const rotation_track_stream& rotations, float threshold_angle IF_ACL_COMPRESSION_OPTIMIZED(, float max_adjusted_shell_distance, float precision))
#endif
		{

#if !defined(ACL_IMPL_ENABLE_WEIGHTED_AVERAGE_CONSTANT_SUB_TRACKS)
			// Calculating the average rotation and comparing every rotation in the track to it
			// to determine if we are within the threshold seems overkill. We can't use the min/max for the range
			// either because neither of those represents a valid rotation. Instead we grab
			// the first rotation, and compare everything else to it.
#endif

			auto sample_to_quat = [](const rotation_track_stream& track, uint32_t sample_index)
			{
				const rtm::vector4f rotation = track.get_raw_sample<rtm::vector4f>(sample_index);

				switch (track.get_rotation_format())
				{
				case rotation_format8::quatf_full:
					return rtm::vector_to_quat(rotation);
				case rotation_format8::quatf_drop_w_full:
				case rotation_format8::quatf_drop_w_variable:
					// quat_from_positive_w might not yield an accurate quaternion because the square-root instruction
					// isn't very accurate on small inputs, we need to normalize
					return rtm::quat_normalize(rtm::quat_from_positive_w(rotation));
				default:
					ACL_ASSERT(false, "Invalid or unsupported rotation format: %s", get_rotation_format_name(track.get_rotation_format()));
					return rtm::vector_to_quat(rotation);
				}
			};

			const uint32_t num_samples = rotations.get_num_samples();
			if (num_samples <= 1)
				return true;

#if !defined(ACL_IMPL_ENABLE_WEIGHTED_AVERAGE_CONSTANT_SUB_TRACKS)
			const rtm::quatf ref_rotation = sample_to_quat(rotations, 0);
#endif

#ifdef ACL_COMPRESSION_OPTIMIZED

			const rtm::vector4f shell_point_x = rtm::vector_set(max_adjusted_shell_distance, 0.0F, 0.0F, 0.0F);
			const rtm::vector4f shell_point_y = rtm::vector_set(0.0F, max_adjusted_shell_distance, 0.0F, 0.0F);

			const rtm::vector4f ref_vtx0 = rtm::quat_mul_vector3(shell_point_x, ref_rotation);
			const rtm::vector4f ref_vtx1 = rtm::quat_mul_vector3(shell_point_y, ref_rotation);

#endif

			const rtm::quatf inv_ref_rotation = rtm::quat_conjugate(ref_rotation);

			// If our error threshold is zero we want to test if we are binary exact
			// This is used by raw clips, we must preserve the original values
			const bool is_threshold_zero = threshold_angle == 0.0F;

#if defined(ACL_IMPL_ENABLE_WEIGHTED_AVERAGE_CONSTANT_SUB_TRACKS)
			for (uint32_t sample_index = 0; sample_index < num_samples; ++sample_index)
#else
			for (uint32_t sample_index = 1; sample_index < num_samples; ++sample_index)
#endif

			{
				const rtm::quatf rotation = sample_to_quat(rotations, sample_index);

				if (is_threshold_zero)
				{
					if (!rtm::quat_are_equal(rotation, ref_rotation))
						return false;
				}
				else
				{
					const rtm::quatf delta = rtm::quat_normalize(rtm::quat_mul(inv_ref_rotation, rotation));
					if (!rtm::quat_near_identity(delta, threshold_angle))
						return false;
				}

#ifdef ACL_COMPRESSION_OPTIMIZED

				if (max_adjusted_shell_distance == 0.0F)
				{
					continue;
				}

				const rtm::vector4f vtx0 = rtm::quat_mul_vector3(shell_point_x, rotation);
				const rtm::vector4f vtx1 = rtm::quat_mul_vector3(shell_point_y, rotation);

				const rtm::scalarf vtx0_error = rtm::vector_distance3(ref_vtx0, vtx0);
				const rtm::scalarf vtx1_error = rtm::vector_distance3(ref_vtx1, vtx1);

				if (rtm::scalar_cast(rtm::scalar_max(vtx0_error, vtx1_error)) > precision)
				{
					return false;
				}

#endif
			}

			return true;
		}

#ifdef ACL_COMPRESSION_OPTIMIZED

		inline bool is_translation_track_constant(const track_stream_range& translation, float threshold, float precision)
		{
			if (!translation.is_constant(threshold))
			{
				return false;
			}

			const float error = rtm::scalar_cast(rtm::vector_length3(translation.get_extent()));
			return (error <= precision);
		}

		inline bool is_scale_track_constant(const track_stream_range& scale, float threshold, float max_adjusted_shell_distance, float precision)
		{
			if (!scale.is_constant(threshold))
			{
				return false;
			}

			if (max_adjusted_shell_distance == 0.0F)
			{
				return true;
			}

			const rtm::vector4f vtx_error = rtm::vector_mul(scale.get_extent(), max_adjusted_shell_distance);
			const float error = rtm::scalar_cast(rtm::vector_get_max_component(vtx_error));
			return (error <= precision);
		}

#endif

		inline void compact_constant_streams(iallocator& allocator, clip_context& context, IF_ACL_COMPRESSION_OPTIMIZED(const clip_context& raw_clip_context, ) const track_array_qvvf& track_list, const compression_settings& settings)
		{
			ACL_ASSERT(context.num_segments == 1, "context must contain a single segment!");
			segment_context& segment = context.segments[0];

			const uint32_t num_bones = context.num_bones;
			const uint32_t num_samples = context.num_samples;

			uint32_t num_default_bone_scales = 0;

#ifdef ACL_COMPRESSION_OPTIMIZED

			ACL_ASSERT(raw_clip_context.num_segments == 1, "Raw context must contain a single segment!");
			segment_context& raw_segment = raw_clip_context.segments[0];
			bool has_constant_bone_rotations = false;
			bool has_constant_bone_translations = false;
			bool has_constant_bone_scales = false;

			const clip_context::transform_link* transform_links = context.transform_links;
			float* max_adjusted_shell_distances = allocate_type_array<float>(allocator, num_bones);
			std::memset(max_adjusted_shell_distances, 0, num_bones * sizeof(float));
			if (!context.has_additive_base)
			{
				// Propagate worst-case shell distance and object space distances, so we can prevent low-magnitude channels from becoming constant when they're exceeded.

				float* adjusted_shell_distances = allocate_type_array<float>(allocator, num_bones);
				rtm::qvvf* original_object_pose = allocate_type_array<rtm::qvvf>(allocator, num_bones);

				for (uint32_t sample_index = 0; sample_index < num_samples; ++sample_index)
				{
					for (uint32_t bone_index = 0; bone_index < num_bones; ++bone_index)
					{
						rtm::qvvf& original_object_transform = original_object_pose[bone_index];
						const transform_streams& raw_bone_stream = raw_segment.bone_streams[bone_index];
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
						else
						{
							original_object_transform = rtm::qvv_normalize(rtm::qvv_mul(original_local_transform, original_object_pose[parent_bone_index]));
						}

						// Initialize adjusted_shell_distance.
						adjusted_shell_distances[bone_index] = context.metadata[bone_index].shell_distance;
					}

					for (uint32_t bone_index = 0; bone_index < num_bones; ++bone_index)
					{
						// Propagate adjusted_shell_distance into ancestors.
						const clip_context::transform_link& cur_link = transform_links[bone_index];
						const uint32_t cur_child = cur_link.child_transform_index;
						const uint32_t cur_parent = cur_link.parent_transform_index;
						if (cur_parent != k_invalid_track_index)
						{
							const float link_distance = rtm::scalar_cast(rtm::vector_distance3(original_object_pose[cur_parent].translation, original_object_pose[cur_child].translation));
							float& adjusted_parent_shell_distance = adjusted_shell_distances[cur_parent];
							adjusted_parent_shell_distance = std::max(adjusted_parent_shell_distance, link_distance + adjusted_shell_distances[cur_child]);
						}
						float& max_adjusted_shell_distance = max_adjusted_shell_distances[cur_child];
						max_adjusted_shell_distance = std::max(max_adjusted_shell_distance, adjusted_shell_distances[cur_child]);
					}
				}

				deallocate_type_array(allocator, original_object_pose, num_bones);
				deallocate_type_array(allocator, adjusted_shell_distances, num_bones);
			}

#endif

			// When a stream is constant, we only keep the first sample
			for (uint32_t bone_index = 0; bone_index < num_bones; ++bone_index)
			{
				const track_desc_transformf& desc = track_list[bone_index].get_description();

				transform_streams& bone_stream = segment.bone_streams[bone_index];
				transform_range& bone_range = context.ranges[bone_index];

#ifdef ACL_COMPRESSION_OPTIMIZED

				ACL_ASSERT(bone_stream.rotations.get_num_samples() == num_samples, "Rotation sample mismatch!");
				ACL_ASSERT(bone_stream.translations.get_num_samples() == num_samples, "Translation sample mismatch!");
				ACL_ASSERT(bone_stream.scales.get_num_samples() == num_samples, "Scale sample mismatch!");

#endif

				// We expect all our samples to have the same width of sizeof(rtm::vector4f)
				ACL_ASSERT(bone_stream.rotations.get_sample_size() == sizeof(rtm::vector4f), "Unexpected rotation sample size. %u != %zu", bone_stream.rotations.get_sample_size(), sizeof(rtm::vector4f));
				ACL_ASSERT(bone_stream.translations.get_sample_size() == sizeof(rtm::vector4f), "Unexpected translation sample size. %u != %zu", bone_stream.translations.get_sample_size(), sizeof(rtm::vector4f));
				ACL_ASSERT(bone_stream.scales.get_sample_size() == sizeof(rtm::vector4f), "Unexpected scale sample size. %u != %zu", bone_stream.scales.get_sample_size(), sizeof(rtm::vector4f));

				// If we request raw data, use a 0.0 threshold for safety
				const float constant_rotation_threshold_angle = settings.rotation_format != rotation_format8::quatf_full ? desc.constant_rotation_threshold_angle : 0.0F;
				const float constant_translation_threshold = settings.translation_format != vector_format8::vector3f_full ? desc.constant_translation_threshold : 0.0F;
				const float constant_scale_threshold = settings.scale_format != vector_format8::vector3f_full ? desc.constant_scale_threshold : 0.0F;

#if defined(ACL_IMPL_ENABLE_WEIGHTED_AVERAGE_CONSTANT_SUB_TRACKS)
				if (is_rotation_track_constant(bone_stream.rotations, constant_rotation_threshold_angle, rtm::vector_to_quat(bone_range.rotation.get_weighted_average())))
#else
				if (

#ifdef ACL_COMPRESSION_OPTIMIZED

					bone_range.rotation.is_constant() || ((constant_rotation_threshold_angle > 0.0F) &&

#endif

					is_rotation_track_constant(bone_stream.rotations, constant_rotation_threshold_angle IF_ACL_COMPRESSION_OPTIMIZED(, max_adjusted_shell_distances[bone_index], desc.precision)))

#ifdef ACL_COMPRESSION_OPTIMIZED

					)

#endif
#endif
				{
					rotation_track_stream constant_stream(allocator, 1, bone_stream.rotations.get_sample_size(), bone_stream.rotations.get_sample_rate(), bone_stream.rotations.get_rotation_format());

					const rtm::vector4f default_bind_rotation = rtm::quat_to_vector(desc.default_value.rotation);

#if defined(ACL_IMPL_ENABLE_WEIGHTED_AVERAGE_CONSTANT_SUB_TRACKS)
					rtm::vector4f rotation = num_samples != 0 ? bone_range.rotation.get_weighted_average() : default_bind_rotation;
#else
					rtm::vector4f rotation = num_samples != 0 ? bone_stream.rotations.get_raw_sample<rtm::vector4f>(0) : default_bind_rotation;
#endif

					bone_stream.is_rotation_constant = true;

					// If our error threshold is zero we want to test if we are binary exact
					// This is used by raw clips, we must preserve the original values
					if (constant_rotation_threshold_angle == 0.0F)
						bone_stream.is_rotation_default = rtm::vector_all_equal(rotation, default_bind_rotation);
					else
						bone_stream.is_rotation_default = rtm::quat_near_identity(rtm::quat_normalize(rtm::quat_mul(rtm::vector_to_quat(rotation), rtm::quat_conjugate(rtm::vector_to_quat(default_bind_rotation)))), constant_rotation_threshold_angle);

					if (bone_stream.is_rotation_default)
						rotation = default_bind_rotation;

					constant_stream.set_raw_sample(0, rotation);
					bone_stream.rotations = std::move(constant_stream);

#if defined(ACL_IMPL_ENABLE_WEIGHTED_AVERAGE_CONSTANT_SUB_TRACKS)
					bone_range.rotation = track_stream_range::from_min_extent(rotation, rtm::vector_zero(), rotation);
#else
					bone_range.rotation = track_stream_range::from_min_extent(rotation, rtm::vector_zero());
#endif

#ifdef ACL_COMPRESSION_OPTIMIZED

					has_constant_bone_rotations = true;

#endif
				}

				
#ifdef ACL_COMPRESSION_OPTIMIZED

				if (bone_range.translation.is_constant() || is_translation_track_constant(bone_range.translation, constant_translation_threshold, desc.precision))

#else

				if (bone_range.translation.is_constant(constant_translation_threshold))

#endif

				{
					translation_track_stream constant_stream(allocator, 1, bone_stream.translations.get_sample_size(), bone_stream.translations.get_sample_rate(), bone_stream.translations.get_vector_format());

					const rtm::vector4f default_bind_translation = desc.default_value.translation;

#if defined(ACL_IMPL_ENABLE_WEIGHTED_AVERAGE_CONSTANT_SUB_TRACKS)
					rtm::vector4f translation = num_samples != 0 ? bone_range.translation.get_weighted_average() : default_bind_translation;
#else
					rtm::vector4f translation = num_samples != 0 ? bone_stream.translations.get_raw_sample<rtm::vector4f>(0) : default_bind_translation;
#endif

					bone_stream.is_translation_constant = true;

					// If our error threshold is zero we want to test if we are binary exact
					// This is used by raw clips, we must preserve the original values
					if (constant_translation_threshold == 0.0F)
						bone_stream.is_translation_default = rtm::vector_all_equal3(translation, default_bind_translation);
					else
						bone_stream.is_translation_default = rtm::vector_all_near_equal3(translation, default_bind_translation, constant_translation_threshold);

					if (bone_stream.is_translation_default)
						translation = default_bind_translation;

					constant_stream.set_raw_sample(0, translation);
					bone_stream.translations = std::move(constant_stream);

#if defined(ACL_IMPL_ENABLE_WEIGHTED_AVERAGE_CONSTANT_SUB_TRACKS)
					bone_range.translation = track_stream_range::from_min_extent(translation, rtm::vector_zero(), translation);
#else
					bone_range.translation = track_stream_range::from_min_extent(translation, rtm::vector_zero());
#endif

#ifdef ACL_COMPRESSION_OPTIMIZED

					has_constant_bone_translations = true;

#endif
				}

#ifdef ACL_COMPRESSION_OPTIMIZED

				if (bone_range.scale.is_constant() || is_scale_track_constant(bone_range.scale, constant_scale_threshold, max_adjusted_shell_distances[bone_index], desc.precision))

#else

				if(bone_range.scale.is_constant(constant_scale_threshold))

#endif

				{
					scale_track_stream constant_stream(allocator, 1, bone_stream.scales.get_sample_size(), bone_stream.scales.get_sample_rate(), bone_stream.scales.get_vector_format());

					const rtm::vector4f default_bind_scale = desc.default_value.scale;

#if defined(ACL_IMPL_ENABLE_WEIGHTED_AVERAGE_CONSTANT_SUB_TRACKS)
					rtm::vector4f scale = (context.has_scale && (num_samples != 0)) ? bone_range.scale.get_weighted_average() : default_bind_scale;
#else
					rtm::vector4f scale = (context.has_scale && num_samples != 0) ? bone_stream.scales.get_raw_sample<rtm::vector4f>(0) : default_bind_scale;
#endif

					bone_stream.is_scale_constant = true;

					// If our error threshold is zero we want to test if we are binary exact
					// This is used by raw clips, we must preserve the original values
					if (constant_scale_threshold == 0.0F)
						bone_stream.is_scale_default = rtm::vector_all_equal3(scale, default_bind_scale);
					else
						bone_stream.is_scale_default = rtm::vector_all_near_equal3(scale, default_bind_scale, constant_scale_threshold);

					if (bone_stream.is_scale_default)
						scale = default_bind_scale;

					constant_stream.set_raw_sample(0, scale);
					bone_stream.scales = std::move(constant_stream);

#if defined(ACL_IMPL_ENABLE_WEIGHTED_AVERAGE_CONSTANT_SUB_TRACKS)
					bone_range.scale = track_stream_range::from_min_extent(scale, rtm::vector_zero(), scale);
#else
					bone_range.scale = track_stream_range::from_min_extent(scale, rtm::vector_zero());
#endif

					num_default_bone_scales += bone_stream.is_scale_default ? 1 : 0;

#ifdef ACL_COMPRESSION_OPTIMIZED

					has_constant_bone_scales = true;

#endif

				}
			}

			context.has_scale = num_default_bone_scales != num_bones;

#ifdef ACL_COMPRESSION_OPTIMIZED

			deallocate_type_array(allocator, max_adjusted_shell_distances, num_bones);
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
