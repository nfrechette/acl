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
#include "acl/core/iallocator.h"
#include "acl/core/impl/compiler_utils.h"
#include "acl/core/error.h"
#include "acl/core/track_formats.h"
#include "acl/compression/compression_settings.h"
#include "acl/compression/impl/clip_context.h"
#include "acl/compression/impl/normalize.transform.h"
#include "acl/compression/impl/segment.transform.h"
#include "acl/compression/impl/track_stream.h"
#include "acl/math/quatf.h"

#include <rtm/quatf.h>
#include <rtm/scalarf.h>
#include <rtm/vector4f.h>

#include <algorithm>
#include <cstdint>

ACL_IMPL_FILE_PRAGMA_PUSH

namespace acl
{
	ACL_IMPL_VERSION_NAMESPACE_BEGIN

	namespace acl_impl
	{
		// Returns the index of the rotation component with the largest smallest absolute value over the samples, W wins a tie.
		// The rebuild error of a dropped component grows with 1 / |c|: the component that stays the farthest from 0 gives the best precision.
		inline uint32_t find_dropped_rotation_component(const rotation_track_stream& rotations, uint32_t first_sample_index, uint32_t num_samples)
		{
			float smallest_values[4] = { 2.0F, 2.0F, 2.0F, 2.0F };
			for (uint32_t sample_index = first_sample_index; sample_index < first_sample_index + num_samples; ++sample_index)
			{
				const rtm::vector4f rotation = rotations.get_raw_sample<rtm::vector4f>(sample_index);
				smallest_values[0] = rtm::scalar_min(smallest_values[0], rtm::scalar_abs(float(rtm::vector_get_x(rotation))));
				smallest_values[1] = rtm::scalar_min(smallest_values[1], rtm::scalar_abs(float(rtm::vector_get_y(rotation))));
				smallest_values[2] = rtm::scalar_min(smallest_values[2], rtm::scalar_abs(float(rtm::vector_get_z(rotation))));
				smallest_values[3] = rtm::scalar_min(smallest_values[3], rtm::scalar_abs(float(rtm::vector_get_w(rotation))));
			}

			uint32_t dropped_component = 3;
			for (uint32_t component = 0; component < 3; ++component)
			{
				if (smallest_values[component] > smallest_values[dropped_component])
					dropped_component = component;
			}

			return dropped_component;
		}

		// With a drop largest rotation format, selects the rotation component that each transform drops and stores
		// each rotation as (kept0, kept1, kept2, dropped), see 'quat_drop_component'.
		//    - A full precision sample drops its largest component, and holds its index in bit 30 of [x] and [y].
		//      This is the case of a constant rotation, and of each sample with the quatf_drop_largest_full format.
		//    - With the quatf_drop_largest_variable format, an animated rotation drops in each segment the component with
		//      the largest smallest absolute value. 'transform_streams::rotation_dropped_component' holds its index.
		// We run before the clip range reduction and the segmenting: the clip ranges bound the kept components of each segment,
		// thus the clip range data keeps its size. Before this function, the rotations drop W, as with a format that always drops W.
		// The segments are the ones that segment_streams(..) creates later with the same settings.
		inline void select_dropped_rotation_components(iallocator& allocator, clip_context& context, const compression_segmenting_settings& segmenting_settings, rotation_format8 rotation_format)
		{
			if (get_rotation_variant(rotation_format) != rotation_variant8::quat_drop_largest)
				return;	// Only the drop largest formats drop another component than W

			ACL_ASSERT(context.num_segments == 1, "context must contain a single segment!");
			ACL_ASSERT(context.rotation_dropped_components == nullptr, "The dropped components are already selected");

			segment_context& clip_segment = context.segments[0];
			const uint32_t num_samples = context.num_samples;
			const uint32_t num_transforms = context.num_bones;

			// The same split as segment_streams(..)
			uint32_t num_estimated_segments = 0;
			uint32_t num_segments = 0;
			uint32_t* num_samples_per_segment = nullptr;
			if (num_samples > segmenting_settings.max_num_samples)
				num_samples_per_segment = split_samples_per_segment(allocator, num_samples, segmenting_settings, num_estimated_segments, num_segments);

			if (num_samples_per_segment != nullptr)
			{
				context.num_rotation_dropped_components = num_segments * num_transforms;
				context.rotation_dropped_components = allocate_type_array<uint8_t>(allocator, context.num_rotation_dropped_components);
				std::fill(context.rotation_dropped_components, context.rotation_dropped_components + context.num_rotation_dropped_components, uint8_t(3));
			}
			else
				num_segments = 1;

			for (uint32_t transform_index = 0; transform_index < num_transforms; ++transform_index)
			{
				transform_streams& bone_stream = clip_segment.bone_streams[transform_index];
				if (bone_stream.is_rotation_default)
					continue;	// A default rotation has no data

				rotation_track_stream& rotations = bone_stream.rotations;

				if (bone_stream.is_rotation_constant || !is_rotation_format_variable(rotation_format))
				{
					// Each full precision sample drops its largest component and holds its index
					for (uint32_t sample_index = 0; sample_index < rotations.get_num_samples(); ++sample_index)
					{
						const rtm::quatf rotation = rotations.get_raw_sample<rtm::quatf>(sample_index);
						const uint32_t dropped_component = quat_get_largest_component(rotation);
						rotations.set_raw_sample(sample_index, encode_dropped_component_in_floats(quat_drop_component(rotation, dropped_component), dropped_component));
					}

					continue;
				}

				ACL_ASSERT(rotations.get_num_samples() == num_samples, "Expected an animated rotation");

				uint32_t first_sample_index = 0;
				for (uint32_t segment_index = 0; segment_index < num_segments; ++segment_index)
				{
					const uint32_t num_segment_samples = num_samples_per_segment != nullptr ? num_samples_per_segment[segment_index] : num_samples;
					const uint32_t dropped_component = find_dropped_rotation_component(rotations, first_sample_index, num_segment_samples);

					for (uint32_t sample_index = first_sample_index; sample_index < first_sample_index + num_segment_samples; ++sample_index)
					{
						const rtm::quatf rotation = rotations.get_raw_sample<rtm::quatf>(sample_index);
						rotations.set_raw_sample(sample_index, quat_drop_component(rotation, dropped_component));
					}

					if (segment_index == 0)
						bone_stream.rotation_dropped_component = uint8_t(dropped_component);

					if (context.rotation_dropped_components != nullptr)
						context.rotation_dropped_components[(size_t(segment_index) * num_transforms) + transform_index] = uint8_t(dropped_component);

					first_sample_index += num_segment_samples;
				}

				// The clip range of each lane now bounds the kept components that it holds in each segment
				context.ranges[transform_index].rotation = calculate_track_range(rotations, true);
			}

			deallocate_type_array(allocator, num_samples_per_segment, num_estimated_segments);
		}
	}

	ACL_IMPL_VERSION_NAMESPACE_END
}

ACL_IMPL_FILE_PRAGMA_POP
