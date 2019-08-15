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

#include "acl/core/track_types.h"
#include "acl/core/algorithm_types.h"
#include "acl/core/compiler_utils.h"
#include "acl/core/ptr_offset.h"

#include <cstdint>

ACL_IMPL_FILE_PRAGMA_PUSH

namespace acl
{
	namespace acl_impl
	{
		struct raw_buffer_header
		{
			// Total size in bytes of the raw buffer.
			uint32_t		size;

			// Hash of the raw buffer.
			uint32_t		hash;
		};

		struct track_metadata
		{
			uint8_t			bit_rate;
		};

		struct tracks_header
		{
			// Serialization tag used to distinguish raw buffer types.
			uint32_t		tag;

			// Serialization version used to compress the tracks.
			uint16_t		version;

			// Algorithm type used to compress the tracks.
			AlgorithmType8	algorithm_type;

			// Type of the tracks contained in this compressed stream.
			track_type8		track_type;

			// The total number of tracks.
			uint32_t		num_tracks;

			// The total number of samples per track.
			uint32_t		num_samples;

			// The sample rate our tracks use.
			float			sample_rate;								// TODO: Store duration as float instead?

			// The number of bits used for a whole frame of data.
			// The sum of one sample per track with all bit rates taken into account.
			uint32_t		num_bits_per_frame;

			// Various data offsets relative to the start of this header.
			PtrOffset32<track_metadata>		metadata_per_track;
			PtrOffset32<float>				track_constant_values;
			PtrOffset32<float>				track_range_values;
			PtrOffset32<uint8_t>			track_animated_values;

			//////////////////////////////////////////////////////////////////////////

			track_metadata*			get_track_metadata() { return metadata_per_track.add_to(this); }
			const track_metadata*	get_track_metadata() const { return metadata_per_track.add_to(this); }

			float*					get_track_constant_values() { return track_constant_values.add_to(this); }
			const float*			get_track_constant_values() const { return track_constant_values.add_to(this); }

			float*					get_track_range_values() { return track_range_values.add_to(this); }
			const float*			get_track_range_values() const { return track_range_values.add_to(this); }

			uint8_t*				get_track_animated_values() { return track_animated_values.add_to(this); }
			const uint8_t*			get_track_animated_values() const { return track_animated_values.add_to(this); }
		};
	}
}

ACL_IMPL_FILE_PRAGMA_POP
