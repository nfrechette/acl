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
#include "acl/compression/track_array.h"
#include "acl/compression/transform_adapters.h"
#include "acl/compression/transform_base_adapter.h"
#include "acl/compression/transform_space.h"
#include "acl/compression/impl/sample_streams.h"
#include "acl/compression/impl/transform_clip_adapters.h"

#include <rtm/quatf.h>
#include <rtm/scalarf.h>
#include <rtm/vqvf.h>

#include <algorithm>
#include <cstdint>
#include <type_traits>

ACL_IMPL_FILE_PRAGMA_PUSH

namespace acl
{
	ACL_IMPL_VERSION_NAMESPACE_BEGIN

	namespace acl_impl
	{
		//////////////////////////////////////////////////////////////////////////
		// Fills 'out_order' with each transform index, a parent before its children.
		// 'scratch_is_sorted' must hold one value for each transform.
		// With a parent cycle (not valid), the transforms of the cycle come last, in index order.
		inline void sort_transforms_parent_first(const uint32_t* parent_indices, uint32_t num_transforms, uint32_t* out_order, bool* scratch_is_sorted)
		{
			std::fill(scratch_is_sorted, scratch_is_sorted + num_transforms, false);

			uint32_t num_sorted = 0;
			while (num_sorted < num_transforms)
			{
				const uint32_t num_sorted_before = num_sorted;

				for (uint32_t transform_index = 0; transform_index < num_transforms; ++transform_index)
				{
					if (scratch_is_sorted[transform_index])
						continue;

					const uint32_t parent_index = parent_indices[transform_index];
					if (parent_index == k_invalid_track_index || parent_index >= num_transforms || scratch_is_sorted[parent_index])
					{
						out_order[num_sorted++] = transform_index;
						scratch_is_sorted[transform_index] = true;
					}
				}

				if (num_sorted == num_sorted_before)
				{
					// A cycle, we cannot sort the remaining transforms
					for (uint32_t transform_index = 0; transform_index < num_transforms; ++transform_index)
					{
						if (!scratch_is_sorted[transform_index])
							out_order[num_sorted++] = transform_index;
					}
				}
			}
		}

		//////////////////////////////////////////////////////////////////////////
		// Puts a local space pose in mesh space with rtm::vqv_mul.
		// 'order' lists each parent before its children, see 'sort_transforms_parent_first'.
		// The output can be the input.
		inline void local_to_mesh_space(const rtm::vqvf* local_pose, const uint32_t* parent_indices, const uint32_t* order, uint32_t num_transforms, rtm::vqvf* out_mesh_pose)
		{
			for (uint32_t order_index = 0; order_index < num_transforms; ++order_index)
			{
				const uint32_t transform_index = order[order_index];
				const uint32_t parent_index = parent_indices[transform_index];

				if (parent_index == k_invalid_track_index || parent_index >= num_transforms)
					out_mesh_pose[transform_index] = local_pose[transform_index];	// A root is in mesh space
				else
					out_mesh_pose[transform_index] = rtm::vqv_normalize(rtm::vqv_mul(local_pose[transform_index], out_mesh_pose[parent_index]));
			}
		}

		//////////////////////////////////////////////////////////////////////////
		// Returns the object transform of a transform from the object transform of its parent, with VQV arithmetic.
		// In mesh rotation space, we use the mesh rotation product, see 'transform_space_t::mesh_rotation'.
		inline rtm::vqvf RTM_SIMD_CALL mul_in_space(transform_space_t space, const rtm::vqvf& transform, const rtm::vqvf& parent_object_transform)
		{
			if (space == transform_space_t::mesh_rotation)
				return vqv_transform_math::mul_mesh_rotation(transform, parent_object_transform);

			return rtm::vqv_normalize(rtm::vqv_mul(transform, parent_object_transform));
		}

		//////////////////////////////////////////////////////////////////////////
		// Puts the rotations of a local space pose in mesh space, the translations and scales stay in local space.
		// This is the base pose of the mesh rotation space, see 'transform_space_t::mesh_rotation'.
		// 'order' lists each parent before its children, see 'sort_transforms_parent_first'.
		// The output can be the input.
		inline void local_to_mesh_rotation_space(const rtm::vqvf* local_pose, const uint32_t* parent_indices, const uint32_t* order, uint32_t num_transforms, rtm::vqvf* out_pose)
		{
			for (uint32_t order_index = 0; order_index < num_transforms; ++order_index)
			{
				const uint32_t transform_index = order[order_index];
				const uint32_t parent_index = parent_indices[transform_index];

				rtm::vqvf transform = local_pose[transform_index];
				if (parent_index != k_invalid_track_index && parent_index < num_transforms)
					transform.rotation = rtm::quat_normalize(rtm::quat_mul(transform.rotation, out_pose[parent_index].rotation));

				out_pose[transform_index] = transform;
			}
		}

		//////////////////////////////////////////////////////////////////////////
		// Puts a local space pose in a transform space: mesh or mesh rotation.
		// The local space does not change the pose.
		inline void local_to_space(transform_space_t space, const rtm::vqvf* local_pose, const uint32_t* parent_indices, const uint32_t* order, uint32_t num_transforms, rtm::vqvf* out_pose)
		{
			if (space == transform_space_t::mesh)
				local_to_mesh_space(local_pose, parent_indices, order, num_transforms, out_pose);
			else if (space == transform_space_t::mesh_rotation)
				local_to_mesh_rotation_space(local_pose, parent_indices, order, num_transforms, out_pose);
			else if (out_pose != local_pose)
				std::copy(local_pose, local_pose + num_transforms, out_pose);
		}

		//////////////////////////////////////////////////////////////////////////
		// Holds the data that compression uses to put a sample of a clip in the space
		// where it measures the error.
		//
		// It holds the transform space of the clip, the additive base of the clip, and its additive format:
		//    - the pose of each sample of the base, as rtm::vqvf, in the transform space of the clip
		//    - the sample of the base that each sample of the clip uses
		//
		// The base holds raw data. Thus we sample it one time, when we initialize the context,
		// and compression reads the base poses from here.
		// The base is in local space. In mesh and mesh rotation space, we put each base pose in that space here.
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
			//    - 'space': the transform space of the clip
			//    - 'base_format': the additive format of the clip, 'none' when the clip has no base
			//    - 'base_adapter': the base adapter of the 'custom' format, null with the other formats
			//    - 'base_clip': the clip adapter of the base, not used when the format is 'none'
			//    - 'clip_track_list': the clip that uses the base, it gives the transform hierarchy
			//    - 'clip_num_samples', 'clip_sample_rate', 'clip_duration': the clip that uses the base
			template<class clip_adapter_t>
			void initialize(iallocator& allocator, transform_space_t space, additive_clip_format8 base_format, const itransform_base_adapter_t* base_adapter,
				const clip_adapter_t& base_clip, const track_array& clip_track_list, uint32_t clip_num_samples, float clip_sample_rate, float clip_duration);

			//////////////////////////////////////////////////////////////////////////
			// Frees the memory. The context then has no base.
			void destroy();

			//////////////////////////////////////////////////////////////////////////
			// Returns the transform space of the clip.
			transform_space_t get_space() const { return m_space; }

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
				return rtm::vqv_normalize(combine_with_base(transform_index, base_transform, transform));
			}

			//////////////////////////////////////////////////////////////////////////
			// Combines a transform with a base transform and returns the result, it does not normalize it.
			// With the 'custom' format, the base adapter combines them.
			rtm::vqvf RTM_SIMD_CALL combine_with_base(uint32_t transform_index, rtm::vqvf_arg0 base_transform, rtm::vqvf_arg1 transform) const
			{
				if (m_base_format == additive_clip_format8::custom)
					return m_base_adapter->apply_base(transform_index, base_transform, transform);

				return acl::apply_additive_to_base(m_base_format, base_transform, transform);
			}

			//////////////////////////////////////////////////////////////////////////
			// Returns the base adapter of the 'custom' format, null with the other formats.
			const itransform_base_adapter_t* get_base_adapter() const { return m_base_adapter; }

		private:
			iallocator* m_allocator = nullptr;

			// One pose for each base sample, one rtm::vqvf for each transform in a pose
			rtm::vqvf* m_base_poses = nullptr;

			// One base sample index for each clip sample
			uint32_t* m_base_sample_indices = nullptr;

			// The base adapter of the 'custom' format, null with the other formats
			const itransform_base_adapter_t* m_base_adapter = nullptr;

			uint32_t m_num_transforms = 0;
			uint32_t m_num_base_samples = 0;
			uint32_t m_num_clip_samples = 0;

			additive_clip_format8 m_base_format = additive_clip_format8::none;
			transform_space_t m_space = transform_space_t::local;
		};

		template<class clip_adapter_t>
		inline void transform_space_context_t::initialize(iallocator& allocator, transform_space_t space, additive_clip_format8 base_format, const itransform_base_adapter_t* base_adapter,
			const clip_adapter_t& base_clip, const track_array& clip_track_list, uint32_t clip_num_samples, float clip_sample_rate, float clip_duration)
		{
			static_assert(std::is_base_of<transform_clip_adapter_t, clip_adapter_t>::value, "Clip adapter must derive from transform_clip_adapter_t");

			destroy();

			m_space = space;

			if (base_format == additive_clip_format8::none)
				return;	// No base, nothing to sample

			ACL_ASSERT(base_clip.is_valid(), "The base clip is not valid");
			ACL_ASSERT((base_format == additive_clip_format8::custom) == (base_adapter != nullptr), "The custom base format requires a base adapter, the other formats do not use one");

			const uint32_t num_transforms = base_clip.get_num_transforms();
			const uint32_t num_base_samples = base_clip.get_num_samples();
			const float base_duration = base_clip.get_duration();

			ACL_ASSERT(num_base_samples != 0, "The base clip has no samples");

			m_allocator = &allocator;
			m_base_format = base_format;
			m_base_adapter = base_adapter;
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

			// In mesh and mesh rotation space, the clip combines with the base in that space
			// The base has the transform hierarchy of the clip
			if (space != transform_space_t::local)
			{
				ACL_ASSERT(clip_track_list.get_num_tracks() == num_transforms, "The base and the clip must have the same number of transforms");

				uint32_t* parent_indices = allocate_type_array<uint32_t>(allocator, num_transforms);
				uint32_t* order = allocate_type_array<uint32_t>(allocator, num_transforms);
				bool* is_sorted = allocate_type_array<bool>(allocator, num_transforms);

				for (uint32_t transform_index = 0; transform_index < num_transforms; ++transform_index)
					parent_indices[transform_index] = clip_track_list[transform_index].get_description<track_desc_transformf>().parent_index;

				sort_transforms_parent_first(parent_indices, num_transforms, order, is_sorted);

				for (uint32_t base_sample_index = 0; base_sample_index < num_base_samples; ++base_sample_index)
				{
					rtm::vqvf* base_pose = m_base_poses + (size_t(base_sample_index) * num_transforms);
					local_to_space(space, base_pose, parent_indices, order, num_transforms, base_pose);
				}

				deallocate_type_array(allocator, parent_indices, num_transforms);
				deallocate_type_array(allocator, order, num_transforms);
				deallocate_type_array(allocator, is_sorted, num_transforms);
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
			m_base_adapter = nullptr;
			m_num_transforms = 0;
			m_num_base_samples = 0;
			m_num_clip_samples = 0;
			m_base_format = additive_clip_format8::none;
			m_space = transform_space_t::local;
		}
	}

	ACL_IMPL_VERSION_NAMESPACE_END
}

ACL_IMPL_FILE_PRAGMA_POP
