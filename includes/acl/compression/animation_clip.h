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

#include "acl/compression/animation_track.h"
#include "acl/compression/skeleton.h"
#include "acl/core/string.h"
#include "acl/math/quat_32.h"
#include "acl/math/vector4_32.h"
#include "acl/math/transform_32.h"

#include <stdint.h>

namespace acl
{
	struct AnimatedBone
	{
		AnimationRotationTrack		rotation_track;
		AnimationTranslationTrack	translation_track;
	};

	class AnimationClip
	{
	public:
		AnimationClip(Allocator& allocator, const RigidSkeleton& skeleton, uint32_t num_samples, uint32_t sample_rate, const String &name, float error_threshold)
			: m_allocator(allocator)
			, m_bones()
			, m_error_threshold(error_threshold)
			, m_num_samples(num_samples)
			, m_sample_rate(sample_rate)
			, m_num_bones(skeleton.get_num_bones())
			, m_name(allocator, name)
		{
			m_bones = allocate_type_array<AnimatedBone>(allocator, m_num_bones);

			for (uint16_t bone_index = 0; bone_index < m_num_bones; ++bone_index)
			{
				m_bones[bone_index].rotation_track = AnimationRotationTrack(allocator, num_samples, sample_rate);
				m_bones[bone_index].translation_track = AnimationTranslationTrack(allocator, num_samples, sample_rate);
			}
		}

		~AnimationClip()
		{
			deallocate_type_array(m_allocator, m_bones, m_num_bones);
		}

		AnimationClip(const AnimationClip&) = delete;
		AnimationClip& operator=(const AnimationClip&) = delete;

		AnimatedBone* get_bones() { return m_bones; }
		const AnimatedBone* get_bones() const { return m_bones; }

		const AnimatedBone& get_animated_bone(uint16_t bone_index) const
		{
			ACL_ENSURE(bone_index < m_num_bones, "Invalid bone index: %u >= %u", bone_index, m_num_bones);
			return m_bones[bone_index];
		}

		uint16_t get_num_bones() const { return m_num_bones; }
		uint32_t get_num_samples() const { return m_num_samples; }
		uint32_t get_sample_rate() const { return m_sample_rate; }
		float get_duration() const
		{
			ACL_ENSURE(m_sample_rate > 0, "Invalid sample rate: %u", m_sample_rate);
			return float(m_num_samples - 1) / float(m_sample_rate);
		}
		const String& get_name() const { return m_name; }
		float get_error_threshold() const { return m_error_threshold; }

		void sample_pose(float sample_time, Transform_32* out_local_pose, uint16_t num_transforms) const
		{
			uint16_t num_bones = get_num_bones();
			ACL_ENSURE(num_bones > 0, "Invalid number of bones: %u", num_bones);
			ACL_ENSURE(num_bones == num_transforms, "Number of transforms does not match the number of bones: %u != %u", num_transforms, num_bones);

			float clip_duration = get_duration();

			uint32_t sample_frame0;
			uint32_t sample_frame1;
			float interpolation_alpha;
			calculate_interpolation_keys(m_num_samples, clip_duration, sample_time, sample_frame0, sample_frame1, interpolation_alpha);

			for (uint16_t bone_index = 0; bone_index < num_bones; ++bone_index)
			{
				const AnimatedBone& bone = m_bones[bone_index];

				Quat_32 rotation0 = quat_cast(bone.rotation_track.get_sample(sample_frame0));
				Quat_32 rotation1 = quat_cast(bone.rotation_track.get_sample(sample_frame1));
				Quat_32 rotation = quat_normalize(quat_lerp(rotation0, rotation1, interpolation_alpha));

				Vector4_32 translation0 = vector_cast(bone.translation_track.get_sample(sample_frame0));
				Vector4_32 translation1 = vector_cast(bone.translation_track.get_sample(sample_frame1));
				Vector4_32 translation = vector_lerp(translation0, translation1, interpolation_alpha);

				out_local_pose[bone_index] = transform_set(rotation, translation);
			}
		}

		uint32_t get_total_size() const
		{
			uint32_t bone_sample_size = (sizeof(float) * 4) + (sizeof(float) * 3);
			return m_num_bones * bone_sample_size * m_num_samples;
		}

	private:
		Allocator&				m_allocator;

		AnimatedBone*			m_bones;

		float					m_error_threshold;
		uint32_t				m_num_samples;
		uint32_t				m_sample_rate;
		uint16_t				m_num_bones;

		String					m_name;
	};
}
