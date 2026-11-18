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
#include "acl/core/impl/compiler_utils.h"
#include "acl/core/impl/compressed_headers.h"
#include "acl/compression/compression_memory_desc.h"
#include "acl/compression/compression_settings.h"
#include "acl/compression/track.h"
#include "acl/compression/impl/track_list_context.h"
#include "acl/compression/impl/track_range.h"

#include <rtm/types.h>

#include <cstddef>
#include <cstdint>

ACL_IMPL_FILE_PRAGMA_PUSH

namespace acl
{
	ACL_IMPL_VERSION_NAMESPACE_BEGIN

	namespace acl_impl
	{
		// The estimate below follows the allocations of 'compress_scalar_track_list'.
		// It does not know which tracks are constant before compression. Thus it assumes that each track is animated.
		// When you change an allocation of the compression of scalar tracks, update this file.

		// The number of bytes that compression can use in addition to its buffers (small allocations and alignment)
		static constexpr size_t k_estimate_scalar_scratch_size = 4096;

		// The number of bytes of the optional metadata, see 'compress_scalar_track_list'
		inline size_t estimate_scalar_metadata_size(const compression_memory_desc_t& desc, const compression_settings& settings)
		{
			const uint32_t num_tracks = desc.num_tracks;

			size_t size = 0;

			// The names and their null terminators
			if (settings.metadata.include_track_list_name || settings.metadata.include_track_names)
				size += size_t(desc.total_name_length) + num_tracks + 1;

			if (settings.metadata.include_track_names)
				size += sizeof(uint32_t) * num_tracks;			// Offsets

			if (settings.metadata.include_track_descriptions)
				size += sizeof(float) * num_tracks;

			if (size != 0)
				size += sizeof(optional_metadata_header) + 4 * 3;	// Header and alignment

			return size;
		}

		inline size_t estimate_scalar_compression_peak_memory_usage(const compression_memory_desc_t& desc, const compression_settings& settings)
		{
			const uint32_t num_tracks = desc.num_tracks;
			const uint32_t num_samples = desc.num_samples_per_track;

			// The copy of the tracks, see 'copy_and_promote_track_list'
			// Each track is a vector4f track with a sample interpolator for each sample
			size_t size = 0;
			size += sizeof(track) * num_tracks;
			size += sizeof(track_vector4f_ex::sample_type) * size_t(num_samples) * num_tracks;

			// The buffers of the context, see 'track_list_context'
			size += sizeof(uint32_t) * num_tracks;										// Output track mapping
			size += sizeof(track_range) * num_tracks;									// Ranges
			size += sizeof(uint32_t) * bitset_description::make_from_num_bits(num_tracks).get_size();	// Constant tracks
			size += sizeof(track_bit_rate) * num_tracks;								// Bit rates
			size += sizeof(sample_interpolation_mode_t) * num_tracks;					// Sample interpolation modes

			// The compressed tracks, a sample uses at most 32 bits for each component
			// Each sample can also store its interpolator
			size += sizeof(raw_buffer_header) + sizeof(tracks_header) + sizeof(scalar_tracks_header);
			size += sizeof(track_metadata) * num_tracks;
			size += sizeof(rtm::vector4f) * num_tracks;									// Constant values
			size += sizeof(rtm::vector4f) * 2 * num_tracks;								// Range values
			size += (sizeof(rtm::vector4f) + sizeof(uint8_t)) * size_t(num_samples) * num_tracks;	// Animated values
			size += 15 + 4 * 4;															// Padding and alignment
			size += estimate_scalar_metadata_size(desc, settings);

			size += k_estimate_scalar_scratch_size;
			return size;
		}
	}

	ACL_IMPL_VERSION_NAMESPACE_END
}

ACL_IMPL_FILE_PRAGMA_POP
