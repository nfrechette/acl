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

#include "acl/core/bitset.h"
#include "acl/core/compiler_utils.h"
#include "acl/core/error.h"
#include "acl/core/iallocator.h"
#include "acl/core/string.h"

#include <rtm/qvvd.h>

#include <cstdint>

ACL_IMPL_FILE_PRAGMA_PUSH

namespace acl
{
	//////////////////////////////////////////////////////////////////////////
	// We only support up to 65534 bones, we reserve 65535 for the invalid index
	constexpr uint16_t k_invalid_bone_index = 0xFFFF;

	namespace acl_impl
	{
		//////////////////////////////////////////////////////////////////////////
		// Simple iterator utility class to allow easy looping
		class BoneChainIterator
		{
		public:
			BoneChainIterator(const uint32_t* bone_chain, BitSetDescription bone_chain_desc, uint16_t bone_index, uint16_t offset)
				: m_bone_chain(bone_chain)
				, m_bone_chain_desc(bone_chain_desc)
				, m_bone_index(bone_index)
				, m_offset(offset)
			{}

			BoneChainIterator& operator++()
			{
				ACL_ASSERT(m_offset <= m_bone_index, "Cannot increment the iterator, it is no longer valid");

				// Skip the current bone
				m_offset++;

				// Iterate until we find the next bone part of the chain or until we reach the end of the chain
				// TODO: Use clz or similar to find the next set bit starting at the current index
				while (m_offset < m_bone_index && !bitset_test(m_bone_chain, m_bone_chain_desc, m_offset))
					m_offset++;

				return *this;
			}

			uint16_t operator*() const
			{
				ACL_ASSERT(m_offset <= m_bone_index, "Returned bone index doesn't belong to the bone chain");
				ACL_ASSERT(bitset_test(m_bone_chain, m_bone_chain_desc, m_offset), "Returned bone index doesn't belong to the bone chain");
				return m_offset;
			}

			// We only compare the offset in the bone chain. Two iterators on the same bone index
			// from two different or equal chains will be equal.
			bool operator==(const BoneChainIterator& other) const { return m_offset == other.m_offset; }
			bool operator!=(const BoneChainIterator& other) const { return m_offset != other.m_offset; }

		private:
			const uint32_t*		m_bone_chain;
			BitSetDescription	m_bone_chain_desc;
			uint16_t			m_bone_index;
			uint16_t			m_offset;
		};
	}

	//////////////////////////////////////////////////////////////////////////
	// Simple bone chain container to allow easy looping
	//
	// A bone chain allows looping over all bones up to a specific bone starting
	// at the root bone.
	//////////////////////////////////////////////////////////////////////////
	struct BoneChain
	{
		BoneChain(const uint32_t* bone_chain, BitSetDescription bone_chain_desc, uint16_t bone_index)
			: m_bone_chain(bone_chain)
			, m_bone_chain_desc(bone_chain_desc)
			, m_bone_index(bone_index)
		{
			// We don't know where this bone chain starts, find the root bone
			// TODO: Use clz or similar to find the next set bit starting at the current index
			uint16_t root_index = 0;
			while (!bitset_test(bone_chain, bone_chain_desc, root_index))
				root_index++;

			m_root_index = root_index;
		}

		acl_impl::BoneChainIterator begin() const { return acl_impl::BoneChainIterator(m_bone_chain, m_bone_chain_desc, m_bone_index, m_root_index); }
		acl_impl::BoneChainIterator end() const { return acl_impl::BoneChainIterator(m_bone_chain, m_bone_chain_desc, m_bone_index, m_bone_index + 1); }

		const uint32_t*		m_bone_chain;
		BitSetDescription	m_bone_chain_desc;
		uint16_t			m_root_index;
		uint16_t			m_bone_index;
	};

	//////////////////////////////////////////////////////////////////////////
	// A rigid bone description
	//
	// Bones are organized in a tree with a single root bone. Each bone has
	// one or more children and every bone except the root has a single parent.
	//////////////////////////////////////////////////////////////////////////
	struct alignas(16) RigidBone
	{
		//////////////////////////////////////////////////////////////////////////
		// Default constructor, initializes a simple root bone with no name
		RigidBone()
			: name()
			, bone_chain(nullptr)
			, vertex_distance(1.0F)
			, parent_index(k_invalid_bone_index)
			, bind_transform(rtm::qvv_identity())
		{
			(void)padding;
		}

		~RigidBone() = default;

		RigidBone(RigidBone&& other)
			: name(std::move(other.name))
			, bone_chain(other.bone_chain)
			, vertex_distance(other.vertex_distance)
			, parent_index(other.parent_index)
			, bind_transform(other.bind_transform)
		{
			new(&other) RigidBone();
		}

		RigidBone& operator=(RigidBone&& other)
		{
			std::swap(name, other.name);
			std::swap(bone_chain, other.bone_chain);
			std::swap(vertex_distance, other.vertex_distance);
			std::swap(parent_index, other.parent_index);
			std::swap(bind_transform, other.bind_transform);

			return *this;
		}

		//////////////////////////////////////////////////////////////////////////
		// Returns whether or not this bone is a root bone
		bool is_root() const { return parent_index == k_invalid_bone_index; }

		// Name of the bone (used for debugging purposes only)
		String			name;

		// A bit set, a set bit at index X indicates the bone at index X is in the chain
		// This can be used to iterate on the bone chain efficiently from root to the current bone
		const uint32_t*	bone_chain;

		// Virtual vertex distance used by hierarchical error function
		// The error metric measures the error of a virtual vertex at this
		// distance from the bone in object space
		float			vertex_distance;

		// The parent bone index or an invalid bone index for the root bone
		// TODO: Introduce a type for bone indices
		uint16_t		parent_index;

		// Unused memory left as padding
		uint8_t			padding[2];

		// The bind transform is in its parent's local space
		// Note that the scale is ignored and this value is only used by the additive error metrics
		rtm::qvvd		bind_transform;
	};

	//////////////////////////////////////////////////////////////////////////
	// A rigid skeleton made up of a tree of rigid bones
	//
	// This hierarchical structure is important and forms the back bone of the
	// error metrics. When calculating the error introduced by lowering the
	// precision of a single bone track, we will walk up the hierarchy and
	// calculate the error relative to the root bones (object/mesh space).
	//////////////////////////////////////////////////////////////////////////
	class RigidSkeleton
	{
	public:
		//////////////////////////////////////////////////////////////////////////
		// Constructs a RigidSkeleton instance and moves the data from the input
		// 'bones' into the skeleton instance (destructive operation on the input array).
		//    - allocator: The allocator instance to use to allocate and free memory
		//    - bones: An array of bones to initialize the skeleton with
		//    - num_bones: The number of input bones
		RigidSkeleton(IAllocator& allocator, RigidBone* bones, uint16_t num_bones)
			: m_allocator(allocator)
			, m_bones(allocate_type_array<RigidBone>(allocator, num_bones))
			, m_num_bones(num_bones)
		{
			// Calculate which bones are leaf bones that have no children
			BitSetDescription bone_bitset_desc = BitSetDescription::make_from_num_bits(num_bones);
			uint32_t* is_leaf_bitset = allocate_type_array<uint32_t>(allocator, bone_bitset_desc.get_size());
			bitset_reset(is_leaf_bitset, bone_bitset_desc, false);

			// By default  and if we find a child, we'll mark it as non-leaf
			bitset_set_range(is_leaf_bitset, bone_bitset_desc, 0, num_bones, true);

#if defined(ACL_HAS_ASSERT_CHECKS)
			uint32_t num_root_bones = 0;
#endif

			// Move and validate the input data
			for (uint16_t bone_index = 0; bone_index < num_bones; ++bone_index)
			{
				RigidBone& bone = bones[bone_index];

				const bool is_root = bone.parent_index == k_invalid_bone_index;

				ACL_ASSERT(bone.bone_chain == nullptr, "Bone chain should be calculated internally");
				ACL_ASSERT(is_root || bone.parent_index < bone_index, "Bones must be sorted parent first");
				ACL_ASSERT(rtm::quat_is_finite(bone.bind_transform.rotation), "Bind rotation is invalid: [%f, %f, %f, %f]", rtm::quat_get_x(bone.bind_transform.rotation), rtm::quat_get_y(bone.bind_transform.rotation), rtm::quat_get_z(bone.bind_transform.rotation), rtm::quat_get_w(bone.bind_transform.rotation));
				ACL_ASSERT(rtm::quat_is_normalized(bone.bind_transform.rotation), "Bind rotation isn't normalized: [%f, %f, %f, %f]", rtm::quat_get_x(bone.bind_transform.rotation), rtm::quat_get_y(bone.bind_transform.rotation), rtm::quat_get_z(bone.bind_transform.rotation), rtm::quat_get_w(bone.bind_transform.rotation));
				ACL_ASSERT(rtm::vector_is_finite3(bone.bind_transform.translation), "Bind translation is invalid: [%f, %f, %f]", rtm::vector_get_x(bone.bind_transform.translation), rtm::vector_get_y(bone.bind_transform.translation), rtm::vector_get_z(bone.bind_transform.translation));

				// If we have a parent, mark it as not being a leaf bone (it has at least one child)
				if (!is_root)
					bitset_set(is_leaf_bitset, bone_bitset_desc, bone.parent_index, false);

#if defined(ACL_HAS_ASSERT_CHECKS)
				if (is_root)
					num_root_bones++;
#endif

				m_bones[bone_index] = std::move(bone);

				// Input scale is ignored and always set to [1.0, 1.0, 1.0]
				m_bones[bone_index].bind_transform.scale = rtm::vector_set(1.0);
			}

			m_num_leaf_bones = safe_static_cast<uint16_t>(bitset_count_set_bits(is_leaf_bitset, bone_bitset_desc));

			m_leaf_bone_chains = allocate_type_array<uint32_t>(allocator, size_t(m_num_leaf_bones) * bone_bitset_desc.get_size());

			uint16_t leaf_index = 0;
			for (uint16_t bone_index = 0; bone_index < num_bones; ++bone_index)
			{
				if (!bitset_test(is_leaf_bitset, bone_bitset_desc, bone_index))
					continue;	// Skip non-leaf bones

				uint32_t* bone_chain = m_leaf_bone_chains + (leaf_index * bone_bitset_desc.get_size());
				bitset_reset(bone_chain, bone_bitset_desc, false);

				uint16_t chain_bone_index = bone_index;
				while (chain_bone_index != k_invalid_bone_index)
				{
					bitset_set(bone_chain, bone_bitset_desc, chain_bone_index, true);

					RigidBone& bone = m_bones[chain_bone_index];

					// We assign a bone chain the first time we find a bone that isn't part of one already
					if (bone.bone_chain == nullptr)
						bone.bone_chain = bone_chain;

					chain_bone_index = bone.parent_index;
				}

				leaf_index++;
			}

			ACL_ASSERT(num_root_bones > 0, "No root bone found. The root bones must have a parent index = 0xFFFF");
			ACL_ASSERT(leaf_index == m_num_leaf_bones, "Invalid number of leaf bone found");
			deallocate_type_array(m_allocator, is_leaf_bitset, bone_bitset_desc.get_size());
		}

		~RigidSkeleton()
		{
			deallocate_type_array(m_allocator, m_bones, m_num_bones);

			BitSetDescription bone_bitset_desc = BitSetDescription::make_from_num_bits(m_num_bones);
			deallocate_type_array(m_allocator, m_leaf_bone_chains, size_t(m_num_leaf_bones) * bone_bitset_desc.get_size());
		}

		RigidSkeleton(const RigidSkeleton&) = delete;
		RigidSkeleton& operator=(const RigidSkeleton&) = delete;

		//////////////////////////////////////////////////////////////////////////
		// Returns the array of bones contained in the skeleton
		const RigidBone* get_bones() const { return m_bones; }

		//////////////////////////////////////////////////////////////////////////
		// Returns a specific bone from its index
		const RigidBone& get_bone(uint16_t bone_index) const
		{
			ACL_ASSERT(bone_index < m_num_bones, "Invalid bone index: %u >= %u", bone_index, m_num_bones);
			return m_bones[bone_index];
		}

		//////////////////////////////////////////////////////////////////////////
		// Returns the number of bones in the skeleton
		uint16_t get_num_bones() const { return m_num_bones; }

		//////////////////////////////////////////////////////////////////////////
		// Returns a bone chain for a specific bone from its index
		BoneChain get_bone_chain(uint16_t bone_index) const
		{
			ACL_ASSERT(bone_index < m_num_bones, "Invalid bone index: %u >= %u", bone_index, m_num_bones);
			const RigidBone& bone = m_bones[bone_index];
			return BoneChain(bone.bone_chain, BitSetDescription::make_from_num_bits(m_num_bones), bone_index);
		}

	private:
		// The allocator instance used to allocate and free memory by this skeleton instance
		IAllocator&	m_allocator;

		// The array of bone data for this skeleton, contains 'm_num_bones' entries
		RigidBone*	m_bones;

		// Contiguous block of memory for the bone chains, contains m_num_leaf_bones * get_bitset_size(m_num_bones) entries
		uint32_t*	m_leaf_bone_chains;

		// Number of bones contained in this skeleton
		uint16_t	m_num_bones;

		// Number of leaf bones contained in this skeleton
		uint16_t	m_num_leaf_bones;
	};
}

ACL_IMPL_FILE_PRAGMA_POP
