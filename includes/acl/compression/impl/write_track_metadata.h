#pragma once

////////////////////////////////////////////////////////////////////////////////
// The MIT License (MIT)
//
// Copyright (c) 2020 Nicholas Frechette & Animation Compression Library contributors
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

#include "acl/core/memory_utils.h"
#include "acl/core/impl/compiler_utils.h"
#include "acl/compression/track_array.h"

#include <cstdint>
#include <memory>

ACL_IMPL_FILE_PRAGMA_PUSH

namespace acl
{
	namespace acl_impl
	{
		inline uint32_t write_track_list_name(const track_array& tracks, char* out_track_list_name)
		{
			uint8_t* output_buffer = reinterpret_cast<uint8_t*>(out_track_list_name);
			const uint8_t* output_buffer_start = output_buffer;

			const string& name = tracks.get_name();
			const uint32_t name_size = uint32_t(name.size() + 1);	// Include null terminator too

			if (out_track_list_name != nullptr)
				std::memcpy(out_track_list_name, name.c_str(), name_size);

			output_buffer += name_size;

			return safe_static_cast<uint32_t>(output_buffer - output_buffer_start);
		}

		inline uint32_t write_track_names(const track_array& tracks, const uint32_t* track_output_indices, uint32_t num_output_tracks, uint32_t* out_track_names)
		{
			uint8_t* output_buffer = reinterpret_cast<uint8_t*>(out_track_names);
			const uint8_t* output_buffer_start = output_buffer;

			// Write offsets first
			uint32_t* track_name_offsets = out_track_names;
			uint32_t offset = sizeof(uint32_t) * num_output_tracks;
			for (uint32_t output_index = 0; output_index < num_output_tracks; ++output_index)
			{
				const uint32_t track_index = track_output_indices[output_index];
				const string& name = tracks[track_index].get_name();
				const uint32_t name_size = uint32_t(name.size() + 1);	// Include null terminator too

				if (out_track_names != nullptr)
					*track_name_offsets = offset;

				track_name_offsets++;
				output_buffer += sizeof(uint32_t);
				offset += name_size;
			}

			// Next write our track names
			char* track_names = safe_ptr_cast<char>(output_buffer);
			for (uint32_t output_index = 0; output_index < num_output_tracks; ++output_index)
			{
				const uint32_t track_index = track_output_indices[output_index];
				const string& name = tracks[track_index].get_name();
				const uint32_t name_size = uint32_t(name.size() + 1);	// Include null terminator too

				if (out_track_names != nullptr)
					std::memcpy(track_names, name.c_str(), name_size);

				track_names += name_size;
				output_buffer += name_size;
			}

			return safe_static_cast<uint32_t>(output_buffer - output_buffer_start);
		}
	}
}

ACL_IMPL_FILE_PRAGMA_POP
