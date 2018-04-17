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
#include "acl/core/additive_utils.h"
#include "acl/core/error_result.h"
#include "acl/core/interpolation_utils.h"
#include "acl/core/string.h"
#include "acl/core/utils.h"
#include "acl/math/quat_32.h"
#include "acl/math/vector4_32.h"
#include "acl/math/transform_32.h"

#include <cstdint>

namespace acl
{
	struct AnimatedBone
	{
		AnimationRotationTrack		rotation_track;
		AnimationTranslationTrack	translation_track;
		AnimationScaleTrack			scale_track;
	};

	class AnimationClip
	{
	public:
		AnimationClip(IAllocator& allocator, const RigidSkeleton& skeleton, uint32_t num_samples, uint32_t sample_rate, const String &name)
			: m_allocator(allocator)
			, m_skeleton(skeleton)
			, m_bones()
			, m_num_samples(num_samples)
			, m_sample_rate(sample_rate)
			, m_num_bones(skeleton.get_num_bones())
			, m_additive_base_clip(nullptr)
			, m_additive_format(AdditiveClipFormat8::None)
			, m_name(allocator, name)
		{
			m_bones = allocate_type_array<AnimatedBone>(allocator, m_num_bones);

			for (uint16_t bone_index = 0; bone_index < m_num_bones; ++bone_index)
			{
				m_bones[bone_index].rotation_track = AnimationRotationTrack(allocator, num_samples, sample_rate);
				m_bones[bone_index].translation_track = AnimationTranslationTrack(allocator, num_samples, sample_rate);
				m_bones[bone_index].scale_track = AnimationScaleTrack(allocator, num_samples, sample_rate);
			}
		}

		~AnimationClip()
		{
			deallocate_type_array(m_allocator, m_bones, m_num_bones);
		}

		AnimationClip(const AnimationClip&) = delete;
		AnimationClip& operator=(const AnimationClip&) = delete;

		const RigidSkeleton& get_skeleton() const { return m_skeleton; }

		AnimatedBone* get_bones() { return m_bones; }
		const AnimatedBone* get_bones() const { return m_bones; }

		const AnimatedBone& get_animated_bone(uint16_t bone_index) const
		{
			ACL_ASSERT(bone_index < m_num_bones, "Invalid bone index: %u >= %u", bone_index, m_num_bones);
			return m_bones[bone_index];
		}

		uint16_t get_num_bones() const { return m_num_bones; }
		uint32_t get_num_samples() const { return m_num_samples; }
		uint32_t get_sample_rate() const { return m_sample_rate; }
		float get_duration() const { return calculate_duration(m_num_samples, m_sample_rate); }
		const String& get_name() const { return m_name; }

		void sample_pose(float sample_time, Transform_32* out_local_pose, uint16_t num_transforms) const
		{
			ACL_ASSERT(m_num_bones > 0, "Invalid number of bones: %u", m_num_bones);
			ACL_ASSERT(m_num_bones == num_transforms, "Number of transforms does not match the number of bones: %u != %u", num_transforms, m_num_bones);

			const float clip_duration = get_duration();

			uint32_t sample_index0;
			uint32_t sample_index1;
			float interpolation_alpha;
			find_linear_interpolation_samples(m_num_samples, clip_duration, sample_time, SampleRoundingPolicy::None, sample_index0, sample_index1, interpolation_alpha);

			for (uint16_t bone_index = 0; bone_index < m_num_bones; ++bone_index)
			{
				const AnimatedBone& bone = m_bones[bone_index];

				const Quat_32 rotation0 = quat_normalize(quat_cast(bone.rotation_track.get_sample(sample_index0)));
				const Quat_32 rotation1 = quat_normalize(quat_cast(bone.rotation_track.get_sample(sample_index1)));
				const Quat_32 rotation = quat_lerp(rotation0, rotation1, interpolation_alpha);

				const Vector4_32 translation0 = vector_cast(bone.translation_track.get_sample(sample_index0));
				const Vector4_32 translation1 = vector_cast(bone.translation_track.get_sample(sample_index1));
				const Vector4_32 translation = vector_lerp(translation0, translation1, interpolation_alpha);

				const Vector4_32 scale0 = vector_cast(bone.scale_track.get_sample(sample_index0));
				const Vector4_32 scale1 = vector_cast(bone.scale_track.get_sample(sample_index1));
				const Vector4_32 scale = vector_lerp(scale0, scale1, interpolation_alpha);

				out_local_pose[bone_index] = transform_set(rotation, translation, scale);
			}
		}

		uint32_t get_raw_size() const
		{
			const uint32_t rotation_size = sizeof(float) * 4;		// Quat == Vector4
			const uint32_t translation_size = sizeof(float) * 3;	// Vector3
			const uint32_t scale_size = sizeof(float) * 3;			// Vector3
			const uint32_t bone_sample_size = rotation_size + translation_size + scale_size;
			return uint32_t(m_num_bones) * bone_sample_size * m_num_samples;
		}

		void set_additive_base(const AnimationClip* base_clip, AdditiveClipFormat8 additive_format) { m_additive_base_clip = base_clip; m_additive_format = additive_format; }
		const AnimationClip* get_additive_base() const { return m_additive_base_clip; }
		AdditiveClipFormat8 get_additive_format() const { return m_additive_format; }

		ErrorResult is_valid() const
		{
			if (m_num_bones == 0)
				return ErrorResult("Clip has no bones");

			if (m_num_samples == 0)
				return ErrorResult("Clip has no samples");

			if (m_additive_base_clip != nullptr)
			{
				if (m_num_bones != m_additive_base_clip->get_num_bones())
					return ErrorResult("The number of bones does not match between the clip and its additive base");

				if (&m_skeleton != &m_additive_base_clip->get_skeleton())
					return ErrorResult("The RigidSkeleton differs between the clip and its additive base");

				return m_additive_base_clip->is_valid();
			}

			return ErrorResult();
		}

	private:
		IAllocator&				m_allocator;

		const RigidSkeleton&	m_skeleton;

		AnimatedBone*			m_bones;

		uint32_t				m_num_samples;
		uint32_t				m_sample_rate;
		uint16_t				m_num_bones;

		const AnimationClip*	m_additive_base_clip;
		AdditiveClipFormat8		m_additive_format;

		String					m_name;
	};
}
