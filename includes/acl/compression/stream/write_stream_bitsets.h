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

#include "acl/core/memory.h"
#include "acl/core/error.h"
#include "acl/core/bitset.h"
#include "acl/compression/stream/clip_context.h"

#include <stdint.h>

namespace acl
{
	inline void write_default_track_bitset(const BoneStreams* bone_streams, uint16_t num_bones, uint32_t* default_tracks_bitset, uint32_t bitset_size)
	{
		ACL_ENSURE(bone_streams != nullptr, "'bone_streams' cannot be null!");
		ACL_ENSURE(default_tracks_bitset != nullptr, "'default_tracks_bitset' cannot be null!");

		uint32_t default_track_offset = 0;

		bitset_reset(default_tracks_bitset, bitset_size, false);

		for (uint16_t bone_index = 0; bone_index < num_bones; ++bone_index)
		{
			const BoneStreams& bone_stream = bone_streams[bone_index];

			bitset_set(default_tracks_bitset, bitset_size, default_track_offset++, bone_stream.is_rotation_default);
			bitset_set(default_tracks_bitset, bitset_size, default_track_offset++, bone_stream.is_translation_default);
		}
	}

	inline void write_default_track_bitset(const ClipContext& clip_context, uint32_t* default_tracks_bitset, uint32_t bitset_size)
	{
		// Only use the first segment, it contains the necessary information
		const SegmentContext& segment = clip_context.segments[0];
		write_default_track_bitset(segment.bone_streams, segment.num_bones, default_tracks_bitset, bitset_size);
	}

	inline void write_constant_track_bitset(const BoneStreams* bone_streams, uint16_t num_bones, uint32_t* constant_tracks_bitset, uint32_t bitset_size)
	{
		uint32_t constant_track_offset = 0;

		bitset_reset(constant_tracks_bitset, bitset_size, false);

		for (uint16_t bone_index = 0; bone_index < num_bones; ++bone_index)
		{
			const BoneStreams& bone_stream = bone_streams[bone_index];

			bitset_set(constant_tracks_bitset, bitset_size, constant_track_offset++, bone_stream.is_rotation_constant);
			bitset_set(constant_tracks_bitset, bitset_size, constant_track_offset++, bone_stream.is_translation_constant);
		}
	}

	inline void write_constant_track_bitset(const ClipContext& clip_context, uint32_t* constant_tracks_bitset, uint32_t bitset_size)
	{
		// Only use the first segment, it contains the necessary information
		const SegmentContext& segment = clip_context.segments[0];
		write_constant_track_bitset(segment.bone_streams, segment.num_bones, constant_tracks_bitset, bitset_size);
	}
}
