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

#include "acl/core/memory.h"
#include "acl/clip_reader/clip_reader_error.h"
#include "acl/compression/animation_clip.h"
#include "acl/sjson/sjson_parser.h"
#include <functional>

namespace acl
{
	class ClipReader
	{
	public:
		ClipReader(Allocator& allocator, const char* const sjson_input, int input_length)
			: m_allocator(allocator),
			  m_parser(sjson_input, input_length),
			  m_error()
		{
		}

		~ClipReader()
		{
			m_allocator.deallocate(m_bone_names);
		}

		bool read()
		{
			if (m_read_already)
			{
				return false;
			}

			m_read_already = true;

			if (read_version() &&
				read_clip_header() &&
				read_and_allocate_bones() &&
				create_clip() &&
				read_tracks())
			{
				return true;
			}

			if (m_clip != nullptr)
			{
				m_clip->~AnimationClip();
				m_allocator.deallocate(m_clip);
				m_clip = nullptr;
			}

			if (m_skeleton != nullptr)
			{
				m_skeleton->~RigidSkeleton();
				m_allocator.deallocate(m_skeleton);
				m_skeleton = nullptr;
			}

			return false;
		}

		AnimationClip* get_clip()
		{
			return m_clip;
		}

		RigidSkeleton* get_skeleton()
		{
			return m_skeleton;
		}

		ClipReaderError get_error()
		{
			return m_error;
		}

	private:
		Allocator& m_allocator;
		bool m_read_already{};
		RigidSkeleton* m_skeleton{};
		AnimationClip* m_clip{};

		int m_num_bones{};
		char* m_bone_names{};

		SJSONParser m_parser;
		ClipReaderError m_error;

		double m_version{};

		void set_error(int reason)
		{
			m_parser.get_position(m_error.line, m_error.column);
			m_error.error = reason;
		}

		bool read_version()
		{
			if (!m_parser.read("version", m_version))
			{
				m_error = m_parser.get_error();
				return false;
			}

			if (m_version != 1.0)
			{
				set_error(ClipReaderError::UnsupportedVersion);
				return false;
			}

			return true;
		}

		struct Header
		{
			const char* clip_name;
			int clip_name_length;
			int num_samples;
			int sample_rate;
			double error_threshold;
			const char* reference_frame;
			int reference_frame_length;
		};

		Header m_header{};

		bool read_clip_header()
		{
			if (!m_parser.object_begins("clip"))
			{
				goto error;
			}

			if (!m_parser.read("name", m_header.clip_name, m_header.clip_name_length) ||
				!m_parser.read("num_samples", m_header.num_samples) ||
				!m_parser.read("sample_rate", m_header.sample_rate) ||
				!m_parser.read("error_threshold", m_header.error_threshold))
			{
				goto error;
			}

			m_parser.try_read("reference_frame", m_header.reference_frame, m_header.reference_frame_length);

			if (!m_parser.object_ends())
			{
				goto error;
			}

			return true;

		error:
			m_error = m_parser.get_error();
			return false;
		}

		bool read_and_allocate_bones()
		{
			int name_buffer_length = 0;
			RigidBone* bones = nullptr;

			SJSONParser::State before_bones = m_parser.save_state();

			if (!for_each_bone([this, &name_buffer_length](Bone input)
			{
				++m_num_bones;
				name_buffer_length += input.name_length + 1;
				return true;
			}))
			{
				goto error;
			}

			bones = allocate_type_array<RigidBone>(m_allocator, m_num_bones);

			m_bone_names = allocate_type_array<char>(m_allocator, name_buffer_length);
			char* current_name = m_bone_names;

			m_parser.restore_state(before_bones);

			if (!for_each_bone([bones, &current_name](Bone src)
			{
				RigidBone* dst = bones + src.index;

				std::memcpy(current_name, src.name, src.name_length);
				current_name[src.name_length] = '\0';
				current_name += src.name_length + 1;

				dst->name = nullptr;
				dst->vertex_distance = src.vertex_distance;
				dst->bind_rotation = quat_unaligned_load(src.bind_rotation);
				dst->bind_translation = vector_unaligned_load3(src.bind_translation);
				// TODO: bind_scale

				return true;
			}))
			{
				goto error;
			}

			m_parser.restore_state(before_bones);

			if (!for_each_bone([this, bones](Bone src)
			{
				if (src.parent_length == 0)
				{
					bones[src.index].parent_index = no_parent_bone;
					return true;
				}

				int parent_index = index_of_bone(src.parent, src.parent_length);
				if (parent_index < 0)
				{
					set_error(ClipReaderError::NoParentWithThatName);
					return false;
				}
				
				bones[src.index].parent_index = parent_index;
				return true;
			}))
			{
				goto error;
			}
			
			m_skeleton = allocate_type<RigidSkeleton>(m_allocator, std::ref(m_allocator), bones, m_num_bones);

			m_allocator.deallocate(bones);

			return true;

		error:
			m_allocator.deallocate(bones);

			return false;
		}

		int index_of_bone(const char* const name, const int name_length)
		{
			const char* bone_name = m_bone_names;

			for (int i = 0; i < m_num_bones; ++i)
			{
				if (m_parser.strings_equal(bone_name, name, name_length))
				{
					return i;
				}

				bone_name += std::strlen(bone_name) + 1;
			}

			return -1;
		}

		struct Bone
		{
			int index;
			const char* name;
			int name_length;
			const char* parent;
			int parent_length;
			double vertex_distance;
			double bind_rotation[4];
			double bind_translation[3];
			double bind_scale[3];
		};

		bool for_each_bone(std::function<bool(Bone)> process)
		{
			if (!m_parser.array_begins("bones"))
			{
				m_error = m_parser.get_error();
				return false;
			}

			int index = 0;

			while (true)
			{
				if (m_parser.peek_if_array_ends())
				{
					break;
				}

				Bone b;

				if (!m_parser.object_begins() ||
					!m_parser.read("name", b.name, b.name_length) ||
					!m_parser.read("parent", b.parent, b.parent_length) ||
					!m_parser.read("vertex_distance", b.vertex_distance))
				{
					m_error = m_parser.get_error();
					return false;
				}

				m_parser.try_read("bind_rotation", b.bind_rotation, 4);
				m_parser.try_read("bind_translation", b.bind_translation, 3);
				m_parser.try_read("bind_scale", b.bind_scale, 3);
				
				if (!m_parser.object_ends())
				{
					m_error = m_parser.get_error();
					return false;
				}

				b.index = index;
				++index;

				if (!process(b))
				{
					return false;
				}
			}

			if (!m_parser.array_ends())
			{
				m_error = m_parser.get_error();
				return false;
			}

			return true;
		}

		bool create_clip()
		{
			m_clip = allocate_type<AnimationClip>(m_allocator, std::ref(m_allocator), std::ref(*m_skeleton), m_header.num_samples, m_header.sample_rate);

			return true;
		}

		bool read_tracks()
		{
			if (!m_parser.array_begins("tracks"))
			{
				goto error;
			}

			while (true)
			{
				if (m_parser.peek_if_array_ends())
				{
					break;
				}

				if (!m_parser.object_begins())
				{
					goto error;
				}

				const char* name;
				int name_length;

				if (!m_parser.read("name", name, name_length))
				{
					goto error;
				}

				int bone_index = index_of_bone(name, name_length);
				if (bone_index < 0)
				{
					set_error(ClipReaderError::NoBoneWithThatName);
					return false;
				}

				AnimatedBone* bone = m_clip->get_bones() + bone_index;

				if (!m_parser.try_array_begins("rotations"))
				{
					goto translations;
				}

				for (int i = 0; i < m_header.num_samples; ++i)
				{
					double quaternion[4];

					if (!m_parser.array_begins() || !m_parser.read(quaternion, 4) || !m_parser.array_ends())
					{
						goto error;
					}

					bone->rotation_track.set_sample(i, quat_unaligned_load(quaternion));
				}

				if (!m_parser.array_ends())
				{
					goto error;
				}

			translations:
				if (!m_parser.try_array_begins("translations"))
				{
					goto scales;
				}

				for (int i = 0; i < m_header.num_samples; ++i)
				{
					double translation[3];

					if (!m_parser.array_begins() || !m_parser.read(translation, 3) || !m_parser.array_ends())
					{
						goto error;
					}

					bone->translation_track.set_sample(i, vector_unaligned_load3(translation));
				}

				if (!m_parser.array_ends())
				{
					goto error;
				}
				
			scales:
				if (!m_parser.array_begins("scales"))
				{
					goto end_of_object;
				}

				for (int i = 0; i < m_header.num_samples; ++i)
				{
					double scale[3];

					if (!m_parser.array_begins() || !m_parser.read(scale, 3) || !m_parser.array_ends())
					{
						goto error;
					}

					// TODO: do something with the scale.
				}

				if (!m_parser.array_ends())
				{
					goto error;
				}

			end_of_object:
				if (!m_parser.object_ends())
				{
					goto error;
				}
			}

			if (!m_parser.array_ends())
			{
				goto error;
			}

			if (!m_parser.remainder_is_comments_and_whitespace())
			{
				goto error;
			}

			return true;

		error:
			m_error = m_parser.get_error();
			return false;
		}
	};
}