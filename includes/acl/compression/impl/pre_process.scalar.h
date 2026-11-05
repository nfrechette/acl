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
#include "acl/core/track_traits.h"
#include "acl/core/impl/compiler_utils.h"
#include "acl/compression/track.h"
#include "acl/compression/track_array.h"
#include "acl/compression/impl/pre_process.common.h"
#include "acl/compression/impl/track_list_context.h"

#include <rtm/vector4f.h>

#include <cstdint>

ACL_IMPL_FILE_PRAGMA_PUSH

namespace acl
{
	ACL_IMPL_VERSION_NAMESPACE_BEGIN

	namespace acl_impl
	{
		// Loads the value of a sample as a vector.
		// With an extended track, this function loads only the value, not the sample interpolator.
		template<track_type8 k_track_type>
		inline rtm::vector4f RTM_SIMD_CALL pre_process_load_scalar_sample_value(const track& track_, uint32_t sample_index)
		{
			using track_trait_t = track_traits<k_track_type>;
			using sample_type_t = typename track_trait_t::sample_type;

			return track_trait_t::load_as_vector(static_cast<const sample_type_t*>(get_sample_value_ptr(track_, sample_index)));
		}

		// Copies the value of a sample into another sample.
		// With an extended track, this function copies only the value. The sample interpolator does not change.
		template<track_type8 k_track_type>
		inline void pre_process_copy_scalar_sample_value(track& track_, uint32_t src_sample_index, uint32_t dst_sample_index)
		{
			using sample_type_t = typename track_traits<k_track_type>::sample_type;

			const sample_type_t value = *static_cast<const sample_type_t*>(get_sample_value_ptr(track_, src_sample_index));
			*static_cast<sample_type_t*>(get_sample_value_ptr(track_, dst_sample_index)) = value;
		}

		template<track_type8 k_track_type>
		inline void pre_process_optimize_looping_scalar(track_array& track_list)
		{
			const uint32_t last_sample_index = track_list.get_num_samples_per_track() - 1;

			bool is_looping = true;
			for (const track& track_ : track_list)
			{
				const track_desc_scalarf& desc = track_.get_description<track_desc_scalarf>();

				const rtm::vector4f first_sample = pre_process_load_scalar_sample_value<k_track_type>(track_, 0);
				const rtm::vector4f last_sample = pre_process_load_scalar_sample_value<k_track_type>(track_, last_sample_index);
				if (!rtm::vector_all_near_equal(first_sample, last_sample, desc.precision))
				{
					is_looping = false;
					break;
				}
			}

			if (is_looping)
			{
				// The interpolator of the last sample has no effect: when we clamp, no interval follows it,
				// and when we wrap, compression removes the last sample. Thus we keep it.
				for (track& track_ : track_list)
					pre_process_copy_scalar_sample_value<k_track_type>(track_, 0, last_sample_index);
			}
		}

		template<track_type8 k_track_type>
		inline void pre_process_sanitize_constant_tracks_scalar(track& track_)
		{
			const uint32_t num_samples_per_track = track_.get_num_samples();

			const track_desc_scalarf& desc = track_.get_description<track_desc_scalarf>();
			const float precision = desc.precision;

			rtm::vector4f min = pre_process_load_scalar_sample_value<k_track_type>(track_, 0);
			rtm::vector4f max = pre_process_load_scalar_sample_value<k_track_type>(track_, num_samples_per_track - 1);

			for (uint32_t sample_index = 1; sample_index < num_samples_per_track; ++sample_index)
			{
				const rtm::vector4f sample = pre_process_load_scalar_sample_value<k_track_type>(track_, sample_index);

				min = rtm::vector_min(min, sample);
				max = rtm::vector_max(max, sample);
			}

			// A constant sample holds the value of a sample, thus the range of the values is also
			// the range of the output when the track has extended samples.
			const rtm::vector4f extent = rtm::vector_sub(max, min);
			if (rtm::vector_all_less_equal(extent, rtm::vector_set(precision)))
			{
				// When all the values are equal, the sample interpolators have no effect. Thus we keep them.
				for (uint32_t sample_index = 1; sample_index < num_samples_per_track; ++sample_index)
					pre_process_copy_scalar_sample_value<k_track_type>(track_, 0, sample_index);
			}
		}
	}

	ACL_IMPL_VERSION_NAMESPACE_END
}

ACL_IMPL_FILE_PRAGMA_POP
