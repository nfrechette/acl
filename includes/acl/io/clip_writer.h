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

#if defined(SJSON_CPP_WRITER)

#include "acl/compression/animation_clip.h"
#include "acl/compression/compression_settings.h"
#include "acl/compression/skeleton.h"
#include "acl/core/iallocator.h"
#include "acl/core/error.h"

#include <cstdint>
#include <cinttypes>
#include <cstdio>

namespace acl
{
	namespace impl
	{
		inline const char* format_hex_double(double value, char* buffer, size_t buffer_size)
		{
			union DoubleToUInt64
			{
				uint64_t u64;
				double dbl;

				constexpr explicit DoubleToUInt64(double value) : dbl(value) {}
			};

			snprintf(buffer, buffer_size, "%" PRIX64, DoubleToUInt64(value).u64);

			return buffer;
		};

		inline void write_sjson_clip(const AnimationClip& clip, sjson::Writer& writer)
		{
			writer["clip"] = [&](sjson::ObjectWriter& writer)
			{
				writer["name"] = clip.get_name().c_str();
				writer["num_samples"] = clip.get_num_samples();
				writer["sample_rate"] = clip.get_sample_rate();
				writer["is_binary_exact"] = true;
			};
			writer.insert_newline();
		}

		inline void write_sjson_settings(const AnimationClip& clip, AlgorithmType8 algorithm, const CompressionSettings& settings, sjson::Writer& writer)
		{
			writer["settings"] = [&](sjson::ObjectWriter& writer)
			{
				writer["algorithm_name"] = get_algorithm_name(algorithm);
				writer["rotation_format"] = get_rotation_format_name(settings.rotation_format);
				writer["translation_format"] = get_vector_format_name(settings.translation_format);
				writer["scale_format"] = get_vector_format_name(settings.scale_format);
				writer["rotation_range_reduction"] = are_any_enum_flags_set(settings.range_reduction, RangeReductionFlags8::Rotations);
				writer["translation_range_reduction"] = are_any_enum_flags_set(settings.range_reduction, RangeReductionFlags8::Translations);
				writer["scale_range_reduction"] = are_any_enum_flags_set(settings.range_reduction, RangeReductionFlags8::Scales);

				writer["segmenting"] = [&](sjson::ObjectWriter& writer)
				{
					writer["enabled"] = settings.segmenting.enabled;
					writer["ideal_num_samples"] = settings.segmenting.ideal_num_samples;
					writer["max_num_samples"] = settings.segmenting.max_num_samples;
					writer["rotation_range_reduction"] = are_any_enum_flags_set(settings.segmenting.range_reduction, RangeReductionFlags8::Rotations);
					writer["translation_range_reduction"] = are_any_enum_flags_set(settings.segmenting.range_reduction, RangeReductionFlags8::Translations);
					writer["scale_range_reduction"] = are_any_enum_flags_set(settings.segmenting.range_reduction, RangeReductionFlags8::Scales);
				};

				writer["constant_rotation_threshold"] = settings.constant_rotation_threshold;
				writer["constant_translation_threshold"] = settings.constant_translation_threshold;
				writer["constant_scale_threshold"] = settings.constant_scale_threshold;
				writer["error_threshold"] = settings.error_threshold;
			};
			writer.insert_newline();
		}

		inline void write_sjson_bones(const RigidSkeleton& skeleton, sjson::Writer& writer)
		{
			char buffer[32] = { 0 };

			writer["bones"] = [&](sjson::ArrayWriter& writer)
			{
				const uint16_t num_bones = skeleton.get_num_bones();
				if (num_bones > 0)
					writer.push_newline();

				for (uint16_t bone_index = 0; bone_index < num_bones; ++bone_index)
				{
					const RigidBone& bone = skeleton.get_bone(bone_index);
					const RigidBone& parent_bone = bone.is_root() ? bone : skeleton.get_bone(bone.parent_index);

					writer.push([&](sjson::ObjectWriter& writer)
					{
						writer["name"] = bone.name.c_str();
						writer["parent"] = bone.is_root() ? "" : parent_bone.name.c_str();
						writer["vertex_distance"] = bone.vertex_distance;

						if (!quat_near_identity(bone.bind_transform.rotation))
							writer["bind_rotation"] = [&](sjson::ArrayWriter& writer)
						{
							writer.push(format_hex_double(quat_get_x(bone.bind_transform.rotation), buffer, sizeof(buffer)));
							writer.push(format_hex_double(quat_get_y(bone.bind_transform.rotation), buffer, sizeof(buffer)));
							writer.push(format_hex_double(quat_get_z(bone.bind_transform.rotation), buffer, sizeof(buffer)));
							writer.push(format_hex_double(quat_get_w(bone.bind_transform.rotation), buffer, sizeof(buffer)));
						};

						if (!vector_all_near_equal3(bone.bind_transform.translation, vector_zero_64()))
							writer["bind_translation"] = [&](sjson::ArrayWriter& writer)
						{
							writer.push(format_hex_double(vector_get_x(bone.bind_transform.translation), buffer, sizeof(buffer)));
							writer.push(format_hex_double(vector_get_y(bone.bind_transform.translation), buffer, sizeof(buffer)));
							writer.push(format_hex_double(vector_get_z(bone.bind_transform.translation), buffer, sizeof(buffer)));
						};

						if (!vector_all_near_equal3(bone.bind_transform.scale, vector_set(1.0)))
							writer["bind_scale"] = [&](sjson::ArrayWriter& writer)
						{
							writer.push(format_hex_double(vector_get_x(bone.bind_transform.scale), buffer, sizeof(buffer)));
							writer.push(format_hex_double(vector_get_y(bone.bind_transform.scale), buffer, sizeof(buffer)));
							writer.push(format_hex_double(vector_get_z(bone.bind_transform.scale), buffer, sizeof(buffer)));
						};
					});
				}
			};
			writer.insert_newline();
		}

		inline void write_sjson_tracks(const RigidSkeleton& skeleton, const AnimationClip& clip, sjson::Writer& writer)
		{
			char buffer[32] = { 0 };

			writer["tracks"] = [&](sjson::ArrayWriter& writer)
			{
				const uint16_t num_bones = skeleton.get_num_bones();
				if (num_bones > 0)
					writer.push_newline();

				for (uint16_t bone_index = 0; bone_index < num_bones; ++bone_index)
				{
					const RigidBone& rigid_bone = skeleton.get_bone(bone_index);
					const AnimatedBone& bone = clip.get_animated_bone(bone_index);

					writer.push([&](sjson::ObjectWriter& writer)
					{
						writer["name"] = rigid_bone.name.c_str();
						writer["rotations"] = [&](sjson::ArrayWriter& writer)
						{
							const uint32_t num_rotation_samples = bone.rotation_track.get_num_samples();
							if (num_rotation_samples > 0)
								writer.push_newline();

							for (uint32_t sample_index = 0; sample_index < num_rotation_samples; ++sample_index)
							{
								const Quat_64 rotation = bone.rotation_track.get_sample(sample_index);
								writer.push([&](sjson::ArrayWriter& writer)
								{
									writer.push(format_hex_double(quat_get_x(rotation), buffer, sizeof(buffer)));
									writer.push(format_hex_double(quat_get_y(rotation), buffer, sizeof(buffer)));
									writer.push(format_hex_double(quat_get_z(rotation), buffer, sizeof(buffer)));
									writer.push(format_hex_double(quat_get_w(rotation), buffer, sizeof(buffer)));
								});
								writer.push_newline();
							}
						};

						writer["translations"] = [&](sjson::ArrayWriter& writer)
						{
							const uint32_t num_translation_samples = bone.translation_track.get_num_samples();
							if (num_translation_samples > 0)
								writer.push_newline();

							for (uint32_t sample_index = 0; sample_index < num_translation_samples; ++sample_index)
							{
								const Vector4_64 translation = bone.translation_track.get_sample(sample_index);
								writer.push([&](sjson::ArrayWriter& writer)
								{
									writer.push(format_hex_double(vector_get_x(translation), buffer, sizeof(buffer)));
									writer.push(format_hex_double(vector_get_y(translation), buffer, sizeof(buffer)));
									writer.push(format_hex_double(vector_get_z(translation), buffer, sizeof(buffer)));
								});
								writer.push_newline();
							}
						};

						writer["scales"] = [&](sjson::ArrayWriter& writer)
						{
							const uint32_t num_scale_samples = bone.scale_track.get_num_samples();
							if (num_scale_samples > 0)
								writer.push_newline();

							for (uint32_t sample_index = 0; sample_index < num_scale_samples; ++sample_index)
							{
								const Vector4_64 scale = bone.scale_track.get_sample(sample_index);
								writer.push([&](sjson::ArrayWriter& writer)
								{
									writer.push(format_hex_double(vector_get_x(scale), buffer, sizeof(buffer)));
									writer.push(format_hex_double(vector_get_y(scale), buffer, sizeof(buffer)));
									writer.push(format_hex_double(vector_get_z(scale), buffer, sizeof(buffer)));
								});
								writer.push_newline();
							}
						};
					});
				}
			};
		}

		inline bool write_acl_clip(const RigidSkeleton& skeleton, const AnimationClip& clip, AlgorithmType8 algorithm, const CompressionSettings* settings, const char* acl_filename)
		{
			if (ACL_TRY_ASSERT(acl_filename != nullptr, "'acl_filename' cannot be NULL!"))
				return false;

			const size_t filename_len = std::strlen(acl_filename);
			const bool is_filename_valid = filename_len < 6 || strncmp(acl_filename + filename_len - 6, ".acl.sjson", 6) != 0;
			if (ACL_TRY_ASSERT(is_filename_valid, "'acl_filename' file must be an ACL SJSON file: %s", acl_filename))
				return false;

			std::FILE* file = nullptr;

#ifdef _WIN32
			fopen_s(&file, acl_filename, "w");
#else
			file = fopen(acl_filename, "w");
#endif

			if (ACL_TRY_ASSERT(file != nullptr, "Failed to open ACL file for writing: %s", acl_filename))
				return false;

			sjson::FileStreamWriter stream_writer(file);
			sjson::Writer writer(stream_writer);

			writer["version"] = 2;
			writer.insert_newline();

			write_sjson_clip(clip, writer);
			if (settings != nullptr)
				write_sjson_settings(clip, algorithm, *settings, writer);
			write_sjson_bones(skeleton, writer);
			write_sjson_tracks(skeleton, clip, writer);

			std::fclose(file);
			return true;
		}
	}

	//////////////////////////////////////////////////////////////////////////
	// Write out an SJSON ACL clip file from a skeleton, a clip,
	// and no specific compression settings.
	//////////////////////////////////////////////////////////////////////////
	inline bool write_acl_clip(const RigidSkeleton& skeleton, const AnimationClip& clip, const char* acl_filename)
	{
		return impl::write_acl_clip(skeleton, clip, AlgorithmType8::UniformlySampled, nullptr, acl_filename);
	}

	//////////////////////////////////////////////////////////////////////////
	// Write out an SJSON ACL clip file from a skeleton, a clip,
	// and compression settings.
	//////////////////////////////////////////////////////////////////////////
	inline bool write_acl_clip(const RigidSkeleton& skeleton, const AnimationClip& clip, AlgorithmType8 algorithm, const CompressionSettings& settings, const char* acl_filename)
	{
		return impl::write_acl_clip(skeleton, clip, algorithm, &settings, acl_filename);
	}
}

#endif	// #if defined(SJSON_CPP_WRITER)
