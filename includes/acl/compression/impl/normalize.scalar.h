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

#include "acl/version.h"
#include "acl/core/impl/compiler_utils.h"
#include "acl/compression/impl/track_list_context.h"

#include <rtm/mask4i.h>
#include <rtm/vector4f.h>

#include <cstdint>

ACL_IMPL_FILE_PRAGMA_PUSH

namespace acl
{
	ACL_IMPL_VERSION_NAMESPACE_BEGIN

	namespace acl_impl
	{
		RTM_FORCE_INLINE rtm::vector4f RTM_SIMD_CALL normalize_scalarf_value(
			rtm::vector4f_arg0 value, rtm::vector4f_arg1 range_min, rtm::vector4f_arg2 range_extent,
			rtm::vector4f_arg3 zero, rtm::vector4f_arg4 one, rtm::mask4f_arg5 is_range_zero_mask
			)
		{
			// normalized value is between [0.0 .. 1.0]
			// value = (normalized value * range extent) + range min
			// normalized value = (value - range min) / range extent
			rtm::vector4f normalized_sample = rtm::vector_div(rtm::vector_sub(value, range_min), range_extent);

			// Clamp because the division might be imprecise
			normalized_sample = rtm::vector_min(normalized_sample, one);
			normalized_sample = rtm::vector_select(is_range_zero_mask, zero, normalized_sample);

			ACL_ASSERT(rtm::vector_all_greater_equal(normalized_sample, zero) && rtm::vector_all_less_equal(normalized_sample, one), "Invalid normalized value. 0.0 <= [%f, %f, %f, %f] <= 1.0", (float)rtm::vector_get_x(normalized_sample), (float)rtm::vector_get_y(normalized_sample), (float)rtm::vector_get_z(normalized_sample), (float)rtm::vector_get_w(normalized_sample));

			return normalized_sample;
		}

		inline void normalize_scalarf_track(track& mut_track, const scalarf_range& range)
		{
			const rtm::vector4f one = rtm::vector_set(1.0F);
			const rtm::vector4f zero = rtm::vector_zero();

			track_vector4f_ex& typed_track = track_cast<track_vector4f_ex>(mut_track);

			const uint32_t num_samples = mut_track.get_num_samples();

			const rtm::vector4f range_min = range.get_min();
			const rtm::vector4f range_extent = range.get_extent();
			const rtm::mask4f is_range_zero_mask = rtm::vector_less_than(range_extent, rtm::vector_set(0.000000001F));

			for (uint32_t sample_index = 0; sample_index < num_samples; ++sample_index)
			{
				track_extended_sample_t<rtm::vector4f>& sample = typed_track[sample_index];

				sample.value = normalize_scalarf_value(sample.value, range_min, range_extent, zero, one, is_range_zero_mask);
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
				case track_category8::scalard:
				case track_category8::transformf:
				case track_category8::transformd:
				default:
					ACL_ASSERT(false, "Invalid track category");
					break;
				}
			}
		}
	}

	ACL_IMPL_VERSION_NAMESPACE_END
}

ACL_IMPL_FILE_PRAGMA_POP
