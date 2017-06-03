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

#include "acl/compression/skeleton.h"
#include "acl/compression/animation_clip.h"
#include "acl/compression/skeleton_error_metric.h"

#include "acl/algorithm/full_precision_encoder.h"
#include "acl/algorithm/full_precision_decoder.h"

#include <Windows.h>
#include <cstring>
#include <cstdio>

struct OutputWriterImpl : public acl::OutputWriter
{
	OutputWriterImpl(acl::Allocator& allocator, uint16_t num_bones)
		: m_allocator(allocator)
		, m_transforms(acl::allocate_type_array<acl::Transform_64>(allocator, num_bones))
		, m_num_bones(num_bones)
	{}

	~OutputWriterImpl()
	{
		m_allocator.deallocate(m_transforms);
	}

	void write_bone_rotation(uint32_t bone_index, const acl::Quat_32& rotation)
	{
		acl::ensure(bone_index <= m_num_bones);
		m_transforms[bone_index].rotation = acl::quat_cast(rotation);
	}

	void write_bone_translation(uint32_t bone_index, const acl::Vector4_32& translation)
	{
		acl::ensure(bone_index <= m_num_bones);
		m_transforms[bone_index].translation = acl::vector_cast(translation);
	}

	acl::Allocator& m_allocator;
	acl::Transform_64* m_transforms;
	uint16_t m_num_bones;
};

struct RawOutputWriterImpl : public acl::OutputWriter
{
	RawOutputWriterImpl(acl::Allocator& allocator, uint16_t num_bones)
		: m_allocator(allocator)
		, m_transforms(acl::allocate_type_array<acl::Transform_64>(allocator, num_bones))
		, m_num_bones(num_bones)
	{}

	~RawOutputWriterImpl()
	{
		m_allocator.deallocate(m_transforms);
	}

	void write_bone_rotation(uint32_t bone_index, const acl::Quat_64& rotation)
	{
		acl::ensure(bone_index <= m_num_bones);
		m_transforms[bone_index].rotation = rotation;
	}

	void write_bone_translation(uint32_t bone_index, const acl::Vector4_64& translation)
	{
		acl::ensure(bone_index <= m_num_bones);
		m_transforms[bone_index].translation = translation;
	}

	acl::Allocator& m_allocator;
	acl::Transform_64* m_transforms;
	uint16_t m_num_bones;
};

struct Options
{
	bool			output_stats;
	const char*		output_stats_filename;

	Options()
		: output_stats(false)
		, output_stats_filename(nullptr)
	{}
};

static Options parse_options(int argc, char** argv)
{
	Options options;

	for (int arg_index = 0; arg_index < argc; ++arg_index)
	{
		char* argument = argv[arg_index];
		if (std::strncmp(argument, "-stats", 6) == 0)
		{
			options.output_stats = true;
			options.output_stats_filename = argument[6] == '=' ? (argument + 7) : nullptr;
		}
	}

	return options;
}

int main(int argc, char** argv)
{
	using namespace acl;

	const Options options = parse_options(argc, argv);

	Allocator allocator;

	// Initialize our skeleton
	RigidBone bones[2];

	bones[0].name = "root";
	bones[0].parent_index = 0xFFFF;
	bones[0].bind_rotation = quat_64_identity();
	bones[0].bind_translation = vector_64_zero();
	bones[0].vertex_distance = 0.01;

	bones[1].name = "bone1";
	bones[1].parent_index = 0;
	bones[1].bind_rotation = quat_set(1.0, 0.0, 0.0, 0.0);
	bones[1].bind_translation = vector_set(1.0, 0.0, 0.0, 0.0);
	bones[1].vertex_distance = 0.01;

	RigidSkeleton skeleton(allocator, &bones[0], 2);

	// Populate our clip with our raw samples
	AnimationClip clip(allocator, skeleton, 2, 30);
	AnimatedBone* clip_bones = clip.get_bones();

	clip_bones[0].rotation_track.set_sample(0, quat_64_identity(), 0.0);
	clip_bones[0].rotation_track.set_sample(1, quat_64_identity(), 1.0);
	clip_bones[0].translation_track.set_sample(0, vector_64_zero(), 0.0);
	clip_bones[0].translation_track.set_sample(1, vector_64_zero(), 1.0);
	clip_bones[1].rotation_track.set_sample(0, quat_64_identity(), 0.0);
	clip_bones[1].rotation_track.set_sample(1, quat_64_identity(), 1.0);
	clip_bones[1].translation_track.set_sample(0, vector_64_zero(), 0.0);
	clip_bones[1].translation_track.set_sample(1, vector_64_zero(), 1.0);

	// Compress & Decompress
	{
		LARGE_INTEGER start_time_cycles;
		QueryPerformanceCounter(&start_time_cycles);

		CompressedClip* compressed_clip = full_precision_encoder(allocator, clip, skeleton);

		LARGE_INTEGER end_time_cycles;
		QueryPerformanceCounter(&end_time_cycles);

		ensure(compressed_clip->is_valid(true));

		RawOutputWriterImpl raw_output_writer(allocator, clip.get_num_bones());
		OutputWriterImpl lossy_output_writer(allocator, clip.get_num_bones());

		double max_error = -1.0;
		double sample_time = 0.0;
		double clip_duration = clip.get_duration();
		double sample_increment = 1.0 / clip.get_sample_rate();
		while (sample_time < clip_duration)
		{
			// TODO: Implement a generic version that calls a function that performs a switch/branch on the encoder type
			full_precision_decoder(*compressed_clip, (float)sample_time, lossy_output_writer);

			clip.sample_pose(sample_time, raw_output_writer);

			double error = calculate_skeleton_error(allocator, skeleton, raw_output_writer.m_transforms, lossy_output_writer.m_transforms);
			max_error = max(max_error, error);

			sample_time += sample_increment;
		}

		{
			full_precision_decoder(*compressed_clip, (float)clip_duration, lossy_output_writer);

			clip.sample_pose(clip_duration, raw_output_writer);

			double error = calculate_skeleton_error(allocator, skeleton, raw_output_writer.m_transforms, lossy_output_writer.m_transforms);
			max_error = max(max_error, error);
		}

		if (options.output_stats)
		{
			uint32_t raw_size = clip.get_raw_size();
			uint32_t compressed_size = compressed_clip->get_size();
			double compression_ratio = double(raw_size) / double(compressed_size);

			LARGE_INTEGER frequency_cycles_per_sec;
			QueryPerformanceFrequency(&frequency_cycles_per_sec);
			double elapsed_time_sec = double(end_time_cycles.QuadPart - start_time_cycles.QuadPart) / double(frequency_cycles_per_sec.QuadPart);

			std::FILE* file = nullptr;
			if (options.output_stats_filename != nullptr)
				fopen_s(&file, options.output_stats_filename, "w");
			file = file != nullptr ? file : stdout;

			fprintf(file, "Clip raw size (bytes): %u\n", raw_size);
			fprintf(file, "Clip compressed size (bytes): %u\n", compressed_size);
			fprintf(file, "Clip compression ratio: %.2f : 1\n", compression_ratio);
			fprintf(file, "Clip max error: %.5f\n", max_error);
			fprintf(file, "Clip compression time (s): %.6f\n", elapsed_time_sec);
			fprintf(file, "Clip duration (s): %.3f\n", clip.get_duration());
			fprintf(file, "Clip num animated tracks: %u\n", clip.get_num_animated_tracks());
			//fprintf(file, "Clip num segments: %u\n", 0);		// TODO

			if (file != stdout)
				std::fclose(file);
			file = nullptr;
		}

		allocator.deallocate(compressed_clip);
	}

	return 0;
}
