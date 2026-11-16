#pragma once

////////////////////////////////////////////////////////////////////////////////
// The MIT License (MIT)
//
// Copyright (c) 2026 Nicholas Frechette & Animation Compression Library contributors
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
#include "acl/core/bitset.h"
#include "acl/core/error.h"
#include "acl/core/sample_looping_policy.h"
#include "acl/core/track_desc.h"
#include "acl/core/track_formats.h"
#include "acl/core/track_types.h"
#include "acl/core/impl/compiler_utils.h"
#include "acl/core/impl/compressed_headers.h"
#include "acl/compression/compression_memory_desc.h"
#include "acl/compression/compression_settings.h"
#include "acl/compression/track_array.h"
#include "acl/compression/transform_error_metrics.h"
#include "acl/compression/transform_space.h"
#include "acl/compression/impl/bit_rate_policies.transform.h"
#include "acl/compression/impl/rigid_shell_utils.h"
#include "acl/compression/impl/segment_context.h"
#include "acl/compression/impl/topology_metadata.h"
#include "acl/compression/impl/track_bit_rate_database.h"
#include "acl/compression/impl/track_stream.h"
#include "acl/compression/impl/transform_runtime.h"

#include <rtm/types.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>

ACL_IMPL_FILE_PRAGMA_PUSH

// This file reads the deprecated error metric of the settings
ACL_IMPL_DEPRECATION_WARNING_PUSH

namespace acl
{
	ACL_IMPL_VERSION_NAMESPACE_BEGIN

	namespace acl_impl
	{
		// The estimate below follows the allocations of 'compress_transform_track_list'.
		// It does not know which sub-tracks are constant or default before compression. Thus it assumes
		// that each sub-track is animated and that the clip has scale. This gives the largest buffers.
		// When you change an allocation of the compression of transform tracks, update this file.

		// The number of bytes of the small buffers with one entry for each transform.
		// Some phases allocate them temporarily (looping optimization, constant sub-tracks,
		// rigid shells, queries, bit rate permutations, conversions). The value is larger than their sum.
		static constexpr size_t k_estimate_scratch_size_per_transform = 1024;

		// The number of bytes that each phase can use in addition to its buffers (small allocations and alignment)
		static constexpr size_t k_estimate_scratch_size = 4096;

		// Returns the largest number of ancestors of a transform (a root transform has a depth of 0)
		inline uint32_t find_max_hierarchy_depth(const track_array& track_list)
		{
			const uint32_t num_transforms = track_list.get_num_tracks();
			uint32_t max_depth = 0;

			for (uint32_t transform_index = 0; transform_index < num_transforms; ++transform_index)
			{
				// Walk the ancestors, the loop stops after 'num_transforms' steps when the hierarchy is not valid
				uint32_t depth = 0;
				uint32_t parent_index = track_list[transform_index].get_description<track_desc_transformf>().parent_index;
				while (parent_index < num_transforms && depth < num_transforms)
				{
					depth++;
					parent_index = track_list[parent_index].get_description<track_desc_transformf>().parent_index;
				}

				max_depth = std::max<uint32_t>(max_depth, depth);
			}

			return max_depth;
		}

		// Returns the largest sum of the depths of all transforms (the number of ancestor and descendant pairs)
		// Each depth up to the largest depth has at least one transform, the other transforms have the largest depth
		inline size_t estimate_sum_hierarchy_depth(uint32_t num_transforms, uint32_t max_depth)
		{
			if (num_transforms == 0)
				return 0;

			const size_t depth = std::min<uint32_t>(max_depth, num_transforms - 1);
			return (depth * (depth + 1)) / 2 + (size_t(num_transforms) - depth - 1) * depth;
		}

		// The number of bytes of the three sample streams of one transform, with 'num_samples' samples
		inline size_t estimate_transform_streams_size(uint32_t num_samples)
		{
			return 3 * track_stream::get_required_size(num_samples, sizeof(rtm::vector4f));
		}

		// The number of bytes of a clip context without its sample streams, see 'initialize_clip_context'
		// We use one leaf transform chain for each transform: in mesh space, each transform is a leaf
		inline size_t estimate_clip_context_metadata_size(uint32_t num_transforms)
		{
			const uint32_t num_leaf_transforms = num_transforms;

			const size_t bitset_size = sizeof(uint32_t) * bitset_description::make_from_num_bits(num_transforms).get_size();

			size_t size = 0;
			size += sizeof(segment_context);
			size += sizeof(transform_metadata) * num_transforms;
			size += sizeof(uint32_t) * num_transforms;				// Sorted transforms
			size += sizeof(transform_streams) * num_transforms;
			size += bitset_size;									// Leaf transforms
			size += bitset_size * num_leaf_transforms;				// Leaf transform chains
			return size;
		}

		// The number of bytes of the topology, see 'build_clip_topology'
		inline size_t estimate_clip_topology_size(uint32_t num_transforms, uint32_t max_depth)
		{
			const size_t sum_depth = estimate_sum_hierarchy_depth(num_transforms, max_depth);

			size_t size = 0;
			size += sizeof(transform_topology_t) * num_transforms;
			size += sizeof(uint32_t) * num_transforms;				// Children
			size += sizeof(uint32_t) * sum_depth;					// Leaves of each transform (at most one for each ancestor of a leaf)
			size += sizeof(uint32_t) * num_transforms;				// Transforms sorted parent first
			size += sizeof(uint32_t) * sum_depth;					// Descendants of each transform
			return size;
		}

		// The number of samples in each segment, see 'split_samples_per_segment'
		struct estimate_segments_t
		{
			uint32_t num_segments = 1;
			uint32_t num_estimated_segments = 0;
			uint32_t num_first_segment_samples = 0;		// The first segment has the most samples
		};

		inline estimate_segments_t estimate_segments(uint32_t num_samples, const compression_segmenting_settings& settings)
		{
			estimate_segments_t result;
			result.num_first_segment_samples = num_samples;

			if (num_samples <= settings.max_num_samples)
				return result;	// A single segment

			const uint32_t num_estimated_segments = (num_samples + settings.ideal_num_samples - 1) / settings.ideal_num_samples;
			const uint32_t max_num_samples = num_estimated_segments * settings.ideal_num_samples;
			const uint32_t num_last_segment_samples = settings.ideal_num_samples - (max_num_samples - num_samples);
			const uint32_t slack_per_segment = settings.max_num_samples - settings.ideal_num_samples;
			const uint32_t total_slack_present = (num_estimated_segments - 1) * slack_per_segment;

			result.num_estimated_segments = num_estimated_segments;

			if (total_slack_present >= num_last_segment_samples)
			{
				// The other segments receive the samples of the last segment, the first segments receive the most
				const uint32_t num_receiving_segments = num_estimated_segments - 1;
				result.num_segments = num_receiving_segments;
				result.num_first_segment_samples = settings.ideal_num_samples + ((num_last_segment_samples + num_receiving_segments - 1) / num_receiving_segments);
			}
			else
			{
				result.num_segments = num_estimated_segments;
				result.num_first_segment_samples = settings.ideal_num_samples;
			}

			return result;
		}

		// The number of bytes of the optional metadata, see 'write_track_metadata.h'
		inline size_t estimate_metadata_size(const compression_memory_desc_t& desc, const compression_settings& settings, uint32_t num_samples)
		{
			const uint32_t num_transforms = desc.num_tracks;
			const bool include_contributing_error = settings.metadata.include_contributing_error || settings.enable_database_support;
			const bool include_parent_track_indices = settings.metadata.include_parent_track_indices || settings.metadata.include_track_descriptions;

			size_t size = 0;

			// The names and their null terminators
			if (settings.metadata.include_track_list_name || settings.metadata.include_track_names)
				size += size_t(desc.total_name_length) + num_transforms + 1;

			if (settings.metadata.include_track_names)
				size += sizeof(uint32_t) * num_transforms;			// Offsets

			if (include_parent_track_indices)
				size += sizeof(uint32_t) * num_transforms;

			if (settings.metadata.include_track_descriptions)
				size += sizeof(float) * 12 * num_transforms;

			if (include_contributing_error)
				size += sizeof(keyframe_stripping_metadata_t) * num_samples;

			if (size != 0)
				size += sizeof(optional_metadata_header) + 4 * 5;	// Header and alignment

			return size;
		}

		// The largest number of bytes of the compressed tracks, see the end of 'compress_transform_track_list'
		inline size_t estimate_compressed_transform_tracks_size(const compression_memory_desc_t& desc, const compression_settings& settings, const estimate_segments_t& segments, uint32_t num_samples)
		{
			const uint32_t num_transforms = desc.num_tracks;
			const uint32_t num_sub_tracks = num_transforms * 3;
			const uint32_t num_segments = segments.num_segments;

			// The largest sample of each sub-track type: a variable format uses at most 32 bits for each component
			const size_t rotation_sample_size = std::max<size_t>(get_packed_rotation_size(settings.rotation_format), sizeof(float) * 3);
			const size_t vector_sample_size = sizeof(float) * 3;
			const size_t pose_size = rotation_sample_size + vector_sample_size * 2;

			size_t size = 0;
			size += sizeof(raw_buffer_header) + sizeof(tracks_header) + sizeof(transform_tracks_header);
			size += sizeof(uint32_t) * (num_segments + 1);							// Segment start indices
			size += std::max(sizeof(segment_header), sizeof(stripped_segment_header_t)) * num_segments;
			size += sizeof(uint32_t) * ((num_transforms + 15) / 16) * 3;			// Packed sub-track types
			size += sizeof(rtm::vector4f) * num_sub_tracks;							// Constant samples
			size += sizeof(float) * 8 * num_sub_tracks;								// Clip range data
			size += (sizeof(uint8_t) + sizeof(uint8_t) * 8) * num_sub_tracks * num_segments;	// Format and range data of each segment
			size += pose_size * num_transforms * num_samples;						// Animated data
			size += 8 * num_segments;												// Alignment of each segment
			size += 15 + 4 * 8;														// Padding and alignment
			size += estimate_metadata_size(desc, settings, num_samples);
			return size;
		}

		inline size_t estimate_transform_compression_peak_memory_usage(const compression_memory_desc_t& desc, const compression_settings& settings)
		{
			const uint32_t num_transforms = desc.num_tracks;
			const uint32_t num_samples = desc.num_samples_per_track;
			if (num_transforms == 0 || num_samples == 0)
				return k_estimate_scratch_size;

			const bool has_base = desc.base_format != additive_clip_format8::none && desc.num_base_tracks != 0 && desc.num_base_samples_per_track != 0;

			// Find the transforms of the error metric that compression uses, like 'compress_transform_track_list'
			// We use the transforms with scale, they are the largest
			bool supports_v2 = true;
			size_t metric_transform_size = sizeof(rtm::vqvf);
			bool needs_conversion = false;

			if (settings.error_metric != nullptr)
			{
				// A legacy error metric
				supports_v2 = settings.error_metric->supports_bit_rate_algorithm_v2();
				metric_transform_size = settings.error_metric->get_transform_size(true);
				needs_conversion = settings.error_metric->needs_conversion(true);
			}
			else
			{
				// See 'transform_runtime_error_metric::initialize'
				const bool has_non_uniform_base_scale = has_base && desc.base_has_non_uniform_scale;

				transform_runtime_selector selector;
				transform_runtime_selection selection;
				const error_result result = selector.select(
					settings.transform_runtime_type, settings.custom_transform_adapter, settings.transform_metric,
					desc.track_type, true, has_non_uniform_base_scale, desc.transform_space, selection);

				// With invalid settings, compression fails before it allocates its buffers
				if (result.any() || selection.adapter == nullptr)
					return k_estimate_scratch_size;

				// The bit rate algorithm v2 uses the transform adapter of the selection
				metric_transform_size = selection.adapter->get_transform_size();
				needs_conversion = selection.adapter->needs_conversion();
			}

			const bool is_rotation_variable = is_rotation_format_variable(settings.rotation_format);
			const bool is_translation_variable = is_vector_format_variable(settings.translation_format);
			const bool is_scale_variable = is_vector_format_variable(settings.scale_format);
			const bool is_any_variable = is_rotation_variable || is_translation_variable || is_scale_variable;

			const bool include_contributing_error = settings.enable_database_support || settings.keyframe_stripping.is_enabled() || settings.metadata.include_contributing_error;

			// Without a variable format, compression does not split the clip into segments
			compression_segmenting_settings segmenting_settings;
			if (!is_any_variable)
			{
				segmenting_settings.ideal_num_samples = 0xFFFFFFFF;
				segmenting_settings.max_num_samples = 0xFFFFFFFF;
			}

			estimate_segments_t segments = estimate_segments(num_samples, segmenting_settings);

			// The looping optimization can remove the last sample, see 'optimize_looping'
			// With one sample less, the clip can have fewer segments with more samples, we use the largest values
			if (settings.optimize_loops && desc.looping_policy != sample_looping_policy::wrap && num_samples > 1)
			{
				const estimate_segments_t wrap_segments = estimate_segments(num_samples - 1, segmenting_settings);
				segments.num_segments = std::max(segments.num_segments, wrap_segments.num_segments);
				segments.num_estimated_segments = std::max(segments.num_estimated_segments, wrap_segments.num_estimated_segments);
				segments.num_first_segment_samples = std::max(segments.num_first_segment_samples, wrap_segments.num_first_segment_samples);
			}

			const uint32_t num_segments = segments.num_segments;
			const uint32_t num_first_segment_samples = segments.num_first_segment_samples;

			// Without a known depth, the hierarchy is a single chain of transforms
			const uint32_t max_depth = desc.max_hierarchy_depth == k_unknown_hierarchy_depth ? (num_transforms - 1) : desc.max_hierarchy_depth;

			// The base
			size_t base_size = 0;
			if (has_base)
			{
				const uint32_t num_base_transforms = desc.num_base_tracks;
				const uint32_t num_base_samples = desc.num_base_samples_per_track;

				// The clip context of the base
				base_size += estimate_clip_context_metadata_size(num_base_transforms);
				base_size += estimate_transform_streams_size(num_base_samples) * num_base_transforms;

				// The base poses of the space context, see 'transform_space_context_t::initialize'
				base_size += sizeof(rtm::vqvf) * size_t(num_base_samples) * num_base_transforms;
				base_size += sizeof(uint32_t) * num_samples;
			}

			// The buffers that live during all of compression
			// The raw clip context has every sample, the lossy clip context has its metadata
			const size_t clip_streams_size = estimate_transform_streams_size(num_samples) * num_transforms;
			const size_t clip_context_metadata_size = estimate_clip_context_metadata_size(num_transforms);

			size_t persistent_size = 0;
			persistent_size += clip_context_metadata_size + clip_streams_size;					// Raw clip context
			persistent_size += clip_context_metadata_size;										// Lossy clip context
			persistent_size += base_size;
			persistent_size += estimate_clip_topology_size(num_transforms, max_depth);
			persistent_size += sizeof(rigid_shell_metadata_t) * num_transforms;					// Clip shell metadata
			persistent_size += sizeof(transform_range) * num_transforms;						// Clip ranges
			persistent_size += k_estimate_scratch_size_per_transform * num_transforms;
			persistent_size += k_estimate_scratch_size;

			// Phase 1: initialization, constant sub-tracks, and rotation conversion
			// The lossy clip context has every sample and the conversion allocates one more stream
			size_t initialization_size = persistent_size;
			initialization_size += clip_streams_size;
			initialization_size += track_stream::get_required_size(num_samples, sizeof(rtm::quatf));

			// The segments, see 'segment_streams'
			size_t segment_streams_size = clip_streams_size;
			if (num_segments > 1)
			{
				segment_streams_size = 0;
				segment_streams_size += sizeof(segment_context) * num_segments;
				segment_streams_size += sizeof(transform_streams) * num_transforms * size_t(num_segments);
				// The first segment has the most samples
				segment_streams_size += estimate_transform_streams_size(num_first_segment_samples) * num_transforms * size_t(num_segments);
			}

			// Phase 2: the split into segments, the lossy clip context and the segments have every sample
			size_t segmenting_size = 0;
			if (num_segments > 1)
			{
				segmenting_size += persistent_size;
				segmenting_size += clip_streams_size;
				segmenting_size += segment_streams_size;
				segmenting_size += sizeof(uint32_t) * segments.num_estimated_segments;
			}

			// The segment ranges, see 'extract_segment_bone_ranges'
			const size_t segment_ranges_size = num_segments > 1 ? (sizeof(transform_range) * num_transforms * size_t(num_segments)) : 0;

			// The contributing error, see 'find_contributing_error' and 'sort_contributing_error'
			size_t contributing_error_size = 0;
			if (include_contributing_error)
			{
				contributing_error_size += sizeof(keyframe_stripping_metadata_t) * 32 * num_segments;
				contributing_error_size += sizeof(keyframe_stripping_metadata_t) * num_samples;
			}

			// Phase 3: the bit rate search and the quantization, see 'quantization_context'
			size_t quantization_size = persistent_size;
			quantization_size += segment_streams_size;
			quantization_size += segment_ranges_size;
			quantization_size += contributing_error_size;
			quantization_size += track_bit_rate_database::get_required_size(num_transforms, num_first_segment_samples, true);

			// A quantized stream replaces each stream, the old stream lives until the new one is complete
			quantization_size += 2 * track_stream::get_required_size(num_first_segment_samples, sizeof(rtm::vector4f));

			if (include_contributing_error)
				quantization_size += 2 * sizeof(rtm::vqvf) * num_transforms;			// Lossy transforms at the start and the end

			if (is_any_variable || include_contributing_error)
			{
				if (supports_v2)
				{
					const size_t num_cached_transforms = size_t(num_transforms) * segmenting_settings.max_num_samples;

					// See 'initialize_v2'
					quantization_size += 2 * sizeof(rtm::vqvf) * num_cached_transforms;

					// See 'initialize_v2_runtime'
					if (needs_conversion)
					{
						quantization_size += 2 * metric_transform_size * num_cached_transforms;
						quantization_size += metric_transform_size * (num_transforms + v2_adapter_policy::k_num_scratch_transforms);
						quantization_size += sizeof(rtm::vqvf) * num_transforms;
					}

					// See 'initialize_v2_scale'
					quantization_size += sizeof(uint32_t) * num_transforms * (std::min<uint32_t>(max_depth, num_transforms - 1) + 1);
				}
				else
				{
					// See 'initialize_v1'
					quantization_size += 2 * sizeof(rtm::vqvf) * num_transforms;
					quantization_size += 2 * metric_transform_size * num_transforms * size_t(num_first_segment_samples);
					quantization_size += 2 * metric_transform_size * num_transforms;
				}
			}

			// Phase 4: the output, the lossy streams are quantized
			size_t output_size = persistent_size;
			output_size += segment_streams_size;
			output_size += segment_ranges_size;
			output_size += contributing_error_size;
			output_size += sizeof(uint32_t) * num_transforms;							// Output track mapping
			output_size += estimate_compressed_transform_tracks_size(desc, settings, segments, num_samples);

			return std::max(std::max(initialization_size, segmenting_size), std::max(quantization_size, output_size));
		}
	}

	ACL_IMPL_VERSION_NAMESPACE_END
}

ACL_IMPL_DEPRECATION_WARNING_POP

ACL_IMPL_FILE_PRAGMA_POP
