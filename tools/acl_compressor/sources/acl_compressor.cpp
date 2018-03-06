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

#include "acl_compressor.h"

#include <assert.h>
#include <cstdlib>
#include <cstdio>
#include <cstdarg>

static void assert_impl(bool expression, const char* format, ...)
{
	if (expression)
		return;

	va_list args;
	va_start(args, format);

	std::vprintf(format, args);
	printf("\n");

	va_end(args);

#if !defined(NDEBUG)
	assert(expression);
#endif

	std::abort();
}

#if !defined(ACL_ASSERT) && !defined(ACL_NO_ERROR_CHECKS)
	#define ACL_ASSERT(expression, format, ...) assert_impl(expression, format, ## __VA_ARGS__)
	#define ACL_ENSURE(expression, format, ...) assert_impl(expression, format, ## __VA_ARGS__)
#endif

// Used to debug and validate that we compile without sjson-cpp
// Defaults to being enabled
#define ACL_ENABLE_STAT_WRITING		1

#if ACL_ENABLE_STAT_WRITING
	#include <sjson/writer.h>
#else
	namespace sjson { class ArrayWriter; }
#endif

#include <sjson/parser.h>

#include "acl/core/iallocator.h"
#include "acl/core/range_reduction_types.h"
#include "acl/core/ansi_allocator.h"
#include "acl/core/string.h"
#include "acl/compression/skeleton.h"
#include "acl/compression/animation_clip.h"
#include "acl/io/clip_reader.h"
#include "acl/compression/skeleton_error_metric.h"

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
#if defined(__ANDROID__)
	const char*		input_buffer;
	size_t			input_buffer_size;
	const char*		config_buffer;
	size_t			config_buffer_size;
#else
	const char*		input_filename;
	const char*		config_filename;
#endif

	bool			output_stats;
	const char*		output_stats_filename;
	std::FILE*		output_stats_file;

	bool			regression_testing;

	//////////////////////////////////////////////////////////////////////////

	Options()
#if defined(__ANDROID__)
		: input_buffer(nullptr)
		, input_buffer_size(0)
		, config_buffer(nullptr)
		, config_buffer_size(0)
#else
		: input_filename(nullptr)
		, config_filename(nullptr)
#endif
		, output_stats(false)
		, output_stats_filename(nullptr)
		, output_stats_file(nullptr)
		, regression_testing(false)
	{}

	Options(Options&& other)
#if defined(__ANDROID__)
		: input_buffer(other.input_buffer)
		, input_buffer_size(other.input_buffer_size)
		, config_buffer(other.config_buffer)
		, config_buffer_size(other.config_buffer_size)
#else
		: input_filename(other.input_filename)
		, config_filename(other.config_filename)
#endif
		, output_stats(other.output_stats)
		, output_stats_filename(other.output_stats_filename)
		, output_stats_file(other.output_stats_file)
		, regression_testing(other.regression_testing)
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
#if defined(__ANDROID__)
		std::swap(input_buffer, rhs.input_buffer);
		std::swap(input_buffer_size, rhs.input_buffer_size);
		std::swap(config_buffer, rhs.config_buffer);
		std::swap(config_buffer_size, rhs.config_buffer_size);
#else
		std::swap(input_filename, rhs.input_filename);
		std::swap(config_filename, rhs.config_filename);
#endif
		std::swap(output_stats, rhs.output_stats);
		std::swap(output_stats_filename, rhs.output_stats_filename);
		std::swap(output_stats_file, rhs.output_stats_file);
		std::swap(regression_testing, rhs.regression_testing);
		return *this;
	}

	Options(const Options&) = delete;
	Options& operator=(const Options&) = delete;

	void open_output_stats_file()
	{
		std::FILE* file = nullptr;
		if (output_stats_filename != nullptr)
		{
#ifdef _WIN32
			fopen_s(&file, output_stats_filename, "w");
#else
			file = fopen(output_stats_filename, "w");
#endif
		}
		output_stats_file = file != nullptr ? file : stdout;
	}
};

constexpr const char* k_acl_input_file_option = "-acl=";
constexpr const char* k_config_input_file_option = "-config=";
constexpr const char* k_stats_output_option = "-stats";
constexpr const char* k_regression_test_option = "-test";

static bool parse_options(int argc, char** argv, Options& options)
{
	for (int arg_index = 1; arg_index < argc; ++arg_index)
	{
		const char* argument = argv[arg_index];

		size_t option_length = std::strlen(k_acl_input_file_option);
		if (std::strncmp(argument, k_acl_input_file_option, option_length) == 0)
		{
#if defined(__ANDROID__)
			unsigned int buffer_size;
			sscanf(argument + option_length, "@%u,%p", &buffer_size, &options.input_buffer);
			options.input_buffer_size = buffer_size;
#else
			options.input_filename = argument + option_length;
#endif
			continue;
		}

		option_length = std::strlen(k_config_input_file_option);
		if (std::strncmp(argument, k_config_input_file_option, option_length) == 0)
		{
#if defined(__ANDROID__)
			unsigned int buffer_size;
			sscanf(argument + option_length, "@%u,%p", &buffer_size, &options.config_buffer);
			options.config_buffer_size = buffer_size;
#else
			options.config_filename = argument + option_length;
#endif
			continue;
		}

		option_length = std::strlen(k_stats_output_option);
		if (std::strncmp(argument, k_stats_output_option, option_length) == 0)
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

		option_length = std::strlen(k_regression_test_option);
		if (std::strncmp(argument, k_regression_test_option, option_length) == 0)
		{
			options.regression_testing = true;
			continue;
		}

		printf("Unrecognized option %s\n", argument);
		return false;
	}

#if defined(__ANDROID__)
	if (options.input_buffer == nullptr || options.input_buffer_size == 0)
#else
	if (options.input_filename == nullptr || std::strlen(options.input_filename) == 0)
#endif
	{
		printf("An input file is required.\n");
		return false;
	}

	return true;
}

static void validate_accuracy(IAllocator& allocator, const AnimationClip& clip, const RigidSkeleton& skeleton, const CompressedClip& compressed_clip, IAlgorithm& algorithm, double regression_error_threshold)
{
	const uint16_t num_bones = clip.get_num_bones();
	const float clip_duration = clip.get_duration();
	const float sample_rate = float(clip.get_sample_rate());
	const uint32_t num_samples = calculate_num_samples(clip_duration, clip.get_sample_rate());
	const ISkeletalErrorMetric& error_metric = *algorithm.get_compression_settings().error_metric;

	Transform_32* raw_pose_transforms = allocate_type_array<Transform_32>(allocator, num_bones);
	Transform_32* lossy_pose_transforms = allocate_type_array<Transform_32>(allocator, num_bones);
	void* context = algorithm.allocate_decompression_context(allocator, compressed_clip);

	// Regression test
	for (uint32_t sample_index = 0; sample_index < num_samples; ++sample_index)
	{
		const float sample_time = min(float(sample_index) / sample_rate, clip_duration);

		clip.sample_pose(sample_time, raw_pose_transforms, num_bones);
		algorithm.decompress_pose(compressed_clip, context, sample_time, lossy_pose_transforms, num_bones);

		for (uint16_t bone_index = 0; bone_index < num_bones; ++bone_index)
		{
			const float error = error_metric.calculate_object_bone_error(skeleton, raw_pose_transforms, lossy_pose_transforms, bone_index);
			ACL_ENSURE(error < regression_error_threshold, "Error too high for bone %u: %f at time %f", bone_index, error, sample_time);
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
}

static void try_algorithm(const Options& options, IAllocator& allocator, const AnimationClip& clip, const RigidSkeleton& skeleton, IAlgorithm &algorithm, StatLogging logging, sjson::ArrayWriter* runs_writer, double regression_error_threshold)
{
	auto try_algorithm_impl = [&](sjson::ObjectWriter* stats_writer)
	{
		OutputStats stats(logging, stats_writer);
		CompressedClip* compressed_clip = algorithm.compress_clip(allocator, clip, skeleton, stats);

		ACL_ENSURE(compressed_clip->is_valid(true), "Compressed clip is invalid");

		if (options.regression_testing)
			validate_accuracy(allocator, clip, skeleton, *compressed_clip, algorithm, regression_error_threshold);

		allocator.deallocate(compressed_clip, compressed_clip->get_size());
	};

#if defined(SJSON_CPP_WRITER)
	if (runs_writer != nullptr)
		runs_writer->push([&](sjson::ObjectWriter& writer) { try_algorithm_impl(&writer); });
	else
#endif
		try_algorithm_impl(nullptr);
}

static bool read_clip(IAllocator& allocator, const Options& options,
					  std::unique_ptr<AnimationClip, Deleter<AnimationClip>>& clip,
					  std::unique_ptr<RigidSkeleton, Deleter<RigidSkeleton>>& skeleton)
{
#if defined(__ANDROID__)
	ClipReader reader(allocator, options.input_buffer, options.input_buffer_size - 1);
#else
	std::ifstream t(options.input_filename);
	std::stringstream buffer;
	buffer << t.rdbuf();
	std::string str = buffer.str();

	ClipReader reader(allocator, str.c_str(), str.length());
#endif

	if (!reader.read(skeleton) || !reader.read(clip, *skeleton))
	{
		ClipReaderError err = reader.get_error();
		printf("\nError on line %d column %d: %s\n", err.line, err.column, err.get_description());
		return false;
	}

	return true;
}

static bool read_config(IAllocator& allocator, const Options& options, AlgorithmType8& out_algorithm_type, CompressionSettings& out_settings, double& out_regression_error_threshold)
{
#if defined(__ANDROID__)
	sjson::Parser parser(options.config_buffer, options.config_buffer_size - 1);
#else
	std::ifstream t(options.config_filename);
	std::stringstream buffer;
	buffer << t.rdbuf();
	std::string str = buffer.str();

	sjson::Parser parser(str.c_str(), str.length());
#endif

	double version = 0.0;
	if (!parser.read("version", version))
	{
		uint32_t line, column;
		parser.get_position(line, column);

		printf("Error on line %d column %d: Missing config version\n", line, column);
		return false;
	}

	if (version != 1.0)
	{
		printf("Unsupported version: %f\n", version);
		return false;
	}

	sjson::StringView algorithm_name;
	if (!parser.read("algorithm_name", algorithm_name))
	{
		uint32_t line, column;
		parser.get_position(line, column);

		printf("Error on line %d column %d: Missing algorithm name\n", line, column);
		return false;
	}

	if (!get_algorithm_type(algorithm_name.c_str(), out_algorithm_type))
	{
		printf("Invalid algorithm name: %s\n", String(allocator, algorithm_name.c_str(), algorithm_name.size()).c_str());
		return false;
	}

	sjson::StringView rotation_format;
	if (parser.try_read("rotation_format", rotation_format, nullptr))
	{
		if (!get_rotation_format(rotation_format.c_str(), out_settings.rotation_format))
		{
			printf("Invalid rotation format: %s\n", String(allocator, rotation_format.c_str(), rotation_format.size()).c_str());
			return false;
		}
	}

	sjson::StringView translation_format;
	if (parser.try_read("translation_format", translation_format, nullptr))
	{
		if (!get_vector_format(translation_format.c_str(), out_settings.translation_format))
		{
			printf("Invalid translation format: %s\n", String(allocator, translation_format.c_str(), translation_format.size()).c_str());
			return false;
		}
	}

	sjson::StringView scale_format;
	if (parser.try_read("scale_format", scale_format, nullptr))
	{
		if (!get_vector_format(scale_format.c_str(), out_settings.scale_format))
		{
			printf("Invalid scale format: %s\n", String(allocator, scale_format.c_str(), scale_format.size()).c_str());
			return false;
		}
	}

	bool rotation_range_reduction;
	if (parser.try_read("rotation_range_reduction", rotation_range_reduction, false) && rotation_range_reduction)
		out_settings.range_reduction |= RangeReductionFlags8::Rotations;

	bool translation_range_reduction;
	if (parser.try_read("translation_range_reduction", translation_range_reduction, false) && translation_range_reduction)
		out_settings.range_reduction |= RangeReductionFlags8::Translations;

	bool scale_range_reduction;
	if (parser.try_read("scale_range_reduction", scale_range_reduction, false) && scale_range_reduction)
		out_settings.range_reduction |= RangeReductionFlags8::Scales;

	if (parser.object_begins("segmenting"))
	{
		parser.try_read("enabled", out_settings.segmenting.enabled, false);

		if (parser.try_read("rotation_range_reduction", rotation_range_reduction, false) && rotation_range_reduction)
			out_settings.segmenting.range_reduction |= RangeReductionFlags8::Rotations;

		if (parser.try_read("translation_range_reduction", translation_range_reduction, false) && translation_range_reduction)
			out_settings.segmenting.range_reduction |= RangeReductionFlags8::Translations;

		if (parser.try_read("scale_range_reduction", scale_range_reduction, false) && scale_range_reduction)
			out_settings.segmenting.range_reduction |= RangeReductionFlags8::Scales;

		if (!parser.object_ends())
		{
			uint32_t line, column;
			parser.get_position(line, column);

			printf("Error on line %d column %d: Expected segmenting object to end\n", line, column);
			return false;
		}
	}

	parser.try_read("regression_error_threshold", out_regression_error_threshold, 0.0);

	if (!parser.remainder_is_comments_and_whitespace())
	{
		uint32_t line, column;
		parser.get_position(line, column);

		printf("Error on line %d column %d: Expected end of file\n", line, column);
		return false;
	}

	return true;
}

static int safe_main_impl(int argc, char* argv[])
{
	Options options;

	if (!parse_options(argc, argv, options))
		return -1;

	ANSIAllocator allocator;
	std::unique_ptr<AnimationClip, Deleter<AnimationClip>> clip;
	std::unique_ptr<RigidSkeleton, Deleter<RigidSkeleton>> skeleton;

	if (!read_clip(allocator, options, clip, skeleton))
		return -1;

	bool use_external_config = false;
	AlgorithmType8 external_algorithm_type = AlgorithmType8::UniformlySampled;
	CompressionSettings external_settings;
	TransformErrorMetric default_error_metric;
	double regression_error_threshold;

#if defined(__ANDROID__)
	if (options.config_buffer != nullptr && options.config_buffer_size != 0)
#else
	if (options.config_filename != nullptr && std::strlen(options.config_filename) != 0)
#endif
	{
		if (!read_config(allocator, options, external_algorithm_type, external_settings, regression_error_threshold))
			return -1;

		use_external_config = true;
		external_settings.error_metric = &default_error_metric;
	}

	// Compress & Decompress
	auto exec_algos = [&](sjson::ArrayWriter* runs_writer)
	{
		StatLogging logging = options.output_stats ? StatLogging::Summary : StatLogging::None;

		if (use_external_config)
		{
			ACL_ENSURE(external_algorithm_type == AlgorithmType8::UniformlySampled, "Only UniformlySampled is supported for now");

			UniformlySampledAlgorithm algorithm(external_settings);
			try_algorithm(options, allocator, *clip.get(), *skeleton.get(), algorithm, logging, runs_writer, regression_error_threshold);
		}
		else
		{
			// Use defaults
			bool use_segmenting_options[] = { false, true };
			for (size_t segmenting_option_index = 0; segmenting_option_index < get_array_size(use_segmenting_options); ++segmenting_option_index)
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
					try_algorithm(options, allocator, *clip.get(), *skeleton.get(), algorithm, logging, runs_writer, regression_error_threshold);
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
					try_algorithm(options, allocator, *clip.get(), *skeleton.get(), algorithm, logging, runs_writer, regression_error_threshold);
			}
		}
	};

#if defined(SJSON_CPP_WRITER)
	if (options.output_stats)
	{
		sjson::FileStreamWriter stream_writer(options.output_stats_file);
		sjson::Writer writer(stream_writer);

		writer["runs"] = [&](sjson::ArrayWriter& writer) { exec_algos(&writer); };
	}
	else
#endif
		exec_algos(nullptr);

	return 0;
}

#ifdef _WIN32
static LONG WINAPI unhandled_exception_filter(EXCEPTION_POINTERS *info)
{
	if (IsDebuggerPresent())
		return EXCEPTION_CONTINUE_SEARCH;
	return EXCEPTION_EXECUTE_HANDLER;
}
#endif

int main_impl(int argc, char* argv[])
{
#ifdef _WIN32
	// Disables Windows OS generated error dialogs and reporting
	SetErrorMode(SEM_FAILCRITICALERRORS);
	SetUnhandledExceptionFilter(&unhandled_exception_filter);
	_set_abort_behavior(0, _CALL_REPORTFAULT);
#endif

	int result = -1;
	try
	{
		result = safe_main_impl(argc, argv);
	}
	catch (const std::runtime_error& exception)
	{
		printf("Exception occurred: %s", exception.what());
		result = -1;
	}
	catch (...)
	{
		printf("Unknown exception occurred");
		result = -1;
	}

#ifdef _WIN32
	if (IsDebuggerPresent())
	{
		printf("Press any key to continue...\n");
		while (_kbhit() == 0);
	}
#endif

	return result;
}
