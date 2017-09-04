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

#include "acl/compression/animation_clip.h"
#include "acl/compression/skeleton.h"
#include "acl/core/memory.h"
#include "acl/core/error.h"
#include "acl/sjson/sjson_writer.h"

#include <stdint.h>

namespace acl
{
	inline bool write_acl_clip(const RigidSkeleton& skeleton, const AnimationClip& clip, const char* acl_filename)
	{
		if (ACL_TRY_ASSERT(acl_filename != nullptr, "'acl_filename' cannot be NULL!"))
			return false;

		size_t filename_len = std::strlen(acl_filename);
		bool is_filename_valid = filename_len < 6 || strncmp(acl_filename + filename_len - 6, ".acl.sjson", 6) != 0;
		if (ACL_TRY_ASSERT(is_filename_valid, "'acl_filename' file must be an ACL SJSON file: %s", acl_filename))
			return false;

		std::FILE* file = nullptr;
		fopen_s(&file, acl_filename, "w");

		if (ACL_TRY_ASSERT(file != nullptr, "Failed to open ACL file for writing: %s", acl_filename))
			return false;

		SJSONFileStreamWriter stream_writer(file);
		SJSONWriter writer(stream_writer);

		writer["version"] = 1;
		writer.insert_newline();

		writer["clip"] = [&](SJSONObjectWriter& writer)
		{
			writer["name"] = clip.get_name().c_str();
			writer["num_samples"] = clip.get_num_samples();
			writer["sample_rate"] = clip.get_sample_rate();
			writer["error_threshold"] = clip.get_error_threshold();
		};
		writer.insert_newline();

		writer["bones"] = [&](SJSONArrayWriter& writer)
		{
			uint16_t num_bones = skeleton.get_num_bones();
			if (num_bones > 0)
				writer.push_newline();

			for (uint16_t bone_index = 0; bone_index < num_bones; ++bone_index)
			{
				const RigidBone& bone = skeleton.get_bone(bone_index);
				const RigidBone& parent_bone = bone.is_root() ? bone : skeleton.get_bone(bone.parent_index);

				writer.push_object([&](SJSONObjectWriter& writer)
				{
					writer["name"] = bone.name.c_str();
					writer["parent"] = bone.is_root() ? "" : parent_bone.name.c_str();
					writer["vertex_distance"] = bone.vertex_distance;
					if (!quat_near_identity(bone.bind_rotation))
						writer["bind_rotation"] = [&](SJSONArrayWriter& writer)
						{
							writer.push_value(quat_get_x(bone.bind_rotation));
							writer.push_value(quat_get_y(bone.bind_rotation));
							writer.push_value(quat_get_z(bone.bind_rotation));
							writer.push_value(quat_get_w(bone.bind_rotation));
						};
					if (!vector_all_near_equal3(bone.bind_translation, vector_zero_64()))
						writer["bind_translation"] = [&](SJSONArrayWriter& writer)
						{
							writer.push_value(vector_get_x(bone.bind_translation));
							writer.push_value(vector_get_y(bone.bind_translation));
							writer.push_value(vector_get_z(bone.bind_translation));
						};
					//fprintf(file, "\t\tbind_scale = [ %.16f, %.16f, %.16f ]\n", vector_get_x(bone.bind_scale), vector_get_y(bone.bind_scale), vector_get_z(bone.bind_scale));
				});
			}
		};
		writer.insert_newline();

		writer["tracks"] = [&](SJSONArrayWriter& writer)
		{
			uint16_t num_bones = skeleton.get_num_bones();
			if (num_bones > 0)
				writer.push_newline();

			for (uint16_t bone_index = 0; bone_index < num_bones; ++bone_index)
			{
				const RigidBone& rigid_bone = skeleton.get_bone(bone_index);
				const AnimatedBone& bone = clip.get_animated_bone(bone_index);

				writer.push_object([&](SJSONObjectWriter& writer)
				{
					writer["name"] = rigid_bone.name.c_str();
					writer["rotations"] = [&](SJSONArrayWriter& writer)
					{
						uint32_t num_rotation_samples = bone.rotation_track.get_num_samples();
						if (num_rotation_samples > 0)
							writer.push_newline();

						for (uint32_t sample_index = 0; sample_index < num_rotation_samples; ++sample_index)
						{
							Quat_64 rotation = bone.rotation_track.get_sample(sample_index);
							writer.push_array([&](SJSONArrayWriter& writer)
							{
								writer.push_value(quat_get_x(rotation));
								writer.push_value(quat_get_y(rotation));
								writer.push_value(quat_get_z(rotation));
								writer.push_value(quat_get_w(rotation));
							});
							writer.push_newline();
						}
					};

					writer["translations"] = [&](SJSONArrayWriter& writer)
					{
						uint32_t num_translation_samples = bone.translation_track.get_num_samples();
						if (num_translation_samples > 0)
							writer.push_newline();

						for (uint32_t sample_index = 0; sample_index < num_translation_samples; ++sample_index)
						{
							Vector4_64 translation = bone.translation_track.get_sample(sample_index);
							writer.push_array([&](SJSONArrayWriter& writer)
							{
								writer.push_value(vector_get_x(translation));
								writer.push_value(vector_get_y(translation));
								writer.push_value(vector_get_z(translation));
							});
							writer.push_newline();
						}
					};

					writer["scales"] = [&](SJSONArrayWriter& writer)
					{
						uint32_t num_scale_samples = bone.scale_track.get_num_samples();
						if (num_scale_samples > 0)
							writer.push_newline();

						for (uint32_t sample_index = 0; sample_index < num_scale_samples; ++sample_index)
						{
							Vector4_64 scale = bone.scale_track.get_sample(sample_index);
							writer.push_array([&](SJSONArrayWriter& writer)
							{
								writer.push_value(vector_get_x(scale));
								writer.push_value(vector_get_y(scale));
								writer.push_value(vector_get_z(scale));
							});
							writer.push_newline();
						}
					};
				});
			}
		};

		std::fclose(file);
		return true;
	}
}
