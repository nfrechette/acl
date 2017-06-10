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
		AnimationClip(Allocator& allocator, std::shared_ptr<RigidSkeleton> skeleton, uint32_t num_samples, uint32_t sample_rate)
			: m_allocator(allocator)
			, m_skeleton(skeleton)
			, m_bones()
			, m_num_samples(num_samples)
			, m_sample_rate(sample_rate)
		{
			const uint16_t num_bones = skeleton->get_num_bones();

			m_bones = allocate_type_array<AnimatedBone>(allocator, num_bones);

			for (uint16_t bone_index = 0; bone_index < num_bones; ++bone_index)
			{
				m_bones[bone_index].rotation_track = AnimationRotationTrack(allocator, num_samples, sample_rate);
				m_bones[bone_index].translation_track = AnimationTranslationTrack(allocator, num_samples, sample_rate);
			}
		}

		~AnimationClip()
		{
			const uint16_t num_bones = m_skeleton->get_num_bones();
			for (uint16_t bone_index = 0; bone_index < num_bones; ++bone_index)
			{
				m_bones[bone_index].rotation_track.~AnimationRotationTrack();
				m_bones[bone_index].translation_track.~AnimationTranslationTrack();
			}

			m_allocator.deallocate(m_bones);
		}

		AnimationClip(const AnimationClip&) = delete;
		AnimationClip& operator=(const AnimationClip&) = delete;

		AnimatedBone* get_bones() { return m_bones; }
		const AnimatedBone* get_bones() const { return m_bones; }

		const AnimatedBone& get_animated_bone(uint16_t bone_index) const
		{
			ACL_ENSURE(bone_index < get_num_bones(), "Invalid bone index: %u >= %u", bone_index, get_num_bones());
			return m_bones[bone_index];
		}

		uint16_t get_num_bones() const { return m_skeleton->get_num_bones(); }
		uint32_t get_num_samples() const { return m_num_samples; }
		uint32_t get_sample_rate() const { return m_sample_rate; }
		double get_duration() const
		{
			ACL_ENSURE(m_sample_rate > 0, "Invalid sample rate: %u", m_sample_rate);
			return (m_num_samples - 1) * (1.0 / m_sample_rate);
		}

		template<class OutputWriterType>
		void sample_pose(double sample_time, OutputWriterType& writer) const
		{
			uint16_t num_bones = get_num_bones();
			ACL_ENSURE(num_bones > 0, "Invalid number of bones: %u", num_bones);

			double clip_duration = get_duration();

			uint32_t sample_frame0;
			uint32_t sample_frame1;
			double interpolation_alpha;
			calculate_interpolation_keys(m_num_samples, clip_duration, sample_time, sample_frame0, sample_frame1, interpolation_alpha);

			for (uint16_t bone_index = 0; bone_index < num_bones; ++bone_index)
			{
				const AnimatedBone& bone = m_bones[bone_index];

				Quat_64 rotation0 = bone.rotation_track.get_sample(sample_frame0);
				Quat_64 rotation1 = bone.rotation_track.get_sample(sample_frame1);
				Quat_64 rotation = quat_lerp(rotation0, rotation1, interpolation_alpha);
				writer.write_bone_rotation(bone_index, rotation);

				Vector4_64 translation0 = bone.translation_track.get_sample(sample_frame0);
				Vector4_64 translation1 = bone.translation_track.get_sample(sample_frame1);
				Vector4_64 translation = vector_lerp(translation0, translation1, interpolation_alpha);
				writer.write_bone_translation(bone_index, translation);
			}
		}

		uint32_t get_raw_size() const
		{
			uint32_t bone_sample_size = (sizeof(float) * 4) + (sizeof(float) * 3);
			return m_skeleton->get_num_bones() * bone_sample_size * m_num_samples;
		}

		uint32_t get_num_animated_tracks() const
		{
			uint32_t num_animated_tracks = 0;

			for (uint16_t bone_index = 0; bone_index < m_skeleton->get_num_bones(); ++bone_index)
			{
				num_animated_tracks += m_bones[bone_index].rotation_track.is_animated() ? 1 : 0;
				num_animated_tracks += m_bones[bone_index].translation_track.is_animated() ? 1 : 0;
			}

			return num_animated_tracks;
		}

	private:
		Allocator&						m_allocator;
		std::shared_ptr<RigidSkeleton>	m_skeleton;

		AnimatedBone*					m_bones;

		uint32_t						m_num_samples;
		uint32_t						m_sample_rate;
	};
}
