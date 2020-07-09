#pragma once

////////////////////////////////////////////////////////////////////////////////
// The MIT License (MIT)
//
// Copyright (c) 2017 Nicholas Frechette & Animation Compression Library contributors
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

#include "acl/core/error.h"
#include "acl/core/bitset.h"
#include "acl/core/impl/compiler_utils.h"
#include "acl/compression/impl/clip_context.h"

#include <cstdint>

ACL_IMPL_FILE_PRAGMA_PUSH

namespace acl
{
	namespace acl_impl
	{
		inline uint32_t write_default_track_bitset(const clip_context& clip, uint32_t* default_tracks_bitset, bitset_description bitset_desc, const uint32_t* output_bone_mapping, uint32_t num_output_bones)
		{
			ACL_ASSERT(default_tracks_bitset != nullptr, "'default_tracks_bitset' cannot be null!");

			// Only use the first segment, it contains the necessary information
			const SegmentContext& segment = clip.segments[0];

			uint32_t default_track_offset = 0;
			uint32_t size_written = 0;

			bitset_reset(default_tracks_bitset, bitset_desc, false);

			for (uint32_t output_index = 0; output_index < num_output_bones; ++output_index)
			{
				const uint32_t bone_index = output_bone_mapping[output_index];
				const BoneStreams& bone_stream = segment.bone_streams[bone_index];

				bitset_set(default_tracks_bitset, bitset_desc, default_track_offset++, bone_stream.is_rotation_default);
				bitset_set(default_tracks_bitset, bitset_desc, default_track_offset++, bone_stream.is_translation_default);

				if (clip.has_scale)
					bitset_set(default_tracks_bitset, bitset_desc, default_track_offset++, bone_stream.is_scale_default);

				size_written += clip.has_scale ? 3 : 2;
			}

			ACL_ASSERT(default_track_offset <= bitset_desc.get_num_bits(), "Too many tracks found for bitset");
			return ((size_written + 31) / 32) * sizeof(uint32_t);	// Convert bits to bytes
		}

		inline uint32_t write_constant_track_bitset(const clip_context& clip, uint32_t* constant_tracks_bitset, bitset_description bitset_desc, const uint32_t* output_bone_mapping, uint32_t num_output_bones)
		{
			ACL_ASSERT(constant_tracks_bitset != nullptr, "'constant_tracks_bitset' cannot be null!");

			// Only use the first segment, it contains the necessary information
			const SegmentContext& segment = clip.segments[0];

			uint32_t constant_track_offset = 0;
			uint32_t size_written = 0;

			bitset_reset(constant_tracks_bitset, bitset_desc, false);

			for (uint32_t output_index = 0; output_index < num_output_bones; ++output_index)
			{
				const uint32_t bone_index = output_bone_mapping[output_index];
				const BoneStreams& bone_stream = segment.bone_streams[bone_index];

				bitset_set(constant_tracks_bitset, bitset_desc, constant_track_offset++, bone_stream.is_rotation_constant);
				bitset_set(constant_tracks_bitset, bitset_desc, constant_track_offset++, bone_stream.is_translation_constant);

				if (clip.has_scale)
					bitset_set(constant_tracks_bitset, bitset_desc, constant_track_offset++, bone_stream.is_scale_constant);

				size_written += clip.has_scale ? 3 : 2;
			}

			ACL_ASSERT(constant_track_offset <= bitset_desc.get_num_bits(), "Too many tracks found for bitset");
			return ((size_written + 31) / 32) * sizeof(uint32_t);	// Convert bits to bytes
		}
	}
}

ACL_IMPL_FILE_PRAGMA_POP
