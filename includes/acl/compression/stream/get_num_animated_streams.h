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

#include "acl/compression/stream/track_stream.h"

#include <stdint.h>

namespace acl
{
	inline void get_num_animated_streams(const BoneStreams* bone_streams, uint16_t num_bones,
		uint32_t& out_num_constant_rotation_streams, uint32_t& out_num_constant_translation_streams,
		uint32_t& out_num_animated_rotation_streams, uint32_t& out_num_animated_translation_streams)
	{
		uint32_t num_constant_rotation_streams = 0;
		uint32_t num_constant_translation_streams = 0;
		uint32_t num_animated_rotation_streams = 0;
		uint32_t num_animated_translation_streams = 0;

		for (uint16_t bone_index = 0; bone_index < num_bones; ++bone_index)
		{
			const BoneStreams& bone_stream = bone_streams[bone_index];

			if (!bone_stream.is_rotation_default)
			{
				if (bone_stream.is_rotation_constant)
					num_constant_rotation_streams++;
				else
					num_animated_rotation_streams++;
			}

			if (!bone_stream.is_translation_default)
			{
				if (bone_stream.is_translation_constant)
					num_constant_translation_streams++;
				else
					num_animated_translation_streams++;
			}
		}

		out_num_constant_rotation_streams = num_constant_rotation_streams;
		out_num_constant_translation_streams = num_constant_translation_streams;
		out_num_animated_rotation_streams = num_animated_rotation_streams;
		out_num_animated_translation_streams = num_animated_translation_streams;
	}
}
