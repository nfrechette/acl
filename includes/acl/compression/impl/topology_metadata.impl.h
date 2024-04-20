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
		inline clip_topology_t::~clip_topology_t()
		{
			if (allocator == nullptr)
				return;	// Not initialized

			deallocate_type_array(*allocator, transforms, num_transforms);
			deallocate_type_array(*allocator, children_indices, num_children_indices);
			deallocate_type_array(*allocator, leaf_indices, num_leaf_indices);
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
						const ptrdiff_t indices_offset = topology_per_transform[parent_index].children - clip_children_indices;
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
						const ptrdiff_t indices_offset = topology_per_transform[cursor_index].leaves - clip_leaf_indices;
						uint32_t* cursor_leaves = clip_leaf_indices + indices_offset;

						cursor_leaves[topology_per_transform[cursor_index].num_leaves] = transform_index;
						topology_per_transform[cursor_index].num_leaves++;

						cursor_index = topology_per_transform[cursor_index].parent_index;
					}
				}
			}

			out_topology.transforms = topology_per_transform;
			out_topology.num_transforms = num_transforms;
			out_topology.children_indices = clip_children_indices;
			out_topology.num_children_indices = num_children_transforms;
			out_topology.leaf_indices = clip_leaf_indices;
			out_topology.num_leaf_indices = num_leaf_transform_indices;
			out_topology.allocator = &allocator;
		}
	}

	ACL_IMPL_VERSION_NAMESPACE_END
}

ACL_IMPL_FILE_PRAGMA_POP
