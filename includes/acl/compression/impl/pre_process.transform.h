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
#include <rtm/vqvf.h>
#include <rtm/scalarf.h>
#include <rtm/vector4f.h>

#include <cstdint>

ACL_IMPL_FILE_PRAGMA_PUSH

namespace acl
{
	ACL_IMPL_VERSION_NAMESPACE_BEGIN

	namespace acl_impl
	{
		// Sets the scale of a transform sample, a VQ sample has no scale
		inline void RTM_SIMD_CALL pre_process_set_sample_scale(rtm::vqvf& sample, rtm::vector4f_arg0 scale) { sample.scale = scale; }
		inline void RTM_SIMD_CALL pre_process_set_sample_scale(rtm::vqf& sample, rtm::vector4f_arg0 scale) { (void)sample; (void)scale; }

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

		inline bool RTM_SIMD_CALL pre_process_are_samples_constant(
			pre_process_context_t& context,
			const track_array& track_list,
			uint32_t track_index,
			rtm::vector4f_arg0 reference,
			animation_track_type8 sub_track_type)
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

			itransform_error_metric::convert_transforms_args convert_transforms_args_local;
			convert_transforms_args_local.dirty_transform_indices = &dirty_transform_indices[0];
			convert_transforms_args_local.num_dirty_transforms = 1;
			convert_transforms_args_local.num_transforms = 1;
			convert_transforms_args_local.is_additive_base = false;

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
				rtm::vqvf lossy_transform = raw_transform;	// Copy the raw transform

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

				const rtm::scalarf vtx_error_sq = error_metric.calculate_error_squared(calculate_error_args);

				// If our error exceeds the desired precision, we are not constant
				if (rtm::scalar_greater_than(vtx_error_sq, precision_sq))
					return false;
			}

			// All samples were tested against the reference value and the error remained within tolerance
			return true;
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

				const rtm::vector4f reference_translation = track_[0].translation;
				if (pre_process_are_samples_constant(context, track_list, track_index, reference_translation, animation_track_type8::translation))
				{
					for (uint32_t sample_index = 1; sample_index < num_samples_per_track; ++sample_index)
						track_[sample_index].translation = reference_translation;
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
						track_[sample_index].translation = reference_translation;
				}

				if (!has_scale)
					continue;

				const rtm::vector4f reference_scale = desc.default_value.scale;
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
			else
				pre_process_sanitize_default_tracks_transform_impl(context, track_array_cast<track_array_vqvf>(context.track_list));
		}

		// Returns true if no track needs its scale: we can remove the scale of each track
		inline bool pre_process_can_remove_scale(pre_process_context_t& context, const track_array_vqvf& track_list)
		{
			const pre_process_settings_t& settings = context.settings;
			const bool is_lossy = settings.precision_policy == pre_process_precision_policy::lossy;

			// The scale of a VQ track is the identity of the additive format
			const rtm::vector4f identity_scale = rtm::vector_set(get_additive_default_scale(settings.additive_format));

			const uint32_t num_tracks = track_list.get_num_tracks();
			const uint32_t num_samples_per_track = track_list.get_num_samples_per_track();

			for (uint32_t track_index = 0; track_index < num_tracks; ++track_index)
			{
				const track_vqvf& track_ = track_list[track_index];

				bool is_identity = true;
				for (uint32_t sample_index = 0; sample_index < num_samples_per_track; ++sample_index)
				{
					if (!rtm::vector_all_near_equal3(track_[sample_index].scale, identity_scale, 0.0F))
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

		inline void pre_process_convert_transform_type(pre_process_context_t& context)
		{
			const pre_process_settings_t& settings = context.settings;
			track_array& track_list = context.track_list;

			const track_type8 source_type = track_list.get_track_type();
			const track_type8 target_type = settings.transform_type;
			if (source_type == target_type)
				return;	// Nothing to do

			iallocator& allocator = context.allocator;
			const uint32_t num_tracks = track_list.get_num_tracks();
			const uint32_t num_samples = track_list.get_num_samples_per_track();
			const float sample_rate = track_list.get_sample_rate();

			if (source_type == track_type8::vqf && target_type == track_type8::vqvf)
			{
				// Promotion: each new scale is the identity of the additive format
				const rtm::vector4f identity_scale = rtm::vector_set(get_additive_default_scale(settings.additive_format));
				const track_array_vqf& vq_tracks = track_array_cast<track_array_vqf>(track_list);

				track_array_vqvf vqv_tracks(allocator, num_tracks);
				vqv_tracks.set_name(vq_tracks.get_name());
				vqv_tracks.set_looping_policy(vq_tracks.get_looping_policy());

				for (uint32_t track_index = 0; track_index < num_tracks; ++track_index)
				{
					const track_vqf& vq_track = vq_tracks[track_index];

					track_desc_transformf desc = vq_track.get_description();
					desc.default_value.scale = identity_scale;

					track_vqvf vqv_track = track_vqvf::make_reserve(desc, allocator, num_samples, sample_rate);
					vqv_track.set_name(vq_track.get_name());

					for (uint32_t sample_index = 0; sample_index < num_samples; ++sample_index)
						vqv_track[sample_index] = rtm::vqv_set(vq_track[sample_index].rotation, vq_track[sample_index].translation, identity_scale);

					vqv_tracks[track_index] = std::move(vqv_track);
				}

				track_list = std::move(vqv_tracks);
			}
			else if (source_type == track_type8::vqvf && target_type == track_type8::vqf)
			{
				// Demotion: only when no track needs its scale
				const track_array_vqvf& vqv_tracks = track_array_cast<track_array_vqvf>(track_list);
				if (!pre_process_can_remove_scale(context, vqv_tracks))
					return;

				track_array_vqf vq_tracks(allocator, num_tracks);
				vq_tracks.set_name(vqv_tracks.get_name());
				vq_tracks.set_looping_policy(vqv_tracks.get_looping_policy());

				for (uint32_t track_index = 0; track_index < num_tracks; ++track_index)
				{
					const track_vqvf& vqv_track = vqv_tracks[track_index];

					// A VQ track has no scale, ACL ignores the scale of its default value
					track_desc_transformf desc = vqv_track.get_description();
					desc.default_value.scale = rtm::vector_set(1.0F);

					track_vqf vq_track = track_vqf::make_reserve(desc, allocator, num_samples, sample_rate);
					vq_track.set_name(vqv_track.get_name());

					for (uint32_t sample_index = 0; sample_index < num_samples; ++sample_index)
						vq_track[sample_index] = rtm::vq_set(vqv_track[sample_index].rotation, vqv_track[sample_index].translation);

					vq_tracks[track_index] = std::move(vq_track);
				}

				track_list = std::move(vq_tracks);
			}
		}
	}

	ACL_IMPL_VERSION_NAMESPACE_END
}

ACL_IMPL_FILE_PRAGMA_POP
