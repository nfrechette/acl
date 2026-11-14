#pragma once

////////////////////////////////////////////////////////////////////////////////
// The MIT License (MIT)
//
// Copyright (c) 2026 Nicholas Frechette & Animation Compression Library contributors
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

#include "acl/version.h"
#include "acl/core/additive_utils.h"
#include "acl/core/error.h"
#include "acl/core/iallocator.h"
#include "acl/core/impl/compiler_utils.h"
#include "acl/compression/impl/sample_streams.h"
#include "acl/compression/impl/transform_clip_adapters.h"

#include <rtm/scalarf.h>
#include <rtm/vqvf.h>

#include <cstdint>
#include <type_traits>

ACL_IMPL_FILE_PRAGMA_PUSH

namespace acl
{
	ACL_IMPL_VERSION_NAMESPACE_BEGIN

	namespace acl_impl
	{
		//////////////////////////////////////////////////////////////////////////
		// Holds the data that compression uses to put a sample of a clip in the space
		// where it measures the error.
		//
		// At the moment, it holds the additive base of a clip and its additive format:
		//    - the pose of each sample of the base, as rtm::vqvf
		//    - the sample of the base that each sample of the clip uses
		//
		// The base holds raw data. Thus we sample it one time, when we initialize the context,
		// and compression reads the base poses from here.
		//
		// A sample of the clip uses the sample of the base at the same normalized time,
		// rounded to the nearest sample. We do not interpolate.
		//////////////////////////////////////////////////////////////////////////
		class transform_space_context_t
		{
		public:
			transform_space_context_t() = default;
			~transform_space_context_t() { destroy(); }

			// We own memory, we cannot be copied
			transform_space_context_t(const transform_space_context_t&) = delete;
			transform_space_context_t& operator=(const transform_space_context_t&) = delete;

			//////////////////////////////////////////////////////////////////////////
			// Samples the base and finds the base sample of each clip sample.
			//    - 'base_format': the additive format of the clip, 'none' when the clip has no base
			//    - 'base_clip': the clip adapter of the base, not used when the format is 'none'
			//    - 'clip_num_samples', 'clip_sample_rate', 'clip_duration': the clip that uses the base
			template<class clip_adapter_t>
			void initialize(iallocator& allocator, additive_clip_format8 base_format, const clip_adapter_t& base_clip,
				uint32_t clip_num_samples, float clip_sample_rate, float clip_duration);

			//////////////////////////////////////////////////////////////////////////
			// Frees the memory. The context then has no base.
			void destroy();

			//////////////////////////////////////////////////////////////////////////
			// Returns whether or not the clip has a base.
			bool has_base() const { return m_base_format != additive_clip_format8::none; }

			//////////////////////////////////////////////////////////////////////////
			// Returns the additive format of the clip, 'none' when the clip has no base.
			additive_clip_format8 get_base_format() const { return m_base_format; }

			//////////////////////////////////////////////////////////////////////////
			// Returns the number of samples in the base.
			uint32_t get_num_base_samples() const { return m_num_base_samples; }

			//////////////////////////////////////////////////////////////////////////
			// Returns the base sample that a clip sample uses.
			uint32_t get_base_sample_index(uint32_t clip_sample_index) const
			{
				ACL_ASSERT(has_base(), "The clip has no base");
				ACL_ASSERT(clip_sample_index < m_num_clip_samples, "Invalid clip sample index");
				return m_base_sample_indices[clip_sample_index];
			}

			//////////////////////////////////////////////////////////////////////////
			// Returns the base pose of a base sample: one rtm::vqvf for each transform.
			const rtm::vqvf* get_base_pose_at_base_sample(uint32_t base_sample_index) const
			{
				ACL_ASSERT(has_base(), "The clip has no base");
				ACL_ASSERT(base_sample_index < m_num_base_samples, "Invalid base sample index");
				return m_base_poses + (size_t(base_sample_index) * m_num_transforms);
			}

			//////////////////////////////////////////////////////////////////////////
			// Returns the base pose that a clip sample uses: one rtm::vqvf for each transform.
			const rtm::vqvf* get_base_pose(uint32_t clip_sample_index) const
			{
				return get_base_pose_at_base_sample(get_base_sample_index(clip_sample_index));
			}

			//////////////////////////////////////////////////////////////////////////
			// Applies the base to a transform of a clip sample and returns the normalized result.
			rtm::vqvf RTM_SIMD_CALL apply_base(uint32_t clip_sample_index, uint32_t transform_index, rtm::vqvf_arg0 transform) const
			{
				ACL_ASSERT(transform_index < m_num_transforms, "Invalid transform index");
				const rtm::vqvf& base_transform = get_base_pose(clip_sample_index)[transform_index];
				return rtm::vqv_normalize(acl::apply_additive_to_base(m_base_format, base_transform, transform));
			}

		private:
			iallocator* m_allocator = nullptr;

			// One pose for each base sample, one rtm::vqvf for each transform in a pose
			rtm::vqvf* m_base_poses = nullptr;

			// One base sample index for each clip sample
			uint32_t* m_base_sample_indices = nullptr;

			uint32_t m_num_transforms = 0;
			uint32_t m_num_base_samples = 0;
			uint32_t m_num_clip_samples = 0;

			additive_clip_format8 m_base_format = additive_clip_format8::none;
		};

		template<class clip_adapter_t>
		inline void transform_space_context_t::initialize(iallocator& allocator, additive_clip_format8 base_format, const clip_adapter_t& base_clip,
			uint32_t clip_num_samples, float clip_sample_rate, float clip_duration)
		{
			static_assert(std::is_base_of<transform_clip_adapter_t, clip_adapter_t>::value, "Clip adapter must derive from transform_clip_adapter_t");

			destroy();

			if (base_format == additive_clip_format8::none)
				return;	// No base, nothing to sample

			ACL_ASSERT(base_clip.is_valid(), "The base clip is not valid");

			const uint32_t num_transforms = base_clip.get_num_transforms();
			const uint32_t num_base_samples = base_clip.get_num_samples();
			const float base_duration = base_clip.get_duration();

			ACL_ASSERT(num_base_samples != 0, "The base clip has no samples");

			m_allocator = &allocator;
			m_base_format = base_format;
			m_num_transforms = num_transforms;
			m_num_base_samples = num_base_samples;
			m_num_clip_samples = clip_num_samples;
			m_base_poses = allocate_type_array<rtm::vqvf>(allocator, size_t(num_base_samples) * num_transforms);
			m_base_sample_indices = allocate_type_array<uint32_t>(allocator, clip_num_samples);

			// Sample the base
			for (uint32_t base_sample_index = 0; base_sample_index < num_base_samples; ++base_sample_index)
			{
				rtm::vqvf* base_pose = m_base_poses + (size_t(base_sample_index) * num_transforms);

				for (uint32_t transform_index = 0; transform_index < num_transforms; ++transform_index)
				{
					const rtm::quatf rotation = base_clip.get_transform_rotation(transform_index, base_sample_index);
					const rtm::vector4f translation = base_clip.get_transform_translation(transform_index, base_sample_index);
					const rtm::vector4f scale = base_clip.get_transform_scale(transform_index, base_sample_index);
					base_pose[transform_index] = rtm::vqv_set(rotation, translation, scale);
				}
			}

			// Find the base sample of each clip sample
			for (uint32_t clip_sample_index = 0; clip_sample_index < clip_num_samples; ++clip_sample_index)
			{
				// The sample time is calculated from the full clip duration to be consistent with decompression
				const float sample_time = rtm::scalar_min(float(clip_sample_index) / clip_sample_rate, clip_duration);

				const float normalized_sample_time = num_base_samples > 1 ? (sample_time / clip_duration) : 0.0F;
				const float base_sample_time = num_base_samples > 1 ? (normalized_sample_time * base_duration) : 0.0F;

				// With uniform sample distributions, we do not interpolate.
				m_base_sample_indices[clip_sample_index] = get_uniform_sample_key(base_clip, transform_segment_adapter_t(), base_sample_time);
			}
		}

		inline void transform_space_context_t::destroy()
		{
			if (m_allocator != nullptr)
			{
				deallocate_type_array(*m_allocator, m_base_poses, size_t(m_num_base_samples) * m_num_transforms);
				deallocate_type_array(*m_allocator, m_base_sample_indices, m_num_clip_samples);
			}

			m_allocator = nullptr;
			m_base_poses = nullptr;
			m_base_sample_indices = nullptr;
			m_num_transforms = 0;
			m_num_base_samples = 0;
			m_num_clip_samples = 0;
			m_base_format = additive_clip_format8::none;
		}
	}

	ACL_IMPL_VERSION_NAMESPACE_END
}

ACL_IMPL_FILE_PRAGMA_POP
