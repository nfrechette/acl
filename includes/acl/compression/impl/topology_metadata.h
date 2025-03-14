#pragma once

////////////////////////////////////////////////////////////////////////////////
// The MIT License (MIT)
//
// Copyright (c) 2024 Nicholas Frechette & Animation Compression Library contributors
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
#include "acl/core/iallocator.h"
#include "acl/core/iterator.h"
#include "acl/core/impl/compiler_utils.h"
#include "acl/compression/track_array.h"

#include <cstdint>

// TODO: Can we use dominance to trim down on the number of descendants we measure?
// Doesn't quite work at the moment as when bit rates change, it impacts dominance
// and we can't account for it easily if we calculate dominance up-front
// We'd have to update the dominance map as bit rates change
//#define ACL_IMPL_DEBUG_ENABLE_DOMINANT_DESCENDANTS

ACL_IMPL_FILE_PRAGMA_PUSH

namespace acl
{
	ACL_IMPL_VERSION_NAMESPACE_BEGIN

	namespace acl_impl
	{
		// Topology metadata for a specific transform
		struct transform_topology_t
		{
			// Transform index of our parent
			uint32_t parent_index = 0;

			// Number of transforms between this one and its chain root (root is 0)
			uint32_t depth_from_root = 0;

			// A list of child transform indices beneath this transform (points into 'aggregate_children_indices' in owner clip_topology_t)
			const uint32_t* children = nullptr;

			// Number of children beneath this transform (and present in the list above)
			uint32_t num_children = 0;

			// A list of leaf transform indices beneath this transform (points into 'aggregate_leaf_indices' in owner clip_topology_t)
			const uint32_t* leaves = nullptr;

			// Number of leaves beneath this transform (and present in the list above)
			uint32_t num_leaves = 0;

			// A list of descendant transform indices beneath this transform (points into 'aggregate_descendant_indices' in owner clip_topology_t)
			// Descendants are sorted parent first
			const uint32_t* descendants = nullptr;

			// Number of descendants beneath this transform (and present in the list above)
			uint32_t num_descendants = 0;

#if defined(ACL_IMPL_DEBUG_ENABLE_DOMINANT_DESCENDANTS)
			// A list of dominant descendant transform indices (points into 'aggregate_dominant_descendant_indices' in owner clip_topology_t)
			// Dominant descendants are those that move the most under rigid deformations (can include leaves)
			const uint32_t* dominant_descendants = nullptr;

			// A list of object space distances from this transform to the corresponding dominant descendant (points into 'foobar' in owner clip_topology_t)
			const float* dominant_distances = nullptr;

			// Number of dominant descendants (and present in the lists above)
			uint32_t num_dominant_descendents = 0;
#endif

			// Whether or not this transform is a leaf
			bool is_leaf() const { return num_children == 0; }

			// Whether or not this transform is a root
			bool is_root() const { return parent_index == k_invalid_track_index; }

			// Returns a transform index iterator over the list of children
			const_array_iterator<uint32_t> children_iterator() const;

			// Returns a transform index iterator over the list of leaves
			const_array_iterator<uint32_t> leaves_iterator() const;

			// Returns a transform index iterator over the list of descendants
			const_array_iterator<uint32_t> descendants_iterator() const;

#if defined(ACL_IMPL_DEBUG_ENABLE_DOMINANT_DESCENDANTS)
			// Returns a transform index iterator over the list of dominant descendants
			const_array_iterator<uint32_t> dominant_descendants_iterator() const;
#endif
		};

		// Topology metadata for a clip
		struct clip_topology_t
		{
			// Topology information per transform
			transform_topology_t* transforms			= nullptr;

			// List of transform indices sorted by parent first then sibling transforms are sorted by their transform index
			uint32_t* transform_indices_sorted_parent_first = nullptr;

			// Number of transforms
			uint32_t num_transforms						= 0;

			// List of root transform indices (points into 'transform_indices_sorted_parent_first')
			const uint32_t* root_transform_indices		= nullptr;

			// Number of root transforms
			uint32_t num_root_transforms				= 0;

			// List of leaf transform indices (points into 'transform_indices_sorted_parent_first')
			const uint32_t* leaf_transform_indices		= nullptr;

			// Number of leaf transforms
			uint32_t num_leaf_transforms				= 0;

			// Highest number of leaves that a transform can have
			uint32_t num_max_leaves_per_transform		= 0;

			// Highest depth that a leaf lives at (number of transforms in the longest chain)
			uint32_t max_leaf_depth						= 0;

			// Global list of children indices
			// Each transform contains a pointer into this list
			uint32_t* aggregate_children_indices		= nullptr;

			// Size of global list of children indices
			uint32_t num_aggregate_children_indices		= 0;

			// Global list of leaf indices
			// Each transform contains a pointer into this list
			uint32_t* aggregate_leaf_indices			= nullptr;

			// Size of global list of leaf indices
			uint32_t num_aggregate_leaf_indices			= 0;

			// Global list of descendant indices
			// Each transform contains a pointer into this list
			uint32_t* aggregate_descendant_indices		= nullptr;

			// Size of global list of descendant indices
			uint32_t num_aggregate_descendant_indices	= 0;

#if defined(ACL_IMPL_DEBUG_ENABLE_DOMINANT_DESCENDANTS)
			// Global list of dominant descendant indices
			// Each transform contains a pointer into this list
			uint32_t* aggregate_dominant_descendant_indices = nullptr;

			// Size of global list of dominant descendant indices
			uint32_t num_aggregate_dominant_descendant_indices = 0;
#endif

			// The allocator used for the topology metadata, never null if initialized
			iallocator* allocator						= nullptr;

			// Returns a transform index iterator over the list of transforms, roots first (root to leaf)
			const_array_iterator<uint32_t> roots_first_iterator() const;

			// Returns a transform index iterator over the list of transforms, leaves first (leaf to root)
			const_array_reverse_iterator<uint32_t> leaves_first_iterator() const;

			// Returns a transform index iterator over the list of leaf transforms
			const_array_iterator<uint32_t> leaves_iterator() const;

			~clip_topology_t();
		};

		void build_clip_topology(iallocator& allocator, const track_array_qvvf& track_list, clip_topology_t& out_topology);
	}

	ACL_IMPL_VERSION_NAMESPACE_END
}

#include "acl/compression/impl/topology_metadata.impl.h"

ACL_IMPL_FILE_PRAGMA_POP
