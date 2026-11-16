#pragma once

////////////////////////////////////////////////////////////////////////////////
// The MIT License (MIT)
//
// Copyright (c) 2020 Nicholas Frechette & Animation Compression Library contributors
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

// Included only once from compress.h

#include "acl/version.h"
#include "acl/core/buffer_tag.h"
#include "acl/core/compressed_tracks.h"
#include "acl/core/error.h"
#include "acl/core/error_result.h"
#include "acl/core/floating_point_control.h"
#include "acl/core/iallocator.h"
#include "acl/compression/compression_memory_desc.h"
#include "acl/compression/compression_settings.h"
#include "acl/compression/output_stats.h"
#include "acl/compression/track_array.h"
#include "acl/compression/transform_base.h"
#include "acl/compression/impl/transform_runtime_error_metric.h"

#include <cstddef>
#include <cstdint>

ACL_IMPL_FILE_PRAGMA_PUSH

namespace acl
{
	ACL_IMPL_VERSION_NAMESPACE_BEGIN

	inline error_result compress_track_list(iallocator& allocator, const track_array& track_list, const compression_settings& settings, compressed_tracks*& out_compressed_tracks, output_stats& out_stats)
	{
		using namespace acl_impl;

		error_result result = track_list.is_valid();
		if (result.any())
			return result;

		// Disable floating point exceptions during compression because we leverage all SIMD lanes
		// and we might intentionally divide by zero, etc.
		scope_disable_fp_exceptions fp_off;

		if (track_list.get_track_category() == track_category8::transformf)
			result = compress_transform_track_list(allocator, track_list, settings, transform_base_desc_t(), out_compressed_tracks, out_stats);
		else
			result = compress_scalar_track_list(allocator, track_list, settings, out_compressed_tracks, out_stats);

		return result;
	}

	inline error_result compress_track_list(iallocator& allocator, const track_array& track_list, const compression_settings& settings, const transform_base_desc_t& transform_base, compressed_tracks*& out_compressed_tracks, output_stats& out_stats)
	{
		using namespace acl_impl;

		error_result result = track_list.is_valid();
		if (result.any())
			return result;

		if (!track_list.is_empty() && !is_transform_track_type(track_list.get_track_type()))
			return error_result("Expected transform tracks");

		result = transform_base.is_valid(track_list.get_transform_space());
		if (result.any())
			return result;

		// Disable floating point exceptions during compression because we leverage all SIMD lanes
		// and we might intentionally divide by zero, etc.
		scope_disable_fp_exceptions fp_off;

		return compress_transform_track_list(allocator, track_list, settings, transform_base, out_compressed_tracks, out_stats);
	}

	inline error_result compress_track_list(iallocator& allocator, const track_array& track_list, const compression_settings& settings, const track_array& additive_base_track_list, additive_clip_format8 additive_format, compressed_tracks*& out_compressed_tracks, output_stats& out_stats)
	{
		using namespace acl_impl;

		error_result result = track_list.is_valid();
		if (result.any())
			return result;

		if (!track_list.is_empty() && !is_transform_track_type(track_list.get_track_type()))
			return error_result("Expected transform tracks");

		// This overload has no base adapter
		if (additive_format == additive_clip_format8::custom)
			return error_result("The custom base format requires a base adapter in 'transform_base_desc_t'");

		if (additive_format != additive_clip_format8::none)
		{
			result = additive_base_track_list.is_valid();
			if (result.any())
				return result;

			if (!additive_base_track_list.is_empty() && !is_transform_track_type(additive_base_track_list.get_track_type()))
				return error_result("Expected transform tracks in the additive base");
		}

		// This overload ignores an empty base: the clip then has no base
		transform_base_desc_t transform_base;
		if (additive_format != additive_clip_format8::none && !additive_base_track_list.is_empty())
		{
			transform_base.format = additive_format;
			transform_base.tracks = &additive_base_track_list;
		}

		result = transform_base.is_valid(track_list.get_transform_space());
		if (result.any())
			return result;

		// Disable floating point exceptions during compression because we leverage all SIMD lanes
		// and we might intentionally divide by zero, etc.
		scope_disable_fp_exceptions fp_off;

		return compress_transform_track_list(allocator, track_list, settings, transform_base, out_compressed_tracks, out_stats);
	}

	namespace acl_impl
	{
		// Fills the description of the memory estimate with the values of a track list and its base
		inline compression_memory_desc_t make_compression_memory_desc(const track_array& track_list, const transform_base_desc_t& transform_base)
		{
			compression_memory_desc_t desc;

			const bool is_transform = track_list.get_track_category() == track_category8::transformf;
			if (!track_list.is_empty())
				desc.track_type = track_list.get_track_type();
			else
				desc.track_type = is_transform ? track_type8::vqvf : track_type8::float1f;

			desc.num_tracks = track_list.get_num_tracks();
			desc.num_samples_per_track = track_list.get_num_samples_per_track();
			desc.looping_policy = track_list.get_looping_policy();

			uint32_t total_name_length = uint32_t(track_list.get_name().size());
			for (const track& track_ : track_list)
				total_name_length += uint32_t(track_.get_name().size());
			desc.total_name_length = total_name_length;

			if (!is_transform)
				return desc;

			desc.transform_space = track_list.get_transform_space();
			desc.max_hierarchy_depth = find_max_hierarchy_depth(track_list);

			const track_array* base_track_list = transform_base.tracks;
			if (transform_base.format != additive_clip_format8::none && base_track_list != nullptr && !base_track_list->is_empty())
			{
				desc.base_format = transform_base.format;
				desc.num_base_tracks = base_track_list->get_num_tracks();
				desc.num_base_samples_per_track = base_track_list->get_num_samples_per_track();

				// Only 'vqsf' tracks use the scale of the base to select their runtime transforms
				if (desc.track_type == track_type8::vqsf)
				{
					bool base_has_scale = false;
					bool base_has_non_uniform_scale = false;
					transform_runtime_error_metric::find_base_scale(*base_track_list, base_has_scale, base_has_non_uniform_scale);
					desc.base_has_non_uniform_scale = base_has_non_uniform_scale;
				}
			}

			return desc;
		}
	}

	inline size_t estimate_compression_peak_memory_usage(const track_array& track_list, const compression_settings& settings)
	{
		using namespace acl_impl;

		// Compression fails before it allocates its buffers
		if (track_list.is_valid().any())
			return 0;

		return estimate_compression_peak_memory_usage(make_compression_memory_desc(track_list, transform_base_desc_t()), settings);
	}

	inline size_t estimate_compression_peak_memory_usage(const track_array& track_list, const compression_settings& settings, const transform_base_desc_t& transform_base)
	{
		using namespace acl_impl;

		// Compression fails before it allocates its buffers
		if (track_list.is_valid().any())
			return 0;

		if (!track_list.is_empty() && !is_transform_track_type(track_list.get_track_type()))
			return 0;

		if (transform_base.is_valid(track_list.get_transform_space()).any())
			return 0;

		return estimate_compression_peak_memory_usage(make_compression_memory_desc(track_list, transform_base), settings);
	}

	inline size_t estimate_compression_peak_memory_usage(const compression_memory_desc_t& desc, const compression_settings& settings)
	{
		using namespace acl_impl;

		if (is_transform_track_type(desc.track_type))
			return estimate_transform_compression_peak_memory_usage(desc, settings);
		else
			return estimate_scalar_compression_peak_memory_usage(desc, settings);
	}

	ACL_IMPL_VERSION_NAMESPACE_END
}

ACL_IMPL_FILE_PRAGMA_POP
