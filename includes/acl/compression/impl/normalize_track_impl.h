#pragma once

////////////////////////////////////////////////////////////////////////////////
// The MIT License (MIT)
//
// Copyright (c) 2019 Nicholas Frechette & Animation Compression Library contributors
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

#include "acl/core/impl/compiler_utils.h"
#include "acl/compression/impl/track_list_context.h"

#include <rtm/mask4i.h>
#include <rtm/vector4f.h>

#include <cstdint>

ACL_IMPL_FILE_PRAGMA_PUSH

namespace acl
{
	namespace acl_impl
	{
		inline void normalize_scalarf_track(track& mut_track, const scalarf_range& range)
		{
			using namespace rtm;

#ifdef ACL_PRECISION_BOOST

			const vector4f half = rtm::vector_set(0.5F);
			const vector4f half_neg = rtm::vector_set(-0.5F);

#else

			const vector4f one = rtm::vector_set(1.0F);
			const vector4f zero = vector_zero();

#endif

			track_vector4f& typed_track = track_cast<track_vector4f>(mut_track);

			const uint32_t num_samples = mut_track.get_num_samples();

#ifdef ACL_PRECISION_BOOST

			const vector4f range_center = range.get_center();

#else
			const vector4f range_min = range.get_min();

#endif

			const vector4f range_extent = range.get_extent();
			const mask4f is_range_zero_mask = vector_less_than(range_extent, rtm::vector_set(0.000000001F));

			for (uint32_t sample_index = 0; sample_index < num_samples; ++sample_index)
			{

#ifdef ACL_PRECISION_BOOST

				// normalized value is between [-0.5 .. 0.5]
				// value = (normalized value * range extent) + range center
				// normalized value = (value - range center) / range extent

#else

				// normalized value is between [0.0 .. 1.0]
				// value = (normalized value * range extent) + range min
				// normalized value = (value - range min) / range extent

#endif

				const vector4f sample = typed_track[sample_index];

#ifdef ACL_PRECISION_BOOST

				vector4f normalized_sample = vector_div(vector_sub(sample, range_center), range_extent);

				// Clamp because the division might be imprecise
				normalized_sample = rtm::vector_clamp(normalized_sample, half_neg, half);
				normalized_sample = rtm::vector_select(is_range_zero_mask, half_neg, normalized_sample);

				ACL_ASSERT(vector_all_greater_equal(normalized_sample, half_neg) && vector_all_less_equal(normalized_sample, half), "Invalid normalized value. -0.5 <= [%f, %f, %f, %f] <= 0.5", (float)vector_get_x(normalized_sample), (float)vector_get_y(normalized_sample), (float)vector_get_z(normalized_sample), (float)vector_get_w(normalized_sample));

#else

				vector4f normalized_sample = vector_div(vector_sub(sample, range_min), range_extent);

				// Clamp because the division might be imprecise
				normalized_sample = vector_min(normalized_sample, one);
				normalized_sample = vector_select(is_range_zero_mask, zero, normalized_sample);

				ACL_ASSERT(vector_all_greater_equal(normalized_sample, zero) && vector_all_less_equal(normalized_sample, one), "Invalid normalized value. 0.0 <= [%f, %f, %f, %f] <= 1.0", (float)vector_get_x(normalized_sample), (float)vector_get_y(normalized_sample), (float)vector_get_z(normalized_sample), (float)vector_get_w(normalized_sample));

#endif

				typed_track[sample_index] = normalized_sample;
			}
		}

		inline void normalize_tracks(track_list_context& context)
		{
			ACL_ASSERT(context.is_valid(), "Invalid context");

			for (uint32_t track_index = 0; track_index < context.num_tracks; ++track_index)
			{
				const bool is_track_constant = context.is_constant(track_index);
				if (is_track_constant)
					continue;	// Constant tracks don't need to be modified

				const track_range& range = context.range_list[track_index];
				track& mut_track = context.track_list[track_index];

				switch (range.category)
				{
				case track_category8::scalarf:
					normalize_scalarf_track(mut_track, range.range.scalarf);
					break;
				default:
					ACL_ASSERT(false, "Invalid track category");
					break;
				}
			}
		}
	}
}

ACL_IMPL_FILE_PRAGMA_POP
