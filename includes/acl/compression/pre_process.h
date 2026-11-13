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
#include "acl/core/additive_utils.h"
#include "acl/core/enum_utils.h"
#include "acl/core/error_result.h"
#include "acl/core/impl/compiler_utils.h"
#include "acl/compression/transform_adapters.h"
#include "acl/compression/transform_error_metrics.h"
#include "acl/compression/transform_metrics.h"
#include "acl/compression/track_array.h"

#include <cstdint>

ACL_IMPL_FILE_PRAGMA_PUSH

// This file reads the deprecated error metric of the settings
ACL_IMPL_DEPRECATION_WARNING_PUSH

namespace acl
{
	ACL_IMPL_VERSION_NAMESPACE_BEGIN

	////////////////////////////////////////////////////////////////////////////////
	// pre_process_action represents various actions that can be executed when
	// pre-processing raw data. Multiple actions can be combined with binary operators.
	// When multiple actions are used, they execute in the order defined here.
	enum class pre_process_actions
	{
		// No action to perform
		none = 0x00,

		// Ensures every rotation quaternion is normalized.
		// A quaternion will be normalized only if it isn't already normalized to preserve
		// its original value where possible.
		// Transform tracks only.
		// This is lossy.
		normalize_rotations = 0x01,

		// Ensures every rotation quaternion has a positive W component.
		// This forces all quaternions into the same side of the hyper-sphere to
		// facilitate interpolation by avoiding wrapping around.
		// Because both halves of the hyper-sphere represent equivalent rotations,
		// this operation is considered lossless.
		// Transform tracks only.
		// This is lossless.
		ensure_quat_w_positive = 0x02,

		// Ensures that if the first and last keyframes match well enough to satisfy the
		// precision requirements, the last keyframe is overwritten with the first keyframe
		// to make sure they match perfectly.
		// This is lossy.
		optimize_looping = 0x04,

		// Sanitizes every constant track (and sub-track) by duplicating the optimal sample.
		// This is lossy.
		sanitize_constant_tracks = 0x08,

		// Sanitizes every default track (and sub-track) by duplicating the default sample.
		// This is lossy.
		sanitize_default_tracks = 0x10,

		// Converts the transform tracks into the transform type of 'pre_process_settings_t::transform_type'.
		// The types hold more and more scale information: vqf (no scale), vqsf (uniform scale), vqvf (3D scale).
		// A promotion (for example vqf -> vqsf, or vqsf -> vqvf) always succeeds and is lossless. When the source
		// has no scale, each new scale is the identity of the additive format (1.0, or 0.0 with additive1).
		// A demotion to vqf removes the scale. It happens only when no track needs its scale:
		//    - With the lossless policy, each scale sample must be exactly the identity.
		//    - With the lossy policy, the error must stay within the precision when each scale becomes the identity.
		// A demotion from vqvf to vqsf makes each scale uniform. It happens only when no track needs a non-uniform scale:
		//    - With the lossless policy, the [xyz] components of each scale sample must be exactly equal.
		//    - With the lossy policy, the error must stay within the precision when each scale becomes the mean of [xyz].
		// When a track needs its scale, the tracks keep their type.
		// The track list must be a 'track_array' instance: a typed track array (for example 'track_array_vqvf')
		// cannot change its type, thus pre-processing returns an error.
		// Transform tracks only.
		// This is lossy with the lossy policy.
		convert_transform_type = 0x20,

		// Performs all recommended pre-process actions
		recommended = normalize_rotations | optimize_looping | sanitize_constant_tracks | sanitize_default_tracks,
	};

	ACL_IMPL_ENUM_FLAGS_OPERATORS(pre_process_actions)

	////////////////////////////////////////////////////////////////////////////////
	// pre_process_precision_policy controls whether we should be lossless or lossy
	enum class pre_process_precision_policy
	{
		// Pre-processing will use binary equality and binary equivalent transformations where possible
		lossless,

		// Pre-processing will use the error metric and precision values provided
		lossy,
	};

	////////////////////////////////////////////////////////////////////////////////
	// Various settings to control pre-processing
	struct pre_process_settings_t
	{
		// The list of actions to perform
		pre_process_actions actions = pre_process_actions::recommended;

		// The precision policy to follow
		pre_process_precision_policy precision_policy = pre_process_precision_policy::lossless;

		// The runtime transform type, the custom transform adapter, and the transform metric
		// to use with transform tracks when lossy
		// They have the same meaning and the same defaults as in the compression settings, see 'compression_settings'
		// Use the same values as for compression
		// Lossy pre-processing only
		// Transform tracks only
		transform_runtime_type_t transform_runtime_type = transform_runtime_type_t::automatic;
		itransform_adapter* custom_transform_adapter = nullptr;
		itransform_metric* transform_metric = nullptr;

		// DEPRECATED: use 'transform_runtime_type', 'custom_transform_adapter', and 'transform_metric' instead.
		// This value will be removed in v2.3.
		// The optional legacy error metric to use with transform tracks when lossy
		// When it is not 'null', you cannot set 'transform_runtime_type', 'custom_transform_adapter', or 'transform_metric'
		// When all are 'null', pre-processing uses the same default as compression
		// Lossy pre-processing only
		// Transform tracks only
		ACL_DEPRECATED("Use transform_runtime_type, custom_transform_adapter, and transform_metric instead, to be removed in v2.3")
		const itransform_error_metric* error_metric = nullptr;

		// The transform type to convert the transform tracks into (vqvf, vqf, or vqsf)
		// 'convert_transform_type' action only
		// Transform tracks only
		track_type8 transform_type = track_type8::vqvf;

		// The optional additive track list to use with transform tracks (vqvf, vqf, or vqsf)
		// It can have a different type than the track list to pre-process
		// Lossy pre-processing only
		// Transform tracks only
		const track_array* additive_base = nullptr;
		additive_clip_format8 additive_format = additive_clip_format8::none;
	};

	//////////////////////////////////////////////////////////////////////////
	// Pre-processes a track array by performing various operations to improve
	// compression.
	//
	// ACL avoids modifying the raw data during compression to ensure that the caller
	// can still measure the same error that the internal algorithm sees. However,
	// this can lead to sub-optimal compression in certain cases. Rotation format
	// conversion and constant/default sub-track collapsing can yield values that
	// the optimizing algorithm can never fully reach, leading it to try too hard
	// for little to no gain. To avoid this issue, the raw data can be pre-processed.
	// This allows the caller to control everything. Raw data can be pre-processed
	// once following import (e.g. from FBX).
	//
	//    allocator:				The allocator instance to use for intermediary allocations
	//    settings:					The settings to use during pre-processing.
	//    track_list:				The track list to pre-process.
	//
	// This function returns an error result. If an error occurred, the raw data
	// may have mutated partially.
	//////////////////////////////////////////////////////////////////////////
	error_result pre_process_track_list(iallocator& allocator, const pre_process_settings_t& settings, track_array& track_list);

	//////////////////////////////////////////////////////////////////////////
	// Pre-processes a typed transform track array, see above.
	// A typed track array cannot change its type: the 'convert_transform_type' action returns an error.
	//////////////////////////////////////////////////////////////////////////
	error_result pre_process_track_list(iallocator& allocator, const pre_process_settings_t& settings, track_array_vqvf& track_list);
	error_result pre_process_track_list(iallocator& allocator, const pre_process_settings_t& settings, track_array_vqf& track_list);
	error_result pre_process_track_list(iallocator& allocator, const pre_process_settings_t& settings, track_array_vqsf& track_list);

	ACL_IMPL_VERSION_NAMESPACE_END
}

#include "acl/compression/impl/pre_process.impl.h"

ACL_IMPL_DEPRECATION_WARNING_POP

ACL_IMPL_FILE_PRAGMA_POP
