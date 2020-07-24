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

#define DEBUG_MEGA_LARGE_CLIP 0

// Enable 64 bit file IO
#ifndef _WIN32
	#define _FILE_OFFSET_BITS 64
#endif

// Used to debug and validate that we compile without sjson-cpp
// Defaults to being enabled
#if defined(ACL_USE_SJSON)
	#include <sjson/writer.h>
	#include <sjson/parser.h>
#else
	namespace sjson { class ArrayWriter; }
#endif

#include "acl/core/ansi_allocator.h"
#include "acl/core/floating_point_exceptions.h"
#include "acl/core/iallocator.h"
#include "acl/core/range_reduction_types.h"
#include "acl/core/string.h"
#include "acl/core/impl/debug_track_writer.h"
#include "acl/compression/compress.h"
#include "acl/compression/transform_error_metrics.h"
#include "acl/compression/transform_pose_utils.h"	// Just to test compilation
#include "acl/compression/impl/write_decompression_stats.h"
#include "acl/compression/track_error.h"
#include "acl/decompression/decompress.h"
#include "acl/io/clip_reader.h"

#include <cstring>
#include <cstdio>
#include <fstream>
#include <streambuf>
#include <sstream>
#include <string>
#include <memory>

#if defined(_WIN32)
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

	bool			do_output_stats;
	const char*		output_stats_filename;
	std::FILE*		output_stats_file;

	const char*		output_bin_filename;

	compression_level8	compression_level;
	bool			compression_level_specified;

	bool			regression_testing;
	bool			profile_decompression;
	bool			exhaustive_compression;

	bool			use_matrix_error_metric;

	bool			is_bind_pose_relative;
	bool			is_bind_pose_additive0;
	bool			is_bind_pose_additive1;

	bool			stat_detailed_output;
	bool			stat_exhaustive_output;

	//////////////////////////////////////////////////////////////////////////

	Options() noexcept
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
		, do_output_stats(false)
		, output_stats_filename(nullptr)
		, output_stats_file(nullptr)
		, output_bin_filename(nullptr)
		, compression_level(compression_level8::lowest)
		, compression_level_specified(false)
		, regression_testing(false)
		, profile_decompression(false)
		, exhaustive_compression(false)
		, use_matrix_error_metric(false)
		, is_bind_pose_relative(false)
		, is_bind_pose_additive0(false)
		, is_bind_pose_additive1(false)
		, stat_detailed_output(false)
		, stat_exhaustive_output(false)
	{}

	~Options()
	{
		if (output_stats_file != nullptr && output_stats_file != stdout)
			std::fclose(output_stats_file);
	}

	Options(Options&& other) noexcept = default;
	Options(const Options&) = delete;
	Options& operator=(Options&& other) noexcept = default;
	Options& operator=(const Options&) = delete;

	void open_output_stats_file()
	{
		std::FILE* file = nullptr;
		if (output_stats_filename != nullptr)
		{
#ifdef _WIN32
			char path[64 * 1024] = { 0 };
			snprintf(path, get_array_size(path), "\\\\?\\%s", output_stats_filename);
			fopen_s(&file, path, "w");
#else
			file = fopen(output_stats_filename, "w");
#endif
			ACL_ASSERT(file != nullptr, "Failed to open output stats file: ", output_stats_filename);
		}

		output_stats_file = file != nullptr ? file : stdout;
	}
};

static constexpr const char* k_acl_input_file_option = "-acl=";
static constexpr const char* k_config_input_file_option = "-config=";
static constexpr const char* k_stats_output_option = "-stats";
static constexpr const char* k_bin_output_option = "-out=";
static constexpr const char* k_compression_level_option = "-level=";
static constexpr const char* k_regression_test_option = "-test";
static constexpr const char* k_profile_decompression_option = "-decomp";
static constexpr const char* k_exhaustive_compression_option = "-exhaustive";
static constexpr const char* k_bind_pose_relative_option = "-bind_rel";
static constexpr const char* k_bind_pose_additive0_option = "-bind_add0";
static constexpr const char* k_bind_pose_additive1_option = "-bind_add1";
static constexpr const char* k_matrix_error_metric_option = "-error_mtx";
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
			options.do_output_stats = true;
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

		option_length = std::strlen(k_compression_level_option);
		if (std::strncmp(argument, k_compression_level_option, option_length) == 0)
		{
			const char* level_name = argument + option_length;
			if (!get_compression_level(level_name, options.compression_level))
			{
				printf("Invalid compression level name specified: %s\n", level_name);
				return false;
			}
			options.compression_level_specified = true;
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

		option_length = std::strlen(k_matrix_error_metric_option);
		if (std::strncmp(argument, k_matrix_error_metric_option, option_length) == 0)
		{
			options.use_matrix_error_metric = true;
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

#if defined(ACL_USE_SJSON)
#if defined(ACL_HAS_ASSERT_CHECKS)
static void validate_accuracy(iallocator& allocator, const track_array_qvvf& raw_tracks, const track_array_qvvf& additive_base_tracks, const itransform_error_metric& error_metric, const compressed_tracks& compressed_tracks_, double regression_error_threshold)
{
	using namespace acl_impl;

	(void)allocator;
	(void)raw_tracks;
	(void)additive_base_tracks;
	(void)error_metric;
	(void)compressed_tracks_;
	(void)regression_error_threshold;

#if defined(ACL_HAS_ASSERT_CHECKS)
	// Disable floating point exceptions since decompression assumes it
	scope_disable_fp_exceptions fp_off;

	acl::decompression_context<acl::debug_transform_decompression_settings> context;
	context.initialize(compressed_tracks_);

	const track_error error = calculate_compression_error(allocator, raw_tracks, context, error_metric, additive_base_tracks);
	(void)error;
	ACL_ASSERT(rtm::scalar_is_finite(error.error), "Returned error is not a finite value");
	ACL_ASSERT(error.error < regression_error_threshold, "Error too high for bone %u: %f at time %f", error.index, error.error, error.sample_time);

	const uint32_t num_bones = raw_tracks.get_num_tracks();
	const float clip_duration = raw_tracks.get_duration();
	const float sample_rate = raw_tracks.get_sample_rate();
	const uint32_t num_samples = raw_tracks.get_num_samples_per_track();

	debug_track_writer track_writer(allocator, track_type8::qvvf, num_bones);

	// Regression test
	for (uint32_t sample_index = 0; sample_index < num_samples; ++sample_index)
	{
		const float sample_time = rtm::scalar_min(float(sample_index) / sample_rate, clip_duration);

		// We use the nearest sample to accurately measure the loss that happened, if any
		context.seek(sample_time, sample_rounding_policy::nearest);
		context.decompress_tracks(track_writer);

		// Validate decompress_track against decompress_tracks
		for (uint32_t bone_index = 0; bone_index < num_bones; ++bone_index)
		{
			const rtm::qvvf transform0 = track_writer.read_qvv(bone_index);

			context.decompress_track(bone_index, track_writer);
			const rtm::qvvf transform1 = track_writer.read_qvv(bone_index);

			ACL_ASSERT(rtm::vector_all_near_equal(rtm::quat_to_vector(transform0.rotation), rtm::quat_to_vector(transform1.rotation), 0.0F), "Failed to sample bone index: %u", bone_index);
			ACL_ASSERT(rtm::vector_all_near_equal3(transform0.translation, transform1.translation, 0.0F), "Failed to sample bone index: %u", bone_index);
			ACL_ASSERT(rtm::vector_all_near_equal3(transform0.scale, transform1.scale, 0.0F), "Failed to sample bone index: %u", bone_index);
		}
	}
#endif
}

static void validate_accuracy(iallocator& allocator, const track_array& raw_tracks, const compressed_tracks& tracks, double regression_error_threshold)
{
	(void)allocator;
	(void)raw_tracks;
	(void)tracks;
	(void)regression_error_threshold;

#if defined(ACL_HAS_ASSERT_CHECKS)
	using namespace acl_impl;

	// Disable floating point exceptions since decompression assumes it
	scope_disable_fp_exceptions fp_off;

	const float regression_error_thresholdf = static_cast<float>(regression_error_threshold);
	const rtm::vector4f regression_error_thresholdv = rtm::vector_set(regression_error_thresholdf);
	(void)regression_error_thresholdf;
	(void)regression_error_thresholdv;

	const float duration = tracks.get_duration();
	const float sample_rate = tracks.get_sample_rate();
	const uint32_t num_tracks = tracks.get_num_tracks();
	const uint32_t num_samples = tracks.get_num_samples_per_track();
	const track_type8 track_type = raw_tracks.get_track_type();

	ACL_ASSERT(rtm::scalar_near_equal(duration, raw_tracks.get_duration(), 1.0E-7F), "Duration mismatch");
	ACL_ASSERT(sample_rate == raw_tracks.get_sample_rate(), "Sample rate mismatch");
	ACL_ASSERT(num_tracks <= raw_tracks.get_num_tracks(), "Num tracks mismatch");
	ACL_ASSERT(num_samples == raw_tracks.get_num_samples_per_track(), "Num samples mismatch");

	decompression_context<debug_scalar_decompression_settings> context;
	context.initialize(tracks);

	debug_track_writer raw_tracks_writer(allocator, track_type, num_tracks);
	debug_track_writer raw_track_writer(allocator, track_type, num_tracks);
	debug_track_writer lossy_tracks_writer(allocator, track_type, num_tracks);
	debug_track_writer lossy_track_writer(allocator, track_type, num_tracks);

	const rtm::vector4f zero = rtm::vector_zero();

	// Regression test
	for (uint32_t sample_index = 0; sample_index < num_samples; ++sample_index)
	{
		const float sample_time = rtm::scalar_min(float(sample_index) / sample_rate, duration);

		// We use the nearest sample to accurately measure the loss that happened, if any
		raw_tracks.sample_tracks(sample_time, sample_rounding_policy::nearest, raw_tracks_writer);

		context.seek(sample_time, sample_rounding_policy::nearest);
		context.decompress_tracks(lossy_tracks_writer);

		// Validate decompress_tracks
		for (uint32_t track_index = 0; track_index < num_tracks; ++track_index)
		{
			const track& track_ = raw_tracks[track_index];
			const uint32_t output_index = track_.get_output_index();
			if (output_index == k_invalid_track_index)
				continue;	// Track is being stripped, ignore it

			rtm::vector4f error = zero;

			switch (track_type)
			{
			case track_type8::float1f:
			{
				const float raw_value = raw_tracks_writer.read_float1(track_index);
				const float lossy_value = lossy_tracks_writer.read_float1(output_index);
				error = rtm::vector_set(rtm::scalar_abs(raw_value - lossy_value));
				break;
			}
			case track_type8::float2f:
			{
				const rtm::vector4f raw_value = raw_tracks_writer.read_float2(track_index);
				const rtm::vector4f lossy_value = lossy_tracks_writer.read_float2(output_index);
				error = rtm::vector_abs(rtm::vector_sub(raw_value, lossy_value));
				error = rtm::vector_mix<rtm::mix4::x, rtm::mix4::y, rtm::mix4::c, rtm::mix4::d>(error, zero);
				break;
			}
			case track_type8::float3f:
			{
				const rtm::vector4f raw_value = raw_tracks_writer.read_float3(track_index);
				const rtm::vector4f lossy_value = lossy_tracks_writer.read_float3(output_index);
				error = rtm::vector_abs(rtm::vector_sub(raw_value, lossy_value));
				error = rtm::vector_mix<rtm::mix4::x, rtm::mix4::y, rtm::mix4::z, rtm::mix4::d>(error, zero);
				break;
			}
			case track_type8::float4f:
			{
				const rtm::vector4f raw_value = raw_tracks_writer.read_float4(track_index);
				const rtm::vector4f lossy_value = lossy_tracks_writer.read_float4(output_index);
				error = rtm::vector_abs(rtm::vector_sub(raw_value, lossy_value));
				break;
			}
			case track_type8::vector4f:
			{
				const rtm::vector4f raw_value = raw_tracks_writer.read_vector4(track_index);
				const rtm::vector4f lossy_value = lossy_tracks_writer.read_vector4(output_index);
				error = rtm::vector_abs(rtm::vector_sub(raw_value, lossy_value));
				break;
			}
			default:
				ACL_ASSERT(false, "Unsupported track type");
				break;
			}

			(void)error;
			ACL_ASSERT(rtm::vector_is_finite(error), "Returned error is not a finite value");
			ACL_ASSERT(rtm::vector_all_less_than(error, regression_error_thresholdv), "Error too high for track %u at time %f", track_index, sample_time);
		}

		// Validate decompress_track
		for (uint32_t track_index = 0; track_index < num_tracks; ++track_index)
		{
			const track& track_ = raw_tracks[track_index];
			const uint32_t output_index = track_.get_output_index();
			if (output_index == k_invalid_track_index)
				continue;	// Track is being stripped, ignore it

			// We use the nearest sample to accurately measure the loss that happened, if any
			raw_tracks.sample_track(track_index, sample_time, sample_rounding_policy::nearest, raw_track_writer);
			context.decompress_track(output_index, lossy_track_writer);

			switch (track_type)
			{
			case track_type8::float1f:
			{
				const float raw_value_ = raw_tracks_writer.read_float1(track_index);
				const float lossy_value_ = lossy_tracks_writer.read_float1(output_index);
				const float raw_value = raw_track_writer.read_float1(track_index);
				const float lossy_value = lossy_track_writer.read_float1(output_index);
				ACL_ASSERT(rtm::scalar_near_equal(raw_value, lossy_value, regression_error_thresholdf), "Error too high for track %u at time %f", track_index, sample_time);
				ACL_ASSERT(rtm::scalar_near_equal(raw_value_, raw_value, 0.00001F), "Failed to sample track %u at time %f", track_index, sample_time);
				ACL_ASSERT(rtm::scalar_near_equal(lossy_value_, lossy_value, 0.00001F), "Failed to sample track %u at time %f", track_index, sample_time);
				break;
			}
			case track_type8::float2f:
			{
				const rtm::vector4f raw_value_ = raw_tracks_writer.read_float2(track_index);
				const rtm::vector4f lossy_value_ = lossy_tracks_writer.read_float2(output_index);
				const rtm::vector4f raw_value = raw_track_writer.read_float2(track_index);
				const rtm::vector4f lossy_value = lossy_track_writer.read_float2(output_index);
				ACL_ASSERT(rtm::vector_all_near_equal2(raw_value, lossy_value, regression_error_thresholdf), "Error too high for track %u at time %f", track_index, sample_time);
				ACL_ASSERT(rtm::vector_all_near_equal2(raw_value_, raw_value, 0.00001F), "Failed to sample track %u at time %f", track_index, sample_time);
				ACL_ASSERT(rtm::vector_all_near_equal2(lossy_value_, lossy_value, 0.00001F), "Failed to sample track %u at time %f", track_index, sample_time);
				break;
			}
			case track_type8::float3f:
			{
				const rtm::vector4f raw_value_ = raw_tracks_writer.read_float3(track_index);
				const rtm::vector4f lossy_value_ = lossy_tracks_writer.read_float3(output_index);
				const rtm::vector4f raw_value = raw_track_writer.read_float3(track_index);
				const rtm::vector4f lossy_value = lossy_track_writer.read_float3(output_index);
				ACL_ASSERT(rtm::vector_all_near_equal3(raw_value, lossy_value, regression_error_thresholdf), "Error too high for track %u at time %f", track_index, sample_time);
				ACL_ASSERT(rtm::vector_all_near_equal3(raw_value_, raw_value, 0.00001F), "Failed to sample track %u at time %f", track_index, sample_time);
				ACL_ASSERT(rtm::vector_all_near_equal3(lossy_value_, lossy_value, 0.00001F), "Failed to sample track %u at time %f", track_index, sample_time);
				break;
			}
			case track_type8::float4f:
			{
				const rtm::vector4f raw_value_ = raw_tracks_writer.read_float4(track_index);
				const rtm::vector4f lossy_value_ = lossy_tracks_writer.read_float4(output_index);
				const rtm::vector4f raw_value = raw_track_writer.read_float4(track_index);
				const rtm::vector4f lossy_value = lossy_track_writer.read_float4(output_index);
				ACL_ASSERT(rtm::vector_all_near_equal(raw_value, lossy_value, regression_error_thresholdf), "Error too high for track %u at time %f", track_index, sample_time);
				ACL_ASSERT(rtm::vector_all_near_equal(raw_value_, raw_value, 0.00001F), "Failed to sample track %u at time %f", track_index, sample_time);
				ACL_ASSERT(rtm::vector_all_near_equal(lossy_value_, lossy_value, 0.00001F), "Failed to sample track %u at time %f", track_index, sample_time);
				break;
			}
			case track_type8::vector4f:
			{
				const rtm::vector4f raw_value_ = raw_tracks_writer.read_vector4(track_index);
				const rtm::vector4f lossy_value_ = lossy_tracks_writer.read_vector4(output_index);
				const rtm::vector4f raw_value = raw_track_writer.read_vector4(track_index);
				const rtm::vector4f lossy_value = lossy_track_writer.read_vector4(output_index);
				ACL_ASSERT(rtm::vector_all_near_equal(raw_value, lossy_value, regression_error_thresholdf), "Error too high for track %u at time %f", track_index, sample_time);
				ACL_ASSERT(rtm::vector_all_near_equal(raw_value_, raw_value, 0.00001F), "Failed to sample track %u at time %f", track_index, sample_time);
				ACL_ASSERT(rtm::vector_all_near_equal(lossy_value_, lossy_value, 0.00001F), "Failed to sample track %u at time %f", track_index, sample_time);
				break;
			}
			default:
				ACL_ASSERT(false, "Unsupported track type");
				break;
			}
		}
	}
#endif	// defined(ACL_HAS_ASSERT_CHECKS)
}

static void validate_metadata(const track_array& raw_tracks, const compressed_tracks& tracks)
{
	const uint32_t num_tracks = raw_tracks.get_num_tracks();

	// Validate track list name
	const string& raw_list_name = raw_tracks.get_name();
	const char* compressed_list_name = tracks.get_name();
	ACL_ASSERT(raw_list_name == compressed_list_name, "Unexpected track list name");

	// Validate track names
	for (uint32_t track_index = 0; track_index < num_tracks; ++track_index)
	{
		const track& raw_track = raw_tracks[track_index];
		const uint32_t output_index = raw_track.get_output_index();
		if (output_index == k_invalid_track_index)
			continue;	// Stripped

		const string& raw_name = raw_track.get_name();
		const char* compressed_name = tracks.get_track_name(output_index);
		ACL_ASSERT(raw_name == compressed_name, "Unexpected track name");
	}

	if (raw_tracks.get_track_type() == track_type8::qvvf)
	{
		// Specific to transform tracks
		const track_array_qvvf& transform_tracks = track_array_cast<track_array_qvvf>(raw_tracks);

		for (uint32_t track_index = 0; track_index < num_tracks; ++track_index)
		{
			const track_qvvf& raw_track = transform_tracks[track_index];
			const uint32_t output_index = raw_track.get_output_index();
			if (output_index == k_invalid_track_index)
				continue;	// Stripped

			const track_desc_transformf& raw_desc = raw_track.get_description();
			const uint32_t parent_track_index = raw_desc.parent_index;
			const uint32_t parent_track_output_index = parent_track_index != k_invalid_track_index ? transform_tracks[parent_track_index].get_output_index() : k_invalid_track_index;

			const uint32_t compressed_parent_track_index = tracks.get_parent_track_index(output_index);
			ACL_ASSERT(parent_track_output_index == compressed_parent_track_index, "Unexpected parent track index");

			track_desc_transformf compressed_desc;
			const bool compressed_track_desc_found = tracks.get_track_description(output_index, compressed_desc);
			ACL_ASSERT(compressed_track_desc_found, "Expected track description");
			ACL_ASSERT(output_index == compressed_desc.output_index, "Unexpected output index");
			ACL_ASSERT(parent_track_output_index == compressed_desc.parent_index, "Unexpected parent track index");
			ACL_ASSERT(raw_desc.precision == compressed_desc.precision, "Unexpected precision");
			ACL_ASSERT(raw_desc.shell_distance == compressed_desc.shell_distance, "Unexpected shell_distance");
			ACL_ASSERT(raw_desc.constant_rotation_threshold_angle == compressed_desc.constant_rotation_threshold_angle, "Unexpected constant_rotation_threshold_angle");
			ACL_ASSERT(raw_desc.constant_translation_threshold == compressed_desc.constant_translation_threshold, "Unexpected constant_translation_threshold");
			ACL_ASSERT(raw_desc.constant_scale_threshold == compressed_desc.constant_scale_threshold, "Unexpected constant_scale_threshold");
		}
	}
	else
	{
		// Specific to scalar tracks
		for (uint32_t track_index = 0; track_index < num_tracks; ++track_index)
		{
			const track& raw_track = raw_tracks[track_index];
			const uint32_t output_index = raw_track.get_output_index();
			if (output_index == k_invalid_track_index)
				continue;	// Stripped

			const track_desc_scalarf& raw_desc = raw_track.get_description<track_desc_scalarf>();

			track_desc_scalarf compressed_desc;
			const bool compressed_track_desc_found = tracks.get_track_description(output_index, compressed_desc);
			ACL_ASSERT(compressed_track_desc_found, "Expected track description");
			ACL_ASSERT(output_index == compressed_desc.output_index, "Unexpected output index");
			ACL_ASSERT(raw_desc.precision == compressed_desc.precision, "Unexpected precision");
		}
	}
}
#endif

static void try_algorithm(const Options& options, iallocator& allocator, track_array_qvvf& transform_tracks, const track_array_qvvf& additive_base, additive_clip_format8 additive_format, compression_settings settings, stat_logging logging, sjson::ArrayWriter* runs_writer, double regression_error_threshold)
{
	(void)runs_writer;
	(void)regression_error_threshold;

	auto try_algorithm_impl = [&](sjson::ObjectWriter* stats_writer)
	{
		if (transform_tracks.get_num_samples_per_track() == 0)
			return;

		// When regression testing, we include all the metadata
		if (options.regression_testing)
		{
			settings.include_track_list_name = true;
			settings.include_track_names = true;
			settings.include_parent_track_indices = true;
			settings.include_track_descriptions = true;
		}

		output_stats stats;
		stats.logging = logging;
		stats.writer = stats_writer;

		compressed_tracks* compressed_tracks_ = nullptr;
		const error_result result = compress_track_list(allocator, transform_tracks, settings, additive_base, additive_format, compressed_tracks_, stats);

		(void)result;
		ACL_ASSERT(result.empty(), result.c_str());
		ACL_ASSERT(compressed_tracks_->is_valid(true).empty(), "Compressed tracks are invalid");

#if defined(SJSON_CPP_WRITER)
		if (logging != stat_logging::none)
		{
			// Disable floating point exceptions since decompression assumes it
			scope_disable_fp_exceptions fp_off;

			acl::decompression_context<acl::debug_transform_decompression_settings> context;
			context.initialize(*compressed_tracks_);

			const track_error error = calculate_compression_error(allocator, transform_tracks, context, *settings.error_metric);

			stats_writer->insert("max_error", error.error);
			stats_writer->insert("worst_track", error.index);
			stats_writer->insert("worst_time", error.sample_time);

			if (are_any_enum_flags_set(logging, stat_logging::summary_decompression))
				acl_impl::write_decompression_performance_stats(allocator, settings, *compressed_tracks_, logging, *stats_writer);
		}
#endif

#if defined(ACL_HAS_ASSERT_CHECKS)
		if (options.regression_testing)
		{
			validate_accuracy(allocator, transform_tracks, additive_base, *settings.error_metric, *compressed_tracks_, regression_error_threshold);
			validate_metadata(transform_tracks, *compressed_tracks_);
		}
#endif

		if (options.output_bin_filename != nullptr)
		{
			std::ofstream output_file_stream(options.output_bin_filename, std::ios_base::out | std::ios_base::binary | std::ios_base::trunc);
			if (output_file_stream.is_open())
				output_file_stream.write(reinterpret_cast<const char*>(compressed_tracks_), compressed_tracks_->get_size());
		}

		allocator.deallocate(compressed_tracks_, compressed_tracks_->get_size());
	};

#if defined(SJSON_CPP_WRITER)
	if (runs_writer != nullptr)
		runs_writer->push([&](sjson::ObjectWriter& writer) { try_algorithm_impl(&writer); });
	else
#endif
		try_algorithm_impl(nullptr);
}

static void try_algorithm(const Options& options, iallocator& allocator, const track_array& track_list, stat_logging logging, sjson::ArrayWriter* runs_writer, double regression_error_threshold)
{
	(void)runs_writer;
	(void)regression_error_threshold;

	auto try_algorithm_impl = [&](sjson::ObjectWriter* stats_writer)
	{
		if (track_list.is_empty())
			return;

		compression_settings settings;

		// When regression testing, we include all the metadata
		if (options.regression_testing)
		{
			settings.include_track_list_name = true;
			settings.include_track_names = true;
			settings.include_track_descriptions = true;
		}

		output_stats stats;
		stats.logging = logging;
		stats.writer = stats_writer;

		compressed_tracks* compressed_tracks_ = nullptr;
		const error_result result = compress_track_list(allocator, track_list, settings, compressed_tracks_, stats);

		ACL_ASSERT(result.empty(), result.c_str()); (void)result;
		ACL_ASSERT(compressed_tracks_->is_valid(true).empty(), "Compressed tracks are invalid");

#if defined(SJSON_CPP_WRITER)
		if (logging != stat_logging::none)
		{
			// Disable floating point exceptions since decompression assumes it
			scope_disable_fp_exceptions fp_off;

			acl::decompression_context<acl::debug_scalar_decompression_settings> context;
			context.initialize(*compressed_tracks_);

			const track_error error = calculate_compression_error(allocator, track_list, context);

			stats_writer->insert("max_error", error.error);
			stats_writer->insert("worst_track", error.index);
			stats_writer->insert("worst_time", error.sample_time);

			// TODO: measure decompression performance
			//if (are_any_enum_flags_set(logging, stat_logging::summary_decompression))
				//write_decompression_performance_stats(allocator, settings, *compressed_clip, logging, *stats_writer);
		}
#endif

#if defined(ACL_HAS_ASSERT_CHECKS)
		if (options.regression_testing)
		{
			validate_accuracy(allocator, track_list, *compressed_tracks_, regression_error_threshold);
			validate_metadata(track_list, *compressed_tracks_);
		}
#endif

		if (options.output_bin_filename != nullptr)
		{
			std::ofstream output_file_stream(options.output_bin_filename, std::ios_base::out | std::ios_base::binary | std::ios_base::trunc);
			if (output_file_stream.is_open())
				output_file_stream.write(reinterpret_cast<const char*>(compressed_tracks_), compressed_tracks_->get_size());
		}

		allocator.deallocate(compressed_tracks_, compressed_tracks_->get_size());
	};

#if defined(SJSON_CPP_WRITER)
	if (runs_writer != nullptr)
		runs_writer->push([&](sjson::ObjectWriter& writer) { try_algorithm_impl(&writer); });
	else
#endif
		try_algorithm_impl(nullptr);
}

static bool read_acl_sjson_file(iallocator& allocator, const Options& options,
	sjson_file_type& out_file_type,
	sjson_raw_clip& out_raw_clip,
	sjson_raw_track_list& out_raw_track_list)
{
	char* sjson_file_buffer = nullptr;
	size_t file_size = 0;

#if defined(__ANDROID__)
	clip_reader reader(allocator, options.input_buffer, options.input_buffer_size - 1);
#else
	// Use the raw C API with a large buffer to ensure this is as fast as possible
	std::FILE* file = nullptr;

#ifdef _WIN32
	char path[64 * 1024] = { 0 };
	snprintf(path, get_array_size(path), "\\\\?\\%s", options.input_filename);
	fopen_s(&file, path, "rb");
#else
	file = fopen(options.input_filename, "rb");
#endif

	if (file == nullptr)
		return false;

	// Make sure to enable buffering with a large buffer
	const int setvbuf_result = setvbuf(file, NULL, _IOFBF, 1 * 1024 * 1024);
	if (setvbuf_result != 0)
		return false;

	const int fseek_result = fseek(file, 0, SEEK_END);
	if (fseek_result != 0)
	{
		fclose(file);
		return false;
	}

#ifdef _WIN32
	file_size = static_cast<size_t>(_ftelli64(file));
#else
	file_size = static_cast<size_t>(ftello(file));
#endif

	if (file_size == static_cast<size_t>(-1L))
	{
		fclose(file);
		return false;
	}

	rewind(file);

	sjson_file_buffer = allocate_type_array<char>(allocator, file_size);
	const size_t result = fread(sjson_file_buffer, 1, file_size, file);
	fclose(file);

	if (result != file_size)
	{
		deallocate_type_array(allocator, sjson_file_buffer, file_size);
		return false;
	}

	clip_reader reader(allocator, sjson_file_buffer, file_size - 1);
#endif

	const sjson_file_type ftype = reader.get_file_type();
	out_file_type = ftype;

	bool success = false;
	switch (ftype)
	{
	case sjson_file_type::unknown:
	default:
		printf("\nUnknown file type\n");
		break;
	case sjson_file_type::raw_clip:
		success = reader.read_raw_clip(out_raw_clip);
		break;
	case sjson_file_type::raw_track_list:
		success = reader.read_raw_track_list(out_raw_track_list);
		break;
	}

	if (!success)
	{
		const clip_reader_error err = reader.get_error();
		if (err.error != clip_reader_error::None)
			printf("\nError on line %d column %d: %s\n", err.line, err.column, err.get_description());
	}

	deallocate_type_array(allocator, sjson_file_buffer, file_size);
	return success;
}

static bool read_config(iallocator& allocator, Options& options, compression_settings& out_settings, double& out_regression_error_threshold)
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
		uint32_t line;
		uint32_t column;
		parser.get_position(line, column);

		printf("Error on line %d column %d: Missing config version\n", line, column);
		return false;
	}

	if (version != 2.0)
	{
		printf("Unsupported version: %f\n", version);
		return false;
	}

	sjson::StringView algorithm_name;
	if (!parser.read("algorithm_name", algorithm_name))
	{
		uint32_t line;
		uint32_t column;
		parser.get_position(line, column);

		printf("Error on line %d column %d: Missing algorithm name\n", line, column);
		return false;
	}

	algorithm_type8 algorithm_type;
	if (!get_algorithm_type(algorithm_name.c_str(), algorithm_type))
	{
		printf("Invalid algorithm name: %s\n", string(allocator, algorithm_name.c_str(), algorithm_name.size()).c_str());
		return false;
	}

	compression_settings default_settings;

	sjson::StringView compression_level;
	parser.try_read("level", compression_level, get_compression_level_name(default_settings.level));
	if (!get_compression_level(compression_level.c_str(), out_settings.level))
	{
		printf("Invalid compression level: %s\n", string(allocator, compression_level.c_str(), compression_level.size()).c_str());
		return false;
	}

	sjson::StringView rotation_format;
	parser.try_read("rotation_format", rotation_format, get_rotation_format_name(default_settings.rotation_format));
	if (!get_rotation_format(rotation_format.c_str(), out_settings.rotation_format))
	{
		printf("Invalid rotation format: %s\n", string(allocator, rotation_format.c_str(), rotation_format.size()).c_str());
		return false;
	}

	sjson::StringView translation_format;
	parser.try_read("translation_format", translation_format, get_vector_format_name(default_settings.translation_format));
	if (!get_vector_format(translation_format.c_str(), out_settings.translation_format))
	{
		printf("Invalid translation format: %s\n", string(allocator, translation_format.c_str(), translation_format.size()).c_str());
		return false;
	}

	sjson::StringView scale_format;
	parser.try_read("scale_format", scale_format, get_vector_format_name(default_settings.scale_format));
	if (!get_vector_format(scale_format.c_str(), out_settings.scale_format))
	{
		printf("Invalid scale format: %s\n", string(allocator, scale_format.c_str(), scale_format.size()).c_str());
		return false;
	}

	double dummy;
	parser.try_read("constant_rotation_threshold_angle", dummy, 0.0F);

	parser.try_read("constant_translation_threshold", dummy, 0.0F);
	parser.try_read("constant_scale_threshold", dummy, 0.0F);
	parser.try_read("error_threshold", dummy, 0.0F);

	parser.try_read("regression_error_threshold", out_regression_error_threshold, 0.0);

	bool is_bind_pose_relative;
	if (parser.try_read("is_bind_pose_relative", is_bind_pose_relative, false))
		options.is_bind_pose_relative = is_bind_pose_relative;

	bool use_matrix_error_metric;
	if (parser.try_read("use_matrix_error_metric", use_matrix_error_metric, false))
		options.use_matrix_error_metric = use_matrix_error_metric;

	if (!parser.is_valid() || !parser.remainder_is_comments_and_whitespace())
	{
		uint32_t line;
		uint32_t column;
		parser.get_position(line, column);

		printf("Error on line %d column %d: Expected end of file\n", line, column);
		return false;
	}

	return true;
}

static itransform_error_metric* create_additive_error_metric(iallocator& allocator, additive_clip_format8 format)
{
	switch (format)
	{
	case additive_clip_format8::relative:
		return allocate_type<additive_qvvf_transform_error_metric<additive_clip_format8::relative>>(allocator);
	case additive_clip_format8::additive0:
		return allocate_type<additive_qvvf_transform_error_metric<additive_clip_format8::additive0>>(allocator);
	case additive_clip_format8::additive1:
		return allocate_type<additive_qvvf_transform_error_metric<additive_clip_format8::additive1>>(allocator);
	default:
		return nullptr;
	}
}

static void create_additive_base_clip(const Options& options, track_array_qvvf& clip, const track_qvvf& bind_pose, track_array_qvvf& out_base_clip, additive_clip_format8& out_additive_format)
{
	// Convert the animation clip to be relative to the bind pose
	const uint32_t num_bones = clip.get_num_tracks();
	const uint32_t num_samples = clip.get_num_samples_per_track();
	iallocator& allocator = *clip.get_allocator();
	const track_desc_transformf bind_desc;

	out_base_clip = track_array_qvvf(*clip.get_allocator(), num_bones);

	additive_clip_format8 additive_format = additive_clip_format8::none;
	if (options.is_bind_pose_relative)
		additive_format = additive_clip_format8::relative;
	else if (options.is_bind_pose_additive0)
		additive_format = additive_clip_format8::additive0;
	else if (options.is_bind_pose_additive1)
		additive_format = additive_clip_format8::additive1;
	out_additive_format = additive_format;

	for (uint32_t bone_index = 0; bone_index < num_bones; ++bone_index)
	{
		// Get the bind transform and make sure it has no scale
		rtm::qvvf bind_transform = bind_pose[bone_index];
		bind_transform.scale = rtm::vector_set(1.0F);

		track_qvvf& track = clip[bone_index];

		for (uint32_t sample_index = 0; sample_index < num_samples; ++sample_index)
		{
			const rtm::qvvf bone_transform = track[sample_index];

			rtm::qvvf bind_local_transform = bone_transform;
			if (options.is_bind_pose_relative)
				bind_local_transform = convert_to_relative(bind_transform, bone_transform);
			else if (options.is_bind_pose_additive0)
				bind_local_transform = convert_to_additive0(bind_transform, bone_transform);
			else if (options.is_bind_pose_additive1)
				bind_local_transform = convert_to_additive1(bind_transform, bone_transform);

			track[sample_index] = bind_local_transform;
		}

		out_base_clip[bone_index] = track_qvvf::make_copy(bind_desc, allocator, &bind_transform, 1, 30.0F);
	}
}

static compression_settings make_settings(rotation_format8 rotation_format, vector_format8 translation_format, vector_format8 scale_format)
{
	compression_settings settings;
	settings.rotation_format = rotation_format;
	settings.translation_format = translation_format;
	settings.scale_format = scale_format;
	return settings;
}
#endif	// defined(ACL_USE_SJSON)

static int safe_main_impl(int argc, char* argv[])
{
	Options options;

	if (!parse_options(argc, argv, options))
		return -1;

	if (options.profile_decompression)
	{
#if defined(_WIN32)
		// Set the process affinity to core 2, we'll use core 0 for the python script
		SetProcessAffinityMask(GetCurrentProcess(), 1 << 2);
#endif
	}

#if defined(ACL_USE_SJSON)
	ansi_allocator allocator;
	track_array_qvvf transform_tracks;
	track_array_qvvf base_clip;
	additive_clip_format8 additive_format = additive_clip_format8::none;
	track_qvvf bind_pose;

#if defined(__ANDROID__)
	const bool is_input_acl_bin_file = options.input_buffer_binary;
#else
	const bool is_input_acl_bin_file = is_acl_bin_file(options.input_filename);
#endif

	bool use_external_config = false;
	compression_settings settings;

	sjson_file_type sjson_type = sjson_file_type::unknown;
	sjson_raw_clip sjson_clip;
	sjson_raw_track_list sjson_track_list;

	if (!is_input_acl_bin_file)
	{
		if (!read_acl_sjson_file(allocator, options, sjson_type, sjson_clip, sjson_track_list))
			return -1;

		transform_tracks = std::move(sjson_clip.track_list);
		base_clip = std::move(sjson_clip.additive_base_track_list);
		additive_format = sjson_clip.additive_format;
		bind_pose = std::move(sjson_clip.bind_pose);
		use_external_config = sjson_clip.has_settings;
		settings = sjson_clip.settings;
	}

#if DEBUG_MEGA_LARGE_CLIP
	track_array_qvvf new_transforms(allocator, transform_tracks.get_num_tracks());
	float new_sample_rate = 19200.0F;
	uint32_t new_num_samples = calculate_num_samples(transform_tracks.get_duration(), new_sample_rate);
	float new_duration = calculate_duration(new_num_samples, new_sample_rate);
	acl_impl::debug_track_writer dummy_writer(allocator, track_type8::qvvf, transform_tracks.get_num_tracks());

	for (uint32_t track_index = 0; track_index < transform_tracks.get_num_tracks(); ++track_index)
	{
		track_qvvf& track = transform_tracks[track_index];
		new_transforms[track_index] = track_qvvf::make_reserve(track.get_description(), allocator, new_num_samples, new_sample_rate);
	}

	for (uint32_t sample_index = 0; sample_index < new_num_samples; ++sample_index)
	{
		const float sample_time = rtm::scalar_min(float(sample_index) / new_sample_rate, new_duration);

		transform_tracks.sample_tracks(sample_time, sample_rounding_policy::none, dummy_writer);

		for (uint32_t track_index = 0; track_index < new_transforms.get_num_tracks(); ++track_index)
		{
			track_qvvf& track = new_transforms[track_index];
			track[sample_index] = dummy_writer.tracks_typed.qvvf[track_index];
		}
	}

	transform_tracks = std::move(new_transforms);
#endif

	double regression_error_threshold = 0.1;

#if defined(__ANDROID__)
	if (options.config_buffer != nullptr && options.config_buffer_size != 0)
#else
	if (options.config_filename != nullptr && std::strlen(options.config_filename) != 0)
#endif
	{
		// Override whatever the ACL SJSON file might have contained
		settings = compression_settings();

		if (!read_config(allocator, options, settings, regression_error_threshold))
			return -1;

		use_external_config = true;
	}

	if (!is_input_acl_bin_file && sjson_type == sjson_file_type::raw_clip)
	{
		// Grab whatever clip we might have read from the sjson file and cast the const away so we can manage the memory
		if (base_clip.is_empty() && !bind_pose.is_empty())
		{
			if (options.is_bind_pose_relative || options.is_bind_pose_additive0 || options.is_bind_pose_additive1)
				create_additive_base_clip(options, transform_tracks, bind_pose, base_clip, additive_format);
		}

		// First try to create an additive error metric
		settings.error_metric = create_additive_error_metric(allocator, additive_format);

		if (settings.error_metric == nullptr)
		{
			if (options.use_matrix_error_metric)
				settings.error_metric = allocate_type<qvvf_matrix3x4f_transform_error_metric>(allocator);
			else
				settings.error_metric = allocate_type<qvvf_transform_error_metric>(allocator);
		}
	}

	// Compress & Decompress
	auto exec_algos = [&](sjson::ArrayWriter* runs_writer)
	{
		stat_logging logging = options.do_output_stats ? stat_logging::summary : stat_logging::none;

		if (options.stat_detailed_output)
			logging |= stat_logging::detailed;

		if (options.stat_exhaustive_output)
			logging |= stat_logging::exhaustive;

		if (options.profile_decompression)
			logging |= stat_logging::summary_decompression | stat_logging::exhaustive_decompression;

		if (is_input_acl_bin_file)
		{
#if defined(SJSON_CPP_WRITER)
			if (options.profile_decompression && runs_writer != nullptr)
			{
				// Disable floating point exceptions since decompression assumes it
				scope_disable_fp_exceptions fp_off;

				settings = get_default_compression_settings();

#if defined(__ANDROID__)
				const compressed_tracks* compressed_clip = make_compressed_tracks(options.input_buffer);
				ACL_ASSERT(compressed_clip != nullptr, "Compressed clip is invalid");
				if (compressed_clip == nullptr)
					return;	// Compressed clip is invalid, early out to avoid crash

				runs_writer->push([&](sjson::ObjectWriter& writer)
				{
					acl_impl::write_decompression_performance_stats(allocator, settings, *compressed_clip, logging, writer);
				});
#else
				std::ifstream input_file_stream(options.input_filename, std::ios_base::in | std::ios_base::binary);
				if (input_file_stream.is_open())
				{
					input_file_stream.seekg(0, std::ios_base::end);
					const size_t buffer_size = size_t(input_file_stream.tellg());
					input_file_stream.seekg(0, std::ios_base::beg);

					char* buffer = (char*)allocator.allocate(buffer_size, alignof(compressed_tracks));
					input_file_stream.read(buffer, buffer_size);

					const compressed_tracks* compressed_clip = make_compressed_tracks(buffer);
					ACL_ASSERT(compressed_clip != nullptr, "Compressed clip is invalid");
					if (compressed_clip == nullptr)
						return;	// Compressed clip is invalid, early out to avoid crash

					runs_writer->push([&](sjson::ObjectWriter& writer)
					{
						acl_impl::write_decompression_performance_stats(allocator, settings, *compressed_clip, logging, writer);
					});

					allocator.deallocate(buffer, buffer_size);
				}
#endif
			}
#endif
		}
		else if (sjson_type == sjson_file_type::raw_clip)
		{
			if (use_external_config)
			{
				if (options.compression_level_specified)
					settings.level = options.compression_level;

				try_algorithm(options, allocator, transform_tracks, base_clip, additive_format, settings, logging, runs_writer, regression_error_threshold);
			}
			else if (options.exhaustive_compression)
			{
				{
					compression_settings uniform_tests[] =
					{
						make_settings(rotation_format8::quatf_full, vector_format8::vector3f_full, vector_format8::vector3f_full),
						make_settings(rotation_format8::quatf_drop_w_full, vector_format8::vector3f_full, vector_format8::vector3f_full),

						make_settings(rotation_format8::quatf_drop_w_variable, vector_format8::vector3f_variable, vector_format8::vector3f_full),
						make_settings(rotation_format8::quatf_drop_w_variable, vector_format8::vector3f_variable, vector_format8::vector3f_variable),
					};

					for (compression_settings test_settings : uniform_tests)
					{
						test_settings.error_metric = settings.error_metric;

						try_algorithm(options, allocator, transform_tracks, base_clip, additive_format, test_settings, logging, runs_writer, regression_error_threshold);
					}
				}

				{
					compression_settings uniform_tests[] =
					{
						make_settings(rotation_format8::quatf_full, vector_format8::vector3f_full, vector_format8::vector3f_full),
						make_settings(rotation_format8::quatf_drop_w_full, vector_format8::vector3f_full, vector_format8::vector3f_full),

						make_settings(rotation_format8::quatf_drop_w_variable, vector_format8::vector3f_variable, vector_format8::vector3f_full),
						make_settings(rotation_format8::quatf_drop_w_variable, vector_format8::vector3f_variable, vector_format8::vector3f_variable),
					};

					for (compression_settings test_settings : uniform_tests)
					{
						test_settings.error_metric = settings.error_metric;

						if (options.compression_level_specified)
							test_settings.level = options.compression_level;

						try_algorithm(options, allocator, transform_tracks, base_clip, additive_format, test_settings, logging, runs_writer, regression_error_threshold);
					}
				}
			}
			else
			{
				compression_settings default_settings = get_default_compression_settings();
				default_settings.error_metric = settings.error_metric;

				if (options.compression_level_specified)
					default_settings.level = options.compression_level;

				try_algorithm(options, allocator, transform_tracks, base_clip, additive_format, default_settings, logging, runs_writer, regression_error_threshold);
			}
		}
		else if (sjson_type == sjson_file_type::raw_track_list)
		{
			try_algorithm(options, allocator, sjson_track_list.track_list, logging, runs_writer, regression_error_threshold);
		}
	};

#if defined(SJSON_CPP_WRITER)
	if (options.do_output_stats)
	{
		sjson::FileStreamWriter stream_writer(options.output_stats_file);
		sjson::Writer writer(stream_writer);

		writer["runs"] = [&](sjson::ArrayWriter& runs_writer) { exec_algos(&runs_writer); };
	}
	else
#endif
		exec_algos(nullptr);

	deallocate_type(allocator, settings.error_metric);
#endif	// defined(ACL_USE_SJSON)

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

int main_impl(int argc, char* argv[])
{
#ifdef _WIN32
	// Disables Windows OS generated error dialogs and reporting
	SetErrorMode(SEM_FAILCRITICALERRORS);
	SetUnhandledExceptionFilter(&unhandled_exception_filter);
	_set_abort_behavior(0, _CALL_REPORTFAULT);
#endif

	// Enable floating point exceptions when possible to detect errors when regression testing
	scope_enable_fp_exceptions fp_on;

	int result = -1;
#if defined(ACL_ON_ASSERT_THROW) || defined(SJSON_CPP_ON_ASSERT_THROW)
	try
#endif
	{
		result = safe_main_impl(argc, argv);
	}
#if defined(ACL_ON_ASSERT_THROW)
	catch (const runtime_assert& exception)
	{
		printf("Assert occurred: %s\n", exception.what());
		result = -1;
	}
#endif
#if defined(SJSON_CPP_ON_ASSERT_THROW)
	catch (const sjson::runtime_assert& exception)
	{
		printf("Assert occurred: %s\n", exception.what());
		result = -1;
	}
#endif

#ifdef _WIN32
	if (IsDebuggerPresent())
	{
		printf("Press any key to continue...\n");
		while (_kbhit() == 0);
	}
#endif

	return result;
}
