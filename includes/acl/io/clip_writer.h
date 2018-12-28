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
#include "acl/core/compiler_utils.h"
#include "acl/core/iallocator.h"
#include "acl/core/error.h"

#include <cstdint>
#include <cinttypes>
#include <cstdio>

ACL_IMPL_FILE_PRAGMA_PUSH

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

				constexpr explicit DoubleToUInt64(double dbl_value) : dbl(dbl_value) {}
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
				writer["additive_format"] = get_additive_clip_format_name(clip.get_additive_format());

				const AnimationClip* base_clip = clip.get_additive_base();
				if (base_clip != nullptr)
				{
					writer["additive_base_name"] = base_clip->get_name().c_str();
					writer["additive_base_num_samples"] = base_clip->get_num_samples();
					writer["additive_base_sample_rate"] = base_clip->get_sample_rate();
				}
			};
			writer.insert_newline();
		}

		inline void write_sjson_settings(AlgorithmType8 algorithm, const CompressionSettings& settings, sjson::Writer& writer)
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

				writer["constant_rotation_threshold_angle"] = settings.constant_rotation_threshold_angle;
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

		inline void write_sjson_tracks(const RigidSkeleton& skeleton, const AnimationClip& clip, bool is_base_clip, sjson::Writer& writer)
		{
			char buffer[32] = { 0 };

			writer[is_base_clip ? "base_tracks" : "tracks"] = [&](sjson::ArrayWriter& writer)
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

		inline const char* write_acl_clip(const RigidSkeleton& skeleton, const AnimationClip& clip, AlgorithmType8 algorithm, const CompressionSettings* settings, const char* acl_filename)
		{
			if (acl_filename == nullptr)
				return "'acl_filename' cannot be NULL!";

			const size_t filename_len = std::strlen(acl_filename);
			if (filename_len < 10 || strncmp(acl_filename + filename_len - 10, ".acl.sjson", 10) != 0)
				return "'acl_filename' file must be an ACL SJSON file of the form: *.acl.sjson";

			std::FILE* file = nullptr;

#ifdef _WIN32
			char path[64 * 1024] = { 0 };
			snprintf(path, get_array_size(path), "\\\\?\\%s", acl_filename);
			fopen_s(&file, path, "w");
#else
			file = fopen(acl_filename, "w");
#endif

			if (file == nullptr)
				return "Failed to open ACL file for writing";

			sjson::FileStreamWriter stream_writer(file);
			sjson::Writer writer(stream_writer);

			writer["version"] = 3;
			writer.insert_newline();

			write_sjson_clip(clip, writer);
			if (settings != nullptr)
				write_sjson_settings(algorithm, *settings, writer);
			write_sjson_bones(skeleton, writer);

			const AnimationClip* base_clip = clip.get_additive_base();
			if (base_clip != nullptr)
				write_sjson_tracks(skeleton, *base_clip, true, writer);

			write_sjson_tracks(skeleton, clip, false, writer);

			std::fclose(file);
			return nullptr;
		}
	}

	//////////////////////////////////////////////////////////////////////////
	// Write out an SJSON ACL clip file from a skeleton, a clip,
	// and no specific compression settings.
	// Returns an error string on failure, null on success.
	//////////////////////////////////////////////////////////////////////////
	inline const char* write_acl_clip(const RigidSkeleton& skeleton, const AnimationClip& clip, const char* acl_filename)
	{
		return impl::write_acl_clip(skeleton, clip, AlgorithmType8::UniformlySampled, nullptr, acl_filename);
	}

	//////////////////////////////////////////////////////////////////////////
	// Write out an SJSON ACL clip file from a skeleton, a clip,
	// and compression settings.
	// Returns an error string on failure, null on success.
	//////////////////////////////////////////////////////////////////////////
	inline const char* write_acl_clip(const RigidSkeleton& skeleton, const AnimationClip& clip, AlgorithmType8 algorithm, const CompressionSettings& settings, const char* acl_filename)
	{
		return impl::write_acl_clip(skeleton, clip, algorithm, &settings, acl_filename);
	}
}

ACL_IMPL_FILE_PRAGMA_POP

#endif	// #if defined(SJSON_CPP_WRITER)
