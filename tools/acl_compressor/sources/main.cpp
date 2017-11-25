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
#include "acl/core/range_reduction_types.h"
#include "acl/compression/skeleton.h"
#include "acl/compression/animation_clip.h"
#include "acl/io/clip_reader.h"
#include "acl/compression/skeleton_error_metric.h"
#include "acl/sjson/sjson_writer.h"

#include "acl/algorithm/uniformly_sampled/algorithm.h"

#include <cstring>
#include <cstdio>
#include <fstream>
#include <streambuf>
#include <sstream>
#include <string>
#include <memory>

#ifdef _WIN32
#include <conio.h>
#if !defined(_WINDOWS_)
// The below excludes some other unused services from the windows headers -- see windows.h for details.
#define NOGDICAPMASKS            // CC_*, LC_*, PC_*, CP_*, TC_*, RC_
#define NOVIRTUALKEYCODES        // VK_*
#define NOWINMESSAGES            // WM_*, EM_*, LB_*, CB_*
#define NOWINSTYLES                // WS_*, CS_*, ES_*, LBS_*, SBS_*, CBS_*
#define NOSYSMETRICS            // SM_*
#define NOMENUS                    // MF_*
#define NOICONS                    // IDI_*
#define NOKEYSTATES                // MK_*
#define NOSYSCOMMANDS            // SC_*
#define NORASTEROPS                // Binary and Tertiary raster ops
#define NOSHOWWINDOW            // SW_*
#define OEMRESOURCE                // OEM Resource values
#define NOATOM                    // Atom Manager routines
#define NOCLIPBOARD                // Clipboard routines
#define NOCOLOR                    // Screen colors
#define NOCTLMGR                // Control and Dialog routines
#define NODRAWTEXT                // DrawText() and DT_*
#define NOGDI                    // All GDI #defines and routines
#define NOKERNEL                // All KERNEL #defines and routines
#define NOUSER                    // All USER #defines and routines
#define NONLS                    // All NLS #defines and routines
#define NOMB                    // MB_* and MessageBox()
#define NOMEMMGR                // GMEM_*, LMEM_*, GHND, LHND, associated routines
#define NOMETAFILE                // typedef METAFILEPICT
#define NOMINMAX                // Macros min(a,b) and max(a,b)
#define NOMSG                    // typedef MSG and associated routines
#define NOOPENFILE                // OpenFile(), OemToAnsi, AnsiToOem, and OF_*
#define NOSCROLL                // SB_* and scrolling routines
#define NOSERVICE                // All Service Controller routines, SERVICE_ equates, etc.
#define NOSOUND                    // Sound driver routines
#define NOTEXTMETRIC            // typedef TEXTMETRIC and associated routines
#define NOWH                    // SetWindowsHook and WH_*
#define NOWINOFFSETS            // GWL_*, GCL_*, associated routines
#define NOCOMM                    // COMM driver routines
#define NOKANJI                    // Kanji support stuff.
#define NOHELP                    // Help engine interface.
#define NOPROFILER                // Profiler interface.
#define NODEFERWINDOWPOS        // DeferWindowPos routines
#define NOMCX                    // Modem Configuration Extensions
#define NOCRYPT
#define NOTAPE
#define NOIMAGE
#define NOPROXYSTUB
#define NORPC

#include <windows.h>
#endif    // _WINDOWS_
#endif    // _WIN32

using namespace acl;

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
		return *this;
	}

	Options(const Options&) = delete;
	Options& operator=(const Options&) = delete;

	void open_output_stats_file()
	{
		std::FILE* file = nullptr;
		if (output_stats_filename != nullptr)
			file = fopen(output_stats_filename, "w");
		output_stats_file = file != nullptr ? file : stdout;
	}
};

constexpr const char* ACL_INPUT_FILE_OPTION = "-acl=";
constexpr const char* STATS_OUTPUT_OPTION = "-stats";

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
			if (argument[option_length] == '=')
			{
				options.output_stats_filename = argument + option_length + 1;
				size_t filename_len = std::strlen(options.output_stats_filename);
				if (filename_len < 6 || strncmp(options.output_stats_filename + filename_len - 6, ".sjson", 6) != 0)
				{
					printf("Stats output file must be an SJSON file.\n");
					return false;
				}
			}
			else
				options.output_stats_filename = nullptr;

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

static void unit_test(Allocator& allocator, const AnimationClip& clip, const RigidSkeleton& skeleton, const CompressedClip& compressed_clip, IAlgorithm& algorithm)
{
#if defined(ACL_USE_ERROR_CHECKS)
	uint16_t num_bones = clip.get_num_bones();
	float clip_duration = clip.get_duration();
	float sample_rate = float(clip.get_sample_rate());
	uint32_t num_samples = calculate_num_samples(clip_duration, clip.get_sample_rate());

	Transform_32* raw_pose_transforms = allocate_type_array<Transform_32>(allocator, num_bones);
	Transform_32* lossy_pose_transforms = allocate_type_array<Transform_32>(allocator, num_bones);
	void* context = algorithm.allocate_decompression_context(allocator, compressed_clip);

	for (uint32_t sample_index = 0; sample_index < num_samples; ++sample_index)
	{
		float sample_time = min(float(sample_index) / sample_rate, clip_duration);

		clip.sample_pose(sample_time, raw_pose_transforms, num_bones);
		algorithm.decompress_pose(compressed_clip, context, sample_time, lossy_pose_transforms, num_bones);

		for (uint16_t bone_index = 0; bone_index < num_bones; ++bone_index)
		{
			float error = calculate_object_bone_error(skeleton, raw_pose_transforms, lossy_pose_transforms, bone_index);
			ACL_ENSURE(error < 10.0f, "Error too high for bone %u: %f at time %f", bone_index, error, sample_time);
		}
	}

	// Unit test
	{
		// Validate that the decoder can decode a single bone at a particular time
		// Use the last bone and last sample time to ensure we can seek properly
		uint16_t sample_bone_index = num_bones - 1;
		float sample_time = clip.get_duration();
		Quat_32 test_rotation;
		Vector4_32 test_translation;
		Vector4_32 test_scale;
		algorithm.decompress_bone(compressed_clip, context, sample_time, sample_bone_index, &test_rotation, &test_translation, &test_scale);
		ACL_ENSURE(quat_near_equal(test_rotation, lossy_pose_transforms[sample_bone_index].rotation), "Failed to sample bone index: %u", sample_bone_index);
		ACL_ENSURE(vector_all_near_equal3(test_translation, lossy_pose_transforms[sample_bone_index].translation), "Failed to sample bone index: %u", sample_bone_index);
		ACL_ENSURE(vector_all_near_equal3(test_scale, lossy_pose_transforms[sample_bone_index].scale), "Failed to sample bone index: %u", sample_bone_index);
	}

	deallocate_type_array(allocator, raw_pose_transforms, num_bones);
	deallocate_type_array(allocator, lossy_pose_transforms, num_bones);
	algorithm.deallocate_decompression_context(allocator, context);
#endif
}

static void try_algorithm(const Options& options, Allocator& allocator, const AnimationClip& clip, const RigidSkeleton& skeleton, IAlgorithm &algorithm, StatLogging logging, SJSONArrayWriter* runs_writer)
{
	auto try_algorithm_impl = [&](SJSONObjectWriter* stats_writer)
	{
		OutputStats stats(logging, stats_writer);
		CompressedClip* compressed_clip = algorithm.compress_clip(allocator, clip, skeleton, stats);

		ACL_ENSURE(compressed_clip->is_valid(true), "Compressed clip is invalid");

		unit_test(allocator, clip, skeleton, *compressed_clip, algorithm);

		allocator.deallocate(compressed_clip, compressed_clip->get_size());
	};

	if (runs_writer != nullptr)
		runs_writer->push_object([&](SJSONObjectWriter& writer) { try_algorithm_impl(&writer); });
	else
		try_algorithm_impl(nullptr);
	
}

#ifndef _WIN32
static int _kbhit()
{
	return 0;
}

static bool IsDebuggerPresent()
{
	return false;
}
#endif

static bool read_clip(Allocator& allocator, const char* filename,
					  std::unique_ptr<AnimationClip, Deleter<AnimationClip>>& clip,
					  std::unique_ptr<RigidSkeleton, Deleter<RigidSkeleton>>& skeleton)
{
	if (IsDebuggerPresent())
		printf("Reading ACL input clip...");

	ScopeProfiler io_read_timer;

	std::ifstream t(filename);
	std::stringstream buffer;
	buffer << t.rdbuf();
	std::string str = buffer.str();

	io_read_timer.stop();

	if (IsDebuggerPresent())
	{
		printf(" Done in %.1f ms!\n", io_read_timer.get_elapsed_milliseconds());
		printf("Parsing ACL input clip...");
	}

	ScopeProfiler clip_reader_timer;
	ClipReader reader(allocator, str.c_str(), str.length());

	if (!reader.read(skeleton) || !reader.read(clip, *skeleton))
	{
		ClipReaderError err = reader.get_error();
		printf("\nError on line %d column %d: %s\n", err.line, err.column, err.get_description());
		return false;
	}

	clip_reader_timer.stop();

	if (IsDebuggerPresent())
		printf(" Done in %.1f ms!\n", clip_reader_timer.get_elapsed_milliseconds());

	return true;
}

static int main_impl(int argc, char** argv)
{
	Options options;

	if (!parse_options(argc, argv, options))
		return -1;

	Allocator allocator;
	std::unique_ptr<AnimationClip, Deleter<AnimationClip>> clip;
	std::unique_ptr<RigidSkeleton, Deleter<RigidSkeleton>> skeleton;

	if (!read_clip(allocator, options.input_filename, clip, skeleton))
		return -1;

	// Compress & Decompress
	auto exec_algos = [&](SJSONArrayWriter* runs_writer)
	{
		bool use_segmenting_options[] = { false, true };
		StatLogging logging = StatLogging::Summary;

		for (size_t segmenting_option_index = 0; segmenting_option_index < sizeof(use_segmenting_options) / sizeof(use_segmenting_options[0]); ++segmenting_option_index)
		{
			bool use_segmenting = use_segmenting_options[segmenting_option_index];

			UniformlySampledAlgorithm uniform_tests[] =
			{
				UniformlySampledAlgorithm(RotationFormat8::Quat_128, VectorFormat8::Vector3_96, VectorFormat8::Vector3_96, RangeReductionFlags8::None, use_segmenting),
				UniformlySampledAlgorithm(RotationFormat8::Quat_128, VectorFormat8::Vector3_96, VectorFormat8::Vector3_96, RangeReductionFlags8::Rotations, use_segmenting),
				UniformlySampledAlgorithm(RotationFormat8::Quat_128, VectorFormat8::Vector3_96, VectorFormat8::Vector3_96, RangeReductionFlags8::Translations, use_segmenting),
				UniformlySampledAlgorithm(RotationFormat8::Quat_128, VectorFormat8::Vector3_96, VectorFormat8::Vector3_96, RangeReductionFlags8::Rotations | RangeReductionFlags8::Translations, use_segmenting),

				UniformlySampledAlgorithm(RotationFormat8::QuatDropW_96, VectorFormat8::Vector3_96, VectorFormat8::Vector3_96, RangeReductionFlags8::None, use_segmenting),
				UniformlySampledAlgorithm(RotationFormat8::QuatDropW_96, VectorFormat8::Vector3_96, VectorFormat8::Vector3_96, RangeReductionFlags8::Rotations, use_segmenting),
				UniformlySampledAlgorithm(RotationFormat8::QuatDropW_96, VectorFormat8::Vector3_96, VectorFormat8::Vector3_96, RangeReductionFlags8::Translations, use_segmenting),
				UniformlySampledAlgorithm(RotationFormat8::QuatDropW_96, VectorFormat8::Vector3_96, VectorFormat8::Vector3_96, RangeReductionFlags8::Rotations | RangeReductionFlags8::Translations, use_segmenting),

				UniformlySampledAlgorithm(RotationFormat8::QuatDropW_Variable, VectorFormat8::Vector3_Variable, VectorFormat8::Vector3_96, RangeReductionFlags8::Translations, use_segmenting),
				UniformlySampledAlgorithm(RotationFormat8::QuatDropW_Variable, VectorFormat8::Vector3_Variable, VectorFormat8::Vector3_96, RangeReductionFlags8::Rotations | RangeReductionFlags8::Translations, use_segmenting),

				UniformlySampledAlgorithm(RotationFormat8::QuatDropW_Variable, VectorFormat8::Vector3_Variable, VectorFormat8::Vector3_Variable, RangeReductionFlags8::Rotations | RangeReductionFlags8::Translations | RangeReductionFlags8::Scales, use_segmenting),
			};

			for (UniformlySampledAlgorithm& algorithm : uniform_tests)
				try_algorithm(options, allocator, *clip.get(), *skeleton.get(), algorithm, logging, runs_writer);
		}

		{
			UniformlySampledAlgorithm uniform_tests[] =
			{
				UniformlySampledAlgorithm(RotationFormat8::Quat_128, VectorFormat8::Vector3_96, VectorFormat8::Vector3_96, RangeReductionFlags8::Rotations, true, RangeReductionFlags8::Rotations),
				UniformlySampledAlgorithm(RotationFormat8::Quat_128, VectorFormat8::Vector3_96, VectorFormat8::Vector3_96, RangeReductionFlags8::Translations, true, RangeReductionFlags8::Translations),
				UniformlySampledAlgorithm(RotationFormat8::Quat_128, VectorFormat8::Vector3_96, VectorFormat8::Vector3_96, RangeReductionFlags8::Rotations | RangeReductionFlags8::Translations, true, RangeReductionFlags8::Rotations | RangeReductionFlags8::Translations),

				UniformlySampledAlgorithm(RotationFormat8::QuatDropW_96, VectorFormat8::Vector3_96, VectorFormat8::Vector3_96, RangeReductionFlags8::Rotations, true, RangeReductionFlags8::Rotations),
				UniformlySampledAlgorithm(RotationFormat8::QuatDropW_96, VectorFormat8::Vector3_96, VectorFormat8::Vector3_96, RangeReductionFlags8::Translations, true, RangeReductionFlags8::Translations),
				UniformlySampledAlgorithm(RotationFormat8::QuatDropW_96, VectorFormat8::Vector3_96, VectorFormat8::Vector3_96, RangeReductionFlags8::Rotations | RangeReductionFlags8::Translations, true, RangeReductionFlags8::Rotations | RangeReductionFlags8::Translations),
				UniformlySampledAlgorithm(RotationFormat8::QuatDropW_Variable, VectorFormat8::Vector3_Variable, VectorFormat8::Vector3_96, RangeReductionFlags8::Translations, true, RangeReductionFlags8::Translations),
				UniformlySampledAlgorithm(RotationFormat8::QuatDropW_Variable, VectorFormat8::Vector3_Variable, VectorFormat8::Vector3_96, RangeReductionFlags8::Rotations | RangeReductionFlags8::Translations, true, RangeReductionFlags8::Rotations | RangeReductionFlags8::Translations),

				UniformlySampledAlgorithm(RotationFormat8::QuatDropW_Variable, VectorFormat8::Vector3_Variable, VectorFormat8::Vector3_Variable, RangeReductionFlags8::Rotations | RangeReductionFlags8::Translations | RangeReductionFlags8::Scales, true, RangeReductionFlags8::Rotations | RangeReductionFlags8::Translations | RangeReductionFlags8::Scales),
			};

			for (UniformlySampledAlgorithm& algorithm : uniform_tests)
				try_algorithm(options, allocator, *clip.get(), *skeleton.get(), algorithm, logging, runs_writer);
		}
	};

	if (options.output_stats)
	{
		SJSONFileStreamWriter stream_writer(options.output_stats_file);
		SJSONWriter writer(stream_writer);

		writer["runs"] = [&](SJSONArrayWriter& writer) { exec_algos(&writer); };
	}
	else
		exec_algos(nullptr);

	return 0;
}

int main(int argc, char** argv)
{
	int result = main_impl(argc, argv);

	if (IsDebuggerPresent())
	{
		printf("Press any key to continue...\n");
		while (_kbhit() == 0);
	}

	return result;
}
