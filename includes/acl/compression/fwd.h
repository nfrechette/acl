#pragma once

////////////////////////////////////////////////////////////////////////////////
// The MIT License (MIT)
//
// Copyright (c) 2022 Nicholas Frechette & Animation Compression Library contributors
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
#include "acl/core/error.h"

// track_type8 needs to be fully defined to be able to specialize some templates
#include "acl/core/track_types.h"

#include <cstdint>

////////////////////////////////////////////////////////////////////////////////
// This header provides forward declarations for all public ACL types.
// Forward declaring symbols from a 3rd party library is a bad idea, use this
// header instead.
// See also: https://blog.libtorrent.org/2017/12/forward-declarations-and-abi/
////////////////////////////////////////////////////////////////////////////////

namespace acl
{
	ACL_IMPL_VERSION_NAMESPACE_BEGIN

	enum class compression_level8 : uint8_t;

	struct compression_database_settings;
	struct compression_metadata_settings;
	struct compression_settings;

	enum class pre_process_actions;
	enum class pre_process_precision_policy;
	struct pre_process_settings_t;

	enum class stat_logging;
	struct output_stats;

	struct track_error;

	class track_array;
	template<track_type8 track_type_, track_interpolator_t track_interpolator_> class track_array_typed;

	using track_array_float1f	= track_array_typed<track_type8::float1f, track_interpolator_t::linear>;
	using track_array_float2f	= track_array_typed<track_type8::float2f, track_interpolator_t::linear>;
	using track_array_float3f	= track_array_typed<track_type8::float3f, track_interpolator_t::linear>;
	using track_array_float4f	= track_array_typed<track_type8::float4f, track_interpolator_t::linear>;
	using track_array_vector4f	= track_array_typed<track_type8::vector4f, track_interpolator_t::linear>;
	using track_array_vqvf		= track_array_typed<track_type8::vqvf, track_interpolator_t::linear>;
	using track_array_vqf		= track_array_typed<track_type8::vqf, track_interpolator_t::linear>;
	using track_array_vqsf		= track_array_typed<track_type8::vqsf, track_interpolator_t::linear>;

	using track_array_qvvf ACL_DEPRECATED("Use 'track_array_vqvf' instead, to be removed in v2.3") = track_array_vqvf;

	class track;
	template<track_type8 track_type_, track_interpolator_t track_interpolator_> class track_typed;

	using track_float1f			= track_typed<track_type8::float1f, track_interpolator_t::linear>;
	using track_float2f			= track_typed<track_type8::float2f, track_interpolator_t::linear>;
	using track_float3f			= track_typed<track_type8::float3f, track_interpolator_t::linear>;
	using track_float4f			= track_typed<track_type8::float4f, track_interpolator_t::linear>;
	using track_vector4f		= track_typed<track_type8::vector4f, track_interpolator_t::linear>;
	using track_vqvf			= track_typed<track_type8::vqvf, track_interpolator_t::linear>;
	using track_vqf				= track_typed<track_type8::vqf, track_interpolator_t::linear>;
	using track_vqsf			= track_typed<track_type8::vqsf, track_interpolator_t::linear>;

	using track_qvvf ACL_DEPRECATED("Use 'track_vqvf' instead, to be removed in v2.3") = track_vqvf;

	enum class additive_clip_format8 : uint8_t;

	enum class transform_runtime_type_t : uint8_t;

	class itransform_adapter;
	class vqf_transform_adapter;
	class vqsf_transform_adapter;
	class vqvf_transform_adapter;
	class vqmf_transform_adapter;
	class matrix3x4f_transform_adapter;

	class itransform_metric;
	class shell_transform_metric;
	class precise_transform_metric;

	class itransform_error_metric;
	class vqvf_transform_error_metric;
	class vqvf_precise_transform_error_metric;
	class vqvf_matrix3x4f_transform_error_metric;
	template<additive_clip_format8 additive_format> class additive_vqvf_transform_error_metric;

	using qvvf_transform_error_metric ACL_DEPRECATED("Use 'vqvf_transform_error_metric' instead, to be removed in v2.3") = vqvf_transform_error_metric;
	using qvvf_precise_transform_error_metric ACL_DEPRECATED("Use 'vqvf_precise_transform_error_metric' instead, to be removed in v2.3") = vqvf_precise_transform_error_metric;
	using qvvf_matrix3x4f_transform_error_metric ACL_DEPRECATED("Use 'vqvf_matrix3x4f_transform_error_metric' instead, to be removed in v2.3") = vqvf_matrix3x4f_transform_error_metric;
	template<additive_clip_format8 additive_format>
	using additive_qvvf_transform_error_metric ACL_DEPRECATED("Use 'additive_vqvf_transform_error_metric' instead, to be removed in v2.3") = additive_vqvf_transform_error_metric<additive_format>;

	ACL_IMPL_VERSION_NAMESPACE_END
}
