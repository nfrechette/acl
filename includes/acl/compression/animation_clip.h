#pragma once

#include "acl/compression/animation_track.h"

#include <stdint.h>
#include <string.h>

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
		AnimationClip(Allocator& allocator, RigidSkeleton& skeleton, uint32_t num_samples, uint32_t sample_rate)
			: m_allocator(allocator)
			, m_skeleton(skeleton)
			, m_bones()
			, m_sample_rate(sample_rate)
		{
			const uint16_t num_bones = skeleton.get_num_bones();

			m_bones = allocate_type_array<AnimatedBone>(allocator, num_bones);

			for (uint32_t bone_index = 0; bone_index < num_bones; ++bone_index)
			{
				m_bones[bone_index].rotation_track = AnimationRotationTrack(allocator, num_samples);
				m_bones[bone_index].translation_track = AnimationTranslationTrack(allocator, num_samples);
			}
		}

		~AnimationClip()
		{
			const uint16_t num_bones = m_skeleton.get_num_bones();
			for (uint32_t bone_index = 0; bone_index < num_bones; ++bone_index)
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

		const AnimatedBone& get_animated_bone(uint16_t bone_index) const { ensure(bone_index < get_num_bones()); return m_bones[bone_index]; }

		uint16_t get_num_bones() const { return m_skeleton.get_num_bones(); }
		uint32_t get_num_samples() const { return m_skeleton.get_num_bones() != 0 ? m_bones[0].rotation_track.get_num_samples() : 0; }
		uint32_t get_sample_rate() const { return m_sample_rate; }

	private:
		Allocator&			m_allocator;
		RigidSkeleton&		m_skeleton;

		AnimatedBone*		m_bones;

		uint32_t			m_sample_rate;
	};
}
