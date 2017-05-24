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
#include "acl/math/quat_32.h"
#include "acl/math/vector4_32.h"
#include "acl/math/scalar_32.h"
#include "acl/decompression/output_writer.h"

#include <stdint.h>
#include <algorithm>

// See encoder for details

namespace acl
{
	inline Quat_32 quat_lerp(const Quat_32& start, const Quat_32& end, float alpha)
	{
		// TODO: Implement coercion operators?
		// TODO: Move this function in quat_32.h
		Vector4_32 start_vector = vector_set(quat_get_x(start), quat_get_y(start), quat_get_z(start), quat_get_w(start));
		Vector4_32 end_vector = vector_set(quat_get_x(end), quat_get_y(end), quat_get_z(end), quat_get_w(end));
		Vector4_32 value = vector_add(start_vector, vector_mul(vector_sub(end_vector, start_vector), vector_set(alpha)));
		return quat_normalize(quat_set(vector_get_x(value), vector_get_y(value), vector_get_z(value), vector_get_w(value)));
	}

	template<class OutputWriterType>
	inline void full_precision_decoder(const CompressedClip& clip, float sample_time, OutputWriterType& writer)
	{
		ensure(clip.get_algorithm_type() == AlgorithmType::FullPrecision);
		ensure(clip.is_valid(false));

		const uint8_t* buffer = reinterpret_cast<const uint8_t*>(&clip);
		const FullPrecisionHeader* header = reinterpret_cast<const FullPrecisionHeader*>(buffer + sizeof(CompressedClip));
		const float* track_data = reinterpret_cast<const float*>(buffer + sizeof(CompressedClip) + sizeof(FullPrecisionHeader));

		// Samples are evenly spaced, trivially calculate the indices that we need
		float clip_duration = float(header->num_samples - 1) / float(header->sample_rate);
		float normalized_sample_time = (sample_time / clip_duration);
		ensure(sample_time >= 0.0f && sample_time <= clip_duration);
		ensure(normalized_sample_time >= 0.0f && normalized_sample_time <= 1.0f);

		float sample_key = normalized_sample_time * float(header->num_samples - 1);
		uint32_t key_frame0 = uint32_t(floor(sample_key));
		uint32_t key_frame1 = std::max(key_frame0 + 1, header->num_samples - 1);
		float interpolation_alpha = sample_key - float(key_frame0);
		ensure(key_frame0 >= 0 && key_frame0 <= key_frame1 && key_frame1 < header->num_samples);
		ensure(interpolation_alpha >= 0.0f && interpolation_alpha <= 1.0f);

		uint32_t num_floats_per_key_frame = (header->num_animated_rotation_tracks * 4) + (header->num_animated_translation_tracks * 3);
		const float* key_frame_data0 = track_data + (key_frame0 * num_floats_per_key_frame);
		const float* key_frame_data1 = track_data + (key_frame1 * num_floats_per_key_frame);

		// TODO: For now, we always compress both tracks of each bone, this is wrong, some could be dropped
		uint32_t num_bones = header->num_animated_rotation_tracks;
		for (uint32_t bone_index = 0; bone_index < num_bones; ++bone_index)
		{
			Quat_32 rotation0 = quat_set(key_frame_data0[0], key_frame_data0[1], key_frame_data0[2], key_frame_data0[3]);
			Vector4_32 translation0 = vector_set(key_frame_data0[4], key_frame_data0[5], key_frame_data0[6], 0.0f);

			Quat_32 rotation1 = quat_set(key_frame_data1[0], key_frame_data1[1], key_frame_data1[2], key_frame_data1[3]);
			Vector4_32 translation1 = vector_set(key_frame_data1[4], key_frame_data1[5], key_frame_data1[6], 0.0f);

			key_frame_data0 += num_floats_per_key_frame;
			key_frame_data1 += num_floats_per_key_frame;

			Quat_32 rotation = quat_lerp(rotation0, rotation1, interpolation_alpha);
			writer.WriteBoneRotation(bone_index, rotation);

			Vector4_32 translation = vector_lerp(translation0, translation1, interpolation_alpha);
			writer.WriteBoneTranslation(bone_index, translation);
		}
	}
}
