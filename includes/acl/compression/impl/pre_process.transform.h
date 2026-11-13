#pragma once

////////////////////////////////////////////////////////////////////////////////
// The MIT License (MIT)
//
// Copyright (c) 2023 Nicholas Frechette & Animation Compression Library contributors
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
#include "acl/core/impl/compiler_utils.h"
#include "acl/compression/impl/optimize_looping.transform.h"
#include "acl/compression/impl/pre_process.common.h"
#include "acl/compression/impl/transform_clip_adapters.h"
#include "acl/compression/impl/transform_track_utils.h"
#include "acl/math/quatf.h"

#include <rtm/quatf.h>
#include <rtm/vqf.h>
#include <rtm/vqsf.h>
#include <rtm/vqvf.h>
#include <rtm/scalarf.h>
#include <rtm/vector4f.h>

#include <cstdint>

ACL_IMPL_FILE_PRAGMA_PUSH

// This file reads the deprecated error metric of the settings
ACL_IMPL_DEPRECATION_WARNING_PUSH

namespace acl
{
	ACL_IMPL_VERSION_NAMESPACE_BEGIN

	namespace acl_impl
	{
		// Sets the scale of a transform sample, a VQ sample has no scale
		// A VQS sample has a uniform scale: we use the X component, its value is in [w]
		inline void RTM_SIMD_CALL pre_process_set_sample_scale(rtm::vqvf& sample, rtm::vector4f_arg0 scale) { sample.scale = scale; }
		inline void RTM_SIMD_CALL pre_process_set_sample_scale(const rtm::vqf& sample, rtm::vector4f_arg0 scale) { (void)sample; (void)scale; }
		inline void RTM_SIMD_CALL pre_process_set_sample_scale(rtm::vqsf& sample, rtm::vector4f_arg0 scale) { sample.translation_scale = rtm::vector_set_w(sample.translation_scale, rtm::vector_get_x(scale)); }

		// Returns the translation of a transform sample, a VQS sample has its scale in [w]
		inline rtm::vector4f RTM_SIMD_CALL pre_process_get_sample_translation(const rtm::vqvf& sample) { return sample.translation; }
		inline rtm::vector4f RTM_SIMD_CALL pre_process_get_sample_translation(const rtm::vqf& sample) { return sample.translation; }
		inline rtm::vector4f RTM_SIMD_CALL pre_process_get_sample_translation(const rtm::vqsf& sample) { return sample.translation_scale; }

		// Sets the translation of a transform sample, a VQS sample keeps its scale in [w]
		inline void RTM_SIMD_CALL pre_process_set_sample_translation(rtm::vqvf& sample, rtm::vector4f_arg0 translation) { sample.translation = translation; }
		inline void RTM_SIMD_CALL pre_process_set_sample_translation(rtm::vqf& sample, rtm::vector4f_arg0 translation) { sample.translation = translation; }
		inline void RTM_SIMD_CALL pre_process_set_sample_translation(rtm::vqsf& sample, rtm::vector4f_arg0 translation) { sample.translation_scale = rtm::vector_set_w(translation, rtm::vector_get_w(sample.translation_scale)); }

		//////////////////////////////////////////////////////////////////////////
		// Returns the uniform value of a 3D scale.
		// When the scale is exactly uniform, it is the X component. Otherwise, it is the mean of [xyz].
		inline float RTM_SIMD_CALL pre_process_get_uniform_scale(rtm::vector4f_arg0 scale)
		{
			const float x = rtm::vector_get_x(scale);
			const float y = rtm::vector_get_y(scale);
			const float z = rtm::vector_get_z(scale);
			if (x == y && x == z)
				return x;

			return (x + y + z) * (1.0F / 3.0F);
		}

		// Returns true if a 3D scale is exactly uniform: [xyz] are equal
		inline bool RTM_SIMD_CALL pre_process_is_scale_uniform(rtm::vector4f_arg0 scale)
		{
			return rtm::vector_get_x(scale) == rtm::vector_get_y(scale) && rtm::vector_get_x(scale) == rtm::vector_get_z(scale);
		}

		// Converts a VQV transform into a transform sample
		// A VQS sample uses the uniform value of the scale, see 'pre_process_get_uniform_scale'
		inline void RTM_SIMD_CALL pre_process_set_sample(const rtm::vqvf& transform, rtm::vqvf& out_sample) { out_sample = transform; }
		inline void RTM_SIMD_CALL pre_process_set_sample(const rtm::vqvf& transform, rtm::vqf& out_sample) { out_sample = rtm::vq_set(transform.rotation, transform.translation); }
		inline void RTM_SIMD_CALL pre_process_set_sample(const rtm::vqvf& transform, rtm::vqsf& out_sample) { out_sample = rtm::vqs_set(transform.rotation, transform.translation, pre_process_get_uniform_scale(transform.scale)); }

		template<class track_array_type>
		inline void pre_process_normalize_rotations_impl(track_array_type& track_list)
		{
			const uint32_t num_samples_per_track = track_list.get_num_samples_per_track();

			for (typename track_array_type::track_member_type& track_ : track_list)
			{
				for (uint32_t sample_index = 0; sample_index < num_samples_per_track; ++sample_index)
				{
					typename track_array_type::track_member_type::sample_type& sample = track_[sample_index];

					if (!rtm::quat_is_normalized(sample.rotation))
						sample.rotation = quat_normalize_stable(sample.rotation);
				}
			}
		}

		inline void pre_process_normalize_rotations(pre_process_context_t& context)
		{
			if (context.settings.precision_policy == pre_process_precision_policy::lossless)
				return;	// This pre-process action is lossy, do nothing

			if (context.track_list.get_track_type() == track_type8::vqf)
				pre_process_normalize_rotations_impl(track_array_cast<track_array_vqf>(context.track_list));
			else if (context.track_list.get_track_type() == track_type8::vqsf)
				pre_process_normalize_rotations_impl(track_array_cast<track_array_vqsf>(context.track_list));
			else
				pre_process_normalize_rotations_impl(track_array_cast<track_array_vqvf>(context.track_list));
		}

		template<class track_array_type>
		inline void pre_process_ensure_quat_w_positive_impl(track_array_type& track_list)
		{
			const uint32_t num_samples_per_track = track_list.get_num_samples_per_track();

			for (typename track_array_type::track_member_type& track_ : track_list)
			{
				for (uint32_t sample_index = 0; sample_index < num_samples_per_track; ++sample_index)
				{
					typename track_array_type::track_member_type::sample_type& sample = track_[sample_index];
					sample.rotation = rtm::quat_ensure_positive_w(sample.rotation);
				}
			}
		}

		inline void pre_process_ensure_quat_w_positive(pre_process_context_t& context)
		{
			if (context.track_list.get_track_type() == track_type8::vqf)
				pre_process_ensure_quat_w_positive_impl(track_array_cast<track_array_vqf>(context.track_list));
			else if (context.track_list.get_track_type() == track_type8::vqsf)
				pre_process_ensure_quat_w_positive_impl(track_array_cast<track_array_vqsf>(context.track_list));
			else
				pre_process_ensure_quat_w_positive_impl(track_array_cast<track_array_vqvf>(context.track_list));
		}

		template<class track_array_type>
		inline void pre_process_copy_first_sample_to_last(track_array_type& track_list)
		{
			const uint32_t last_sample_index = track_list.get_num_samples_per_track() - 1;

			for (typename track_array_type::track_member_type& track_ : track_list)
				track_[last_sample_index] = track_[0];
		}

		inline void pre_process_optimize_looping_transform(pre_process_context_t& context)
		{
			track_array& track_list = context.track_list;

			transform_track_array_adapter_t clip_adapter(&track_list, context.settings.additive_format);

			const bool is_looping = is_clip_looping(
				context.allocator,
				clip_adapter,
				transform_track_array_adapter_t(context.settings.additive_base, additive_clip_format8::none),
				*context.settings.error_metric);

			if (is_looping)
			{
				// We just replace the last sample with a copy of the first to ensure that
				// we have perfect looping. We do not change the wrapping mode of the track list.
				if (track_list.get_track_type() == track_type8::vqf)
					pre_process_copy_first_sample_to_last(track_array_cast<track_array_vqf>(track_list));
				else if (track_list.get_track_type() == track_type8::vqsf)
					pre_process_copy_first_sample_to_last(track_array_cast<track_array_vqsf>(track_list));
				else
					pre_process_copy_first_sample_to_last(track_array_cast<track_array_vqvf>(track_list));
			}
		}

		inline uint32_t pre_process_get_uniform_sample_key(const track_array& track_list, float sample_time)
		{
			return get_uniform_sample_key(
				transform_track_array_adapter_t(track_list),
				transform_segment_adapter_t(),
				sample_time);
		}

		// Replaces a sub-track of a transform with a reference value
		struct pre_process_constant_sub_track_fn
		{
			rtm::vector4f reference;
			animation_track_type8 sub_track_type;

			rtm::vqvf RTM_SIMD_CALL operator()(const rtm::vqvf& raw_transform) const
			{
				rtm::vqvf lossy_transform = raw_transform;	// Copy the raw transform

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

				return lossy_transform;
			}
		};

		// Replaces the scale of a transform with its uniform value, see 'pre_process_get_uniform_scale'
		struct pre_process_uniform_scale_fn
		{
			rtm::vqvf RTM_SIMD_CALL operator()(const rtm::vqvf& raw_transform) const
			{
				rtm::vqvf lossy_transform = raw_transform;	// Copy the raw transform
				lossy_transform.scale = rtm::vector_set(pre_process_get_uniform_scale(raw_transform.scale));
				return lossy_transform;
			}
		};

		// Returns true if the error stays within the precision when we replace each sample of a track with a lossy transform
		// The functor 'lossy_transform_fn' returns the lossy transform from the raw transform
		template<class lossy_transform_fn_type>
		inline bool pre_process_is_lossy_track_within_precision(
			pre_process_context_t& context,
			const track_array& track_list,
			uint32_t track_index,
			const lossy_transform_fn_type& lossy_transform_fn)
		{
			const pre_process_settings_t& settings = context.settings;
			const track& track_ = track_list[track_index];

			const rigid_shell_metadata_t& shell = context.get_shell_metadata()[track_index];

			const itransform_error_metric& error_metric = *settings.error_metric;

			const bool has_scale = true;	// Assume we have scale
			const bool has_additive_base = settings.additive_format != additive_clip_format8::none;
			const bool needs_conversion = error_metric.needs_conversion(has_scale);

			// A VQ track has no scale, each of its samples uses this scale
			// The additive base is a regular pose
			const rtm::vector4f vq_scale = rtm::vector_set(get_additive_default_scale(settings.additive_format));
			const rtm::vector4f vq_base_scale = rtm::vector_set(get_additive_default_scale(additive_clip_format8::none));

			const uint32_t dirty_transform_indices[2] = { 0, 1 };
			rtm::vqvf local_transforms[2];
			rtm::vqvf base_transforms[2];
			alignas(16) uint8_t local_transforms_converted[1024];	// Big enough for 2 transforms for sure

			const size_t metric_transform_size = error_metric.get_transform_size(has_scale);
			ACL_ASSERT(metric_transform_size * 2 <= sizeof(local_transforms_converted), "Transform size is too large");

			// The error metric receives the real transform index and the index of the sample in the clip
			const single_transform_converter converter(context.allocator, error_metric, has_scale, track_list.get_num_tracks());

			itransform_error_metric::apply_additive_to_base_args apply_additive_to_base_args;
			apply_additive_to_base_args.dirty_transform_indices = &dirty_transform_indices[0];
			apply_additive_to_base_args.num_dirty_transforms = 2;
			apply_additive_to_base_args.base_transforms = &base_transforms[0];
			apply_additive_to_base_args.local_transforms = &local_transforms[0];
			apply_additive_to_base_args.num_transforms = 2;

			itransform_error_metric::calculate_error_args calculate_error_args;
			calculate_error_args.construct_sphere_shell(shell.local_shell_distance);
			calculate_error_args.transform0 = &local_transforms_converted[metric_transform_size * 0];
			calculate_error_args.transform1 = &local_transforms_converted[metric_transform_size * 1];

			const rtm::scalarf precision_sq = rtm::scalar_set(shell.precision * shell.precision);

			const uint32_t num_samples = track_list.get_num_samples_per_track();
			const float sample_rate = track_list.get_sample_rate();
			const float duration = track_list.get_finite_duration();
			const uint32_t num_base_samples = has_additive_base ? settings.additive_base->get_num_samples_per_track() : 0;
			const float base_duration = has_additive_base ? settings.additive_base->get_finite_duration() : 0.0F;

			for (uint32_t sample_index = 0; sample_index < num_samples; ++sample_index)
			{
				const rtm::vqvf raw_transform = get_transform_track_sample(track_, sample_index, vq_scale);
				const rtm::vqvf lossy_transform = lossy_transform_fn(raw_transform);

				local_transforms[0] = raw_transform;
				local_transforms[1] = lossy_transform;

				if (has_additive_base)
				{
					// The sample time is calculated from the full clip duration to be consistent with decompression
					const float sample_time = rtm::scalar_min(float(sample_index) / sample_rate, duration);

					const float normalized_sample_time = num_base_samples > 1 ? (sample_time / duration) : 0.0F;
					const float additive_sample_time = num_base_samples > 1 ? (normalized_sample_time * base_duration) : 0.0F;

					// With uniform sample distributions, we do not interpolate.
					const uint32_t base_sample_index = pre_process_get_uniform_sample_key(*settings.additive_base, additive_sample_time);

					const track& base_track = (*settings.additive_base)[track_index];
					const rtm::vqvf base_transform = get_transform_track_sample(base_track, base_sample_index, vq_base_scale);

					base_transforms[0] = base_transform;
					base_transforms[1] = base_transform;

					// The additive applies to the source transforms, before the conversion
					error_metric.apply_additive_to_base(apply_additive_to_base_args, &local_transforms[0]);
				}

				if (needs_conversion)
				{
					converter.convert(local_transforms[0], track_index, sample_index, false, &local_transforms_converted[metric_transform_size * 0]);
					converter.convert(local_transforms[1], track_index, sample_index, true, &local_transforms_converted[metric_transform_size * 1]);
				}
				else
					std::memcpy(&local_transforms_converted[0], &local_transforms[0], metric_transform_size * 2);

				const rtm::scalarf vtx_error_sq = error_metric.calculate_error_squared(calculate_error_args);

				// If our error exceeds the desired precision, we are not constant
				if (rtm::scalar_greater_than(vtx_error_sq, precision_sq))
					return false;
			}

			// All samples were tested against the lossy transform and the error remained within tolerance
			return true;
		}

		inline bool RTM_SIMD_CALL pre_process_are_samples_constant(
			pre_process_context_t& context,
			const track_array& track_list,
			uint32_t track_index,
			rtm::vector4f_arg0 reference,
			animation_track_type8 sub_track_type)
		{
			pre_process_constant_sub_track_fn lossy_transform_fn;
			lossy_transform_fn.reference = reference;
			lossy_transform_fn.sub_track_type = sub_track_type;

			return pre_process_is_lossy_track_within_precision(context, track_list, track_index, lossy_transform_fn);
		}

		template<class track_array_type>
		inline void pre_process_sanitize_constant_tracks_transform_impl(pre_process_context_t& context, track_array_type& track_list)
		{
			const uint32_t num_tracks = track_list.get_num_tracks();
			const uint32_t num_samples_per_track = track_list.get_num_samples_per_track();

			// A VQ track has no scale
			const bool has_scale = track_array_type::type != track_type8::vqf;

			// Iterate in any order, doesn't matter
			for (uint32_t track_index = 0; track_index < num_tracks; ++track_index)
			{
				typename track_array_type::track_member_type& track_ = track_list[track_index];

				const rtm::quatf reference_rotation = track_[0].rotation;
				if (pre_process_are_samples_constant(context, track_list, track_index, rtm::quat_to_vector(reference_rotation), animation_track_type8::rotation))
				{
					for (uint32_t sample_index = 1; sample_index < num_samples_per_track; ++sample_index)
						track_[sample_index].rotation = reference_rotation;
				}

				const rtm::vector4f reference_translation = pre_process_get_sample_translation(track_[0]);
				if (pre_process_are_samples_constant(context, track_list, track_index, reference_translation, animation_track_type8::translation))
				{
					for (uint32_t sample_index = 1; sample_index < num_samples_per_track; ++sample_index)
						pre_process_set_sample_translation(track_[sample_index], reference_translation);
				}

				if (!has_scale)
					continue;

				const rtm::vector4f reference_scale = get_transform_track_sample(track_, 0, rtm::vector_set(1.0F)).scale;
				if (pre_process_are_samples_constant(context, track_list, track_index, reference_scale, animation_track_type8::scale))
				{
					for (uint32_t sample_index = 1; sample_index < num_samples_per_track; ++sample_index)
						pre_process_set_sample_scale(track_[sample_index], reference_scale);
				}
			}
		}

		inline void pre_process_sanitize_constant_tracks_transform(pre_process_context_t& context)
		{
			if (context.track_list.get_track_type() == track_type8::vqf)
				pre_process_sanitize_constant_tracks_transform_impl(context, track_array_cast<track_array_vqf>(context.track_list));
			else if (context.track_list.get_track_type() == track_type8::vqsf)
				pre_process_sanitize_constant_tracks_transform_impl(context, track_array_cast<track_array_vqsf>(context.track_list));
			else
				pre_process_sanitize_constant_tracks_transform_impl(context, track_array_cast<track_array_vqvf>(context.track_list));
		}

		template<class track_array_type>
		inline void pre_process_sanitize_default_tracks_transform_impl(pre_process_context_t& context, track_array_type& track_list)
		{
			const uint32_t num_tracks = track_list.get_num_tracks();
			const uint32_t num_samples_per_track = track_list.get_num_samples_per_track();

			// A VQ track has no scale
			const bool has_scale = track_array_type::type != track_type8::vqf;

			// Iterate in any order, doesn't matter
			for (uint32_t track_index = 0; track_index < num_tracks; ++track_index)
			{
				typename track_array_type::track_member_type& track_ = track_list[track_index];
				const track_desc_transformf& desc = track_.get_description();

				const rtm::quatf reference_rotation = desc.default_value.rotation;
				if (pre_process_are_samples_constant(context, track_list, track_index, rtm::quat_to_vector(reference_rotation), animation_track_type8::rotation))
				{
					for (uint32_t sample_index = 0; sample_index < num_samples_per_track; ++sample_index)
						track_[sample_index].rotation = reference_rotation;
				}

				const rtm::vector4f reference_translation = desc.default_value.translation;
				if (pre_process_are_samples_constant(context, track_list, track_index, reference_translation, animation_track_type8::translation))
				{
					for (uint32_t sample_index = 0; sample_index < num_samples_per_track; ++sample_index)
						pre_process_set_sample_translation(track_[sample_index], reference_translation);
				}

				if (!has_scale)
					continue;

				// A VQS track uses the X component of the default scale
				const rtm::vector4f reference_scale = get_transform_track_desc(track_, rtm::vector_set(1.0F)).default_value.scale;
				if (pre_process_are_samples_constant(context, track_list, track_index, reference_scale, animation_track_type8::scale))
				{
					for (uint32_t sample_index = 0; sample_index < num_samples_per_track; ++sample_index)
						pre_process_set_sample_scale(track_[sample_index], reference_scale);
				}
			}
		}

		inline void pre_process_sanitize_default_tracks_transform(pre_process_context_t& context)
		{
			if (context.track_list.get_track_type() == track_type8::vqf)
				pre_process_sanitize_default_tracks_transform_impl(context, track_array_cast<track_array_vqf>(context.track_list));
			else if (context.track_list.get_track_type() == track_type8::vqsf)
				pre_process_sanitize_default_tracks_transform_impl(context, track_array_cast<track_array_vqsf>(context.track_list));
			else
				pre_process_sanitize_default_tracks_transform_impl(context, track_array_cast<track_array_vqvf>(context.track_list));
		}

		// Returns true if no track needs its scale: we can remove the scale of each track
		// The track list can contain vqvf or vqsf tracks
		inline bool pre_process_can_remove_scale(pre_process_context_t& context, const track_array& track_list)
		{
			const pre_process_settings_t& settings = context.settings;
			const bool is_lossy = settings.precision_policy == pre_process_precision_policy::lossy;

			// The scale of a VQ track is the identity of the additive format
			const rtm::vector4f identity_scale = rtm::vector_set(get_additive_default_scale(settings.additive_format));

			const uint32_t num_tracks = track_list.get_num_tracks();
			const uint32_t num_samples_per_track = track_list.get_num_samples_per_track();

			for (uint32_t track_index = 0; track_index < num_tracks; ++track_index)
			{
				const track& track_ = track_list[track_index];

				bool is_identity = true;
				for (uint32_t sample_index = 0; sample_index < num_samples_per_track; ++sample_index)
				{
					if (!rtm::vector_all_near_equal3(get_transform_track_sample(track_, sample_index, identity_scale).scale, identity_scale, 0.0F))
					{
						is_identity = false;
						break;
					}
				}

				if (is_identity)
					continue;	// The samples have no scale

				// With the lossy policy, the error metric tells if the scale is visible
				if (!is_lossy || !pre_process_are_samples_constant(context, track_list, track_index, identity_scale, animation_track_type8::scale))
					return false;	// This track needs its scale
			}

			return true;
		}

		// Returns true if no track needs a non-uniform scale: we can use a uniform scale for each track
		inline bool pre_process_can_use_uniform_scale(pre_process_context_t& context, const track_array_vqvf& track_list)
		{
			const bool is_lossy = context.settings.precision_policy == pre_process_precision_policy::lossy;

			const uint32_t num_tracks = track_list.get_num_tracks();
			const uint32_t num_samples_per_track = track_list.get_num_samples_per_track();

			for (uint32_t track_index = 0; track_index < num_tracks; ++track_index)
			{
				const track_vqvf& track_ = track_list[track_index];

				bool is_uniform = true;
				for (uint32_t sample_index = 0; sample_index < num_samples_per_track; ++sample_index)
				{
					if (!pre_process_is_scale_uniform(track_[sample_index].scale))
					{
						is_uniform = false;
						break;
					}
				}

				if (is_uniform)
					continue;	// The samples have a uniform scale

				// With the lossy policy, the error metric tells if the non-uniform scale is visible
				if (!is_lossy || !pre_process_is_lossy_track_within_precision(context, track_list, track_index, pre_process_uniform_scale_fn()))
					return false;	// This track needs its non-uniform scale
			}

			return true;
		}

		// Returns how much scale information a transform type holds: none (vqf), uniform (vqsf), or 3D (vqvf)
		inline uint32_t pre_process_get_scale_rank(track_type8 type)
		{
			switch (type)
			{
			case track_type8::vqf:	return 0;
			case track_type8::vqsf:	return 1;
			case track_type8::vqvf:
			default:				return 2;
			}
		}

		// Builds a track list of the target type from transform tracks
		// Each sample is first a VQV transform, see 'get_transform_track_sample'
		template<class target_track_array_type>
		inline void pre_process_build_transform_tracks(pre_process_context_t& context, const track_array& track_list, target_track_array_type& out_track_list)
		{
			using target_track_type = typename target_track_array_type::track_member_type;
			const track_type8 target_type = context.settings.transform_type;
			ACL_ASSERT(target_type == target_track_array_type::type, "Unexpected target type");

			// A VQ track has no scale, its new scale is the identity of the additive format
			const rtm::vector4f identity_scale = rtm::vector_set(get_additive_default_scale(context.settings.additive_format));

			const uint32_t num_tracks = track_list.get_num_tracks();
			const uint32_t num_samples = track_list.get_num_samples_per_track();
			const float sample_rate = track_list.get_sample_rate();

			out_track_list.set_name(track_list.get_name());
			out_track_list.set_looping_policy(track_list.get_looping_policy());

			for (uint32_t track_index = 0; track_index < num_tracks; ++track_index)
			{
				const track& track_ = track_list[track_index];

				track_desc_transformf desc = get_transform_track_desc(track_, identity_scale);
				if (target_type == track_type8::vqf)
					desc.default_value.scale = rtm::vector_set(1.0F);	// A VQ track has no scale, ACL ignores the scale of its default value
				else if (target_type == track_type8::vqsf)
					desc.default_value.scale = rtm::vector_set(pre_process_get_uniform_scale(desc.default_value.scale));

				target_track_type target_track = target_track_type::make_reserve(desc, context.allocator, num_samples, sample_rate);
				target_track.set_name(track_.get_name());

				for (uint32_t sample_index = 0; sample_index < num_samples; ++sample_index)
					pre_process_set_sample(get_transform_track_sample(track_, sample_index, identity_scale), target_track[sample_index]);

				out_track_list[track_index] = std::move(target_track);
			}
		}

		inline void pre_process_convert_transform_type(pre_process_context_t& context)
		{
			const pre_process_settings_t& settings = context.settings;
			track_array& track_list = context.track_list;

			const track_type8 source_type = track_list.get_track_type();
			const track_type8 target_type = settings.transform_type;
			if (source_type == target_type)
				return;	// Nothing to do

			// A promotion (for example vqf -> vqsf) always succeeds
			// A demotion (for example vqvf -> vqsf) happens only when no track needs the scale it removes
			if (pre_process_get_scale_rank(target_type) < pre_process_get_scale_rank(source_type))
			{
				if (target_type == track_type8::vqf)
				{
					if (!pre_process_can_remove_scale(context, track_list))
						return;
				}
				else if (!pre_process_can_use_uniform_scale(context, track_array_cast<track_array_vqvf>(track_list)))
					return;
			}

			iallocator& allocator = context.allocator;
			const uint32_t num_tracks = track_list.get_num_tracks();

			if (target_type == track_type8::vqf)
			{
				track_array_vqf target_tracks(allocator, num_tracks);
				pre_process_build_transform_tracks(context, track_list, target_tracks);
				track_list = std::move(target_tracks);
			}
			else if (target_type == track_type8::vqsf)
			{
				track_array_vqsf target_tracks(allocator, num_tracks);
				pre_process_build_transform_tracks(context, track_list, target_tracks);
				track_list = std::move(target_tracks);
			}
			else
			{
				track_array_vqvf target_tracks(allocator, num_tracks);
				pre_process_build_transform_tracks(context, track_list, target_tracks);
				track_list = std::move(target_tracks);
			}
		}
	}

	ACL_IMPL_VERSION_NAMESPACE_END
}

ACL_IMPL_DEPRECATION_WARNING_POP

ACL_IMPL_FILE_PRAGMA_POP
