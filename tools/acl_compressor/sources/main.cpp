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
#include "acl/compression/skeleton.h"
#include "acl/compression/animation_clip.h"
#include "acl/io/clip_reader.h"
#include "acl/compression/skeleton_error_metric.h"

#include "acl/algorithm/uniformly_sampled/algorithm.h"

#include <Windows.h>
#include <conio.h>

#include <cstring>
#include <cstdio>
#include <fstream>
#include <streambuf>
#include <sstream>
#include <string>
#include <memory>

using namespace acl;

struct OutputWriterImpl : public OutputWriter
{
	OutputWriterImpl(Allocator& allocator, uint16_t num_bones)
		: m_allocator(allocator)
		, m_transforms(allocate_type_array<Transform_32>(allocator, num_bones))
		, m_num_bones(num_bones)
	{}

	~OutputWriterImpl()
	{
		deallocate_type_array(m_allocator, m_transforms, m_num_bones);
	}

	void write_bone_rotation(uint32_t bone_index, const Quat_32& rotation)
	{
		ACL_ENSURE(bone_index < m_num_bones, "Invalid bone index. %u >= %u", bone_index, m_num_bones);
		m_transforms[bone_index].rotation = rotation;
	}

	void write_bone_translation(uint32_t bone_index, const Vector4_32& translation)
	{
		ACL_ENSURE(bone_index < m_num_bones, "Invalid bone index. %u >= %u", bone_index, m_num_bones);
		m_transforms[bone_index].translation = translation;
	}

	Allocator& m_allocator;
	Transform_32* m_transforms;
	uint16_t m_num_bones;
};

struct Options
{
	const char*		input_filename;
	bool			output_stats;
	const char*		output_stats_filename;

	//////////////////////////////////////////////////////////////////////////

	std::FILE*		output_stats_file;

	Options()
		: input_filename(nullptr),
		  output_stats(false)
		, output_stats_filename(nullptr)
		, output_stats_file(nullptr)
	{}

	Options(Options&& other)
		: output_stats(other.output_stats)
		, output_stats_filename(other.output_stats_filename)
		, output_stats_file(other.output_stats_file)
	{
		new (&other) Options();
	}

	~Options()
	{
		if (output_stats_file != nullptr && output_stats_file != stdout)
			std::fclose(output_stats_file);
	}

	Options& operator=(Options&& rhs)
	{
		std::swap(output_stats, rhs.output_stats);
		std::swap(output_stats_filename, rhs.output_stats_filename);
		std::swap(output_stats_file, rhs.output_stats_file);
	}

	Options(const Options&) = delete;
	Options& operator=(const Options&) = delete;

	void open_output_stats_file()
	{
		std::FILE* file = nullptr;
		if (output_stats_filename != nullptr)
			fopen_s(&file, output_stats_filename, "w");
		output_stats_file = file != nullptr ? file : stdout;
	}
};

constexpr char* ACL_INPUT_FILE_OPTION = "-acl=";
constexpr char* STATS_OUTPUT_OPTION = "-stats";

static bool parse_options(int argc, char** argv, Options& options)
{
	for (int arg_index = 1; arg_index < argc; ++arg_index)
	{
		const char* argument = argv[arg_index];

		size_t option_length = std::strlen(ACL_INPUT_FILE_OPTION);
		if (std::strncmp(argument, ACL_INPUT_FILE_OPTION, option_length) == 0)
		{
			options.input_filename = argument + option_length;
			continue;
		}

		option_length = std::strlen(STATS_OUTPUT_OPTION);
		if (std::strncmp(argument, STATS_OUTPUT_OPTION, option_length) == 0)
		{
			options.output_stats = true;
			options.output_stats_filename = argument[option_length] == '=' ? argument + option_length + 1 : nullptr;
			options.open_output_stats_file();
			continue;
		}

		printf("Unrecognized option %s\n", argument);
		return false;
	}

	if (options.input_filename == nullptr || std::strlen(options.input_filename) == 0)
	{
		printf("An input file is required.\n");
		return false;
	}

	return true;
}

struct BoneError
{
	uint16_t index;
	double error;
	double sample_time;
};

static void print_stats(const Options& options, const AnimationClip& clip, const CompressedClip& compressed_clip, uint64_t elapsed_cycles, BoneError error, IAlgorithm& algorithm)
{
	if (!options.output_stats)
		return;

	uint32_t raw_size = clip.get_total_size();
	uint32_t compressed_size = compressed_clip.get_size();
	double compression_ratio = double(raw_size) / double(compressed_size);

	LARGE_INTEGER frequency_cycles_per_sec;
	QueryPerformanceFrequency(&frequency_cycles_per_sec);
	double elapsed_time_sec = double(elapsed_cycles) / double(frequency_cycles_per_sec.QuadPart);

	std::FILE* file = options.output_stats_file;

	fprintf(file, "Clip algorithm: %s\n", get_algorithm_name(compressed_clip.get_algorithm_type()));
	fprintf(file, "Clip raw size (bytes): %u\n", raw_size);
	fprintf(file, "Clip compressed size (bytes): %u\n", compressed_size);
	fprintf(file, "Clip compression ratio: %.2f : 1\n", compression_ratio);
	fprintf(file, "Clip max error: %.5f\n", error.error);
	fprintf(file, "Clip worst bone: %u\n", error.index);
	fprintf(file, "Clip worst time (s): %.5f\n", error.sample_time);
	fprintf(file, "Clip compression time (s): %.6f\n", elapsed_time_sec);
	fprintf(file, "Clip duration (s): %.3f\n", clip.get_duration());
	fprintf(file, "Clip num samples: %u\n", clip.get_num_samples());
	//fprintf(file, "Clip num segments: %u\n", 0);		// TODO
	algorithm.print_stats(compressed_clip, file);
	fprintf(file, "\n");
}

static BoneError find_max_error(Allocator& allocator, const AnimationClip& clip, const RigidSkeleton& skeleton, const CompressedClip& compressed_clip, IAlgorithm& algorithm)
{
	using namespace acl;

	uint16_t num_bones = clip.get_num_bones();
	Transform_32* raw_pose_transforms = allocate_type_array<Transform_32>(allocator, num_bones);
	Transform_32* lossy_pose_transforms = allocate_type_array<Transform_32>(allocator, num_bones);
	float* error_per_bone = allocate_type_array<float>(allocator, num_bones);

	uint16_t worst_bone = INVALID_BONE_INDEX;
	float max_error = -1.0f;
	float worst_sample_time = 0.0f;
	float clip_duration = clip.get_duration();
	uint32_t sample_rate = clip.get_sample_rate();
	uint32_t num_samples = calculate_num_samples(clip_duration, sample_rate);
	for (uint32_t sample_index = 0; sample_index < num_samples; ++sample_index)
	{
		float sample_time = min(float(sample_index) / float(sample_rate), clip_duration);

		clip.sample_pose(sample_time, raw_pose_transforms, num_bones);
		algorithm.decompress_pose(compressed_clip, sample_time, lossy_pose_transforms, num_bones);

		calculate_skeleton_error(allocator, skeleton, raw_pose_transforms, lossy_pose_transforms, error_per_bone);

		for (uint16_t bone_index = 0; bone_index < num_bones; ++bone_index)
		{
			if (error_per_bone[bone_index] > max_error)
			{
				max_error = error_per_bone[bone_index];
				worst_bone = bone_index;
				worst_sample_time = sample_time;
			}
		}
	}

	// Unit test
	{
		// Validate that the decoder can decode a single bone at a particular time
		// Use the last bone and last sample time to ensure we can seek properly
		uint16_t sample_bone_index = num_bones - 1;
		Quat_32 test_rotation;
		Vector4_32 test_translation;
		algorithm.decompress_bone(compressed_clip, clip_duration, sample_bone_index, &test_rotation, &test_translation);
		ACL_ENSURE(quat_near_equal(test_rotation, lossy_pose_transforms[sample_bone_index].rotation), "Failed to sample bone index: %u", sample_bone_index);
		ACL_ENSURE(vector_near_equal3(test_translation, lossy_pose_transforms[sample_bone_index].translation), "Failed to sample bone index: %u", sample_bone_index);
	}

	deallocate_type_array(allocator, raw_pose_transforms, num_bones);
	deallocate_type_array(allocator, lossy_pose_transforms, num_bones);
	deallocate_type_array(allocator, error_per_bone, num_bones);

	return BoneError{worst_bone, max_error, worst_sample_time};
}

static void try_algorithm(const Options& options, Allocator& allocator, const AnimationClip& clip, const RigidSkeleton& skeleton, IAlgorithm &algorithm)
{
	using namespace acl;

	LARGE_INTEGER start_time_cycles;
	QueryPerformanceCounter(&start_time_cycles);

	CompressedClip* compressed_clip = algorithm.compress_clip(allocator, clip, skeleton);

	LARGE_INTEGER end_time_cycles;
	QueryPerformanceCounter(&end_time_cycles);

	ACL_ENSURE(compressed_clip->is_valid(true), "Compressed clip is invalid");

	BoneError error = find_max_error(allocator, clip, skeleton, *compressed_clip, algorithm);

	print_stats(options, clip, *compressed_clip, end_time_cycles.QuadPart - start_time_cycles.QuadPart, error, algorithm);

	allocator.deallocate(compressed_clip, compressed_clip->get_size());
}

static bool read_clip(Allocator& allocator, const char* filename,
					  std::unique_ptr<AnimationClip, Deleter<AnimationClip>>& clip,
					  std::unique_ptr<RigidSkeleton, Deleter<RigidSkeleton>>& skeleton)
{
	printf("Reading ACL input clip...");

	LARGE_INTEGER read_start_time_cycles;
	QueryPerformanceCounter(&read_start_time_cycles);

	std::ifstream t(filename);
	std::stringstream buffer;
	buffer << t.rdbuf();
	std::string str = buffer.str();

	LARGE_INTEGER read_end_time_cycles;
	QueryPerformanceCounter(&read_end_time_cycles);

	uint64_t elapsed_cycles = read_end_time_cycles.QuadPart - read_start_time_cycles.QuadPart;
	LARGE_INTEGER frequency_cycles_per_sec;
	QueryPerformanceFrequency(&frequency_cycles_per_sec);
	double elapsed_time_sec = double(elapsed_cycles) / double(frequency_cycles_per_sec.QuadPart);
	double elapsed_time_ms = elapsed_time_sec * 1000.0;

	printf(" Done in %.1f ms!\n", elapsed_time_ms);
	printf("Parsing ACL input clip...");

	ClipReader reader(allocator, str.c_str(), str.length());

	if (!reader.read(skeleton) || !reader.read(clip, *skeleton))
	{
		ClipReaderError err = reader.get_error();
		printf("\nError on line %d column %d: %s\n", err.line, err.column, err.get_description());
		return false;
	}

	LARGE_INTEGER parse_end_time_cycles;
	QueryPerformanceCounter(&parse_end_time_cycles);

	elapsed_cycles = parse_end_time_cycles.QuadPart - read_end_time_cycles.QuadPart;
	elapsed_time_sec = double(elapsed_cycles) / double(frequency_cycles_per_sec.QuadPart);
	elapsed_time_ms = elapsed_time_sec * 1000.0;

	printf(" Done in %.1f ms!\n", elapsed_time_ms);
	return true;
}

int main(int argc, char** argv)
{
	Options options;

	if (!parse_options(argc, argv, options))
	{
		return -1;
	}

	Allocator allocator;
	std::unique_ptr<AnimationClip, Deleter<AnimationClip>> clip;
	std::unique_ptr<RigidSkeleton, Deleter<RigidSkeleton>> skeleton;

	if (!read_clip(allocator, options.input_filename, clip, skeleton))
	{
		return -1;
	}

	// Compress & Decompress
	{
		UniformlySampledAlgorithm uniform_tests[] =
		{
			UniformlySampledAlgorithm(RotationFormat8::Quat_128, VectorFormat8::Vector3_96, RangeReductionFlags8::None),
			UniformlySampledAlgorithm(RotationFormat8::Quat_128, VectorFormat8::Vector3_96, RangeReductionFlags8::PerClip | RangeReductionFlags8::Rotations),
			UniformlySampledAlgorithm(RotationFormat8::Quat_128, VectorFormat8::Vector3_96, RangeReductionFlags8::PerClip | RangeReductionFlags8::Translations),
			UniformlySampledAlgorithm(RotationFormat8::Quat_128, VectorFormat8::Vector3_96, RangeReductionFlags8::PerClip | RangeReductionFlags8::Rotations | RangeReductionFlags8::Translations),
			UniformlySampledAlgorithm(RotationFormat8::Quat_128, VectorFormat8::Vector3_48, RangeReductionFlags8::PerClip | RangeReductionFlags8::Translations),
			UniformlySampledAlgorithm(RotationFormat8::Quat_128, VectorFormat8::Vector3_48, RangeReductionFlags8::PerClip | RangeReductionFlags8::Rotations | RangeReductionFlags8::Translations),
			UniformlySampledAlgorithm(RotationFormat8::Quat_128, VectorFormat8::Vector3_32, RangeReductionFlags8::PerClip | RangeReductionFlags8::Translations),
			UniformlySampledAlgorithm(RotationFormat8::Quat_128, VectorFormat8::Vector3_32, RangeReductionFlags8::PerClip | RangeReductionFlags8::Rotations | RangeReductionFlags8::Translations),
			UniformlySampledAlgorithm(RotationFormat8::Quat_128, VectorFormat8::Vector3_Variable, RangeReductionFlags8::PerClip | RangeReductionFlags8::Translations),
			UniformlySampledAlgorithm(RotationFormat8::Quat_128, VectorFormat8::Vector3_Variable, RangeReductionFlags8::PerClip | RangeReductionFlags8::Rotations | RangeReductionFlags8::Translations),

			UniformlySampledAlgorithm(RotationFormat8::QuatDropW_96, VectorFormat8::Vector3_96, RangeReductionFlags8::None),
			UniformlySampledAlgorithm(RotationFormat8::QuatDropW_96, VectorFormat8::Vector3_96, RangeReductionFlags8::PerClip | RangeReductionFlags8::Rotations),
			UniformlySampledAlgorithm(RotationFormat8::QuatDropW_96, VectorFormat8::Vector3_96, RangeReductionFlags8::PerClip | RangeReductionFlags8::Translations),
			UniformlySampledAlgorithm(RotationFormat8::QuatDropW_96, VectorFormat8::Vector3_96, RangeReductionFlags8::PerClip | RangeReductionFlags8::Rotations | RangeReductionFlags8::Translations),
			UniformlySampledAlgorithm(RotationFormat8::QuatDropW_96, VectorFormat8::Vector3_48, RangeReductionFlags8::PerClip | RangeReductionFlags8::Translations),
			UniformlySampledAlgorithm(RotationFormat8::QuatDropW_96, VectorFormat8::Vector3_48, RangeReductionFlags8::PerClip | RangeReductionFlags8::Rotations | RangeReductionFlags8::Translations),
			UniformlySampledAlgorithm(RotationFormat8::QuatDropW_96, VectorFormat8::Vector3_32, RangeReductionFlags8::PerClip | RangeReductionFlags8::Translations),
			UniformlySampledAlgorithm(RotationFormat8::QuatDropW_96, VectorFormat8::Vector3_32, RangeReductionFlags8::PerClip | RangeReductionFlags8::Rotations | RangeReductionFlags8::Translations),
			UniformlySampledAlgorithm(RotationFormat8::QuatDropW_96, VectorFormat8::Vector3_Variable, RangeReductionFlags8::PerClip | RangeReductionFlags8::Translations),
			UniformlySampledAlgorithm(RotationFormat8::QuatDropW_96, VectorFormat8::Vector3_Variable, RangeReductionFlags8::PerClip | RangeReductionFlags8::Rotations | RangeReductionFlags8::Translations),

			UniformlySampledAlgorithm(RotationFormat8::QuatDropW_48, VectorFormat8::Vector3_96, RangeReductionFlags8::None),
			UniformlySampledAlgorithm(RotationFormat8::QuatDropW_48, VectorFormat8::Vector3_96, RangeReductionFlags8::PerClip | RangeReductionFlags8::Rotations),
			UniformlySampledAlgorithm(RotationFormat8::QuatDropW_48, VectorFormat8::Vector3_96, RangeReductionFlags8::PerClip | RangeReductionFlags8::Translations),
			UniformlySampledAlgorithm(RotationFormat8::QuatDropW_48, VectorFormat8::Vector3_96, RangeReductionFlags8::PerClip | RangeReductionFlags8::Rotations | RangeReductionFlags8::Translations),
			UniformlySampledAlgorithm(RotationFormat8::QuatDropW_48, VectorFormat8::Vector3_48, RangeReductionFlags8::PerClip | RangeReductionFlags8::Translations),
			UniformlySampledAlgorithm(RotationFormat8::QuatDropW_48, VectorFormat8::Vector3_48, RangeReductionFlags8::PerClip | RangeReductionFlags8::Rotations | RangeReductionFlags8::Translations),
			UniformlySampledAlgorithm(RotationFormat8::QuatDropW_48, VectorFormat8::Vector3_32, RangeReductionFlags8::PerClip | RangeReductionFlags8::Translations),
			UniformlySampledAlgorithm(RotationFormat8::QuatDropW_48, VectorFormat8::Vector3_32, RangeReductionFlags8::PerClip | RangeReductionFlags8::Rotations | RangeReductionFlags8::Translations),
			UniformlySampledAlgorithm(RotationFormat8::QuatDropW_48, VectorFormat8::Vector3_Variable, RangeReductionFlags8::PerClip | RangeReductionFlags8::Translations),
			UniformlySampledAlgorithm(RotationFormat8::QuatDropW_48, VectorFormat8::Vector3_Variable, RangeReductionFlags8::PerClip | RangeReductionFlags8::Rotations | RangeReductionFlags8::Translations),

			UniformlySampledAlgorithm(RotationFormat8::QuatDropW_32, VectorFormat8::Vector3_96, RangeReductionFlags8::None),
			UniformlySampledAlgorithm(RotationFormat8::QuatDropW_32, VectorFormat8::Vector3_96, RangeReductionFlags8::PerClip | RangeReductionFlags8::Rotations),
			UniformlySampledAlgorithm(RotationFormat8::QuatDropW_32, VectorFormat8::Vector3_96, RangeReductionFlags8::PerClip | RangeReductionFlags8::Translations),
			UniformlySampledAlgorithm(RotationFormat8::QuatDropW_32, VectorFormat8::Vector3_96, RangeReductionFlags8::PerClip | RangeReductionFlags8::Rotations | RangeReductionFlags8::Translations),
			UniformlySampledAlgorithm(RotationFormat8::QuatDropW_32, VectorFormat8::Vector3_48, RangeReductionFlags8::PerClip | RangeReductionFlags8::Translations),
			UniformlySampledAlgorithm(RotationFormat8::QuatDropW_32, VectorFormat8::Vector3_48, RangeReductionFlags8::PerClip | RangeReductionFlags8::Rotations | RangeReductionFlags8::Translations),
			UniformlySampledAlgorithm(RotationFormat8::QuatDropW_32, VectorFormat8::Vector3_32, RangeReductionFlags8::PerClip | RangeReductionFlags8::Translations),
			UniformlySampledAlgorithm(RotationFormat8::QuatDropW_32, VectorFormat8::Vector3_32, RangeReductionFlags8::PerClip | RangeReductionFlags8::Rotations | RangeReductionFlags8::Translations),
			UniformlySampledAlgorithm(RotationFormat8::QuatDropW_32, VectorFormat8::Vector3_Variable, RangeReductionFlags8::PerClip | RangeReductionFlags8::Translations),
			UniformlySampledAlgorithm(RotationFormat8::QuatDropW_32, VectorFormat8::Vector3_Variable, RangeReductionFlags8::PerClip | RangeReductionFlags8::Rotations | RangeReductionFlags8::Translations),

			UniformlySampledAlgorithm(RotationFormat8::QuatDropW_Variable, VectorFormat8::Vector3_96, RangeReductionFlags8::None),
			UniformlySampledAlgorithm(RotationFormat8::QuatDropW_Variable, VectorFormat8::Vector3_96, RangeReductionFlags8::PerClip | RangeReductionFlags8::Rotations),
			UniformlySampledAlgorithm(RotationFormat8::QuatDropW_Variable, VectorFormat8::Vector3_96, RangeReductionFlags8::PerClip | RangeReductionFlags8::Translations),
			UniformlySampledAlgorithm(RotationFormat8::QuatDropW_Variable, VectorFormat8::Vector3_96, RangeReductionFlags8::PerClip | RangeReductionFlags8::Rotations | RangeReductionFlags8::Translations),
			UniformlySampledAlgorithm(RotationFormat8::QuatDropW_Variable, VectorFormat8::Vector3_48, RangeReductionFlags8::PerClip | RangeReductionFlags8::Translations),
			UniformlySampledAlgorithm(RotationFormat8::QuatDropW_Variable, VectorFormat8::Vector3_48, RangeReductionFlags8::PerClip | RangeReductionFlags8::Rotations | RangeReductionFlags8::Translations),
			UniformlySampledAlgorithm(RotationFormat8::QuatDropW_Variable, VectorFormat8::Vector3_32, RangeReductionFlags8::PerClip | RangeReductionFlags8::Translations),
			UniformlySampledAlgorithm(RotationFormat8::QuatDropW_Variable, VectorFormat8::Vector3_32, RangeReductionFlags8::PerClip | RangeReductionFlags8::Rotations | RangeReductionFlags8::Translations),
			UniformlySampledAlgorithm(RotationFormat8::QuatDropW_Variable, VectorFormat8::Vector3_Variable, RangeReductionFlags8::PerClip | RangeReductionFlags8::Translations),
			UniformlySampledAlgorithm(RotationFormat8::QuatDropW_Variable, VectorFormat8::Vector3_Variable, RangeReductionFlags8::PerClip | RangeReductionFlags8::Rotations | RangeReductionFlags8::Translations),
		};

		for (UniformlySampledAlgorithm& algorithm : uniform_tests)
			try_algorithm(options, allocator, *clip.get(), *skeleton.get(), algorithm);
	}

	if (IsDebuggerPresent())
	{
		printf("Press any key to continue...\n");
		while (_kbhit() == 0);
	}

	return 0;
}
