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

#include "acl/compressed_clip.h"

#include <stdint.h>

namespace acl
{
	struct FullPrecisionConstants
	{
		static constexpr uint32_t NUM_TRACKS_PER_BONE = 2;
		static constexpr uint32_t BITSET_WIDTH = 32;
	};

	struct FullPrecisionHeader
	{
		uint32_t	num_bones;
		uint32_t	num_samples;
		uint32_t	sample_rate;		// TODO: Store duration as float instead
		uint32_t	num_animated_rotation_tracks;
		uint32_t	num_animated_translation_tracks;

		uint16_t	default_tracks_bitset_offset;
		uint16_t	track_data_offset;

		//////////////////////////////////////////////////////////////////////////

		uint32_t*		get_default_tracks_bitset()			{ return reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(this) + default_tracks_bitset_offset); }
		const uint32_t*	get_default_tracks_bitset() const	{ return reinterpret_cast<const uint32_t*>(reinterpret_cast<const uint8_t*>(this) + default_tracks_bitset_offset); }

		float*			get_track_data()					{ return reinterpret_cast<float*>(reinterpret_cast<uint8_t*>(this) + track_data_offset); }
		const float*	get_track_data() const				{ return reinterpret_cast<const float*>(reinterpret_cast<const uint8_t*>(this) + track_data_offset); }
	};

	inline FullPrecisionHeader& get_full_precision_header(CompressedClip& clip)
	{
		return *reinterpret_cast<FullPrecisionHeader*>(reinterpret_cast<uint8_t*>(&clip) + sizeof(CompressedClip));
	}

	inline const FullPrecisionHeader& get_full_precision_header(const CompressedClip& clip)
	{
		return *reinterpret_cast<const FullPrecisionHeader*>(reinterpret_cast<const uint8_t*>(&clip) + sizeof(CompressedClip));
	}
}
