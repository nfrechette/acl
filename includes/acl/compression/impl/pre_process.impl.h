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
#include "acl/core/floating_point_control.h"
#include "acl/compression/impl/pre_process.scalar.h"
#include "acl/compression/impl/transform_runtime.h"
#include "acl/compression/impl/transform_runtime_error_metric.h"
#include "acl/compression/impl/pre_process.transform.h"

#include <rtm/quatf.h>
#include <rtm/scalarf.h>
#include <rtm/vector4f.h>

ACL_IMPL_FILE_PRAGMA_PUSH

// This file reads the deprecated error metric of the settings
ACL_IMPL_DEPRECATION_WARNING_PUSH

namespace acl
{
	ACL_IMPL_VERSION_NAMESPACE_BEGIN

	namespace acl_impl
	{
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
				case track_type8::vqvf:
				default:
					ACL_ASSERT(false, "Unexpected track type");
					break;
				}
			}
			else if (track_category == track_category8::transformf)
			{
				ACL_ASSERT(is_transform_track_type(track_type), "Expected vqvf, vqf, or vqsf");

				// Transform tracks are looping if replacing the last keyframe with the first satisfies
				// the error metric.

				pre_process_optimize_looping_transform(context);
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
					case track_type8::vqvf:
					default:
						ACL_ASSERT(false, "Unexpected track type");
						break;
					}
				}
			}
			else if (track_category == track_category8::transformf)
			{
				ACL_ASSERT(is_transform_track_type(track_type), "Expected vqvf, vqf, or vqsf");

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

			const track_array& track_list = context.track_list;
			const track_category8 track_category = track_list.get_track_category();

			if (track_category == track_category8::scalarf)
			{
				ACL_ASSERT(false, "Not implemented");
			}
			else if (track_category == track_category8::transformf)
			{
				ACL_ASSERT(is_transform_track_type(track_list.get_track_type()), "Expected vqvf, vqf, or vqsf");

				// Transform tracks are default when the error metric is satisfied when the default
				// sample is repeated

				pre_process_sanitize_default_tracks_transform(context);
			}
		}
	}

	inline error_result pre_process_track_list(
		iallocator& allocator,
		const pre_process_settings_t& input_settings,
		track_array& track_list)
	{
		using namespace acl_impl;

		if (input_settings.error_metric != nullptr)
		{
			// The legacy error metric selects the runtime transform type and the error measurement
			if (input_settings.transform_runtime_type != transform_runtime_type_t::automatic || input_settings.custom_transform_adapter != nullptr || input_settings.transform_metric != nullptr)
				return error_result("Cannot use the legacy error metric with a transform runtime type, a custom transform adapter, or a transform metric");
		}
		else
		{
			const transform_runtime_selector selector;
			const error_result transform_runtime_result = selector.validate(input_settings.transform_runtime_type, input_settings.custom_transform_adapter, input_settings.transform_metric);
			if (transform_runtime_result.any())
				return transform_runtime_result;
		}

		// When the settings do not supply a legacy error metric, we use the runtime transform type
		// and the transform metric of the settings, as compression does
		// Pre-processing does not know whether the clip has scale: it uses the selection with scale
		transform_runtime_error_metric runtime_error_metric;
		pre_process_settings_t settings = input_settings;
		if (settings.error_metric == nullptr)
		{
			// An empty track array has no type, we use vqvf
			const track_type8 track_type = track_list.is_empty() ? track_type8::vqvf : track_list.get_track_type();
			const error_result runtime_error_metric_result = runtime_error_metric.initialize(
				settings.transform_runtime_type, settings.custom_transform_adapter, settings.transform_metric,
				track_type, settings.additive_format, settings.additive_base);
			if (runtime_error_metric_result.any())
				return runtime_error_metric_result;

			settings.error_metric = &runtime_error_metric;
		}

		if (track_list.is_empty())
			return error_result();	// No tracks, nothing to do

		if (track_list.get_num_samples_per_track() == 0)
			return error_result();	// No samples, nothing to do

		if (settings.additive_format != additive_clip_format8::none)
		{
			if (!is_transform_track_type(track_list.get_track_type()))
				return error_result("'additive_format' is only supported with transform tracks");

			if (settings.additive_base == nullptr)
				return error_result("Missing 'additive_base' when 'additive_format' is used");

			if (settings.additive_base->is_empty())
				return error_result("'additive_base' cannot be empty when 'additive_format' is used");

			if (!is_transform_track_type(settings.additive_base->get_track_type()))
				return error_result("'additive_base' must contain transform tracks");
		}

		const bool is_transform = is_transform_track_type(track_list.get_track_type());
		if (is_transform && are_any_enum_flags_set(settings.actions, pre_process_actions::convert_transform_type) && !is_transform_track_type(settings.transform_type))
			return error_result("'transform_type' must be a transform track type: vqvf, vqf, or vqsf");

		// Disable floating point exceptions during compression because we leverage all SIMD lanes
		// and we might intentionally divide by zero, etc.
		scope_disable_fp_exceptions fp_off;

		pre_process_context_t context(allocator, settings, track_list);

		const track_type8 track_type = track_list.get_track_type();

		if (are_any_enum_flags_set(settings.actions, pre_process_actions::normalize_rotations) && is_transform_track_type(track_type))
		{
			pre_process_normalize_rotations(context);
		}

		if (are_any_enum_flags_set(settings.actions, pre_process_actions::ensure_quat_w_positive) && is_transform_track_type(track_type))
		{
			pre_process_ensure_quat_w_positive(context);
		}

		if (are_any_enum_flags_set(settings.actions, pre_process_actions::optimize_looping))
		{
			pre_process_optimize_looping(context);
		}

		if (are_any_enum_flags_set(settings.actions, pre_process_actions::sanitize_constant_tracks))
		{
			pre_process_sanitize_constant_tracks(context);
		}

		if (are_any_enum_flags_set(settings.actions, pre_process_actions::sanitize_default_tracks) && is_transform_track_type(track_type))
		{
			pre_process_sanitize_default_tracks(context);
		}

		if (are_any_enum_flags_set(settings.actions, pre_process_actions::convert_transform_type) && is_transform_track_type(track_type))
		{
			pre_process_convert_transform_type(context);
		}

		// We are done!
		return error_result();
	}

	inline error_result pre_process_track_list(
		iallocator& allocator,
		const pre_process_settings_t& settings,
		track_array_vqvf& track_list)
	{
		// A typed track array cannot change its type
		if (are_any_enum_flags_set(settings.actions, pre_process_actions::convert_transform_type) && settings.transform_type != track_type8::vqvf)
			return error_result("'convert_transform_type' cannot change the type of a typed track array, use 'track_array'");

		return pre_process_track_list(allocator, settings, static_cast<track_array&>(track_list));
	}

	inline error_result pre_process_track_list(
		iallocator& allocator,
		const pre_process_settings_t& settings,
		track_array_vqf& track_list)
	{
		// A typed track array cannot change its type
		if (are_any_enum_flags_set(settings.actions, pre_process_actions::convert_transform_type) && settings.transform_type != track_type8::vqf)
			return error_result("'convert_transform_type' cannot change the type of a typed track array, use 'track_array'");

		return pre_process_track_list(allocator, settings, static_cast<track_array&>(track_list));
	}

	inline error_result pre_process_track_list(
		iallocator& allocator,
		const pre_process_settings_t& settings,
		track_array_vqsf& track_list)
	{
		// A typed track array cannot change its type
		if (are_any_enum_flags_set(settings.actions, pre_process_actions::convert_transform_type) && settings.transform_type != track_type8::vqsf)
			return error_result("'convert_transform_type' cannot change the type of a typed track array, use 'track_array'");

		return pre_process_track_list(allocator, settings, static_cast<track_array&>(track_list));
	}

	ACL_IMPL_VERSION_NAMESPACE_END
}

ACL_IMPL_DEPRECATION_WARNING_POP

ACL_IMPL_FILE_PRAGMA_POP
