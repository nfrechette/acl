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
#include "acl/core/error.h"
#include "acl/core/iallocator.h"
#include "acl/core/string.h"
#include "acl/math/transform_64.h"

#include <cstdint>

namespace acl
{
	constexpr uint16_t k_invalid_bone_index = 0xFFFF;

	namespace impl
	{
		class BoneChainIterator
		{
		public:
			// Root bone is always part of the current chain, default offset is our root bone
			BoneChainIterator(const uint32_t* bone_chain, BitSetDescription bone_chain_desc, uint16_t bone_index, uint16_t offset = 0)
				: m_bone_chain(bone_chain)
				, m_bone_chain_desc(bone_chain_desc)
				, m_bone_index(bone_index)
				, m_offset(offset)
			{}

			BoneChainIterator& operator++()
			{
				ACL_ENSURE(m_offset <= m_bone_index, "Cannot increment the iterator, it is no longer valid");

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
				ACL_ENSURE(m_offset <= m_bone_index, "Returned bone index doesn't belong to the bone chain");
				ACL_ENSURE(bitset_test(m_bone_chain, m_bone_chain_desc, m_offset), "Returned bone index doesn't belong to the bone chain");
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

	struct BoneChain
	{
		constexpr BoneChain(const uint32_t* bone_chain, BitSetDescription bone_chain_desc, uint16_t bone_index)
			: m_bone_chain(bone_chain)
			, m_bone_chain_desc(bone_chain_desc)
			, m_bone_index(bone_index)
		{}

		impl::BoneChainIterator begin() const { return impl::BoneChainIterator(m_bone_chain, m_bone_chain_desc, m_bone_index); }
		impl::BoneChainIterator end() const { return impl::BoneChainIterator(m_bone_chain, m_bone_chain_desc, m_bone_index, m_bone_index + 1); }

		const uint32_t*		m_bone_chain;
		BitSetDescription	m_bone_chain_desc;
		uint16_t			m_bone_index;
	};

	struct RigidBone
	{
		RigidBone()
			: name()
			, bone_chain(nullptr)
			, bind_transform(transform_identity_64())
			, vertex_distance(1.0)
			, parent_index(k_invalid_bone_index)
		{
		}

		RigidBone(RigidBone&& other)
			: name(std::move(other.name))
			, bone_chain(other.bone_chain)
			, bind_transform(other.bind_transform)
			, vertex_distance(other.vertex_distance)
			, parent_index(other.parent_index)
		{
			new(&other) RigidBone();
		}

		RigidBone& operator=(RigidBone&& other)
		{
			std::swap(name, other.name);
			std::swap(bone_chain, other.bone_chain);
			std::swap(bind_transform, other.bind_transform);
			std::swap(vertex_distance, other.vertex_distance);
			std::swap(parent_index, other.parent_index);

			return *this;
		}

		bool is_root() const { return parent_index == k_invalid_bone_index; }

		String			name;

		// A bit set, a set bit at index X indicates the bone at index X is in the chain
		// This can be used to iterate on the bone chain efficiently from root to the current bone
		const uint32_t*	bone_chain;

		Transform_64	bind_transform;		// Bind transform is in parent bone local space
		double			vertex_distance;	// Virtual vertex distance used by hierarchical error function
		uint16_t		parent_index;		// TODO: Introduce a type for bone indices
	};

	class RigidSkeleton
	{
	public:
		RigidSkeleton(IAllocator& allocator, RigidBone* bones, uint16_t num_bones)
			: m_allocator(allocator)
			, m_bones(allocate_type_array<RigidBone>(allocator, num_bones))
			, m_num_bones(num_bones)
		{
			BitSetDescription bone_bitset_desc = BitSetDescription::make_from_num_bits(num_bones);
			uint32_t* is_leaf_bitset = allocate_type_array<uint32_t>(allocator, bone_bitset_desc.get_size());
			bitset_reset(is_leaf_bitset, bone_bitset_desc, false);
			bitset_set_range(is_leaf_bitset, bone_bitset_desc, 0, num_bones, true);

			// Move and validate the input data
			bool found_root = false;
			for (uint16_t bone_index = 0; bone_index < num_bones; ++bone_index)
			{
				RigidBone& bone = bones[bone_index];

				const bool is_root = bone.parent_index == k_invalid_bone_index;

				ACL_ENSURE(bone.bone_chain == nullptr, "Bone chain should be calculated internally");
				ACL_ENSURE(is_root || bone.parent_index < bone_index, "Bones must be sorted parent first");
				ACL_ENSURE((is_root && !found_root) || !is_root, "Multiple root bones found");
				ACL_ENSURE(quat_is_finite(bone.bind_transform.rotation), "Bind rotation is invalid: [%f, %f, %f, %f]", quat_get_x(bone.bind_transform.rotation), quat_get_y(bone.bind_transform.rotation), quat_get_z(bone.bind_transform.rotation), quat_get_w(bone.bind_transform.rotation));
				ACL_ENSURE(quat_is_normalized(bone.bind_transform.rotation), "Bind rotation isn't normalized: [%f, %f, %f, %f]", quat_get_x(bone.bind_transform.rotation), quat_get_y(bone.bind_transform.rotation), quat_get_z(bone.bind_transform.rotation), quat_get_w(bone.bind_transform.rotation));
				ACL_ENSURE(vector_is_finite3(bone.bind_transform.translation), "Bind translation is invalid: [%f, %f, %f]", vector_get_x(bone.bind_transform.translation), vector_get_y(bone.bind_transform.translation), vector_get_z(bone.bind_transform.translation));
				ACL_ENSURE(vector_is_finite3(bone.bind_transform.scale), "Bind scale is invalid: [%f, %f, %f]", vector_get_x(bone.bind_transform.scale), vector_get_y(bone.bind_transform.scale), vector_get_z(bone.bind_transform.scale));
				ACL_ENSURE(!vector_any_near_equal3(bone.bind_transform.scale, vector_zero_64()), "Bind scale is zero: [%f, %f, %f]", vector_get_x(bone.bind_transform.scale), vector_get_y(bone.bind_transform.scale), vector_get_z(bone.bind_transform.scale));

				if (!is_root)
					bitset_set(is_leaf_bitset, bone_bitset_desc, bone.parent_index, false);
				else
					found_root = true;

				m_bones[bone_index] = std::move(bone);
			}

			m_num_leaf_bones = safe_static_cast<uint16_t>(bitset_count_set_bits(is_leaf_bitset, bone_bitset_desc));

			m_leaf_bone_chains = allocate_type_array<uint32_t>(allocator, m_num_leaf_bones * bone_bitset_desc.get_size());

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

			ACL_ENSURE(found_root, "No root bone found. The root bone must have a parent index = 0xFFFF");
			ACL_ENSURE(leaf_index == m_num_leaf_bones, "Invalid number of leaf bone found");
			deallocate_type_array(m_allocator, is_leaf_bitset, bone_bitset_desc.get_size());
		}

		~RigidSkeleton()
		{
			deallocate_type_array(m_allocator, m_bones, m_num_bones);

			BitSetDescription bone_bitset_desc = BitSetDescription::make_from_num_bits(m_num_bones);
			deallocate_type_array(m_allocator, m_leaf_bone_chains, m_num_leaf_bones * bone_bitset_desc.get_size());
		}

		RigidSkeleton(const RigidSkeleton&) = delete;
		RigidSkeleton& operator=(const RigidSkeleton&) = delete;

		const RigidBone* get_bones() const { return m_bones; }
		const RigidBone& get_bone(uint16_t bone_index) const
		{
			ACL_ENSURE(bone_index < m_num_bones, "Invalid bone index: %u >= %u", bone_index, m_num_bones);
			return m_bones[bone_index];
		}

		uint16_t get_num_bones() const { return m_num_bones; }

		BoneChain get_bone_chain(uint16_t bone_index) const
		{
			ACL_ENSURE(bone_index < m_num_bones, "Invalid bone index: %u >= %u", bone_index, m_num_bones);
			const RigidBone& bone = m_bones[bone_index];
			return BoneChain(bone.bone_chain, BitSetDescription::make_from_num_bits(m_num_bones), bone_index);
		}

	private:
		IAllocator&	m_allocator;
		RigidBone*	m_bones;				// Array of RigidBone entries, contains m_num_bones entries
		uint32_t*	m_leaf_bone_chains;		// Contiguous block of memory for the bone chains, contains m_num_leaf_bones * get_bitset_size(m_num_bones) entries

		uint16_t	m_num_bones;
		uint16_t	m_num_leaf_bones;
	};
}
