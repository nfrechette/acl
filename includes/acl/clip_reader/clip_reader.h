#pragma once

#include "acl/memory.h"
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

		bool read()
		{
			// TODO: deallocate upon failure

			return read_version() &&
				read_clip_header() &&
				read_and_allocate_bones() &&
				create_clip() &&
				read_tracks();
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
		RigidSkeleton* m_skeleton{};
		AnimationClip* m_clip{};

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
			if (!m_parser.object_begins("clip") ||
				!m_parser.read("name", m_header.clip_name, m_header.clip_name_length) ||
				!m_parser.read("num_samples", m_header.num_samples) ||
				!m_parser.read("sample_rate", m_header.sample_rate) ||
				!m_parser.read("error_threshold", m_header.error_threshold) ||
				!m_parser.read("reference_frame", m_header.reference_frame, m_header.reference_frame_length) ||
				!m_parser.object_ends())
			{
				m_error = m_parser.get_error();
				return false;
			}

			return true;
		}

		bool read_and_allocate_bones()
		{
			int num_bones = 0, name_buffer_length = 0;
			char* name_buffer = nullptr;

			SJSONParser::State before_bones = m_parser.save_state();

			if (!for_each_bone([&num_bones, &name_buffer_length](Bone input)
			{
				++num_bones;
				name_buffer_length += input.name_length + 1;
				return true;
			}))
			{
				goto error;
			}

			RigidSkeleton* p = allocate_type<RigidSkeleton>(m_allocator);
			m_skeleton = new (p) RigidSkeleton(m_allocator, num_bones);

			name_buffer = allocate_type_array<char>(m_allocator, name_buffer_length);
			char* current_name = name_buffer;

			m_parser.restore_state(before_bones);

			if (!for_each_bone([this, &current_name](Bone src)
			{
				RigidBone* dst = this->m_skeleton->get_bones() + src.index;

				std::memcpy(current_name, src.name, src.name_length);
				current_name[src.name_length] = '\0';
				dst->name = current_name;
				current_name += src.name_length + 1;

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

			if (!for_each_bone([this, num_bones](Bone src)
			{
				if (src.parent_length == 0)
				{
					this->m_skeleton->get_bones()[src.index].parent_index = 0xFFFF;
				}
				else
				{
					int parent_index = index_of_bone_name(src.parent, src.parent_length);

					if (parent_index >= 0)
					{
						this->m_skeleton->get_bones()[src.index].parent_index = parent_index;
						return true;
					}
					else
					{
						set_error(ClipReaderError::NoParentWithThatName);
						return false;
					}
				}
			}))
			{
				goto error;
			}

			m_allocator.deallocate(name_buffer);
			return true;

		error:
			for (int i = 0; i < m_skeleton->get_num_bones(); ++i)
			{
				m_skeleton->get_bones()[i].name = nullptr;
			}

			// TODO: deallocate m_skeleton too - here?

			m_allocator.deallocate(name_buffer);

			return false;
		}

		int index_of_bone_name(const char* const name, const int name_length)
		{
			RigidBone* bones = m_skeleton->get_bones();

			for (int i = 0; i < m_skeleton->get_num_bones(); ++i)
			{
				if (m_parser.strings_equal(bones[i].name, name, name_length))
				{
					return i;
				}
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
					!m_parser.read("vertex_distance", b.vertex_distance) ||
					!m_parser.read("bind_rotation", b.bind_rotation, 4) ||
					!m_parser.read("bind_translation", b.bind_translation, 3) ||
					!m_parser.read("bind_scale", b.bind_scale, 3) ||
					!m_parser.object_ends())
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
			AnimationClip* p = allocate_type<AnimationClip>(m_allocator);
			m_clip = new (p) AnimationClip(m_allocator, *m_skeleton, m_header.num_samples, m_header.sample_rate);

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

				const char* name;
				int name_length;

				if (!m_parser.read("name", name, name_length))
				{
					goto error;
				}

				int bone_index = index_of_bone_name(name, name_length);
				if (bone_index < 0)
				{
					set_error(ClipReaderError::NoBoneWithThatName);
					return false;
				}

				AnimatedBone* bone = m_clip->get_bones() + bone_index;

				if (!m_parser.array_begins("rotations"))
				{
					goto error;
				}

				for (int i = 0; i < m_header.num_samples; ++i)
				{
					double quaternion[4];

					if (!m_parser.array_begins() || !m_parser.read(quaternion, 4) || !m_parser.array_ends())
					{
						goto error;
					}

					// TODO: is this the correct way to set the time of the sample?
					bone->rotation_track.set_sample(i, quat_unaligned_load(quaternion), static_cast<double>(i));
				}

				if (!m_parser.array_ends() || !m_parser.array_begins("translations"))
				{
					goto error;
				}

				for (int i = 0; i < m_header.num_samples; ++i)
				{
					double translation[3];

					if (!m_parser.array_begins() || !m_parser.read(translation, 3) || !m_parser.array_ends())
					{
						goto error;
					}

					// TODO: is this the correct way to set the time of the sample?
					bone->translation_track.set_sample(i, vector_unaligned_load3(translation), static_cast<double>(i));
				}

				if (!m_parser.array_ends() || !m_parser.array_begins("scales"))
				{
					goto error;
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