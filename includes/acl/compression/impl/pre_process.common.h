#pragma once

////////////////////////////////////////////////////////////////////////////////
// The MIT License (MIT)
//
// Copyright (c) 2023 Nicholas Frechette & Animation Compression Library contributors
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
#include "acl/core/impl/compiler_utils.h"
#include "acl/compression/impl/clip_context.h"
#include "acl/compression/impl/rigid_shell_utils.h"
#include "acl/compression/impl/topology_metadata.h"
#include "acl/compression/impl/transform_clip_adapters.h"
#include "acl/compression/impl/transform_space_context.h"

#include <cstdint>

ACL_IMPL_FILE_PRAGMA_PUSH

namespace acl
{
	ACL_IMPL_VERSION_NAMESPACE_BEGIN

	namespace acl_impl
	{
		struct pre_process_context_t
		{
			iallocator& allocator;

			const pre_process_settings_t& settings;
			track_array& track_list;

		private:
			rigid_shell_metadata_t* shell_metadata = nullptr;
			uint32_t* sorted_transforms_parent_first = nullptr;
			clip_topology_t* topology = nullptr;
			transform_space_context_t space_context;
			bool is_space_context_initialized = false;

			//////////////////////////////////////////////////////////////////////////

		public:
			explicit pre_process_context_t(
				iallocator& allocator_,
				const pre_process_settings_t& settings_,
				track_array& track_list_)
				: allocator(allocator_)
				, settings(settings_)
				, track_list(track_list_)
			{
			}

			// Cannot copy or move
			pre_process_context_t(const pre_process_context_t&) = delete;
			pre_process_context_t& operator=(const pre_process_context_t&) = delete;

			~pre_process_context_t()
			{
				deallocate_type_array(allocator, shell_metadata, track_list.get_num_tracks());
				deallocate_type_array(allocator, sorted_transforms_parent_first, track_list.get_num_tracks());
				deallocate_type(allocator, topology);
			}

			const rigid_shell_metadata_t* get_shell_metadata()
			{
				if (shell_metadata == nullptr)
				{
					transform_track_array_adapter_t clip_adapter(
						&track_list,
						settings.transform_space.base_format,
						get_base_default_scale(settings.transform_space));
					clip_adapter.sorted_transforms_parent_first = get_sorted_transforms_parent_first();
					clip_adapter.topology = get_topology();
					clip_adapter.space = settings.transform_space.space;

					shell_metadata = compute_clip_shell_distances(
						allocator,
						clip_adapter,
						get_space_context());
				}

				return shell_metadata;
			}

			//////////////////////////////////////////////////////////////////////////
			// Returns the space context of the track list: it holds the base poses.
			// The pre-process actions do not change the number of samples, the sample rate, or the base.
			const transform_space_context_t& get_space_context()
			{
				if (!is_space_context_initialized)
				{
					space_context.initialize(
						allocator,
						settings.transform_space.space,
						settings.transform_space.base_format,
						settings.transform_space.base_adapter,
						transform_track_array_adapter_t(settings.transform_space.base, additive_clip_format8::none, 1.0F),
						track_list,
						track_list.get_num_samples_per_track(),
						track_list.get_sample_rate(),
						track_list.get_finite_duration());

					is_space_context_initialized = true;
				}

				return space_context;
			}

			const uint32_t* get_sorted_transforms_parent_first()
			{
				if (sorted_transforms_parent_first == nullptr)
				{
					const uint32_t num_transforms = track_list.get_num_tracks();

					sorted_transforms_parent_first = allocate_type_array<uint32_t>(allocator, num_transforms);

					for (uint32_t transform_index = 0; transform_index < num_transforms; ++transform_index)
						sorted_transforms_parent_first[transform_index] = transform_index;

					transform_track_array_adapter_t clip_adapter(track_list);
					clip_adapter.space = settings.transform_space.space;

					sort_transform_indices_parent_first(
						clip_adapter,
						sorted_transforms_parent_first,
						num_transforms);
				}

				return sorted_transforms_parent_first;
			}

			const clip_topology_t* get_topology()
			{
				if (topology == nullptr)
				{
					topology = allocate_type<clip_topology_t>(allocator);

					build_clip_topology(allocator, track_list, *topology, settings.transform_space.space);
				}

				return topology;
			}
		};
	}

	ACL_IMPL_VERSION_NAMESPACE_END
}

ACL_IMPL_FILE_PRAGMA_POP
