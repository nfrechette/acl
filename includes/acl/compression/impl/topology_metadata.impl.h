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
#include "acl/core/impl/compiler_utils.h"
#include "acl/compression/track_array.h"

#include <cstdint>

ACL_IMPL_FILE_PRAGMA_PUSH

namespace acl
{
	ACL_IMPL_VERSION_NAMESPACE_BEGIN

	namespace acl_impl
	{
		inline const_array_iterator<uint32_t> transform_topology_t::children_iterator() const
		{
			return make_iterator(children, num_children);
		}

		inline const_array_iterator<uint32_t> transform_topology_t::leaves_iterator() const
		{
			return make_iterator(leaves, num_leaves);
		}

		inline const_array_iterator<uint32_t> transform_topology_t::descendants_iterator() const
		{
			return make_iterator(descendants, num_descendants);
		}

#if defined(ACL_IMPL_DEBUG_ENABLE_DOMINANT_DESCENDANTS)
		inline const_array_iterator<uint32_t> transform_topology_t::dominant_descendants_iterator() const
		{
			return make_iterator(dominant_descendants, num_dominant_descendents);
		}
#endif

		inline const_array_iterator<uint32_t> clip_topology_t::roots_first_iterator() const
		{
			return make_iterator(static_cast<const uint32_t*>(transform_indices_sorted_parent_first), num_transforms);
		}

		inline const_array_reverse_iterator<uint32_t> clip_topology_t::leaves_first_iterator() const
		{
			return make_reverse_iterator(static_cast<const uint32_t*>(transform_indices_sorted_parent_first), num_transforms);
		}

		inline const_array_iterator<uint32_t> clip_topology_t::leaves_iterator() const
		{
			return make_iterator(static_cast<const uint32_t*>(leaf_transform_indices), num_leaf_transforms);
		}

		inline clip_topology_t::~clip_topology_t()
		{
			if (allocator == nullptr)
				return;	// Not initialized

			deallocate_type_array(*allocator, transforms, num_transforms);
			deallocate_type_array(*allocator, transform_indices_sorted_parent_first, num_transforms);
			deallocate_type_array(*allocator, aggregate_children_indices, num_aggregate_children_indices);
			deallocate_type_array(*allocator, aggregate_leaf_indices, num_aggregate_leaf_indices);
			deallocate_type_array(*allocator, aggregate_descendant_indices, num_aggregate_descendant_indices);

#if defined(ACL_IMPL_DEBUG_ENABLE_DOMINANT_DESCENDANTS)
			deallocate_type_array(*allocator, aggregate_dominant_descendant_indices, num_aggregate_dominant_descendant_indices);
#endif
		}

		inline void build_clip_topology(iallocator& allocator, const track_array_qvvf& track_list, clip_topology_t& out_topology)
		{
			ACL_ASSERT(out_topology.allocator == nullptr, "Topology has already been built");

			const uint32_t num_transforms = track_list.get_num_tracks();

			transform_topology_t* topology_per_transform = allocate_type_array<transform_topology_t>(allocator, num_transforms);

			// Assign our parent indices and find our leaves
			for (uint32_t transform_index = 0; transform_index < num_transforms; ++transform_index)
			{
				const uint32_t parent_index = track_list[transform_index].get_description().parent_index;

				topology_per_transform[transform_index].parent_index = parent_index;
			}

			uint32_t num_children_transforms = 0;
			uint32_t* clip_children_indices = nullptr;
			{
				// Find how many children we have
				for (uint32_t transform_index = 0; transform_index < num_transforms; ++transform_index)
				{
					const uint32_t parent_index = topology_per_transform[transform_index].parent_index;

					if (parent_index != k_invalid_track_index)
					{
						topology_per_transform[parent_index].num_children++;
						num_children_transforms++;
					}
				}

				// Allocate the list of children and partition it among the transforms
				clip_children_indices = allocate_type_array<uint32_t>(allocator, num_children_transforms);
				uint32_t assigned_children_transforms = 0;

				for (uint32_t transform_index = 0; transform_index < num_transforms; ++transform_index)
				{
					topology_per_transform[transform_index].children = clip_children_indices + assigned_children_transforms;
					assigned_children_transforms += topology_per_transform[transform_index].num_children;

					// Reset the children count, we'll use it to write our indices below and repopulate it
					topology_per_transform[transform_index].num_children = 0;
				}

				// Populate the list of children
				for (uint32_t transform_index = 0; transform_index < num_transforms; ++transform_index)
				{
					const uint32_t parent_index = topology_per_transform[transform_index].parent_index;

					if (parent_index != k_invalid_track_index)
					{
						const std::ptrdiff_t indices_offset = topology_per_transform[parent_index].children - clip_children_indices;
						uint32_t* parent_children = clip_children_indices + indices_offset;

						parent_children[topology_per_transform[parent_index].num_children] = transform_index;
						topology_per_transform[parent_index].num_children++;
					}
				}
			}

			uint32_t num_leaf_transform_indices = 0;
			uint32_t* clip_leaf_indices = nullptr;
			{
				// Find how many leaf indices we need
				for (uint32_t transform_index = 0; transform_index < num_transforms; ++transform_index)
				{
					if (!topology_per_transform[transform_index].is_leaf())
						continue;	// Skip non-leaf transforms

					uint32_t cursor_index = topology_per_transform[transform_index].parent_index;
					while (cursor_index != k_invalid_track_index)
					{
						topology_per_transform[cursor_index].num_leaves++;
						num_leaf_transform_indices++;

						cursor_index = topology_per_transform[cursor_index].parent_index;
					}
				}

				// Allocate the list of leaf indices and partition it among the transforms
				clip_leaf_indices = allocate_type_array<uint32_t>(allocator, num_leaf_transform_indices);
				uint32_t assigned_leaf_indices = 0;

				for (uint32_t transform_index = 0; transform_index < num_transforms; ++transform_index)
				{
					topology_per_transform[transform_index].leaves = clip_leaf_indices + assigned_leaf_indices;
					assigned_leaf_indices += topology_per_transform[transform_index].num_leaves;

					// Reset the leaf count, we'll use it to write our indices below and repopulate it
					topology_per_transform[transform_index].num_leaves = 0;
				}

				// Populate the list of leaves
				for (uint32_t transform_index = 0; transform_index < num_transforms; ++transform_index)
				{
					if (!topology_per_transform[transform_index].is_leaf())
						continue;	// Skip non-leaf transforms

					uint32_t cursor_index = topology_per_transform[transform_index].parent_index;
					while (cursor_index != k_invalid_track_index)
					{
						const std::ptrdiff_t indices_offset = topology_per_transform[cursor_index].leaves - clip_leaf_indices;
						uint32_t* cursor_leaves = clip_leaf_indices + indices_offset;

						cursor_leaves[topology_per_transform[cursor_index].num_leaves] = transform_index;
						topology_per_transform[cursor_index].num_leaves++;

						cursor_index = topology_per_transform[cursor_index].parent_index;
					}
				}
			}

			uint32_t* transform_indices_sorted_parent_first = allocate_type_array<uint32_t>(allocator, num_transforms);
			uint32_t num_root_transforms = 0;
			uint32_t num_leaf_transforms = 0;
			{
				for (uint32_t transform_index = 0; transform_index < num_transforms; ++transform_index)
				{
					transform_indices_sorted_parent_first[transform_index] = transform_index;

					if (topology_per_transform[transform_index].is_root())
						num_root_transforms++;
					else if (topology_per_transform[transform_index].is_leaf())
						num_leaf_transforms++;
				}

				// We sort our transform indices by parent first
				// If two transforms have the same parent index, we sort them by their transform index
				const auto sort_predicate = [&topology_per_transform](const uint32_t lhs_transform_index, const uint32_t rhs_transform_index)
					{
						const uint32_t lhs_parent_index = topology_per_transform[lhs_transform_index].parent_index;
						const uint32_t rhs_parent_index = topology_per_transform[rhs_transform_index].parent_index;

						// If the transforms don't have the same parent, sort by the parent index
						// We add 1 to parent indices to cause the invalid index to wrap around to 0
						// since parents come first, they'll have the lowest value
						if (lhs_parent_index != rhs_parent_index)
							return (lhs_parent_index + 1) < (rhs_parent_index + 1);

						// Both transforms have the same parent, sort by their index
						return lhs_transform_index < rhs_transform_index;
					};

				std::sort(transform_indices_sorted_parent_first, transform_indices_sorted_parent_first + num_transforms, sort_predicate);
			}

			uint32_t num_aggregate_descendant_indices = 0;
			uint32_t* aggregate_descendant_indices = nullptr;
			{
				// Find our descendants
				for (uint32_t transform_index : make_iterator(transform_indices_sorted_parent_first, num_transforms))
				{
					uint32_t cursor_index = topology_per_transform[transform_index].parent_index;
					while (cursor_index != k_invalid_track_index)
					{
						topology_per_transform[cursor_index].num_descendants++;
						num_aggregate_descendant_indices++;

						cursor_index = topology_per_transform[cursor_index].parent_index;
					}
				}

				// Allocate the list of descendant indices and partition it among the transforms
				aggregate_descendant_indices = allocate_type_array<uint32_t>(allocator, num_aggregate_descendant_indices);
				uint32_t num_assigned_descendant_indices = 0;

				for (uint32_t transform_index = 0; transform_index < num_transforms; ++transform_index)
				{
					topology_per_transform[transform_index].descendants = aggregate_descendant_indices + num_assigned_descendant_indices;
					num_assigned_descendant_indices += topology_per_transform[transform_index].num_descendants;

					// Reset the descendant count, we'll use it to write our indices below and repopulate it
					topology_per_transform[transform_index].num_descendants = 0;
				}

				// Populate the list of descendants
				for (uint32_t transform_index : make_iterator(transform_indices_sorted_parent_first, num_transforms))
				{
					uint32_t cursor_index = topology_per_transform[transform_index].parent_index;
					while (cursor_index != k_invalid_track_index)
					{
						const std::ptrdiff_t indices_offset = topology_per_transform[cursor_index].descendants - aggregate_descendant_indices;
						uint32_t* cursor_descendants = aggregate_descendant_indices + indices_offset;

						cursor_descendants[topology_per_transform[cursor_index].num_descendants] = transform_index;
						topology_per_transform[cursor_index].num_descendants++;

						cursor_index = topology_per_transform[cursor_index].parent_index;
					}
				}
			}

			uint32_t num_max_leaves_per_transform = 0;
			for (uint32_t root_index = 0; root_index < num_root_transforms; ++root_index)
			{
				num_max_leaves_per_transform = std::max<uint32_t>(num_max_leaves_per_transform, topology_per_transform[transform_indices_sorted_parent_first[root_index]].num_leaves);
			}

			uint32_t max_leaf_depth = 0;
			for (uint32_t transform_index : make_iterator(transform_indices_sorted_parent_first, num_transforms))
			{
				const uint32_t parent_index = topology_per_transform[transform_index].parent_index;
				if (parent_index != k_invalid_track_index)
				{
					topology_per_transform[transform_index].depth_from_root = topology_per_transform[parent_index].depth_from_root + 1;

					max_leaf_depth = std::max<uint32_t>(max_leaf_depth, topology_per_transform[transform_index].depth_from_root);
				}
			}

			out_topology.transforms = topology_per_transform;
			out_topology.transform_indices_sorted_parent_first = transform_indices_sorted_parent_first;
			out_topology.root_transform_indices = transform_indices_sorted_parent_first;
			out_topology.num_root_transforms = num_root_transforms;
			out_topology.leaf_transform_indices = transform_indices_sorted_parent_first + (num_transforms - num_leaf_transforms);
			out_topology.num_leaf_transforms = num_leaf_transforms;
			out_topology.num_max_leaves_per_transform = num_max_leaves_per_transform;
			out_topology.max_leaf_depth = max_leaf_depth;
			out_topology.num_transforms = num_transforms;
			out_topology.aggregate_children_indices = clip_children_indices;
			out_topology.num_aggregate_children_indices = num_children_transforms;
			out_topology.aggregate_leaf_indices = clip_leaf_indices;
			out_topology.num_aggregate_leaf_indices = num_leaf_transform_indices;
			out_topology.aggregate_descendant_indices = aggregate_descendant_indices;
			out_topology.num_aggregate_descendant_indices = num_aggregate_descendant_indices;
			out_topology.allocator = &allocator;
		}
	}

	ACL_IMPL_VERSION_NAMESPACE_END
}

ACL_IMPL_FILE_PRAGMA_POP
