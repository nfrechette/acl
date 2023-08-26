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

// Included only once from pre_process.h

#include "acl/version.h"
#include "acl/core/track_traits.h"
#include "acl/core/floating_point_exceptions.h"
#include "acl/compression/impl/transform_clip_adapters.h"

#include <rtm/quatf.h>
#include <rtm/scalarf.h>
#include <rtm/vector4f.h>

ACL_IMPL_FILE_PRAGMA_PUSH

namespace acl
{
	ACL_IMPL_VERSION_NAMESPACE_BEGIN

	namespace acl_impl
	{
		struct pre_process_context_t
		{
			iallocator& allocator;

			const pre_process_settings_t& settings;
			track_array& track_list;

		private:
			rigid_shell_metadata_t* shell_metadata = nullptr;
			uint32_t* sorted_transforms_parent_first = nullptr;

			//////////////////////////////////////////////////////////////////////////

		public:
			explicit pre_process_context_t(
				iallocator& allocator_,
				const pre_process_settings_t& settings_,
				track_array& track_list_)
				: allocator(allocator_)
				, settings(settings_)
				, track_list(track_list_)
			{
			}

			~pre_process_context_t()
			{
				deallocate_type_array(allocator, shell_metadata, track_list.get_num_tracks());
				deallocate_type_array(allocator, sorted_transforms_parent_first, track_list.get_num_tracks());
			}

			const rigid_shell_metadata_t* get_shell_metadata()
			{
				if (shell_metadata == nullptr)
				{
					transform_track_array_adapter_t clip_adapter(
						&track_array_cast<track_array_qvvf>(track_list),
						settings.additive_format);
					clip_adapter.sorted_transforms_parent_first = get_sorted_transforms_parent_first();

					shell_metadata = compute_clip_shell_distances(
						allocator,
						clip_adapter,
						transform_track_array_adapter_t(settings.additive_base, additive_clip_format8::none));
				}

				return shell_metadata;
			}

			const uint32_t* get_sorted_transforms_parent_first()
			{
				if (sorted_transforms_parent_first == nullptr)
				{
					const uint32_t num_transforms = track_list.get_num_tracks();

					sorted_transforms_parent_first = allocate_type_array<uint32_t>(allocator, num_transforms);

					for (uint32_t transform_index = 0; transform_index < num_transforms; ++transform_index)
						sorted_transforms_parent_first[transform_index] = transform_index;

					sort_transform_indices_parent_first(
						transform_track_array_adapter_t(track_array_cast<track_array_qvvf>(track_list)),
						sorted_transforms_parent_first,
						num_transforms);
				}

				return sorted_transforms_parent_first;
			}
		};

		inline void pre_process_normalize_rotations(pre_process_context_t& context)
		{
			if (context.settings.precision_policy == pre_process_precision_policy::lossless)
				return;	// This pre-process action is lossy, do nothing

			track_array_qvvf& track_list = track_array_cast<track_array_qvvf>(context.track_list);
			const uint32_t num_samples_per_track = track_list.get_num_samples_per_track();

			for (track_qvvf& track_ : track_list)
			{
				for (uint32_t sample_index = 0; sample_index < num_samples_per_track; ++sample_index)
				{
					rtm::qvvf& sample = track_[sample_index];

					if (!rtm::quat_is_normalized(sample.rotation))
						sample.rotation = rtm::quat_normalize(sample.rotation);
				}
			}
		}

		inline void pre_process_ensure_quat_w_positive(pre_process_context_t& context)
		{
			track_array_qvvf& track_list = track_array_cast<track_array_qvvf>(context.track_list);
			const uint32_t num_samples_per_track = track_list.get_num_samples_per_track();

			for (track_qvvf& track_ : track_list)
			{
				for (uint32_t sample_index = 0; sample_index < num_samples_per_track; ++sample_index)
				{
					rtm::qvvf& sample = track_[sample_index];
					sample.rotation = rtm::quat_ensure_positive_w(sample.rotation);
				}
			}
		}

		template<track_type8 k_track_type>
		inline void pre_process_optimize_looping_scalar(track_array& track_list)
		{
			using track_trait_t = track_traits<k_track_type>;
			using track_array_type_t = track_array_typed<k_track_type>;
			using track_type_t = track_typed<k_track_type>;

			track_array_type_t& track_list_ = track_array_cast<track_array_type_t>(track_list);
			const uint32_t last_sample_index = track_list_.get_num_samples_per_track() - 1;

			bool is_looping = true;
			for (track_type_t& track_ : track_list_)
			{
				const track_desc_scalarf& desc = track_.get_description();

				const rtm::vector4f first_sample = track_trait_t::load_as_vector(&track_[0]);
				const rtm::vector4f last_sample = track_trait_t::load_as_vector(&track_[last_sample_index]);
				if (!rtm::vector_all_near_equal(first_sample, last_sample, desc.precision))
				{
					is_looping = false;
					break;
				}
			}

			if (is_looping)
			{
				for (track_type_t& track_ : track_list_)
					track_[last_sample_index] = track_[0];
			}
		}

		inline void pre_process_optimize_looping_transform(pre_process_context_t& context)
		{
			track_array_qvvf& track_list = track_array_cast<track_array_qvvf>(context.track_list);

			transform_track_array_adapter_t clip_adapter(&track_list, context.settings.additive_format);
			clip_adapter.rigid_shell_metadata = context.get_shell_metadata();

			const bool is_looping = is_clip_looping(
				clip_adapter,
				transform_track_array_adapter_t(context.settings.additive_base, additive_clip_format8::none),
				*context.settings.error_metric);

			if (is_looping)
			{
				// We just replace the last sample with a copy of the first to ensure that
				// we have perfect looping. We do not change the wrapping mode of the track list.
				const uint32_t last_sample_index = track_list.get_num_samples_per_track() - 1;

				for (track_qvvf& track_ : track_list)
					track_[last_sample_index] = track_[0];
			}
		}

		inline void pre_process_optimize_looping(pre_process_context_t& context)
		{
			if (context.settings.precision_policy == pre_process_precision_policy::lossless)
				return;	// This pre-process action is lossy, do nothing

			track_array& track_list = context.track_list;

			if (track_list.get_looping_policy() == sample_looping_policy::wrap)
				return;	// This track list has already been optimized, the wrapping keyframe has been stripped

			if (track_list.get_num_samples_per_track() <= 1)
				return;	// We need at least two keyframes to detect if we loop or not

			const track_category8 track_category = track_list.get_track_category();
			const track_type8 track_type = track_list.get_track_type();

			if (track_category == track_category8::scalarf)
			{
				// Scalar tracks are looping if the first and last keyframes are within the precision threshold

				switch (track_type)
				{
				case track_type8::float1f:
					pre_process_optimize_looping_scalar<track_type8::float1f>(track_list);
					break;
				case track_type8::float2f:
					pre_process_optimize_looping_scalar<track_type8::float2f>(track_list);
					break;
				case track_type8::float3f:
					pre_process_optimize_looping_scalar<track_type8::float3f>(track_list);
					break;
				case track_type8::float4f:
					pre_process_optimize_looping_scalar<track_type8::float4f>(track_list);
					break;
				case track_type8::vector4f:
					pre_process_optimize_looping_scalar<track_type8::vector4f>(track_list);
					break;
				default:
					ACL_ASSERT(false, "Unexpected track type");
					break;
				}
			}
			else if (track_category == track_category8::transformf)
			{
				ACL_ASSERT(track_type == track_type8::qvvf, "Expected qvvf");

				// Transform tracks are looping if replacing the last keyframe with the first satisfies
				// the error metric.

				pre_process_optimize_looping_transform(context);
			}
		}

		template<track_type8 k_track_type>
		inline void pre_process_sanitize_constant_tracks_scalar(track& track_)
		{
			using track_trait_t = track_traits<k_track_type>;
			using sample_type_t = typename track_trait_t::sample_type;
			using track_type_t = track_typed<k_track_type>;

			const uint32_t num_samples_per_track = track_.get_num_samples();
			track_type_t& track__ = track_cast<track_type_t>(track_);

			const track_desc_scalarf& desc = track_.get_description<track_desc_scalarf>();
			const float precision = desc.precision;

			rtm::vector4f min = track_trait_t::load_as_vector(&track__[0]);
			rtm::vector4f max = track_trait_t::load_as_vector(&track__[num_samples_per_track - 1]);

			for (uint32_t sample_index = 1; sample_index < num_samples_per_track; ++sample_index)
			{
				const rtm::vector4f sample = track_trait_t::load_as_vector(&track__[sample_index]);

				min = rtm::vector_min(min, sample);
				max = rtm::vector_max(max, sample);
			}

			const rtm::vector4f extent = rtm::vector_sub(max, min);
			if (rtm::vector_all_less_equal(extent, rtm::vector_set(precision)))
			{
				const sample_type_t constant_sample = track__[0];

				for (uint32_t sample_index = 1; sample_index < num_samples_per_track; ++sample_index)
					track__[sample_index] = constant_sample;
			}
		}

		inline uint32_t pre_process_get_uniform_sample_key(const track_array_qvvf& track_list, float sample_time)
		{
			return get_uniform_sample_key(
				transform_track_array_adapter_t(track_list),
				transform_segment_adapter_t(),
				sample_time);
		}

		inline bool RTM_SIMD_CALL pre_process_are_samples_constant(
			pre_process_context_t& context,
			const track_array_qvvf& track_list,
			uint32_t track_index,
			rtm::vector4f_arg0 reference,
			animation_track_type8 sub_track_type)
		{
			const pre_process_settings_t& settings = context.settings;
			const track_qvvf& track_ = track_list[track_index];

			const rigid_shell_metadata_t& shell = context.get_shell_metadata()[track_index];

			const itransform_error_metric& error_metric = *settings.error_metric;

			const bool has_scale = true;	// Assume we have scale
			const bool has_additive_base = settings.additive_format != additive_clip_format8::none;
			const bool needs_conversion = error_metric.needs_conversion(has_scale);

			const uint32_t dirty_transform_indices[2] = { 0, 1 };
			rtm::qvvf local_transforms[2];
			rtm::qvvf base_transforms[2];
			alignas(16) uint8_t local_transforms_converted[1024];	// Big enough for 2 transforms for sure
			alignas(16) uint8_t base_transforms_converted[1024];	// Big enough for 2 transforms for sure

			const size_t metric_transform_size = error_metric.get_transform_size(has_scale);
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

			qvvf_transform_error_metric::calculate_error_args calculate_error_args;
			calculate_error_args.construct_sphere_shell(shell.local_shell_distance);
			calculate_error_args.transform0 = &local_transforms_converted[metric_transform_size * 0];
			calculate_error_args.transform1 = &local_transforms_converted[metric_transform_size * 1];

			const rtm::scalarf precision = rtm::scalar_set(shell.precision);

			const uint32_t num_samples = track_list.get_num_samples_per_track();
			const float sample_rate = track_list.get_sample_rate();
			const float duration = track_list.get_finite_duration();
			const uint32_t num_base_samples = has_additive_base ? settings.additive_base->get_num_samples_per_track() : 0;
			const float base_duration = has_additive_base ? settings.additive_base->get_finite_duration() : 0.0F;

			for (uint32_t sample_index = 0; sample_index < num_samples; ++sample_index)
			{
				const rtm::quatf raw_rotation = track_[sample_index].rotation;
				const rtm::vector4f raw_translation = track_[sample_index].translation;
				const rtm::vector4f raw_scale = track_[sample_index].scale;

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
					// The sample time is calculated from the full clip duration to be consistent with decompression
					const float sample_time = rtm::scalar_min(float(sample_index) / sample_rate, duration);

					const float normalized_sample_time = num_base_samples > 1 ? (sample_time / duration) : 0.0F;
					const float additive_sample_time = num_base_samples > 1 ? (normalized_sample_time * base_duration) : 0.0F;

					// With uniform sample distributions, we do not interpolate.
					const uint32_t base_sample_index = pre_process_get_uniform_sample_key(*settings.additive_base, additive_sample_time);

					const track_qvvf& base_track = (*settings.additive_base)[track_index];
					const rtm::quatf base_rotation = base_track[base_sample_index].rotation;
					const rtm::vector4f base_translation = base_track[base_sample_index].translation;
					const rtm::vector4f base_scale = base_track[base_sample_index].scale;

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

				const rtm::scalarf vtx_error = error_metric.calculate_error(calculate_error_args);

				// If our error exceeds the desired precision, we are not constant
				if (rtm::scalar_greater_than(vtx_error, precision))
					return false;
			}

			// All samples were tested against the reference value and the error remained within tolerance
			return true;
		}

		inline void pre_process_sanitize_constant_tracks_transform(pre_process_context_t& context)
		{
			track_array_qvvf& track_list = track_array_cast<track_array_qvvf>(context.track_list);
			const uint32_t num_tracks = track_list.get_num_tracks();
			const uint32_t num_samples_per_track = track_list.get_num_samples_per_track();

			// Iterate in any order, doesn't matter
			for (uint32_t track_index = 0; track_index < num_tracks; ++track_index)
			{
				track_qvvf& track_ = track_list[track_index];

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

				const rtm::vector4f reference_scale = track_[0].scale;
				if (pre_process_are_samples_constant(context, track_list, track_index, reference_scale, animation_track_type8::scale))
				{
					for (uint32_t sample_index = 1; sample_index < num_samples_per_track; ++sample_index)
						track_[sample_index].scale = reference_scale;
				}
			}
		}

		inline void pre_process_sanitize_default_tracks_transform(pre_process_context_t& context)
		{
			track_array_qvvf& track_list = track_array_cast<track_array_qvvf>(context.track_list);
			const uint32_t num_tracks = track_list.get_num_tracks();
			const uint32_t num_samples_per_track = track_list.get_num_samples_per_track();

			// Iterate in any order, doesn't matter
			for (uint32_t track_index = 0; track_index < num_tracks; ++track_index)
			{
				track_qvvf& track_ = track_list[track_index];
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

				const rtm::vector4f reference_scale = desc.default_value.scale;
				if (pre_process_are_samples_constant(context, track_list, track_index, reference_scale, animation_track_type8::scale))
				{
					for (uint32_t sample_index = 0; sample_index < num_samples_per_track; ++sample_index)
						track_[sample_index].scale = reference_scale;
				}
			}
		}

		// Compacts constant sub-tracks
		// A sub-track is constant if every sample can be replaced by a single unique sample without exceeding
		// our error threshold.
		// Constant sub-tracks will retain the first sample.
		// A constant sub-track is a default sub-track if its unique sample can be replaced by the default value
		// without exceeding our error threshold.
		inline void pre_process_sanitize_constant_tracks(pre_process_context_t& context)
		{
			const pre_process_settings_t& settings = context.settings;

			if (settings.precision_policy == pre_process_precision_policy::lossless)
				return;	// This pre-process action is lossy, do nothing

			track_array& track_list = context.track_list;
			const track_category8 track_category = track_list.get_track_category();
			const track_type8 track_type = track_list.get_track_type();

			if (track_category == track_category8::scalarf)
			{
				// Scalar tracks are constant if their range extent is less than the precision threshold

				for (track& track_ : track_list)
				{
					switch (track_type)
					{
					case track_type8::float1f:
						pre_process_sanitize_constant_tracks_scalar<track_type8::float1f>(track_);
						break;
					case track_type8::float2f:
						pre_process_sanitize_constant_tracks_scalar<track_type8::float2f>(track_);
						break;
					case track_type8::float3f:
						pre_process_sanitize_constant_tracks_scalar<track_type8::float3f>(track_);
						break;
					case track_type8::float4f:
						pre_process_sanitize_constant_tracks_scalar<track_type8::float4f>(track_);
						break;
					case track_type8::vector4f:
						pre_process_sanitize_constant_tracks_scalar<track_type8::vector4f>(track_);
						break;
					default:
						ACL_ASSERT(false, "Unexpected track type");
						break;
					}
				}
			}
			else if (track_category == track_category8::transformf)
			{
				ACL_ASSERT(track_type == track_type8::qvvf, "Expected qvvf");

				// Transform tracks are constant when the error metric is satisfied when the first
				// sample is repeated

				pre_process_sanitize_constant_tracks_transform(context);
			}
		}

		// See pre_process_sanitize_constant_tracks(..) above
		inline void pre_process_sanitize_default_tracks(pre_process_context_t& context)
		{
			if (context.settings.precision_policy == pre_process_precision_policy::lossless)
				return;	// This pre-process action is lossy, do nothing

			track_array& track_list = context.track_list;
			const track_category8 track_category = track_list.get_track_category();

			if (track_category == track_category8::scalarf)
			{
				ACL_ASSERT(false, "Not implemented");
			}
			else if (track_category == track_category8::transformf)
			{
				ACL_ASSERT(track_list.get_track_type() == track_type8::qvvf, "Expected qvvf");

				// Transform tracks are default when the error metric is satisfied when the default
				// sample is repeated

				pre_process_sanitize_default_tracks_transform(context);
			}
		}
	}

	inline error_result pre_process_track_list(
		iallocator& allocator,
		const pre_process_settings_t& settings,
		track_array& track_list)
	{
		using namespace acl_impl;

		if (track_list.is_empty())
			return error_result();	// No tracks, nothing to do

		if (track_list.get_num_samples_per_track() == 0)
			return error_result();	// No samples, nothing to do

		if (settings.additive_format != additive_clip_format8::none)
		{
			if (track_list.get_track_type() != track_type8::qvvf)
				return error_result("'additive_format' is only supported with transform tracks");

			if (settings.additive_base == nullptr)
				return error_result("Missing 'additive_base' when 'additive_format' is used");
		}

		// Disable floating point exceptions during compression because we leverage all SIMD lanes
		// and we might intentionally divide by zero, etc.
		scope_disable_fp_exceptions fp_off;

		pre_process_context_t context(allocator, settings, track_list);

		const track_type8 track_type = track_list.get_track_type();

		if (are_any_enum_flags_set(settings.actions, pre_process_actions::normalize_rotations))
		{
			if (track_type == track_type8::qvvf)
				pre_process_normalize_rotations(context);
		}

		if (are_any_enum_flags_set(settings.actions, pre_process_actions::ensure_quat_w_positive))
		{
			if (track_type == track_type8::qvvf)
				pre_process_ensure_quat_w_positive(context);
		}

		if (are_any_enum_flags_set(settings.actions, pre_process_actions::optimize_looping))
		{
			if (track_type == track_type8::qvvf &&
				settings.precision_policy == pre_process_precision_policy::lossy &&
				settings.error_metric == nullptr)
				return error_result("'error_metric' is required when optimizing looping transform tracks");

			pre_process_optimize_looping(context);
		}

		if (are_any_enum_flags_set(settings.actions, pre_process_actions::sanitize_constant_tracks))
		{
			if (track_type == track_type8::qvvf &&
				settings.precision_policy == pre_process_precision_policy::lossy &&
				settings.error_metric == nullptr)
				return error_result("'error_metric' is required when sanitizing lossy constant transform tracks");

			pre_process_sanitize_constant_tracks(context);
		}

		if (are_any_enum_flags_set(settings.actions, pre_process_actions::sanitize_default_tracks))
		{
			if (track_type == track_type8::qvvf &&
				settings.precision_policy == pre_process_precision_policy::lossy &&
				settings.error_metric == nullptr)
				return error_result("'error_metric' is required when sanitizing lossy default transform tracks");

			if (track_type == track_type8::qvvf)
				pre_process_sanitize_default_tracks(context);
		}

		// We are done!
		return error_result();
	}

	ACL_IMPL_VERSION_NAMESPACE_END
}

ACL_IMPL_FILE_PRAGMA_POP
