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
#include "acl/core/impl/compiler_utils.h"
#include "acl/core/error_result.h"
#include "acl/core/interpolation_utils.h"
#include "acl/core/string.h"
#include "acl/core/utils.h"

#include <rtm/quatf.h>
#include <rtm/qvvf.h>
#include <rtm/vector4f.h>

#include <cstdint>

ACL_IMPL_FILE_PRAGMA_PUSH

namespace acl
{
	//////////////////////////////////////////////////////////////////////////
	// Small structure to wrap the three tracks a bone can own: rotation, translation and scale.
	struct AnimatedBone
	{
		AnimationRotationTrack		rotation_track;
		AnimationTranslationTrack	translation_track;
		AnimationScaleTrack			scale_track;

		// The bone output index. When writing out the compressed data stream, this index
		// will be used instead of the bone index. This allows custom reordering for things
		// like LOD sorting or skeleton remapping. A value of 'k_invalid_bone_index' will strip the bone
		// from the compressed data stream. Defaults to the bone index. The output index
		// must be unique and they must be contiguous.
		uint16_t						output_index;

		bool is_stripped_from_output() const { return output_index == k_invalid_bone_index; }
	};

	//////////////////////////////////////////////////////////////////////////
	// A raw animation clip.
	//
	// A clip is a collection of animated bones that map directly to a rigid skeleton.
	// Each bone has a rotation track, a translation track, and a scale track.
	// All tracks should have the same number of samples at a particular
	// sample rate.
	//
	// A clip can also have an additive base. Such clips are deemed additive in nature
	// and also have a corresponding additive format that dictates the mathematical
	// operation to add it onto its base clip.
	//
	// Instances of this class manage and own the raw animation data within.
	//////////////////////////////////////////////////////////////////////////
	class AnimationClip
	{
	public:
		//////////////////////////////////////////////////////////////////////////
		// Creates an instance and initializes it.
		//    - allocator: The allocator instance to use to allocate and free memory
		//    - skeleton: The rigid skeleton this clip is based on
		//    - num_samples: The number of samples per track
		//    - sample_rate: The rate at which samples are recorded (e.g. 30 means 30 FPS)
		//    - name: Name of the clip (used for debugging purposes only)
		AnimationClip(IAllocator& allocator, const RigidSkeleton& skeleton, uint32_t num_samples, float sample_rate, const String &name)
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
				m_bones[bone_index].output_index = bone_index;
			}
		}

		~AnimationClip()
		{
			deallocate_type_array(m_allocator, m_bones, m_num_bones);
		}

		AnimationClip(const AnimationClip&) = delete;
		AnimationClip& operator=(const AnimationClip&) = delete;

		//////////////////////////////////////////////////////////////////////////
		// Returns the rigid skeleton this clip was created with
		const RigidSkeleton& get_skeleton() const { return m_skeleton; }

		//////////////////////////////////////////////////////////////////////////
		// Returns the array of animated bone data
		AnimatedBone* get_bones() { return m_bones; }
		const AnimatedBone* get_bones() const { return m_bones; }

		//////////////////////////////////////////////////////////////////////////
		// Returns the animated bone data for the provided bone index
		AnimatedBone& get_animated_bone(uint16_t bone_index)
		{
			ACL_ASSERT(bone_index < m_num_bones, "Invalid bone index: %u >= %u", bone_index, m_num_bones);
			return m_bones[bone_index];
		}

		//////////////////////////////////////////////////////////////////////////
		// Returns the animated bone data for the provided bone index
		const AnimatedBone& get_animated_bone(uint16_t bone_index) const
		{
			ACL_ASSERT(bone_index < m_num_bones, "Invalid bone index: %u >= %u", bone_index, m_num_bones);
			return m_bones[bone_index];
		}

		//////////////////////////////////////////////////////////////////////////
		// Returns the number of bones in this clip
		uint16_t get_num_bones() const { return m_num_bones; }

		//////////////////////////////////////////////////////////////////////////
		// Returns the number of samples per track in this clip
		uint32_t get_num_samples() const { return m_num_samples; }

		//////////////////////////////////////////////////////////////////////////
		// Returns the sample rate of this clip
		float get_sample_rate() const { return m_sample_rate; }

		//////////////////////////////////////////////////////////////////////////
		// Returns the clip playback duration in seconds
		float get_duration() const { return calculate_duration(m_num_samples, m_sample_rate); }

		//////////////////////////////////////////////////////////////////////////
		// Returns the clip name
		const String& get_name() const { return m_name; }

		//////////////////////////////////////////////////////////////////////////
		// Samples a whole pose at a particular sample time
		//    - sample_time: The time at which to sample the clip
		//    - rounding_policy: The rounding policy to use when sampling
		//    - out_local_pose: An array of at least 'num_transforms' to output the data in
		//    - num_transforms: The number of transforms in the output array
		void sample_pose(float sample_time, SampleRoundingPolicy rounding_policy, rtm::qvvf* out_local_pose, uint16_t num_transforms) const
		{
			ACL_ASSERT(m_num_bones > 0, "Invalid number of bones: %u", m_num_bones);
			ACL_ASSERT(m_num_bones == num_transforms, "Number of transforms does not match the number of bones: %u != %u", num_transforms, m_num_bones);
			(void)num_transforms;

			const float clip_duration = get_duration();

			// Clamp for safety, the caller should normally handle this but in practice, it often isn't the case
			sample_time = rtm::scalar_clamp(sample_time, 0.0F, clip_duration);

			uint32_t sample_index0;
			uint32_t sample_index1;
			float interpolation_alpha;
			find_linear_interpolation_samples_with_sample_rate(m_num_samples, m_sample_rate, sample_time, rounding_policy, sample_index0, sample_index1, interpolation_alpha);

			for (uint16_t bone_index = 0; bone_index < m_num_bones; ++bone_index)
			{
				const AnimatedBone& bone = m_bones[bone_index];

				const rtm::quatf rotation0 = rtm::quat_normalize(quat_cast(bone.rotation_track.get_sample(sample_index0)));
				const rtm::quatf rotation1 = rtm::quat_normalize(quat_cast(bone.rotation_track.get_sample(sample_index1)));
				const rtm::quatf rotation = rtm::quat_lerp(rotation0, rotation1, interpolation_alpha);

				const rtm::vector4f translation0 = rtm::vector_cast(bone.translation_track.get_sample(sample_index0));
				const rtm::vector4f translation1 = rtm::vector_cast(bone.translation_track.get_sample(sample_index1));
				const rtm::vector4f translation = rtm::vector_lerp(translation0, translation1, interpolation_alpha);

				const rtm::vector4f scale0 = rtm::vector_cast(bone.scale_track.get_sample(sample_index0));
				const rtm::vector4f scale1 = rtm::vector_cast(bone.scale_track.get_sample(sample_index1));
				const rtm::vector4f scale = rtm::vector_lerp(scale0, scale1, interpolation_alpha);

				out_local_pose[bone_index] = rtm::qvv_set(rotation, translation, scale);
			}
		}

		//////////////////////////////////////////////////////////////////////////
		// Samples a whole pose at a particular sample time
		//    - sample_time: The time at which to sample the clip
		//    - out_local_pose: An array of at least 'num_transforms' to output the data in
		//    - num_transforms: The number of transforms in the output array
		void sample_pose(float sample_time, rtm::qvvf* out_local_pose, uint16_t num_transforms) const
		{
			sample_pose(sample_time, SampleRoundingPolicy::None, out_local_pose, num_transforms);
		}

		//////////////////////////////////////////////////////////////////////////
		// Returns the raw size for this clip. Note that this differs from the actual
		// memory used by an instance of this class. It is meant for comparison against
		// the compressed size.
		uint32_t get_raw_size() const
		{
			const uint32_t rotation_size = sizeof(float) * 4;		// Quat == Vector4
			const uint32_t translation_size = sizeof(float) * 3;	// Vector3
			const uint32_t scale_size = sizeof(float) * 3;			// Vector3
			const uint32_t bone_sample_size = rotation_size + translation_size + scale_size;
			return uint32_t(m_num_bones) * bone_sample_size * m_num_samples;
		}

		//////////////////////////////////////////////////////////////////////////
		// Sets the base animation clip and marks this instance as an additive clip of the provided format
		void set_additive_base(const AnimationClip* base_clip, AdditiveClipFormat8 additive_format) { m_additive_base_clip = base_clip; m_additive_format = additive_format; }

		//////////////////////////////////////////////////////////////////////////
		// Returns the additive base clip, if any
		const AnimationClip* get_additive_base() const { return m_additive_base_clip; }

		//////////////////////////////////////////////////////////////////////////
		// Returns the additive format of this clip, if any
		AdditiveClipFormat8 get_additive_format() const { return m_additive_format; }

		//////////////////////////////////////////////////////////////////////////
		// Checks if the instance of this clip is valid and returns an error if it isn't
		ErrorResult is_valid() const
		{
			if (m_num_bones == 0)
				return ErrorResult("Clip has no bones");

			if (m_num_samples == 0)
				return ErrorResult("Clip has no samples");

			if (m_num_samples == 0xFFFFFFFFU)
				return ErrorResult("Clip has too many samples");

			if (m_sample_rate <= 0.0F)
				return ErrorResult("Clip has an invalid sample rate");

			uint16_t num_output_bones = 0;
			for (uint16_t bone_index = 0; bone_index < m_num_bones; ++bone_index)
			{
				const uint16_t output_index = m_bones[bone_index].output_index;
				if (output_index != k_invalid_bone_index && output_index >= m_num_bones)
					return ErrorResult("The output_index must be 'k_invalid_bone_index' or less than the number of bones");

				if (output_index != k_invalid_bone_index)
				{
					for (uint16_t bone_index2 = bone_index + 1; bone_index2 < m_num_bones; ++bone_index2)
					{
						if (output_index == m_bones[bone_index2].output_index)
							return ErrorResult("Duplicate output_index found");
					}

					num_output_bones++;
				}
			}

			for (uint16_t output_index = 0; output_index < num_output_bones; ++output_index)
			{
				bool found = false;
				for (uint16_t bone_index = 0; bone_index < m_num_bones; ++bone_index)
				{
					if (output_index == m_bones[bone_index].output_index)
					{
						found = true;
						break;
					}
				}

				if (!found)
					return ErrorResult("Output indices are not contiguous");
			}

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

		//////////////////////////////////////////////////////////////////////////
		// Returns whether this clip has scale or not. A clip has scale if at least one
		// bone has a scale sample that isn't equivalent to the default scale.
		bool has_scale(float threshold) const
		{
			const rtm::vector4f default_scale = get_default_scale(m_additive_format);

			for (uint16_t bone_index = 0; bone_index < m_num_bones; ++bone_index)
			{
				const AnimatedBone& bone = m_bones[bone_index];
				const uint32_t num_samples = bone.scale_track.get_num_samples();
				if (num_samples != 0)
				{
					const rtm::vector4f scale = rtm::vector_cast(bone.scale_track.get_sample(0));

					rtm::vector4f min = scale;
					rtm::vector4f max = scale;

					for (uint32_t sample_index = 1; sample_index < num_samples; ++sample_index)
					{
						const rtm::vector4f sample = rtm::vector_cast(bone.scale_track.get_sample(sample_index));

						min = rtm::vector_min(min, sample);
						max = rtm::vector_max(max, sample);
					}

					const rtm::vector4f extent = rtm::vector_sub(max, min);
					const bool is_constant = rtm::vector_all_less_than3(rtm::vector_abs(extent), rtm::vector_set(threshold));
					if (!is_constant)
						return true;	// Not constant means we have scale

					const bool is_default = rtm::vector_all_near_equal3(scale, default_scale, threshold);
					if (!is_default)
						return true;	// Constant but not default means we have scale
				}
			}

			// We have no tracks with non-default scale
			return false;
		}

	private:
		// The allocator instance used to allocate and free memory by this clip instance
		IAllocator&				m_allocator;

		// The rigid skeleton this clip is based on
		const RigidSkeleton&	m_skeleton;

		// The array of animated bone data. There are 'm_num_bones' entries
		AnimatedBone*			m_bones;

		// The number of samples per animated track
		uint32_t				m_num_samples;

		// The rate at which the samples were recorded
		float					m_sample_rate;

		// The number of bones in this clip
		uint16_t				m_num_bones;

		// The optional clip the current additive clip is based on
		const AnimationClip*	m_additive_base_clip;

		// If we have an additive base, this is the format we are in
		AdditiveClipFormat8		m_additive_format;

		// The name of the clip
		String					m_name;
	};

	//////////////////////////////////////////////////////////////////////////
	// Allocates an array of integers that correspond to the output bone mapping: result[output_index] = bone_index
	//    - allocator: The allocator instance to use to allocate and free memory
	//    - clip: The animation clip that dictates the bone output
	//    - out_num_output_bones: The number of output bones
	//////////////////////////////////////////////////////////////////////////
	inline uint16_t* create_output_bone_mapping(IAllocator& allocator, const AnimationClip& clip, uint16_t& out_num_output_bones)
	{
		const uint16_t num_bones = clip.get_num_bones();
		const AnimatedBone* bones = clip.get_bones();
		uint16_t num_output_bones = num_bones;
		for (uint16_t bone_index = 0; bone_index < num_bones; ++bone_index)
		{
			if (bones[bone_index].is_stripped_from_output())
				num_output_bones--;
		}

		uint16_t* output_bone_mapping = allocate_type_array<uint16_t>(allocator, num_output_bones);
		for (uint16_t bone_index = 0; bone_index < num_bones; ++bone_index)
		{
			const uint16_t output_index = bones[bone_index].output_index;
			if (output_index != k_invalid_bone_index)
				output_bone_mapping[output_index] = bone_index;
		}

		out_num_output_bones = num_output_bones;
		return output_bone_mapping;
	}
}

ACL_IMPL_FILE_PRAGMA_POP
