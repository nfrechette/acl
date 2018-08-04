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
#include "acl/compression/utils.h"
#include "acl/io/clip_reader.h"
#include "acl/io/clip_writer.h"							// Included just so we compile it to test for basic errors
#include "acl/compression/skeleton_error_metric.h"

#include "acl/algorithm/uniformly_sampled/encoder.h"
#include "acl/algorithm/uniformly_sampled/decoder.h"

#include <cstring>
#include <cstdio>
#include <fstream>
#include <streambuf>
#include <sstream>
#include <string>
#include <memory>

#ifdef _WIN32

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
	#include <conio.h>

#endif    // _WIN32

using namespace acl;

struct Options
{
#if defined(__ANDROID__)
	const char*		input_buffer;
	size_t			input_buffer_size;
	bool			input_buffer_binary;
	const char*		config_buffer;
	size_t			config_buffer_size;
#else
	const char*		input_filename;
	const char*		config_filename;
#endif

	bool			output_stats;
	const char*		output_stats_filename;
	std::FILE*		output_stats_file;

	const char*		output_bin_filename;

	bool			regression_testing;
	bool			profile_decompression;
	bool			exhaustive_compression;

	bool			is_bind_pose_relative;
	bool			is_bind_pose_additive0;
	bool			is_bind_pose_additive1;

	bool			stat_detailed_output;
	bool			stat_exhaustive_output;

	//////////////////////////////////////////////////////////////////////////

	Options()
#if defined(__ANDROID__)
		: input_buffer(nullptr)
		, input_buffer_size(0)
		, input_buffer_binary(false)
		, config_buffer(nullptr)
		, config_buffer_size(0)
#else
		: input_filename(nullptr)
		, config_filename(nullptr)
#endif
		, output_stats(false)
		, output_stats_filename(nullptr)
		, output_stats_file(nullptr)
		, output_bin_filename(nullptr)
		, regression_testing(false)
		, profile_decompression(false)
		, exhaustive_compression(false)
		, is_bind_pose_relative(false)
		, is_bind_pose_additive0(false)
		, is_bind_pose_additive1(false)
		, stat_detailed_output(false)
		, stat_exhaustive_output(false)
	{}

	Options(Options&& other)
#if defined(__ANDROID__)
		: input_buffer(other.input_buffer)
		, input_buffer_size(other.input_buffer_size)
		, input_buffer_binary(other.input_buffer_binary)
		, config_buffer(other.config_buffer)
		, config_buffer_size(other.config_buffer_size)
#else
		: input_filename(other.input_filename)
		, config_filename(other.config_filename)
#endif
		, output_stats(other.output_stats)
		, output_stats_filename(other.output_stats_filename)
		, output_stats_file(other.output_stats_file)
		, output_bin_filename(other.output_bin_filename)
		, regression_testing(other.regression_testing)
		, profile_decompression(other.profile_decompression)
		, exhaustive_compression(other.exhaustive_compression)
		, is_bind_pose_relative(other.is_bind_pose_relative)
		, is_bind_pose_additive0(other.is_bind_pose_additive0)
		, is_bind_pose_additive1(other.is_bind_pose_additive1)
		, stat_detailed_output(other.stat_detailed_output)
		, stat_exhaustive_output(other.stat_exhaustive_output)
	{
		new (&other) Options();
	}

	~Options()
	{
		if (output_stats_file != nullptr && output_stats_file != stdout)
			std::fclose(output_stats_file);
	}

	Options& operator=(Options&& other)
	{
#if defined(__ANDROID__)
		std::swap(input_buffer, other.input_buffer);
		std::swap(input_buffer_size, other.input_buffer_size);
		std::swap(input_buffer_binary, other.input_buffer_binary);
		std::swap(config_buffer, other.config_buffer);
		std::swap(config_buffer_size, other.config_buffer_size);
#else
		std::swap(input_filename, other.input_filename);
		std::swap(config_filename, other.config_filename);
#endif
		std::swap(output_stats, other.output_stats);
		std::swap(output_stats_filename, other.output_stats_filename);
		std::swap(output_stats_file, other.output_stats_file);
		std::swap(output_bin_filename, other.output_bin_filename);
		std::swap(regression_testing, other.regression_testing);
		std::swap(profile_decompression, other.profile_decompression);
		std::swap(exhaustive_compression, other.exhaustive_compression);
		std::swap(is_bind_pose_relative, other.is_bind_pose_relative);
		std::swap(is_bind_pose_additive0, other.is_bind_pose_additive0);
		std::swap(is_bind_pose_additive1, other.is_bind_pose_additive1);
		std::swap(stat_detailed_output, other.stat_detailed_output);
		std::swap(stat_exhaustive_output, other.stat_exhaustive_output);
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

static constexpr const char* k_acl_input_file_option = "-acl=";
static constexpr const char* k_config_input_file_option = "-config=";
static constexpr const char* k_stats_output_option = "-stats";
static constexpr const char* k_bin_output_option = "-out=";
static constexpr const char* k_regression_test_option = "-test";
static constexpr const char* k_profile_decompression_option = "-decomp";
static constexpr const char* k_exhaustive_compression_option = "-exhaustive";
static constexpr const char* k_bind_pose_relative_option = "-bind_rel";
static constexpr const char* k_bind_pose_additive0_option = "-bind_add0";
static constexpr const char* k_bind_pose_additive1_option = "-bind_add1";
static constexpr const char* k_stat_detailed_output_option = "-stat_detailed";
static constexpr const char* k_stat_exhaustive_output_option = "-stat_exhaustive";

bool is_acl_sjson_file(const char* filename)
{
	const size_t filename_len = std::strlen(filename);
	return filename_len >= 10 && strncmp(filename + filename_len - 10, ".acl.sjson", 10) == 0;
}

bool is_acl_bin_file(const char* filename)
{
	const size_t filename_len = std::strlen(filename);
	return filename_len >= 8 && strncmp(filename + filename_len - 8, ".acl.bin", 8) == 0;
}

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
			int is_acl_bin_buffer;
			sscanf(argument + option_length, "@%u,%p,%d", &buffer_size, &options.input_buffer, &is_acl_bin_buffer);
			options.input_buffer_size = buffer_size;
			options.input_buffer_binary = is_acl_bin_buffer != 0;
#else
			options.input_filename = argument + option_length;
			if (!is_acl_sjson_file(options.input_filename) && !is_acl_bin_file(options.input_filename))
			{
				printf("Input file must be an ACL SJSON file of the form: [*.acl.sjson] or a binary ACL file of the form: [*.acl.bin]\n");
				return false;
			}
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
			const size_t filename_len = std::strlen(options.config_filename);
			if (filename_len < 13 || strncmp(options.config_filename + filename_len - 13, ".config.sjson", 13) != 0)
			{
				printf("Configuration file must be a config SJSON file of the form: [*.config.sjson]\n");
				return false;
			}
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
				const size_t filename_len = std::strlen(options.output_stats_filename);
				if (filename_len < 6 || strncmp(options.output_stats_filename + filename_len - 6, ".sjson", 6) != 0)
				{
					printf("Stats output file must be an SJSON file of the form: [*.sjson]\n");
					return false;
				}
			}
			else
				options.output_stats_filename = nullptr;

			options.open_output_stats_file();
			continue;
		}

		option_length = std::strlen(k_bin_output_option);
		if (std::strncmp(argument, k_bin_output_option, option_length) == 0)
		{
			options.output_bin_filename = argument + option_length;
			const size_t filename_len = std::strlen(options.output_bin_filename);
			if (filename_len < 8 || strncmp(options.output_bin_filename + filename_len - 8, ".acl.bin", 8) != 0)
			{
				printf("Binary output file must be an ACL binary file of the form: [*.acl.bin]\n");
				return false;
			}
			continue;
		}

		option_length = std::strlen(k_regression_test_option);
		if (std::strncmp(argument, k_regression_test_option, option_length) == 0)
		{
			options.regression_testing = true;
			continue;
		}

		option_length = std::strlen(k_profile_decompression_option);
		if (std::strncmp(argument, k_profile_decompression_option, option_length) == 0)
		{
			options.profile_decompression = true;
			continue;
		}

		option_length = std::strlen(k_exhaustive_compression_option);
		if (std::strncmp(argument, k_exhaustive_compression_option, option_length) == 0)
		{
			options.exhaustive_compression = true;
			continue;
		}

		option_length = std::strlen(k_bind_pose_relative_option);
		if (std::strncmp(argument, k_bind_pose_relative_option, option_length) == 0)
		{
			options.is_bind_pose_relative = true;
			continue;
		}

		option_length = std::strlen(k_bind_pose_additive0_option);
		if (std::strncmp(argument, k_bind_pose_additive0_option, option_length) == 0)
		{
			options.is_bind_pose_additive0 = true;
			continue;
		}

		option_length = std::strlen(k_bind_pose_additive1_option);
		if (std::strncmp(argument, k_bind_pose_additive1_option, option_length) == 0)
		{
			options.is_bind_pose_additive1 = true;
			continue;
		}

		option_length = std::strlen(k_stat_detailed_output_option);
		if (std::strncmp(argument, k_stat_detailed_output_option, option_length) == 0)
		{
			options.stat_detailed_output = true;
			continue;
		}

		option_length = std::strlen(k_stat_exhaustive_output_option);
		if (std::strncmp(argument, k_stat_exhaustive_output_option, option_length) == 0)
		{
			options.stat_exhaustive_output = true;
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

	if (options.profile_decompression && options.exhaustive_compression)
	{
		printf("Exhaustive compression is not supported with decompression profiling.\n");
		return false;
	}

	return true;
}

template<class DecompressionContextType>
static void validate_accuracy(IAllocator& allocator, const AnimationClip& clip, const CompressionSettings& settings, DecompressionContextType& context, double regression_error_threshold)
{
	(void)regression_error_threshold;

	const uint16_t num_bones = clip.get_num_bones();
	const float clip_duration = clip.get_duration();
	const float sample_rate = float(clip.get_sample_rate());
	const uint32_t num_samples = calculate_num_samples(clip_duration, clip.get_sample_rate());
	const ISkeletalErrorMetric& error_metric = *settings.error_metric;
	const RigidSkeleton& skeleton = clip.get_skeleton();

	const AnimationClip* additive_base_clip = clip.get_additive_base();
	const uint32_t additive_num_samples = additive_base_clip != nullptr ? additive_base_clip->get_num_samples() : 0;
	const float additive_duration = additive_base_clip != nullptr ? additive_base_clip->get_duration() : 0.0f;

	Transform_32* raw_pose_transforms = allocate_type_array<Transform_32>(allocator, num_bones);
	Transform_32* base_pose_transforms = allocate_type_array<Transform_32>(allocator, num_bones);
	Transform_32* lossy_pose_transforms = allocate_type_array<Transform_32>(allocator, num_bones);

	DefaultOutputWriter pose_writer(lossy_pose_transforms, num_bones);

	// Regression test
	for (uint32_t sample_index = 0; sample_index < num_samples; ++sample_index)
	{
		const float sample_time = min(float(sample_index) / sample_rate, clip_duration);

		clip.sample_pose(sample_time, raw_pose_transforms, num_bones);

		context.seek(sample_time, SampleRoundingPolicy::None);
		context.decompress_pose(pose_writer);

		if (additive_base_clip != nullptr)
		{
			const float normalized_sample_time = additive_num_samples > 1 ? (sample_time / clip_duration) : 0.0f;
			const float additive_sample_time = normalized_sample_time * additive_duration;
			additive_base_clip->sample_pose(additive_sample_time, base_pose_transforms, num_bones);
		}

		for (uint16_t bone_index = 0; bone_index < num_bones; ++bone_index)
		{
			const float error = error_metric.calculate_object_bone_error(skeleton, raw_pose_transforms, base_pose_transforms, lossy_pose_transforms, bone_index);
			(void)error;
			ACL_ASSERT(is_finite(error), "Returned error is not a finite value");
			ACL_ASSERT(error < regression_error_threshold, "Error too high for bone %u: %f at time %f", bone_index, error, sample_time);
		}
	}

	// Unit test
	{
		// Validate that the decoder can decode a single bone at a particular time
		// Use the last bone and last sample time to ensure we can seek properly
		const uint16_t sample_bone_index = num_bones - 1;
		const float sample_time = clip.get_duration();
		Quat_32 test_rotation;
		Vector4_32 test_translation;
		Vector4_32 test_scale;
		context.seek(sample_time, SampleRoundingPolicy::None);
		context.decompress_bone(sample_bone_index, &test_rotation, &test_translation, &test_scale);
		ACL_ASSERT(quat_near_equal(test_rotation, lossy_pose_transforms[sample_bone_index].rotation), "Failed to sample bone index: %u", sample_bone_index);
		ACL_ASSERT(vector_all_near_equal3(test_translation, lossy_pose_transforms[sample_bone_index].translation), "Failed to sample bone index: %u", sample_bone_index);
		ACL_ASSERT(vector_all_near_equal3(test_scale, lossy_pose_transforms[sample_bone_index].scale), "Failed to sample bone index: %u", sample_bone_index);
	}

	deallocate_type_array(allocator, raw_pose_transforms, num_bones);
	deallocate_type_array(allocator, base_pose_transforms, num_bones);
	deallocate_type_array(allocator, lossy_pose_transforms, num_bones);
}

static void try_algorithm(const Options& options, IAllocator& allocator, const AnimationClip& clip, const CompressionSettings& settings, AlgorithmType8 algorithm_type, StatLogging logging, sjson::ArrayWriter* runs_writer, double regression_error_threshold)
{
	(void)runs_writer;

	auto try_algorithm_impl = [&](sjson::ObjectWriter* stats_writer)
	{
		OutputStats stats(logging, stats_writer);
		CompressedClip* compressed_clip = nullptr;
		ErrorResult error_result; (void)error_result;
		switch (algorithm_type)
		{
		case AlgorithmType8::UniformlySampled:
			error_result = uniformly_sampled::compress_clip(allocator, clip, settings, compressed_clip, stats);
			break;
		}

		ACL_ASSERT(error_result.empty(), error_result.c_str());
		ACL_ASSERT(compressed_clip->is_valid(true).empty(), "Compressed clip is invalid");

#if defined(SJSON_CPP_WRITER)
		if (logging != StatLogging::None)
		{
			// Use the compressed clip to make sure the decoder works properly
			BoneError bone_error;
			switch (algorithm_type)
			{
			case AlgorithmType8::UniformlySampled:
			{
				uniformly_sampled::DecompressionContext<uniformly_sampled::DebugDecompressionSettings> context;
				context.initialize(*compressed_clip);
				bone_error = calculate_compressed_clip_error(allocator, clip, settings, context);
				break;
			}
			}

			stats_writer->insert("max_error", bone_error.error);
			stats_writer->insert("worst_bone", bone_error.index);
			stats_writer->insert("worst_time", bone_error.sample_time);

			if (are_any_enum_flags_set(logging, StatLogging::SummaryDecompression))
				write_decompression_performance_stats(allocator, settings, *compressed_clip, logging, *stats_writer);
		}
#endif

		if (options.regression_testing)
		{
			switch (algorithm_type)
			{
			case AlgorithmType8::UniformlySampled:
			{
				uniformly_sampled::DecompressionContext<uniformly_sampled::DebugDecompressionSettings> context;
				context.initialize(*compressed_clip);
				validate_accuracy(allocator, clip, settings, context, regression_error_threshold);
				break;
			}
			}
		}

		if (options.output_bin_filename != nullptr)
		{
			std::ofstream output_file_stream(options.output_bin_filename, std::ios_base::out | std::ios_base::binary | std::ios_base::trunc);
			if (output_file_stream.is_open())
				output_file_stream.write((const char*)compressed_clip, compressed_clip->get_size());
		}

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
					  std::unique_ptr<AnimationClip, Deleter<AnimationClip>>& out_clip,
					  std::unique_ptr<RigidSkeleton, Deleter<RigidSkeleton>>& out_skeleton,
					  bool& has_settings,
					  AlgorithmType8& out_algorithm_type,
					  CompressionSettings& out_settings)
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

	if (!reader.read_settings(has_settings, out_algorithm_type, out_settings)
		|| !reader.read_skeleton(out_skeleton)
		|| !reader.read_clip(out_clip, *out_skeleton))
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

	CompressionSettings default_settings;

	sjson::StringView rotation_format;
	parser.try_read("rotation_format", rotation_format, get_rotation_format_name(default_settings.rotation_format));
	if (!get_rotation_format(rotation_format.c_str(), out_settings.rotation_format))
	{
		printf("Invalid rotation format: %s\n", String(allocator, rotation_format.c_str(), rotation_format.size()).c_str());
		return false;
	}

	sjson::StringView translation_format;
	parser.try_read("translation_format", translation_format, get_vector_format_name(default_settings.translation_format));
	if (!get_vector_format(translation_format.c_str(), out_settings.translation_format))
	{
		printf("Invalid translation format: %s\n", String(allocator, translation_format.c_str(), translation_format.size()).c_str());
		return false;
	}

	sjson::StringView scale_format;
	parser.try_read("scale_format", scale_format, get_vector_format_name(default_settings.scale_format));
	if (!get_vector_format(scale_format.c_str(), out_settings.scale_format))
	{
		printf("Invalid scale format: %s\n", String(allocator, scale_format.c_str(), scale_format.size()).c_str());
		return false;
	}

	RangeReductionFlags8 range_reduction = RangeReductionFlags8::None;

	bool rotation_range_reduction;
	parser.try_read("rotation_range_reduction", rotation_range_reduction, are_any_enum_flags_set(default_settings.range_reduction, RangeReductionFlags8::Rotations));
	if (rotation_range_reduction)
		range_reduction |= RangeReductionFlags8::Rotations;

	bool translation_range_reduction;
	parser.try_read("translation_range_reduction", translation_range_reduction, are_any_enum_flags_set(default_settings.range_reduction, RangeReductionFlags8::Translations));
	if (translation_range_reduction)
		range_reduction |= RangeReductionFlags8::Translations;

	bool scale_range_reduction;
	parser.try_read("scale_range_reduction", scale_range_reduction, are_any_enum_flags_set(default_settings.range_reduction, RangeReductionFlags8::Scales));
	if (scale_range_reduction)
		range_reduction |= RangeReductionFlags8::Scales;

	out_settings.range_reduction = range_reduction;

	if (parser.object_begins("segmenting"))
	{
		parser.try_read("enabled", out_settings.segmenting.enabled, false);

		range_reduction = RangeReductionFlags8::None;
		parser.try_read("rotation_range_reduction", rotation_range_reduction, are_any_enum_flags_set(default_settings.segmenting.range_reduction, RangeReductionFlags8::Rotations));
		parser.try_read("translation_range_reduction", translation_range_reduction, are_any_enum_flags_set(default_settings.segmenting.range_reduction, RangeReductionFlags8::Translations));
		parser.try_read("scale_range_reduction", scale_range_reduction, are_any_enum_flags_set(default_settings.segmenting.range_reduction, RangeReductionFlags8::Scales));

		if (rotation_range_reduction)
			range_reduction |= RangeReductionFlags8::Rotations;

		if (translation_range_reduction)
			range_reduction |= RangeReductionFlags8::Translations;

		if (scale_range_reduction)
			range_reduction |= RangeReductionFlags8::Scales;

		out_settings.segmenting.range_reduction = range_reduction;

		if (!parser.object_ends())
		{
			uint32_t line, column;
			parser.get_position(line, column);

			printf("Error on line %d column %d: Expected segmenting object to end\n", line, column);
			return false;
		}
	}

	parser.try_read("constant_rotation_threshold_angle", out_settings.constant_rotation_threshold_angle, default_settings.constant_rotation_threshold_angle);
	parser.try_read("constant_translation_threshold", out_settings.constant_translation_threshold, default_settings.constant_translation_threshold);
	parser.try_read("constant_scale_threshold", out_settings.constant_scale_threshold, default_settings.constant_scale_threshold);
	parser.try_read("error_threshold", out_settings.error_threshold, default_settings.error_threshold);

	parser.try_read("regression_error_threshold", out_regression_error_threshold, 0.0);

	if (!parser.is_valid() || !parser.remainder_is_comments_and_whitespace())
	{
		uint32_t line, column;
		parser.get_position(line, column);

		printf("Error on line %d column %d: Expected end of file\n", line, column);
		return false;
	}

	return true;
}

static ISkeletalErrorMetric* create_additive_error_metric(IAllocator& allocator, AdditiveClipFormat8 format)
{
	switch (format)
	{
	case AdditiveClipFormat8::Relative:
		return allocate_type<AdditiveTransformErrorMetric<AdditiveClipFormat8::Relative>>(allocator);
	case AdditiveClipFormat8::Additive0:
		return allocate_type<AdditiveTransformErrorMetric<AdditiveClipFormat8::Additive0>>(allocator);
	case AdditiveClipFormat8::Additive1:
		return allocate_type<AdditiveTransformErrorMetric<AdditiveClipFormat8::Additive1>>(allocator);
	default:
		return nullptr;
	}
}

static void create_additive_base_clip(const Options& options, AnimationClip& clip, const RigidSkeleton& skeleton, AnimationClip& out_base_clip)
{
	// Convert the animation clip to be relative to the bind pose
	const uint16_t num_bones = clip.get_num_bones();
	const uint32_t num_samples = clip.get_num_samples();
	AnimatedBone* bones = clip.get_bones();

	AdditiveClipFormat8 additive_format = AdditiveClipFormat8::None;
	if (options.is_bind_pose_relative)
		additive_format = AdditiveClipFormat8::Relative;
	else if (options.is_bind_pose_additive0)
		additive_format = AdditiveClipFormat8::Additive0;
	else if (options.is_bind_pose_additive1)
		additive_format = AdditiveClipFormat8::Additive1;

	for (uint16_t bone_index = 0; bone_index < num_bones; ++bone_index)
	{
		AnimatedBone& anim_bone = bones[bone_index];

		// Get the bind transform and make sure it has no scale
		const RigidBone& skel_bone = skeleton.get_bone(bone_index);
		const Transform_64 bind_transform = transform_set(skel_bone.bind_transform.rotation, skel_bone.bind_transform.translation, vector_set(1.0));

		for (uint32_t sample_index = 0; sample_index < num_samples; ++sample_index)
		{
			const Quat_64 rotation = quat_normalize(anim_bone.rotation_track.get_sample(sample_index));
			const Vector4_64 translation = anim_bone.translation_track.get_sample(sample_index);
			const Vector4_64 scale = anim_bone.scale_track.get_sample(sample_index);

			const Transform_64 bone_transform = transform_set(rotation, translation, scale);

			Transform_64 bind_local_transform = bone_transform;
			if (options.is_bind_pose_relative)
				bind_local_transform = convert_to_relative(bind_transform, bone_transform);
			else if (options.is_bind_pose_additive0)
				bind_local_transform = convert_to_additive0(bind_transform, bone_transform);
			else if (options.is_bind_pose_additive1)
				bind_local_transform = convert_to_additive1(bind_transform, bone_transform);

			anim_bone.rotation_track.set_sample(sample_index, bind_local_transform.rotation);
			anim_bone.translation_track.set_sample(sample_index, bind_local_transform.translation);
			anim_bone.scale_track.set_sample(sample_index, bind_local_transform.scale);
		}

		AnimatedBone& base_bone = out_base_clip.get_animated_bone(bone_index);
		base_bone.rotation_track.set_sample(0, bind_transform.rotation);
		base_bone.translation_track.set_sample(0, bind_transform.translation);
		base_bone.scale_track.set_sample(0, bind_transform.scale);
	}

	clip.set_additive_base(&out_base_clip, additive_format);
}

static CompressionSettings make_settings(RotationFormat8 rotation_format, VectorFormat8 translation_format, VectorFormat8 scale_format,
	RangeReductionFlags8 clip_range_reduction,
	bool use_segmenting = false, RangeReductionFlags8 segment_range_reduction = RangeReductionFlags8::None)
{
	CompressionSettings settings;
	settings.rotation_format = rotation_format;
	settings.translation_format = translation_format;
	settings.scale_format = scale_format;
	settings.range_reduction = clip_range_reduction;
	settings.segmenting.enabled = use_segmenting;
	settings.segmenting.range_reduction = segment_range_reduction;
	return settings;
}

static int safe_main_impl(int argc, char* argv[])
{
	Options options;

	if (!parse_options(argc, argv, options))
		return -1;

	ANSIAllocator allocator;
	std::unique_ptr<AnimationClip, Deleter<AnimationClip>> clip;
	std::unique_ptr<RigidSkeleton, Deleter<RigidSkeleton>> skeleton;

#if defined(__ANDROID__)
	const bool is_input_acl_bin_file = options.input_buffer_binary;
#else
	const bool is_input_acl_bin_file = is_acl_bin_file(options.input_filename);
#endif

	bool use_external_config = false;
	AlgorithmType8 algorithm_type = AlgorithmType8::UniformlySampled;
	CompressionSettings settings;

	if (!is_input_acl_bin_file && !read_clip(allocator, options, clip, skeleton, use_external_config, algorithm_type, settings))
		return -1;

	double regression_error_threshold;

#if defined(__ANDROID__)
	if (options.config_buffer != nullptr && options.config_buffer_size != 0)
#else
	if (options.config_filename != nullptr && std::strlen(options.config_filename) != 0)
#endif
	{
		// Override whatever the ACL clip might have contained
		algorithm_type = AlgorithmType8::UniformlySampled;
		settings = CompressionSettings();

		if (!read_config(allocator, options, algorithm_type, settings, regression_error_threshold))
			return -1;

		use_external_config = true;
	}

	AnimationClip* base_clip = nullptr;

	if (!is_input_acl_bin_file)
	{
		// Grab whatever clip we might have read from the sjson file and cast the const away so we can manage the memory
		base_clip = const_cast<AnimationClip*>(clip->get_additive_base());
		if (base_clip == nullptr)
		{
			base_clip = allocate_type<AnimationClip>(allocator, allocator, *skeleton, 1, 30, String(allocator, "Base Clip"));

			if (options.is_bind_pose_relative || options.is_bind_pose_additive0 || options.is_bind_pose_additive1)
				create_additive_base_clip(options, *clip, *skeleton, *base_clip);
		}

		// First try to create an additive error metric
		settings.error_metric = create_additive_error_metric(allocator, clip->get_additive_format());

		if (settings.error_metric == nullptr)
			settings.error_metric = allocate_type<TransformErrorMetric>(allocator);
	}

	// Compress & Decompress
	auto exec_algos = [&](sjson::ArrayWriter* runs_writer)
	{
		StatLogging logging = options.output_stats ? StatLogging::Summary : StatLogging::None;

		if (options.stat_detailed_output)
			logging |= StatLogging::Detailed;

		if (options.stat_exhaustive_output)
			logging |= StatLogging::Exhaustive;

		if (options.profile_decompression)
			logging |= StatLogging::SummaryDecompression | StatLogging::ExhaustiveDecompression;

		if (is_input_acl_bin_file)
		{
#if defined(SJSON_CPP_WRITER)
			if (options.profile_decompression && runs_writer != nullptr)
			{
				const CompressionSettings default_settings = get_default_compression_settings();

#if defined(__ANDROID__)
				const CompressedClip* compressed_clip = reinterpret_cast<const CompressedClip*>(options.input_buffer);
				ACL_ASSERT(compressed_clip->is_valid(true).empty(), "Compressed clip is invalid");

				runs_writer->push([&](sjson::ObjectWriter& writer)
				{
					write_decompression_performance_stats(allocator, default_settings, *compressed_clip, logging, writer);
				});
#else
				std::ifstream input_file_stream(options.input_filename, std::ios_base::in | std::ios_base::binary);
				if (input_file_stream.is_open())
				{
					input_file_stream.seekg(0, input_file_stream.end);
					const size_t buffer_size = size_t(input_file_stream.tellg());
					input_file_stream.seekg(0, input_file_stream.beg);

					char* buffer = (char*)allocator.allocate(buffer_size, alignof(CompressedClip));
					input_file_stream.read(buffer, buffer_size);

					const CompressedClip* compressed_clip = reinterpret_cast<const CompressedClip*>(buffer);
					ACL_ASSERT(compressed_clip->is_valid(true).empty(), "Compressed clip is invalid");

					runs_writer->push([&](sjson::ObjectWriter& writer)
					{
						write_decompression_performance_stats(allocator, default_settings, *compressed_clip, logging, writer);
					});

					allocator.deallocate(buffer, buffer_size);
				}
#endif
			}
#endif
		}
		else if (use_external_config)
		{
			ACL_ASSERT(algorithm_type == AlgorithmType8::UniformlySampled, "Only UniformlySampled is supported for now");
			try_algorithm(options, allocator, *clip, settings, AlgorithmType8::UniformlySampled, logging, runs_writer, regression_error_threshold);
		}
		else if (options.exhaustive_compression)
		{
			const bool use_segmenting_options[] = { false, true };
			for (size_t segmenting_option_index = 0; segmenting_option_index < get_array_size(use_segmenting_options); ++segmenting_option_index)
			{
				const bool use_segmenting = use_segmenting_options[segmenting_option_index];

				CompressionSettings uniform_tests[] =
				{
					make_settings(RotationFormat8::Quat_128, VectorFormat8::Vector3_96, VectorFormat8::Vector3_96, RangeReductionFlags8::None, use_segmenting),
					make_settings(RotationFormat8::Quat_128, VectorFormat8::Vector3_96, VectorFormat8::Vector3_96, RangeReductionFlags8::Rotations, use_segmenting),
					make_settings(RotationFormat8::Quat_128, VectorFormat8::Vector3_96, VectorFormat8::Vector3_96, RangeReductionFlags8::Translations, use_segmenting),
					make_settings(RotationFormat8::Quat_128, VectorFormat8::Vector3_96, VectorFormat8::Vector3_96, RangeReductionFlags8::Rotations | RangeReductionFlags8::Translations, use_segmenting),

					make_settings(RotationFormat8::QuatDropW_96, VectorFormat8::Vector3_96, VectorFormat8::Vector3_96, RangeReductionFlags8::None, use_segmenting),
					make_settings(RotationFormat8::QuatDropW_96, VectorFormat8::Vector3_96, VectorFormat8::Vector3_96, RangeReductionFlags8::Rotations, use_segmenting),
					make_settings(RotationFormat8::QuatDropW_96, VectorFormat8::Vector3_96, VectorFormat8::Vector3_96, RangeReductionFlags8::Translations, use_segmenting),
					make_settings(RotationFormat8::QuatDropW_96, VectorFormat8::Vector3_96, VectorFormat8::Vector3_96, RangeReductionFlags8::Rotations | RangeReductionFlags8::Translations, use_segmenting),

					make_settings(RotationFormat8::QuatDropW_Variable, VectorFormat8::Vector3_Variable, VectorFormat8::Vector3_96, RangeReductionFlags8::Translations, use_segmenting),
					make_settings(RotationFormat8::QuatDropW_Variable, VectorFormat8::Vector3_Variable, VectorFormat8::Vector3_96, RangeReductionFlags8::Rotations | RangeReductionFlags8::Translations, use_segmenting),

					make_settings(RotationFormat8::QuatDropW_Variable, VectorFormat8::Vector3_Variable, VectorFormat8::Vector3_Variable, RangeReductionFlags8::AllTracks, use_segmenting),
				};

				for (CompressionSettings test_settings : uniform_tests)
				{
					test_settings.error_metric = settings.error_metric;

					try_algorithm(options, allocator, *clip, test_settings, AlgorithmType8::UniformlySampled, logging, runs_writer, regression_error_threshold);
				}
			}

			{
				CompressionSettings uniform_tests[] =
				{
					make_settings(RotationFormat8::Quat_128, VectorFormat8::Vector3_96, VectorFormat8::Vector3_96, RangeReductionFlags8::Rotations, true, RangeReductionFlags8::Rotations),
					make_settings(RotationFormat8::Quat_128, VectorFormat8::Vector3_96, VectorFormat8::Vector3_96, RangeReductionFlags8::Translations, true, RangeReductionFlags8::Translations),
					make_settings(RotationFormat8::Quat_128, VectorFormat8::Vector3_96, VectorFormat8::Vector3_96, RangeReductionFlags8::Rotations | RangeReductionFlags8::Translations, true, RangeReductionFlags8::Rotations | RangeReductionFlags8::Translations),

					make_settings(RotationFormat8::QuatDropW_96, VectorFormat8::Vector3_96, VectorFormat8::Vector3_96, RangeReductionFlags8::Rotations, true, RangeReductionFlags8::Rotations),
					make_settings(RotationFormat8::QuatDropW_96, VectorFormat8::Vector3_96, VectorFormat8::Vector3_96, RangeReductionFlags8::Translations, true, RangeReductionFlags8::Translations),
					make_settings(RotationFormat8::QuatDropW_96, VectorFormat8::Vector3_96, VectorFormat8::Vector3_96, RangeReductionFlags8::Rotations | RangeReductionFlags8::Translations, true, RangeReductionFlags8::Rotations | RangeReductionFlags8::Translations),
					make_settings(RotationFormat8::QuatDropW_Variable, VectorFormat8::Vector3_Variable, VectorFormat8::Vector3_96, RangeReductionFlags8::Translations, true, RangeReductionFlags8::Translations),
					make_settings(RotationFormat8::QuatDropW_Variable, VectorFormat8::Vector3_Variable, VectorFormat8::Vector3_96, RangeReductionFlags8::Rotations | RangeReductionFlags8::Translations, true, RangeReductionFlags8::Rotations | RangeReductionFlags8::Translations),

					make_settings(RotationFormat8::QuatDropW_Variable, VectorFormat8::Vector3_Variable, VectorFormat8::Vector3_Variable, RangeReductionFlags8::AllTracks, true, RangeReductionFlags8::AllTracks),
				};

				for (CompressionSettings test_settings : uniform_tests)
				{
					test_settings.error_metric = settings.error_metric;

					try_algorithm(options, allocator, *clip, test_settings, AlgorithmType8::UniformlySampled, logging, runs_writer, regression_error_threshold);
				}
			}
		}
		else
		{
			CompressionSettings default_settings = get_default_compression_settings();
			default_settings.error_metric = settings.error_metric;

			try_algorithm(options, allocator, *clip, default_settings, AlgorithmType8::UniformlySampled, logging, runs_writer, regression_error_threshold);
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

	deallocate_type(allocator, settings.error_metric);
	deallocate_type(allocator, base_clip);

	return 0;
}

#ifdef _WIN32
static LONG WINAPI unhandled_exception_filter(EXCEPTION_POINTERS *info)
{
	(void)info;

	if (IsDebuggerPresent())
		return EXCEPTION_CONTINUE_SEARCH;

	return EXCEPTION_EXECUTE_HANDLER;
}
#endif

static Quat_32 s_temp[512];

#if defined(ACL_SSE2_INTRINSICS)
#define NOINLINE __declspec(noinline)
#define VECTORCALL __vectorcall
#else
#define NOINLINE __attribute__((noinline))
#define VECTORCALL
#endif

#if defined(__ANDROID__)
#include <android/log.h>
#define log_(...) __android_log_print(ANDROID_LOG_INFO, "acl", ## __VA_ARGS__)
#else
#define log_(...) printf(## __VA_ARGS__)
#endif

#if 0
NOINLINE Vector4_32 VECTORCALL unpack_vector3_24_ref(const uint8_t* vector_data)
{
	uint8_t x8 = vector_data[0];
	uint8_t y8 = vector_data[1];
	uint8_t z8 = vector_data[2];
	float x = unpack_scalar_unsigned(x8, 8);
	float y = unpack_scalar_unsigned(y8, 8);
	float z = unpack_scalar_unsigned(z8, 8);

	uint8_t x82 = vector_data[3];
	uint8_t y82 = vector_data[4];
	uint8_t z82 = vector_data[5];
	float x2 = unpack_scalar_unsigned(x82, 8);
	float y2 = unpack_scalar_unsigned(y82, 8);
	float z2 = unpack_scalar_unsigned(z82, 8);

	return vector_add(vector_set(x, y, z), vector_set(x2, y2, z2));
}

NOINLINE Vector4_32 VECTORCALL unpack_vector3_24_1(const uint8_t* vector_data)
{
	uint8x8x2_t x8y8z8 = vld2_u8(vector_data);
	uint16x8_t x16y16z16 = vmovl_u8(x8y8z8.val[0]);
	uint32x4_t x32y32z32 = vmovl_u16(x16y16z16);

	float32x4_t value = vcvtq_f32_u32(x32y32z32);
	return vmulq_n_f32(value, 1.0f / 255.0f);
}

NOINLINE Vector4_32 VECTORCALL unpack_vector3_24_2(const uint8_t* vector_data)
{
	__m128i zero = _mm_setzero_si128();
	__m128i exponent = _mm_set1_epi32(0x3f800000);

	__m128i x8y8z8 = _mm_loadu_si128((const __m128i*)vector_data);
	__m128i x16y16z16 = _mm_unpacklo_epi8(x8y8z8, zero);
	__m128i x32y32z32 = _mm_unpacklo_epi16(x16y16z16, zero);
	__m128i segment_extent_i32 = _mm_or_si128(_mm_slli_epi32(x32y32z32, 23 - 8), exponent);
	__m128 value = _mm_sub_ps(_mm_castsi128_ps(segment_extent_i32), _mm_castsi128_ps(exponent));

	__m128i x8y8z8_2 = _mm_loadu_si128((const __m128i*)(vector_data + 3));
	__m128i x16y16z16_2 = _mm_unpacklo_epi8(x8y8z8_2, zero);
	__m128i x32y32z32_2 = _mm_unpacklo_epi16(x16y16z16_2, zero);
	__m128i segment_extent_i32_2 = _mm_or_si128(_mm_slli_epi32(x32y32z32_2, 23 - 8), exponent);
	__m128 value_2 = _mm_sub_ps(_mm_castsi128_ps(segment_extent_i32_2), _mm_castsi128_ps(exponent));
	return _mm_add_ps(value, value_2);
}
#endif

NOINLINE Vector4_32 VECTORCALL unpack_vector3_n_ref(uint8_t XBits, uint8_t YBits, uint8_t ZBits, const uint8_t* vector_data, int32_t bit_offset)
{
	const uint8_t num_bits_to_read = XBits + YBits + ZBits;

	int32_t byte_offset = bit_offset / 8;
	uint64_t vector_u64 = unaligned_load<uint64_t>(vector_data + byte_offset);
	vector_u64 = byte_swap(vector_u64);
	vector_u64 <<= bit_offset % 8;
	vector_u64 >>= 64 - num_bits_to_read;

	const uint32_t x32 = safe_static_cast<uint32_t>(vector_u64 >> (YBits + ZBits));
	const uint32_t y32 = safe_static_cast<uint32_t>((vector_u64 >> ZBits) & ((1 << YBits) - 1));
	uint32_t z32;
	z32 = safe_static_cast<uint32_t>(vector_u64 & ((1 << ZBits) - 1));

	const float x = unpack_scalar_unsigned(x32, XBits);
	const float y = unpack_scalar_unsigned(y32, YBits);
	const float z = unpack_scalar_unsigned(z32, ZBits);
	return vector_set(x, y, z);
}

NOINLINE Vector4_32 VECTORCALL unpack_vector3_n_o00(uint8_t XBits, uint8_t YBits, uint8_t ZBits, const uint8_t* vector_data, int32_t bit_offset)
{
	int32_t byte_offset = bit_offset / 8;
	uint64_t vector_u64 = unaligned_load<uint64_t>(vector_data + byte_offset);
	vector_u64 = byte_swap(vector_u64);
	//vector_u64 <<= bit_offset % 8;
	//vector_u64 >>= 64 - num_bits_to_read;

	int32_t bit_offset2 = bit_offset % 8;
	//const uint32_t x32 = safe_static_cast<uint32_t>(vector_u64 >> (YBits + ZBits));
	const uint32_t x32 = safe_static_cast<uint32_t>(_bextr_u64(vector_u64, bit_offset2, XBits));
	bit_offset2 += XBits;
	//const uint32_t y32 = safe_static_cast<uint32_t>((vector_u64 >> ZBits) & ((1 << YBits) - 1));
	const uint32_t y32 = safe_static_cast<uint32_t>(_bextr_u64(vector_u64, bit_offset2, YBits));
	uint32_t z32;

	bit_offset += XBits + YBits;
	byte_offset = bit_offset / 8;
	vector_u64 = unaligned_load<uint64_t>(vector_data + byte_offset);
	vector_u64 = byte_swap(vector_u64);
	//z32 = safe_static_cast<uint32_t>(vector_u64 & ((1 << ZBits) - 1));
	z32 = safe_static_cast<uint32_t>(_bextr_u64(vector_u64, bit_offset % 8, ZBits));

	const float x = unpack_scalar_unsigned(x32, XBits);
	const float y = unpack_scalar_unsigned(y32, YBits);
	const float z = unpack_scalar_unsigned(z32, ZBits);
	return vector_set(x, y, z);
}

NOINLINE Vector4_32 VECTORCALL unpack_vector3_n_o01(uint8_t NumBits, const uint8_t* vector_data, int32_t bit_offset)
{
	int32_t byte_offset = bit_offset / 8;
	uint64_t vector_u64 = unaligned_load<uint64_t>(vector_data + byte_offset);
	vector_u64 = byte_swap(vector_u64);

	int32_t bit_offset2 = bit_offset % 8;
	const uint32_t x32 = safe_static_cast<uint32_t>(_bextr_u64(vector_u64, bit_offset2, NumBits));
	bit_offset2 += NumBits;
	const uint32_t y32 = safe_static_cast<uint32_t>(_bextr_u64(vector_u64, bit_offset2, NumBits));

	bit_offset += NumBits * 2;
	byte_offset = bit_offset / 8;
	vector_u64 = unaligned_load<uint64_t>(vector_data + byte_offset);
	vector_u64 = byte_swap(vector_u64);
	const uint32_t z32 = safe_static_cast<uint32_t>(_bextr_u64(vector_u64, bit_offset % 8, NumBits));

	const float x = unpack_scalar_unsigned(x32, NumBits);
	const float y = unpack_scalar_unsigned(y32, NumBits);
	const float z = unpack_scalar_unsigned(z32, NumBits);
	return vector_set(x, y, z);
}

NOINLINE Vector4_32 VECTORCALL unpack_vector3_n_o02(uint8_t NumBits, const uint8_t* vector_data, int32_t bit_offset)
{
	int32_t byte_offset = bit_offset / 8;
	uint64_t vector_u64 = unaligned_load<uint64_t>(vector_data + byte_offset);
	vector_u64 = byte_swap(vector_u64);

	int32_t bit_offset2 = bit_offset % 8;
	const uint32_t x32 = safe_static_cast<uint32_t>(_bextr_u64(vector_u64, bit_offset2, NumBits));
	bit_offset2 += NumBits;
	const uint32_t y32 = safe_static_cast<uint32_t>(_bextr_u64(vector_u64, bit_offset2, NumBits));

	bit_offset += NumBits * 2;
	byte_offset = bit_offset / 8;
	vector_u64 = unaligned_load<uint64_t>(vector_data + byte_offset);
	vector_u64 = byte_swap(vector_u64);
	const uint32_t z32 = safe_static_cast<uint32_t>(_bextr_u64(vector_u64, bit_offset % 8, NumBits));

	__m128 value = _mm_cvtepi32_ps(_mm_set_epi32(0, z32, y32, x32));
	return _mm_mul_ps(value, _mm_set_ps1(1.0f / (1 << NumBits) - 1));
}

NOINLINE Vector4_32 VECTORCALL unpack_vector3_n_o03(uint8_t NumBits, const uint8_t* vector_data, int32_t bit_offset)
{
	int32_t byte_offset = bit_offset / 8;
	uint64_t vector_u64 = unaligned_load<uint64_t>(vector_data + byte_offset);
	vector_u64 = byte_swap(vector_u64);

	int32_t bit_offset2 = bit_offset % 8;
	const uint32_t x32 = safe_static_cast<uint32_t>(_bextr_u64(vector_u64, bit_offset2, NumBits));
	bit_offset2 += NumBits;
	const uint32_t y32 = safe_static_cast<uint32_t>(_bextr_u64(vector_u64, bit_offset2, NumBits));

	bit_offset += NumBits * 2;
	byte_offset = bit_offset / 8;
	vector_u64 = unaligned_load<uint64_t>(vector_data + byte_offset);
	vector_u64 = byte_swap(vector_u64);
	const uint32_t z32 = safe_static_cast<uint32_t>(_bextr_u64(vector_u64, bit_offset % 8, NumBits));

	__m128 value = _mm_cvtepi32_ps(_mm_set_epi32(0, z32, y32, x32));
	return _mm_div_ps(value, _mm_set_ps1(float((1 << NumBits) - 1)));
}

// 64 bit loads with bit extract
NOINLINE Vector4_32 VECTORCALL unpack_vector3_n_o04(uint8_t NumBits, const uint8_t* vector_data, int32_t bit_offset)
{
	int32_t byte_offset = bit_offset / 8;
	uint64_t vector_u64 = unaligned_load<uint64_t>(vector_data + byte_offset);
	vector_u64 = byte_swap(vector_u64);

	int32_t bit_offset2 = bit_offset % 8;
	const uint32_t x32 = uint32_t(_bextr_u64(vector_u64, bit_offset2, NumBits));
	bit_offset2 += NumBits;
	const uint32_t y32 = uint32_t(_bextr_u64(vector_u64, bit_offset2, NumBits));

	bit_offset += NumBits * 2;
	byte_offset = bit_offset / 8;
	vector_u64 = unaligned_load<uint64_t>(vector_data + byte_offset);
	vector_u64 = byte_swap(vector_u64);
	const uint32_t z32 = uint32_t(_bextr_u64(vector_u64, bit_offset % 8, NumBits));

	__m128 value = _mm_cvtepi32_ps(_mm_set_epi32(0, z32, y32, x32));
	return _mm_div_ps(value, _mm_set_ps1(float((1 << NumBits) - 1)));
}

// 64 bit loads with bit extract, unsigned bit offset
NOINLINE Vector4_32 VECTORCALL unpack_vector3_n_o05(uint8_t NumBits, const uint8_t* vector_data, uint32_t bit_offset)
{
	uint32_t byte_offset = bit_offset / 8;
	uint64_t vector_u64 = unaligned_load<uint64_t>(vector_data + byte_offset);
	vector_u64 = byte_swap(vector_u64);

	uint32_t bit_offset2 = bit_offset % 8;
	const uint32_t x32 = uint32_t(_bextr_u64(vector_u64, bit_offset2, NumBits));
	bit_offset2 += NumBits;
	const uint32_t y32 = uint32_t(_bextr_u64(vector_u64, bit_offset2, NumBits));

	bit_offset += NumBits * 2;
	byte_offset = bit_offset / 8;
	vector_u64 = unaligned_load<uint64_t>(vector_data + byte_offset);
	vector_u64 = byte_swap(vector_u64);
	const uint32_t z32 = uint32_t(_bextr_u64(vector_u64, bit_offset % 8, NumBits));

	__m128 value = _mm_cvtepi32_ps(_mm_set_epi32(x32, z32, y32, x32));
	return _mm_div_ps(value, _mm_set_ps1(float((1 << NumBits) - 1)));
}

// 64 bit loads with bit extract, fixed
NOINLINE Vector4_32 VECTORCALL unpack_vector3_n_o06(uint8_t num_bits, const uint8_t* vector_data, uint32_t bit_offset)
{
	uint32_t byte_offset = bit_offset / 8;
	uint64_t vector_u64 = unaligned_load<uint64_t>(vector_data + byte_offset);
	vector_u64 = byte_swap(vector_u64);

	uint32_t value_bit_offset = 64 - (bit_offset % 8) - num_bits;
	const uint32_t x32 = uint32_t(_bextr_u64(vector_u64, value_bit_offset, num_bits));
	value_bit_offset -= num_bits;
	const uint32_t y32 = uint32_t(_bextr_u64(vector_u64, value_bit_offset, num_bits));

	bit_offset += num_bits * 2;
	byte_offset = bit_offset / 8;
	vector_u64 = unaligned_load<uint64_t>(vector_data + byte_offset);
	vector_u64 = byte_swap(vector_u64);

	value_bit_offset = 64 - (bit_offset % 8) - num_bits;
	const uint32_t z32 = uint32_t(_bextr_u64(vector_u64, value_bit_offset, num_bits));

	__m128 value = _mm_cvtepi32_ps(_mm_set_epi32(x32, z32, y32, x32));
	return _mm_div_ps(value, _mm_set_ps1(float((1 << num_bits) - 1)));
}

// 32 bit loads with bit extract
NOINLINE Vector4_32 VECTORCALL unpack_vector3_n_o07(uint8_t num_bits, const uint8_t* vector_data, uint32_t bit_offset)
{
	uint32_t byte_offset = bit_offset / 8;
	uint32_t vector_u32 = unaligned_load<uint32_t>(vector_data + byte_offset);
	vector_u32 = byte_swap(vector_u32);
	uint32_t value_bit_offset = 32 - (bit_offset % 8) - num_bits;
	const uint32_t x32 = _bextr_u32(vector_u32, value_bit_offset, num_bits);

	bit_offset += num_bits;

	byte_offset = bit_offset / 8;
	vector_u32 = unaligned_load<uint32_t>(vector_data + byte_offset);
	vector_u32 = byte_swap(vector_u32);
	value_bit_offset = 32 - (bit_offset % 8) - num_bits;
	const uint32_t y32 = _bextr_u32(vector_u32, value_bit_offset, num_bits);

	bit_offset += num_bits;

	byte_offset = bit_offset / 8;
	vector_u32 = unaligned_load<uint32_t>(vector_data + byte_offset);
	vector_u32 = byte_swap(vector_u32);
	value_bit_offset = 32 - (bit_offset % 8) - num_bits;
	const uint32_t z32 = _bextr_u32(vector_u32, value_bit_offset, num_bits);

	__m128 value = _mm_cvtepi32_ps(_mm_set_epi32(x32, z32, y32, x32));
	return _mm_div_ps(value, _mm_set_ps1(float((1 << num_bits) - 1)));
}

// try every scalar 32bit loads with >> & mask
NOINLINE Vector4_32 VECTORCALL unpack_vector3_n_o08(uint8_t num_bits, const uint8_t* vector_data, uint32_t bit_offset)
{
	uint32_t mask = (1 << num_bits) - 1;
	uint32_t bit_shift = 32 - num_bits;

	uint32_t byte_offset = bit_offset / 8;
	uint32_t vector_u32 = unaligned_load<uint32_t>(vector_data + byte_offset);
	vector_u32 = byte_swap(vector_u32);
	const uint32_t x32 = (vector_u32 >> (bit_shift - (bit_offset % 8))) & mask;

	bit_offset += num_bits;

	byte_offset = bit_offset / 8;
	vector_u32 = unaligned_load<uint32_t>(vector_data + byte_offset);
	vector_u32 = byte_swap(vector_u32);
	const uint32_t y32 = (vector_u32 >> (bit_shift - (bit_offset % 8))) & mask;

	bit_offset += num_bits;

	byte_offset = bit_offset / 8;
	vector_u32 = unaligned_load<uint32_t>(vector_data + byte_offset);
	vector_u32 = byte_swap(vector_u32);
	const uint32_t z32 = (vector_u32 >> (bit_shift - (bit_offset % 8))) & mask;

	__m128 value = _mm_cvtepi32_ps(_mm_set_epi32(x32, z32, y32, x32));
	return _mm_div_ps(value, _mm_set_ps1(float((1 << num_bits) - 1)));
}

// same as 08, cleaned up
NOINLINE Vector4_32 VECTORCALL unpack_vector3_n_o09(uint8_t num_bits, const uint8_t* vector_data, uint32_t bit_offset)
{
	const uint32_t mask = (1 << num_bits) - 1;
	const uint32_t bit_shift = 32 - num_bits;
	const __m128 max_value = _mm_set_ps1(float(mask));

	uint32_t byte_offset = bit_offset / 8;
	uint32_t vector_u32 = unaligned_load<uint32_t>(vector_data + byte_offset);
	vector_u32 = byte_swap(vector_u32);
	const uint32_t x32 = (vector_u32 >> (bit_shift - (bit_offset % 8))) & mask;

	bit_offset += num_bits;

	byte_offset = bit_offset / 8;
	vector_u32 = unaligned_load<uint32_t>(vector_data + byte_offset);
	vector_u32 = byte_swap(vector_u32);
	const uint32_t y32 = (vector_u32 >> (bit_shift - (bit_offset % 8))) & mask;

	bit_offset += num_bits;

	byte_offset = bit_offset / 8;
	vector_u32 = unaligned_load<uint32_t>(vector_data + byte_offset);
	vector_u32 = byte_swap(vector_u32);
	const uint32_t z32 = (vector_u32 >> (bit_shift - (bit_offset % 8))) & mask;

	const __m128 value = _mm_cvtepi32_ps(_mm_set_epi32(x32, z32, y32, x32));
	return _mm_div_ps(value, max_value);
}

// try with loading the max value from a constant lookup
NOINLINE Vector4_32 VECTORCALL unpack_vector3_n_o10(uint8_t num_bits, const uint8_t* vector_data, uint32_t bit_offset)
{
	static constexpr float max_values[] =
	{
		1.0f, 1.0f / float((1 << 1) - 1), 1.0f / float((1 << 2) - 1), 1.0f / float((1 << 3) - 1),
		1.0f / float((1 << 4) - 1), 1.0f / float((1 << 5) - 1), 1.0f / float((1 << 6) - 1), 1.0f / float((1 << 7) - 1),
		1.0f / float((1 << 8) - 1), 1.0f / float((1 << 9) - 1), 1.0f / float((1 << 10) - 1), 1.0f / float((1 << 11) - 1),
		1.0f / float((1 << 12) - 1), 1.0f / float((1 << 13) - 1), 1.0f / float((1 << 14) - 1), 1.0f / float((1 << 15) - 1),
		1.0f / float((1 << 16) - 1), 1.0f / float((1 << 17) - 1), 1.0f / float((1 << 18) - 1), 1.0f / float((1 << 19) - 1),
	};

	const uint32_t mask = (1 << num_bits) - 1;
	const uint32_t bit_shift = 32 - num_bits;
	const __m128 inv_max_value = _mm_load_ps1(&max_values[num_bits]);

	uint32_t byte_offset = bit_offset / 8;
	uint32_t vector_u32 = unaligned_load<uint32_t>(vector_data + byte_offset);
	vector_u32 = byte_swap(vector_u32);
	const uint32_t x32 = (vector_u32 >> (bit_shift - (bit_offset % 8))) & mask;

	bit_offset += num_bits;

	byte_offset = bit_offset / 8;
	vector_u32 = unaligned_load<uint32_t>(vector_data + byte_offset);
	vector_u32 = byte_swap(vector_u32);
	const uint32_t y32 = (vector_u32 >> (bit_shift - (bit_offset % 8))) & mask;

	bit_offset += num_bits;

	byte_offset = bit_offset / 8;
	vector_u32 = unaligned_load<uint32_t>(vector_data + byte_offset);
	vector_u32 = byte_swap(vector_u32);
	const uint32_t z32 = (vector_u32 >> (bit_shift - (bit_offset % 8))) & mask;

	const __m128 value = _mm_cvtepi32_ps(_mm_set_epi32(x32, z32, y32, x32));
	return _mm_mul_ps(value, inv_max_value);
}

// try with loading the mask from a constant lookup
NOINLINE Vector4_32 VECTORCALL unpack_vector3_n_o11(uint8_t num_bits, const uint8_t* vector_data, uint32_t bit_offset)
{
	static constexpr float max_values[] =
	{
		1.0f, 1.0f / float((1 << 1) - 1), 1.0f / float((1 << 2) - 1), 1.0f / float((1 << 3) - 1),
		1.0f / float((1 << 4) - 1), 1.0f / float((1 << 5) - 1), 1.0f / float((1 << 6) - 1), 1.0f / float((1 << 7) - 1),
		1.0f / float((1 << 8) - 1), 1.0f / float((1 << 9) - 1), 1.0f / float((1 << 10) - 1), 1.0f / float((1 << 11) - 1),
		1.0f / float((1 << 12) - 1), 1.0f / float((1 << 13) - 1), 1.0f / float((1 << 14) - 1), 1.0f / float((1 << 15) - 1),
		1.0f / float((1 << 16) - 1), 1.0f / float((1 << 17) - 1), 1.0f / float((1 << 18) - 1), 1.0f / float((1 << 19) - 1),
	};

	static constexpr uint32_t mask_values[] =
	{
		(1 << 0) - 1, (1 << 1) - 1, (1 << 2) - 1, (1 << 3) - 1,
		(1 << 4) - 1, (1 << 5) - 1, (1 << 6) - 1, (1 << 7) - 1,
		(1 << 8) - 1, (1 << 9) - 1, (1 << 10) - 1, (1 << 11) - 1,
		(1 << 12) - 1, (1 << 13) - 1, (1 << 14) - 1, (1 << 15) - 1,
		(1 << 16) - 1, (1 << 17) - 1, (1 << 18) - 1, (1 << 19) - 1,
	};

	const uint32_t mask = mask_values[num_bits];
	const uint32_t bit_shift = 32 - num_bits;
	const __m128 inv_max_value = _mm_load_ps1(&max_values[num_bits]);

	uint32_t byte_offset = bit_offset / 8;
	uint32_t vector_u32 = unaligned_load<uint32_t>(vector_data + byte_offset);
	vector_u32 = byte_swap(vector_u32);
	const uint32_t x32 = (vector_u32 >> (bit_shift - (bit_offset % 8))) & mask;

	bit_offset += num_bits;

	byte_offset = bit_offset / 8;
	vector_u32 = unaligned_load<uint32_t>(vector_data + byte_offset);
	vector_u32 = byte_swap(vector_u32);
	const uint32_t y32 = (vector_u32 >> (bit_shift - (bit_offset % 8))) & mask;

	bit_offset += num_bits;

	byte_offset = bit_offset / 8;
	vector_u32 = unaligned_load<uint32_t>(vector_data + byte_offset);
	vector_u32 = byte_swap(vector_u32);
	const uint32_t z32 = (vector_u32 >> (bit_shift - (bit_offset % 8))) & mask;

	const __m128 value = _mm_cvtepi32_ps(_mm_set_epi32(x32, z32, y32, x32));
	return _mm_mul_ps(value, inv_max_value);
}

// try SSE load/swap/shift/mask
NOINLINE Vector4_32 VECTORCALL unpack_vector3_n_o12(uint8_t num_bits, const uint8_t* vector_data, uint32_t bit_offset)
{
	// We need the 4 bytes that contain our value.
	// The input is in big-endian order, byte 0 is the first byte
	static constexpr uint8_t shuffle_values[][16] =
	{
		{ 3, 2, 1, 0, 3, 2, 1, 0, 3, 2, 1, 0, 0x80, 0x80, 0x80, 0x80 },	// 0
		{ 3, 2, 1, 0, 3, 2, 1, 0, 3, 2, 1, 0, 0x80, 0x80, 0x80, 0x80 },	// 1
		{ 3, 2, 1, 0, 3, 2, 1, 0, 3, 2, 1, 0, 0x80, 0x80, 0x80, 0x80 },	// 2
		{ 3, 2, 1, 0, 3, 2, 1, 0, 3, 2, 1, 0, 0x80, 0x80, 0x80, 0x80 },	// 3
		{ 3, 2, 1, 0, 3, 2, 1, 0, 3, 2, 1, 0, 0x80, 0x80, 0x80, 0x80 },	// 4
		{ 3, 2, 1, 0, 3, 2, 1, 0, 3, 2, 1, 0, 0x80, 0x80, 0x80, 0x80 },	// 5
		{ 3, 2, 1, 0, 3, 2, 1, 0, 3, 2, 1, 0, 0x80, 0x80, 0x80, 0x80 },	// 6
		{ 3, 2, 1, 0, 3, 2, 1, 0, 3, 2, 1, 0, 0x80, 0x80, 0x80, 0x80 },	// 7
		{ 3, 2, 1, 0, 3, 2, 1, 0, 3, 2, 1, 0, 0x80, 0x80, 0x80, 0x80 },	// 8
		{ 3, 2, 1, 0, 3, 2, 1, 0, 3, 2, 1, 0, 0x80, 0x80, 0x80, 0x80 },	// 9
		{ 3, 2, 1, 0, 3, 2, 1, 0, 3, 2, 1, 0, 0x80, 0x80, 0x80, 0x80 },	// 10
		{ 3, 2, 1, 0, 3, 2, 1, 0, 3, 2, 1, 0, 0x80, 0x80, 0x80, 0x80 },	// 11
		{ 3, 2, 1, 0, 3, 2, 1, 0, 3, 2, 1, 0, 0x80, 0x80, 0x80, 0x80 },	// 12
		{ 3, 2, 1, 0, 3, 2, 1, 0, 3, 2, 1, 0, 0x80, 0x80, 0x80, 0x80 },	// 13
		{ 3, 2, 1, 0, 3, 2, 1, 0, 3, 2, 1, 0, 0x80, 0x80, 0x80, 0x80 },	// 14
		{ 3, 2, 1, 0, 3, 2, 1, 0, 3, 2, 1, 0, 0x80, 0x80, 0x80, 0x80 },	// 15
		{ 3, 2, 1, 0, 3, 2, 1, 0, 7, 6, 5, 4, 0x80, 0x80, 0x80, 0x80 },	// 16
		{ 3, 2, 1, 0, 3, 2, 1, 0, 3, 2, 1, 0, 0x80, 0x80, 0x80, 0x80 },	// 17
		{ 3, 2, 1, 0, 3, 2, 1, 0, 3, 2, 1, 0, 0x80, 0x80, 0x80, 0x80 },	// 18
		{ 3, 2, 1, 0, 3, 2, 1, 0, 3, 2, 1, 0, 0x80, 0x80, 0x80, 0x80 },	// 19
	};

	static constexpr uint32_t shift_values[][4] =
	{
		{ 0, 0, 0, 0 },	// 0
		{ 31, 30, 29, 0 },	// 1
		{ 30, 28, 27, 0 },	// 2
		{ 30, 28, 27, 0 },	// 3
		{ 30, 28, 27, 0 },	// 4
		{ 30, 28, 27, 0 },	// 5
		{ 30, 28, 27, 0 },	// 6
		{ 30, 28, 27, 0 },	// 7
		{ 30, 28, 27, 0 },	// 8
		{ 30, 28, 27, 0 },	// 9
		{ 30, 28, 27, 0 },	// 10
		{ 30, 28, 27, 0 },	// 11
		{ 30, 28, 27, 0 },	// 12
		{ 30, 28, 27, 0 },	// 13
		{ 30, 28, 27, 0 },	// 14
		{ 30, 28, 27, 0 },	// 15
		{ 16, 0, 16, 0 },	// 16
		{ 30, 28, 27, 0 },	// 17
		{ 30, 28, 27, 0 },	// 18
		{ 30, 28, 27, 0 },	// 19
	};

	static constexpr uint32_t mask_values[] =
	{
		(1 << 0) - 1, (1 << 1) - 1, (1 << 2) - 1, (1 << 3) - 1,
		(1 << 4) - 1, (1 << 5) - 1, (1 << 6) - 1, (1 << 7) - 1,
		(1 << 8) - 1, (1 << 9) - 1, (1 << 10) - 1, (1 << 11) - 1,
		(1 << 12) - 1, (1 << 13) - 1, (1 << 14) - 1, (1 << 15) - 1,
		(1 << 16) - 1, (1 << 17) - 1, (1 << 18) - 1, (1 << 19) - 1,
	};

	static constexpr float max_values[] =
	{
		1.0f, 1.0f / float((1 << 1) - 1), 1.0f / float((1 << 2) - 1), 1.0f / float((1 << 3) - 1),
		1.0f / float((1 << 4) - 1), 1.0f / float((1 << 5) - 1), 1.0f / float((1 << 6) - 1), 1.0f / float((1 << 7) - 1),
		1.0f / float((1 << 8) - 1), 1.0f / float((1 << 9) - 1), 1.0f / float((1 << 10) - 1), 1.0f / float((1 << 11) - 1),
		1.0f / float((1 << 12) - 1), 1.0f / float((1 << 13) - 1), 1.0f / float((1 << 14) - 1), 1.0f / float((1 << 15) - 1),
		1.0f / float((1 << 16) - 1), 1.0f / float((1 << 17) - 1), 1.0f / float((1 << 18) - 1), 1.0f / float((1 << 19) - 1),
	};

	uint32_t byte_offset = bit_offset / 8;
	__m128i bytes = _mm_loadu_si128((const __m128i*)(vector_data + byte_offset));

	// Select the bytes we need and byte swap them
	__m128i vector_xyz = _mm_shuffle_epi8(bytes, _mm_loadu_si128((const __m128i*)(&shuffle_values[num_bits][0])));

	__m128i shift_offset = _mm_sub_epi32(_mm_loadu_si128((const __m128i*)(&shift_values[num_bits][0])), _mm_set1_epi32(bit_offset % 8));
	__m128i shift_offset_x = _mm_shuffle_epi32(shift_offset, _MM_SHUFFLE(3, 3, 3, 0));
	__m128i shift_offset_y = _mm_shuffle_epi32(shift_offset, _MM_SHUFFLE(3, 3, 3, 1));
	__m128i shift_offset_z = _mm_shuffle_epi32(shift_offset, _MM_SHUFFLE(3, 3, 3, 2));
	__m128i vector_x = _mm_srl_epi32(vector_xyz, shift_offset_x);
	__m128i vector_y = _mm_srl_epi32(vector_xyz, shift_offset_y);
	__m128i vector_z = _mm_srl_epi32(vector_xyz, shift_offset_z);
	__m128i vector_xxyy = _mm_castps_si128(_mm_shuffle_ps(_mm_castsi128_ps(vector_x), _mm_castsi128_ps(vector_y), _MM_SHUFFLE(1, 1, 0, 0)));
	vector_xyz = _mm_castps_si128(_mm_shuffle_ps(_mm_castsi128_ps(vector_xxyy), _mm_castsi128_ps(vector_z), _MM_SHUFFLE(2, 2, 2, 0)));

	__m128i mask = _mm_castps_si128(_mm_load_ps1((const float*)&mask_values[num_bits]));
	vector_xyz = _mm_and_si128(vector_xyz, mask);

	__m128 value = _mm_cvtepi32_ps(vector_xyz);
	__m128 inv_max_value = _mm_load_ps1(&max_values[num_bits]);
	return _mm_mul_ps(value, inv_max_value);
}

// scalar code path, try to go to SIMD sooner, perform AND mask in SIMD
NOINLINE Vector4_32 VECTORCALL unpack_vector3_n_o13(uint8_t num_bits, const uint8_t* vector_data, uint32_t bit_offset)
{
	static constexpr float max_values[20] =
	{
		1.0f, 1.0f / float((1 << 1) - 1), 1.0f / float((1 << 2) - 1), 1.0f / float((1 << 3) - 1),
		1.0f / float((1 << 4) - 1), 1.0f / float((1 << 5) - 1), 1.0f / float((1 << 6) - 1), 1.0f / float((1 << 7) - 1),
		1.0f / float((1 << 8) - 1), 1.0f / float((1 << 9) - 1), 1.0f / float((1 << 10) - 1), 1.0f / float((1 << 11) - 1),
		1.0f / float((1 << 12) - 1), 1.0f / float((1 << 13) - 1), 1.0f / float((1 << 14) - 1), 1.0f / float((1 << 15) - 1),
		1.0f / float((1 << 16) - 1), 1.0f / float((1 << 17) - 1), 1.0f / float((1 << 18) - 1), 1.0f / float((1 << 19) - 1),
	};

	static constexpr uint32_t mask_values[20] =
	{
		(1 << 0) - 1, (1 << 1) - 1, (1 << 2) - 1, (1 << 3) - 1,
		(1 << 4) - 1, (1 << 5) - 1, (1 << 6) - 1, (1 << 7) - 1,
		(1 << 8) - 1, (1 << 9) - 1, (1 << 10) - 1, (1 << 11) - 1,
		(1 << 12) - 1, (1 << 13) - 1, (1 << 14) - 1, (1 << 15) - 1,
		(1 << 16) - 1, (1 << 17) - 1, (1 << 18) - 1, (1 << 19) - 1,
	};

	const __m128i mask = _mm_castps_si128(_mm_load_ps1((const float*)&mask_values[num_bits]));
	const uint32_t bit_shift = 32 - num_bits;
	const __m128 inv_max_value = _mm_load_ps1(&max_values[num_bits]);

	uint32_t byte_offset = bit_offset / 8;
	uint32_t vector_u32 = unaligned_load<uint32_t>(vector_data + byte_offset);
	vector_u32 = byte_swap(vector_u32);
	const uint32_t x32 = (vector_u32 >> (bit_shift - (bit_offset % 8)));

	bit_offset += num_bits;

	byte_offset = bit_offset / 8;
	vector_u32 = unaligned_load<uint32_t>(vector_data + byte_offset);
	vector_u32 = byte_swap(vector_u32);
	const uint32_t y32 = (vector_u32 >> (bit_shift - (bit_offset % 8)));

	bit_offset += num_bits;

	byte_offset = bit_offset / 8;
	vector_u32 = unaligned_load<uint32_t>(vector_data + byte_offset);
	vector_u32 = byte_swap(vector_u32);
	const uint32_t z32 = (vector_u32 >> (bit_shift - (bit_offset % 8)));

	__m128i int_value = _mm_set_epi32(x32, z32, y32, x32);
	int_value = _mm_and_si128(int_value, mask);
	const __m128 value = _mm_cvtepi32_ps(int_value);
	return _mm_mul_ps(value, inv_max_value);
}

// try to build lookup table for shift from numbits/bitoffset
NOINLINE Vector4_32 VECTORCALL unpack_vector3_n_o14(uint8_t num_bits, const uint8_t* vector_data, uint32_t bit_offset)
{
	static constexpr float max_values[] =
	{
		1.0f, 1.0f / float((1 << 1) - 1), 1.0f / float((1 << 2) - 1), 1.0f / float((1 << 3) - 1),
		1.0f / float((1 << 4) - 1), 1.0f / float((1 << 5) - 1), 1.0f / float((1 << 6) - 1), 1.0f / float((1 << 7) - 1),
		1.0f / float((1 << 8) - 1), 1.0f / float((1 << 9) - 1), 1.0f / float((1 << 10) - 1), 1.0f / float((1 << 11) - 1),
		1.0f / float((1 << 12) - 1), 1.0f / float((1 << 13) - 1), 1.0f / float((1 << 14) - 1), 1.0f / float((1 << 15) - 1),
		1.0f / float((1 << 16) - 1), 1.0f / float((1 << 17) - 1), 1.0f / float((1 << 18) - 1), 1.0f / float((1 << 19) - 1),
	};

	static constexpr uint32_t mask_values[] =
	{
		(1 << 0) - 1, (1 << 1) - 1, (1 << 2) - 1, (1 << 3) - 1,
		(1 << 4) - 1, (1 << 5) - 1, (1 << 6) - 1, (1 << 7) - 1,
		(1 << 8) - 1, (1 << 9) - 1, (1 << 10) - 1, (1 << 11) - 1,
		(1 << 12) - 1, (1 << 13) - 1, (1 << 14) - 1, (1 << 15) - 1,
		(1 << 16) - 1, (1 << 17) - 1, (1 << 18) - 1, (1 << 19) - 1,
	};

	static constexpr uint8_t shift_values[8][19][3] =
	{
		{ { 32, 32, 32 }, { 31, 30, 29 }, { 28, 26, 24 }, { 23, 28, 25 }, { 24, 28, 24 }, { 23, 26, 21 }, { 20, 22, 24 }, { 23, 24, 25 }, { 24, 24, 24 }, { 23, 22, 21 }, { 20, 18, 16 }, { 15, 20, 17 }, { 16, 20, 16 }, { 15, 18, 13 }, { 12, 14, 16 }, { 15, 16, 17 }, { 16, 16, 16 }, { 15, 14, 13 }, { 12, 10, 8 } },
		{ { 31, 31, 31 }, { 30, 29, 28 }, { 27, 25, 23 }, { 22, 27, 24 }, { 23, 27, 23 }, { 22, 25, 20 }, { 19, 21, 23 }, { 22, 23, 24 }, { 23, 23, 23 }, { 22, 21, 20 }, { 19, 17, 15 }, { 14, 19, 16 }, { 15, 19, 15 }, { 14, 17, 12 }, { 11, 13, 15 }, { 14, 15, 16 }, { 15, 15, 15 }, { 14, 13, 12 }, { 11, 9, 7 } },
		{ { 30, 30, 30 }, { 29, 28, 27 }, { 26, 24, 30 }, { 29, 26, 23 }, { 22, 26, 22 }, { 21, 24, 27 }, { 26, 20, 22 }, { 21, 22, 23 }, { 22, 22, 22 }, { 21, 20, 19 }, { 18, 16, 22 }, { 21, 18, 15 }, { 14, 18, 14 }, { 13, 16, 19 }, { 18, 12, 14 }, { 13, 14, 15 }, { 14, 14, 14 }, { 13, 12, 11 }, { 10, 8, 14 } },
		{ { 29, 29, 29 }, { 28, 27, 26 }, { 25, 23, 29 }, { 28, 25, 22 }, { 21, 25, 21 }, { 20, 23, 26 }, { 25, 19, 21 }, { 20, 21, 22 }, { 21, 21, 21 }, { 20, 19, 18 }, { 17, 15, 21 }, { 20, 17, 14 }, { 13, 17, 13 }, { 12, 15, 18 }, { 17, 11, 13 }, { 12, 13, 14 }, { 13, 13, 13 }, { 12, 11, 10 }, { 9, 7, 13 } },
		{ { 28, 28, 28 }, { 27, 26, 25 }, { 24, 30, 28 }, { 27, 24, 29 }, { 28, 24, 28 }, { 27, 22, 25 }, { 24, 26, 20 }, { 19, 20, 21 }, { 20, 20, 20 }, { 19, 18, 17 }, { 16, 22, 20 }, { 19, 16, 21 }, { 20, 16, 20 }, { 19, 14, 17 }, { 16, 18, 12 }, { 11, 12, 13 }, { 12, 12, 12 }, { 11, 10, 9 }, { 8, 14, 12 } },
		{ { 27, 27, 27 }, { 26, 25, 24 }, { 23, 29, 27 }, { 26, 23, 28 }, { 27, 23, 27 }, { 26, 21, 24 }, { 23, 25, 19 }, { 18, 19, 20 }, { 19, 19, 19 }, { 18, 17, 16 }, { 15, 21, 19 }, { 18, 15, 20 }, { 19, 15, 19 }, { 18, 13, 16 }, { 15, 17, 11 }, { 10, 11, 12 }, { 11, 11, 11 }, { 10, 9, 8 }, { 7, 13, 11 } },
		{ { 26, 26, 26 }, { 25, 24, 31 }, { 30, 28, 26 }, { 25, 22, 27 }, { 26, 22, 26 }, { 25, 20, 23 }, { 22, 24, 26 }, { 25, 18, 19 }, { 18, 18, 18 }, { 17, 16, 23 }, { 22, 20, 18 }, { 17, 14, 19 }, { 18, 14, 18 }, { 17, 12, 15 }, { 14, 16, 18 }, { 17, 10, 11 }, { 10, 10, 10 }, { 9, 8, 15 }, { 14, 12, 10 } },
		{ { 25, 25, 25 }, { 24, 31, 30 }, { 29, 27, 25 }, { 24, 29, 26 }, { 25, 21, 25 }, { 24, 27, 22 }, { 21, 23, 25 }, { 24, 25, 18 }, { 17, 17, 17 }, { 16, 23, 22 }, { 21, 19, 17 }, { 16, 21, 18 }, { 17, 13, 17 }, { 16, 19, 14 }, { 13, 15, 17 }, { 16, 17, 10 }, { 9, 9, 9 }, { 8, 15, 14 }, { 13, 11, 9 } },
	};

	const __m128i mask = _mm_castps_si128(_mm_load_ps1((const float*)&mask_values[num_bits]));
	const __m128 inv_max_value = _mm_load_ps1(&max_values[num_bits]);

	uint32_t byte_offset = bit_offset / 8;
	uint32_t start_bit_offset = bit_offset % 8;
	uint32_t vector_u32 = unaligned_load<uint32_t>(vector_data + byte_offset);
	vector_u32 = byte_swap(vector_u32);
	const uint32_t x32 = vector_u32 >> shift_values[start_bit_offset][num_bits][0];

	bit_offset += num_bits;

	byte_offset = bit_offset / 8;
	vector_u32 = unaligned_load<uint32_t>(vector_data + byte_offset);
	vector_u32 = byte_swap(vector_u32);
	const uint32_t y32 = vector_u32 >> shift_values[start_bit_offset][num_bits][1];

	bit_offset += num_bits;

	byte_offset = bit_offset / 8;
	vector_u32 = unaligned_load<uint32_t>(vector_data + byte_offset);
	vector_u32 = byte_swap(vector_u32);
	const uint32_t z32 = vector_u32 >> shift_values[start_bit_offset][num_bits][2];

	__m128i int_value = _mm_set_epi32(x32, z32, y32, x32);
	int_value = _mm_and_si128(int_value, mask);
	const __m128 value = _mm_cvtepi32_ps(int_value);
	return _mm_mul_ps(value, inv_max_value);
}

// try to build lookup table for byte offsets from numbits/bitoffset
NOINLINE Vector4_32 VECTORCALL unpack_vector3_n_o15(uint8_t num_bits, const uint8_t* vector_data, uint32_t bit_offset)
{
	static constexpr float max_values[] =
	{
		1.0f, 1.0f / float((1 << 1) - 1), 1.0f / float((1 << 2) - 1), 1.0f / float((1 << 3) - 1),
		1.0f / float((1 << 4) - 1), 1.0f / float((1 << 5) - 1), 1.0f / float((1 << 6) - 1), 1.0f / float((1 << 7) - 1),
		1.0f / float((1 << 8) - 1), 1.0f / float((1 << 9) - 1), 1.0f / float((1 << 10) - 1), 1.0f / float((1 << 11) - 1),
		1.0f / float((1 << 12) - 1), 1.0f / float((1 << 13) - 1), 1.0f / float((1 << 14) - 1), 1.0f / float((1 << 15) - 1),
		1.0f / float((1 << 16) - 1), 1.0f / float((1 << 17) - 1), 1.0f / float((1 << 18) - 1), 1.0f / float((1 << 19) - 1),
	};

	static constexpr uint32_t mask_values[] =
	{
		(1 << 0) - 1, (1 << 1) - 1, (1 << 2) - 1, (1 << 3) - 1,
		(1 << 4) - 1, (1 << 5) - 1, (1 << 6) - 1, (1 << 7) - 1,
		(1 << 8) - 1, (1 << 9) - 1, (1 << 10) - 1, (1 << 11) - 1,
		(1 << 12) - 1, (1 << 13) - 1, (1 << 14) - 1, (1 << 15) - 1,
		(1 << 16) - 1, (1 << 17) - 1, (1 << 18) - 1, (1 << 19) - 1,
	};

	static constexpr uint8_t shift_values[8][19][3] =
	{
		{ { 32, 32, 32 },{ 31, 30, 29 },{ 28, 26, 24 },{ 23, 28, 25 },{ 24, 28, 24 },{ 23, 26, 21 },{ 20, 22, 24 },{ 23, 24, 25 },{ 24, 24, 24 },{ 23, 22, 21 },{ 20, 18, 16 },{ 15, 20, 17 },{ 16, 20, 16 },{ 15, 18, 13 },{ 12, 14, 16 },{ 15, 16, 17 },{ 16, 16, 16 },{ 15, 14, 13 },{ 12, 10, 8 } },
		{ { 31, 31, 31 },{ 30, 29, 28 },{ 27, 25, 23 },{ 22, 27, 24 },{ 23, 27, 23 },{ 22, 25, 20 },{ 19, 21, 23 },{ 22, 23, 24 },{ 23, 23, 23 },{ 22, 21, 20 },{ 19, 17, 15 },{ 14, 19, 16 },{ 15, 19, 15 },{ 14, 17, 12 },{ 11, 13, 15 },{ 14, 15, 16 },{ 15, 15, 15 },{ 14, 13, 12 },{ 11, 9, 7 } },
		{ { 30, 30, 30 },{ 29, 28, 27 },{ 26, 24, 30 },{ 29, 26, 23 },{ 22, 26, 22 },{ 21, 24, 27 },{ 26, 20, 22 },{ 21, 22, 23 },{ 22, 22, 22 },{ 21, 20, 19 },{ 18, 16, 22 },{ 21, 18, 15 },{ 14, 18, 14 },{ 13, 16, 19 },{ 18, 12, 14 },{ 13, 14, 15 },{ 14, 14, 14 },{ 13, 12, 11 },{ 10, 8, 14 } },
		{ { 29, 29, 29 },{ 28, 27, 26 },{ 25, 23, 29 },{ 28, 25, 22 },{ 21, 25, 21 },{ 20, 23, 26 },{ 25, 19, 21 },{ 20, 21, 22 },{ 21, 21, 21 },{ 20, 19, 18 },{ 17, 15, 21 },{ 20, 17, 14 },{ 13, 17, 13 },{ 12, 15, 18 },{ 17, 11, 13 },{ 12, 13, 14 },{ 13, 13, 13 },{ 12, 11, 10 },{ 9, 7, 13 } },
		{ { 28, 28, 28 },{ 27, 26, 25 },{ 24, 30, 28 },{ 27, 24, 29 },{ 28, 24, 28 },{ 27, 22, 25 },{ 24, 26, 20 },{ 19, 20, 21 },{ 20, 20, 20 },{ 19, 18, 17 },{ 16, 22, 20 },{ 19, 16, 21 },{ 20, 16, 20 },{ 19, 14, 17 },{ 16, 18, 12 },{ 11, 12, 13 },{ 12, 12, 12 },{ 11, 10, 9 },{ 8, 14, 12 } },
		{ { 27, 27, 27 },{ 26, 25, 24 },{ 23, 29, 27 },{ 26, 23, 28 },{ 27, 23, 27 },{ 26, 21, 24 },{ 23, 25, 19 },{ 18, 19, 20 },{ 19, 19, 19 },{ 18, 17, 16 },{ 15, 21, 19 },{ 18, 15, 20 },{ 19, 15, 19 },{ 18, 13, 16 },{ 15, 17, 11 },{ 10, 11, 12 },{ 11, 11, 11 },{ 10, 9, 8 },{ 7, 13, 11 } },
		{ { 26, 26, 26 },{ 25, 24, 31 },{ 30, 28, 26 },{ 25, 22, 27 },{ 26, 22, 26 },{ 25, 20, 23 },{ 22, 24, 26 },{ 25, 18, 19 },{ 18, 18, 18 },{ 17, 16, 23 },{ 22, 20, 18 },{ 17, 14, 19 },{ 18, 14, 18 },{ 17, 12, 15 },{ 14, 16, 18 },{ 17, 10, 11 },{ 10, 10, 10 },{ 9, 8, 15 },{ 14, 12, 10 } },
		{ { 25, 25, 25 },{ 24, 31, 30 },{ 29, 27, 25 },{ 24, 29, 26 },{ 25, 21, 25 },{ 24, 27, 22 },{ 21, 23, 25 },{ 24, 25, 18 },{ 17, 17, 17 },{ 16, 23, 22 },{ 21, 19, 17 },{ 16, 21, 18 },{ 17, 13, 17 },{ 16, 19, 14 },{ 13, 15, 17 },{ 16, 17, 10 },{ 9, 9, 9 },{ 8, 15, 14 },{ 13, 11, 9 } },
	};

	static constexpr uint8_t byte_offset_values[8][19][2] =
	{
			{ { 0, 0 }, { 0, 0 }, { 0, 0 }, { 1, 1 }, { 2, 2 }, { 3, 3 }, { 4, 5 }, { 6, 7 }, { 8, 9 }, { 10, 11 }, { 12, 13 }, { 15, 16 }, { 18, 19 }, { 21, 22 }, { 24, 26 }, { 28, 30 }, { 32, 34 }, { 36, 38 }, { 40, 42 } },
			{ { 0, 0 }, { 0, 0 }, { 0, 0 }, { 1, 1 }, { 2, 2 }, { 3, 3 }, { 4, 5 }, { 6, 7 }, { 8, 9 }, { 10, 11 }, { 12, 13 }, { 15, 16 }, { 18, 19 }, { 21, 22 }, { 24, 26 }, { 28, 30 }, { 32, 34 }, { 36, 38 }, { 40, 42 } },
			{ { 0, 0 }, { 0, 0 }, { 0, 1 }, { 1, 1 }, { 2, 2 }, { 3, 4 }, { 4, 5 }, { 6, 7 }, { 8, 9 }, { 10, 11 }, { 12, 14 }, { 15, 16 }, { 18, 19 }, { 21, 23 }, { 24, 26 }, { 28, 30 }, { 32, 34 }, { 36, 38 }, { 40, 43 } },
			{ { 0, 0 }, { 0, 0 }, { 0, 1 }, { 1, 1 }, { 2, 2 }, { 3, 4 }, { 4, 5 }, { 6, 7 }, { 8, 9 }, { 10, 11 }, { 12, 14 }, { 15, 16 }, { 18, 19 }, { 21, 23 }, { 24, 26 }, { 28, 30 }, { 32, 34 }, { 36, 38 }, { 40, 43 } },
			{ { 0, 0 }, { 0, 0 }, { 1, 1 }, { 1, 2 }, { 2, 3 }, { 3, 4 }, { 5, 5 }, { 6, 7 }, { 8, 9 }, { 10, 11 }, { 13, 14 }, { 15, 17 }, { 18, 20 }, { 21, 23 }, { 25, 26 }, { 28, 30 }, { 32, 34 }, { 36, 38 }, { 41, 43 } },
			{ { 0, 0 }, { 0, 0 }, { 1, 1 }, { 1, 2 }, { 2, 3 }, { 3, 4 }, { 5, 5 }, { 6, 7 }, { 8, 9 }, { 10, 11 }, { 13, 14 }, { 15, 17 }, { 18, 20 }, { 21, 23 }, { 25, 26 }, { 28, 30 }, { 32, 34 }, { 36, 38 }, { 41, 43 } },
			{ { 0, 0 }, { 0, 1 }, { 1, 1 }, { 1, 2 }, { 2, 3 }, { 3, 4 }, { 5, 6 }, { 6, 7 }, { 8, 9 }, { 10, 12 }, { 13, 14 }, { 15, 17 }, { 18, 20 }, { 21, 23 }, { 25, 27 }, { 28, 30 }, { 32, 34 }, { 36, 39 }, { 41, 43 } },
			{ { 0, 0 }, { 1, 1 }, { 1, 1 }, { 2, 2 }, { 2, 3 }, { 4, 4 }, { 5, 6 }, { 7, 7 }, { 8, 9 }, { 11, 12 }, { 13, 14 }, { 16, 17 }, { 18, 20 }, { 22, 23 }, { 25, 27 }, { 29, 30 }, { 32, 34 }, { 37, 39 }, { 41, 43 } },
	};

	const __m128i mask = _mm_castps_si128(_mm_load_ps1((const float*)&mask_values[num_bits]));
	const __m128 inv_max_value = _mm_load_ps1(&max_values[num_bits]);

	uint32_t byte_offset = bit_offset / 8;
	uint32_t start_bit_offset = bit_offset % 8;
	uint32_t vector_u32 = unaligned_load<uint32_t>(vector_data + byte_offset);
	vector_u32 = byte_swap(vector_u32);
	const uint32_t x32 = vector_u32 >> shift_values[start_bit_offset][num_bits][0];

	vector_u32 = unaligned_load<uint32_t>(vector_data + byte_offset + byte_offset_values[start_bit_offset][num_bits][0]);
	vector_u32 = byte_swap(vector_u32);
	const uint32_t y32 = vector_u32 >> shift_values[start_bit_offset][num_bits][1];

	vector_u32 = unaligned_load<uint32_t>(vector_data + byte_offset + byte_offset_values[start_bit_offset][num_bits][1]);
	vector_u32 = byte_swap(vector_u32);
	const uint32_t z32 = vector_u32 >> shift_values[start_bit_offset][num_bits][2];

	__m128i int_value = _mm_set_epi32(x32, z32, y32, x32);
	int_value = _mm_and_si128(int_value, mask);
	const __m128 value = _mm_cvtepi32_ps(int_value);
	return _mm_mul_ps(value, inv_max_value);
}

int main_impl(int argc, char* argv[])
{
#ifdef _WIN32
	// Disables Windows OS generated error dialogs and reporting
	SetErrorMode(SEM_FAILCRITICALERRORS);
	SetUnhandledExceptionFilter(&unhandled_exception_filter);
	_set_abort_behavior(0, _CALL_REPORTFAULT);
#endif

#if 0
	Quat_32 quat0 = quat_from_euler(deg2rad(30.0f), deg2rad(-45.0f), deg2rad(90.0f));
	Quat_32 quat1 = quat_from_euler(deg2rad(45.0f), deg2rad(60.0f), deg2rad(120.0f));

	const int32_t num_iterations = 100;
	double iter_results[num_iterations];
	volatile uint8_t vector_data[1024 * 4];

	memset((void*)&s_temp[0], 0, sizeof(s_temp));
	memset((void*)&iter_results[0], 0, sizeof(iter_results));

	for (int32_t iter = 0; iter < num_iterations; ++iter)
	{
		std::this_thread::sleep_for(std::chrono::nanoseconds(1));

		ScopeProfiler perf;
		//Quat_32 result = quat0;
		Vector4_32 result;
		for (int32_t i = 0; i < 1000000; ++i)
		{
			result = unpack_vector3_24_ref((const uint8_t*)&vector_data[i % (3 * 1024)]);
			vector_unaligned_write(result, (float*)&vector_data[(i + 2048) % (3 * 1024)]);
		}
		perf.stop();
		//quat_unaligned_write(result, (float*)&s_temp[0]);
		vector_unaligned_write(result, (float*)&s_temp[0]);
		iter_results[iter] = perf.get_elapsed_milliseconds();
		//printf("quat_lerp_ref took: %.3fms\n", perf.get_elapsed_milliseconds());
	}
	std::sort(&iter_results[0], &iter_results[num_iterations]);
	double median = (iter_results[(num_iterations / 2) - 1] + iter_results[num_iterations / 2]) * 0.5;
	double* iter_min = std::min_element(&iter_results[0], &iter_results[num_iterations]);
	log_("unpack_vector3_24_ref took: %.3fms | %.3fms\n", median, *iter_min);

	for (int32_t iter = 0; iter < num_iterations; ++iter)
	{
		std::this_thread::sleep_for(std::chrono::nanoseconds(1));

		ScopeProfiler perf;
		//Quat_32 result = quat0;
		Vector4_32 result;
		for (int32_t i = 0; i < 1000000; ++i)
		{
			result = unpack_vector3_24_1((const uint8_t*)&vector_data[i % (3 * 1024)]);
			vector_unaligned_write(result, (float*)&vector_data[(i + 2048) % (3 * 1024)]);
		}
		perf.stop();
		//quat_unaligned_write(result, (float*)&s_temp[0]);
		vector_unaligned_write(result, (float*)&s_temp[0]);
		iter_results[iter] = perf.get_elapsed_milliseconds();
		//printf("quat_lerp_ov7 took: %.3fms\n", perf.get_elapsed_milliseconds());
	}
	std::sort(&iter_results[0], &iter_results[num_iterations]);
	median = (iter_results[(num_iterations / 2) - 1] + iter_results[num_iterations / 2]) * 0.5;
	iter_min = std::min_element(&iter_results[0], &iter_results[num_iterations]);
	log_("unpack_vector3_24_1   took: %.3fms | %.3fms\n", median, *iter_min);

	for (int32_t iter = 0; iter < num_iterations; ++iter)
	{
		std::this_thread::sleep_for(std::chrono::nanoseconds(1));

		ScopeProfiler perf;
		//Quat_32 result = quat0;
		Vector4_32 result;
		for (int32_t i = 0; i < 1000000; ++i)
		{
			result = unpack_vector3_24_2((const uint8_t*)&vector_data[i % (3 * 1024)]);
			vector_unaligned_write(result, (float*)&vector_data[(i + 2048) % (3 * 1024)]);
		}
		perf.stop();
		//quat_unaligned_write(result, (float*)&s_temp[0]);
		vector_unaligned_write(result, (float*)&s_temp[0]);
		iter_results[iter] = perf.get_elapsed_milliseconds();
		//printf("quat_lerp_ov7 took: %.3fms\n", perf.get_elapsed_milliseconds());
}
	std::sort(&iter_results[0], &iter_results[num_iterations]);
	median = (iter_results[(num_iterations / 2) - 1] + iter_results[num_iterations / 2]) * 0.5;
	iter_min = std::min_element(&iter_results[0], &iter_results[num_iterations]);
	log_("unpack_vector3_24_2   took: %.3fms | %.3fms\n", median, *iter_min);

#if defined(ACL_SSE2_INTRINSICS)
	if (IsDebuggerPresent())
	{
		printf("Press any key to continue...\n");
		while (_kbhit() == 0);
		return 0;
	}
#else
	return 0;
#endif
#endif

#if 0
	const int32_t num_iterations = 100;
	double iter_results[num_iterations];
	volatile uint8_t vector_data[1024 * 4];

	memset((void*)&s_temp[0], 0, sizeof(s_temp));
	memset((void*)&iter_results[0], 0, sizeof(iter_results));
	memset((void*)&vector_data[0], 0, sizeof(vector_data));

#define RUN_TESTS(fun_name) \
	do { \
		for (int32_t iter = 0; iter < num_iterations; ++iter) \
		{ \
			std::this_thread::sleep_for(std::chrono::nanoseconds(1)); \
			ScopeProfiler perf; \
			Vector4_32 result; \
			for (int32_t i = 0; i < 1000000; ++i) \
			{ \
				result = fun_name(16, 16, 16, (const uint8_t*)&vector_data[i % (3 * 1024)], 0); \
				vector_unaligned_write(result, (float*)&vector_data[(i + 2048) % (3 * 1024)]); \
			} \
			perf.stop(); \
			vector_unaligned_write(result, (float*)&s_temp[0]); \
			iter_results[iter] = perf.get_elapsed_milliseconds(); \
		} \
		std::sort(&iter_results[0], &iter_results[num_iterations]); \
		double median = (iter_results[(num_iterations / 2) - 1] + iter_results[num_iterations / 2]) * 0.5; \
		double* iter_min = std::min_element(&iter_results[0], &iter_results[num_iterations]); \
		log_("%s took: %.3fms | %.3fms\n", #fun_name, median, *iter_min); \
	} while (0)

#define RUN_TESTS1(fun_name) \
	do { \
		for (int32_t iter = 0; iter < num_iterations; ++iter) \
		{ \
			std::this_thread::sleep_for(std::chrono::nanoseconds(1)); \
			ScopeProfiler perf; \
			Vector4_32 result; \
			for (int32_t i = 0; i < 1000000; ++i) \
			{ \
				result = fun_name(16, (const uint8_t*)&vector_data[i % (3 * 1024)], 0); \
				vector_unaligned_write(result, (float*)&vector_data[(i + 2048) % (3 * 1024)]); \
			} \
			perf.stop(); \
			vector_unaligned_write(result, (float*)&s_temp[0]); \
			iter_results[iter] = perf.get_elapsed_milliseconds(); \
		} \
		std::sort(&iter_results[0], &iter_results[num_iterations]); \
		double median = (iter_results[(num_iterations / 2) - 1] + iter_results[num_iterations / 2]) * 0.5; \
		double* iter_min = std::min_element(&iter_results[0], &iter_results[num_iterations]); \
		log_("%s took: %.3fms | %.3fms\n", #fun_name, median, *iter_min); \
	} while (0)

	RUN_TESTS(unpack_vector3_n_ref);
	RUN_TESTS(unpack_vector3_n_o00);
	RUN_TESTS1(unpack_vector3_n_o01);
	RUN_TESTS1(unpack_vector3_n_o02);
	RUN_TESTS1(unpack_vector3_n_o03);
	RUN_TESTS1(unpack_vector3_n_o04);
	RUN_TESTS1(unpack_vector3_n_o05);
	RUN_TESTS1(unpack_vector3_n_o06);
	RUN_TESTS1(unpack_vector3_n_o07);
	RUN_TESTS1(unpack_vector3_n_o08);
	RUN_TESTS1(unpack_vector3_n_o09);
	RUN_TESTS1(unpack_vector3_n_o10);
	RUN_TESTS1(unpack_vector3_n_o11);
	RUN_TESTS1(unpack_vector3_n_o12);
	RUN_TESTS1(unpack_vector3_n_o13);
	RUN_TESTS1(unpack_vector3_n_o14);
	RUN_TESTS1(unpack_vector3_n_o15);

#if defined(ACL_SSE2_INTRINSICS)
	if (IsDebuggerPresent())
	{
		printf("Press any key to continue...\n");
		while (_kbhit() == 0);
		return 0;
	}
#else
	return 0;
#endif
#endif

	int result = -1;
	try
	{
		result = safe_main_impl(argc, argv);
	}
	catch (const std::runtime_error& exception)
	{
		printf("Exception occurred: %s\n", exception.what());
		result = -1;
	}
	catch (...)
	{
		printf("Unknown exception occurred\n");
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
