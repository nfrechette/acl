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
#include "acl/math/quat_64.h"
#include "acl/math/quat_packing.h"
#include "acl/math/vector4_64.h"
#include "acl/compression/stream/track_stream.h"

#include <stdint.h>

namespace acl
{
	inline void write_constant_track_data(const BoneStreams* bone_streams, uint16_t num_bones, uint8_t* constant_data, uint32_t constant_data_size)
	{
		const uint8_t* constant_data_end = add_offset_to_ptr<uint8_t>(constant_data, constant_data_size);

		for (uint16_t bone_index = 0; bone_index < num_bones; ++bone_index)
		{
			const BoneStreams& bone_stream = bone_streams[bone_index];

			if (!bone_stream.is_rotation_default && bone_stream.is_rotation_constant)
			{
				const uint8_t* rotation_ptr = bone_stream.rotations.get_sample_ptr(0);
				uint32_t sample_size = bone_stream.rotations.get_sample_size();
				memcpy(constant_data, rotation_ptr, sample_size);
				constant_data += sample_size;
			}

			if (!bone_stream.is_translation_default && bone_stream.is_translation_constant)
			{
				const uint8_t* translation_ptr = bone_stream.translations.get_sample_ptr(0);
				uint32_t sample_size = bone_stream.translations.get_sample_size();
				memcpy(constant_data, translation_ptr, sample_size);
				constant_data += sample_size;
			}

			ACL_ENSURE(constant_data <= constant_data_end, "Invalid constant data offset. Wrote too much data.");
		}

		ACL_ENSURE(constant_data == constant_data_end, "Invalid constant data offset. Wrote too little data.");
	}

	inline void write_animated_track_data(const BoneStreams* bone_streams, uint16_t num_bones, uint8_t* animated_track_data, uint32_t animated_data_size)
	{
		const uint8_t* animated_track_data_end = add_offset_to_ptr<uint8_t>(animated_track_data, animated_data_size);

		uint32_t num_samples = 1;
		for (uint16_t bone_index = 0; bone_index < num_bones; ++bone_index)
		{
			const BoneStreams& bone_stream = bone_streams[bone_index];
			num_samples = std::max(num_samples, bone_stream.rotations.get_num_samples());
			num_samples = std::max(num_samples, bone_stream.translations.get_num_samples());

			if (num_samples != 1)
				break;
		}

		ACL_ENSURE(num_samples > 1, "No samples to write!");

		// Data is sorted first by time, second by bone.
		// This ensures that all bones are contiguous in memory when we sample a particular time.
		for (uint32_t sample_index = 0; sample_index < num_samples; ++sample_index)
		{
			for (uint16_t bone_index = 0; bone_index < num_bones; ++bone_index)
			{
				const BoneStreams& bone_stream = bone_streams[bone_index];

				if (bone_stream.is_rotation_animated())
				{
					const uint8_t* rotation_ptr = bone_stream.rotations.get_sample_ptr(sample_index);
					uint32_t sample_size = bone_stream.rotations.get_sample_size();
					memcpy(animated_track_data, rotation_ptr, sample_size);
					animated_track_data += sample_size;
				}

				if (bone_stream.is_translation_animated())
				{
					const uint8_t* translation_ptr = bone_stream.translations.get_sample_ptr(sample_index);
					uint32_t sample_size = bone_stream.translations.get_sample_size();
					memcpy(animated_track_data, translation_ptr, sample_size);
					animated_track_data += sample_size;
				}

				ACL_ENSURE(animated_track_data <= animated_track_data_end, "Invalid animated track data offset. Wrote too much data.");
			}
		}

		ACL_ENSURE(animated_track_data == animated_track_data_end, "Invalid animated track data offset. Wrote too little data.");
	}
}
