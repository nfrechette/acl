////////////////////////////////////////////////////////////////////////////////
// The MIT License (MIT)
//
// Copyright (c) 2018 Nicholas Frechette & Animation Compression Library contributors
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

#include <catch2/catch.hpp>

// Enable allocation tracking
#define ACL_ALLOCATOR_TRACK_NUM_ALLOCATIONS
#define ACL_ALLOCATOR_TRACK_ALL_ALLOCATIONS

#if defined(ACL_USE_SJSON)
	#include <sjson/parser.h>
	#include <sjson/writer.h>
#endif

#include <acl/core/ansi_allocator.h>
#include <acl/io/clip_reader.h>
#include <acl/io/clip_writer.h>

#include <rtm/qvvd.h>
#include <rtm/scalarf.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <thread>

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

#if defined(RTM_SSE2_INTRINSICS) && defined(ACL_USE_SJSON)
#ifdef _WIN32
	constexpr uint32_t k_max_filename_size = MAX_PATH;
#else
	constexpr uint32_t k_max_filename_size = 1024;
#endif

static void get_temporary_filename(char* filename, uint32_t filename_size, const char* prefix)
{
#ifdef _WIN32
	DWORD result = GetTempPathA(filename_size, filename);
	REQUIRE(result != 0);

	char id[1024];
	snprintf(id, get_array_size(id), "%u", std::rand());

	strcat_s(filename, filename_size, prefix);
	strcat_s(filename, filename_size, id);
	strcat_s(filename, filename_size, ".acl.sjson");
#else
	(void)filename_size;
	
	char id[1024];
	snprintf(id, get_array_size(id), "%u", std::rand());

	std::strcat(filename, "/tmp/");
	std::strcat(filename, prefix);
	std::strcat(filename, id);
	std::strcat(filename, ".acl.sjson");
#endif
}
#endif

TEST_CASE("sjson_clip_reader_writer", "[io]")
{
	// Only test the reader/writer on non-mobile platforms
#if defined(RTM_SSE2_INTRINSICS) && defined(ACL_USE_SJSON)
	ansi_allocator allocator;

	const uint32_t num_tracks = 3;
	const uint32_t num_samples = 4;
	track_array_qvvf track_list(allocator, num_tracks);

	track_desc_transformf desc0;
	desc0.output_index = 0;
	desc0.precision = 0.001F;
	track_qvvf track0 = track_qvvf::make_reserve(desc0, allocator, num_samples, 32.0F);
	track0[0].rotation = rtm::quat_from_euler(0.1F, 0.5F, 1.2F);
	track0[0].translation = rtm::vector_set(0.0F, 0.6F, 2.3F);
	track0[0].scale = rtm::vector_set(1.4F, 2.1F, 0.2F);
	for (uint32_t i = 1; i < num_samples; ++i)
	{
		track0[i].rotation = rtm::quat_lerp(rtm::quat_identity(), track0[0].rotation, 0.1F * float(i));
		track0[i].translation = rtm::vector_lerp(rtm::vector_zero(), track0[0].translation, 0.1F * float(i));
		track0[i].scale = rtm::vector_lerp(rtm::vector_zero(), track0[0].scale, 0.1F * float(i));
	}
	track0.set_name(acl::string(allocator, "track 0"));
	track_list[0] = track0.get_ref();

	track_desc_transformf desc1;
	desc1.output_index = 0;
	desc1.precision = 0.001F;
	desc1.parent_index = 0;
	desc1.shell_distance = 0.1241F;
	desc1.constant_rotation_threshold_angle = 21.0F;
	desc1.constant_translation_threshold = 0.11F;
	desc1.constant_scale_threshold = 12.0F;
	track_qvvf track1 = track_qvvf::make_reserve(desc1, allocator, num_samples, 32.0F);
	track1[0].rotation = rtm::quat_from_euler(1.1F, 1.5F, 1.7F);
	track1[0].translation = rtm::vector_set(0.0221F, 10.6F, 22.3F);
	track1[0].scale = rtm::vector_set(1.451F, 24.1F, 10.2F);
	for (uint32_t i = 1; i < num_samples; ++i)
	{
		track1[i].rotation = rtm::quat_lerp(rtm::quat_identity(), track1[0].rotation, 0.1F * float(i));
		track1[i].translation = rtm::vector_lerp(rtm::vector_zero(), track1[0].translation, 0.1F * float(i));
		track1[i].scale = rtm::vector_lerp(rtm::vector_zero(), track1[0].scale, 0.1F * float(i));
	}
	track1.set_name(acl::string(allocator, "track 1"));
	track_list[1] = track1.get_ref();

	track_qvvf track2 = track_qvvf::make_reserve(desc1, allocator, num_samples, 32.0F);
	track2[0].rotation = rtm::quat_from_euler(1.11F, 1.5333F, 0.17F);
	track2[0].translation = rtm::vector_set(30.0221F, 101.6F, 22.3214F);
	track2[0].scale = rtm::vector_set(21.451F, 244.1F, 100.2F);
	for (uint32_t i = 1; i < num_samples; ++i)
	{
		track2[i].rotation = rtm::quat_lerp(rtm::quat_identity(), track2[0].rotation, 0.1F * float(i));
		track2[i].translation = rtm::vector_lerp(rtm::vector_zero(), track2[0].translation, 0.1F * float(i));
		track2[i].scale = rtm::vector_lerp(rtm::vector_zero(), track2[0].scale, 0.1F * float(i));
	}
	track_list[2] = track2.get_ref();

	track_list.set_name(acl::string(allocator, "some track list"));

	compression_settings settings;
	settings.level = compression_level8::high;
	settings.rotation_format = rotation_format8::quatf_drop_w_variable;
	settings.scale_format = vector_format8::vector3f_variable;
	settings.translation_format = vector_format8::vector3f_variable;

	const uint32_t filename_size = k_max_filename_size;
	char filename[filename_size] = { 0 };

	error_result error;
	for (uint32_t try_count = 0; try_count < 20; ++try_count)
	{
		get_temporary_filename(filename, filename_size, "clip_");

		// Write the clip to a temporary file
		error = write_track_list(track_list, settings, filename);

		if (error.empty())
			break;	// Everything worked, stop trying
	}
	REQUIRE(error.empty());

	std::FILE* file = nullptr;

	for (uint32_t try_count = 0; try_count < 20; ++try_count)
	{
#ifdef _WIN32
		fopen_s(&file, filename, "rb");
#else
		file = fopen(filename, "rb");
#endif

		if (file != nullptr)
			break;	// File is open, all good

		// Sleep a bit before tring again
		std::this_thread::sleep_for(std::chrono::seconds(1));
	}
	REQUIRE(file != nullptr);

	char sjson_file_buffer[256 * 1024];
	const size_t clip_size = fread(sjson_file_buffer, 1, get_array_size(sjson_file_buffer), file);
	fclose(file);

	std::remove(filename);

	// Read back the clip
	clip_reader reader(allocator, sjson_file_buffer, clip_size - 1);

	REQUIRE(reader.get_file_type() == sjson_file_type::raw_track_list);

	sjson_raw_track_list file_clip;
	const bool success = reader.read_raw_track_list(file_clip);
	REQUIRE(success);

	CHECK(file_clip.has_settings);
	CHECK(file_clip.settings.get_hash() == settings.get_hash());

	CHECK(file_clip.track_list.get_num_samples_per_track() == track_list.get_num_samples_per_track());
	CHECK(file_clip.track_list.get_sample_rate() == track_list.get_sample_rate());
	CHECK(file_clip.track_list.get_num_tracks() == track_list.get_num_tracks());
	CHECK(rtm::scalar_near_equal(file_clip.track_list.get_duration(), track_list.get_duration(), 1.0E-8F));
	CHECK(file_clip.track_list.get_track_type() == track_list.get_track_type());
	CHECK(file_clip.track_list.get_track_category() == track_list.get_track_category());
	CHECK(file_clip.track_list.get_name() == track_list.get_name());

	for (uint32_t track_index = 0; track_index < num_tracks; ++track_index)
	{
		const track_qvvf& ref_track = track_cast<track_qvvf>(track_list[track_index]);
		const track_qvvf& file_track = track_cast<track_qvvf>(file_clip.track_list[track_index]);

		CHECK(file_track.get_description().output_index == ref_track.get_description().output_index);
		CHECK(file_track.get_description().parent_index == ref_track.get_description().parent_index);
		CHECK(rtm::scalar_near_equal(file_track.get_description().precision, ref_track.get_description().precision, 0.0F));
		CHECK(rtm::scalar_near_equal(file_track.get_description().shell_distance, ref_track.get_description().shell_distance, 0.0F));
		CHECK(rtm::scalar_near_equal(file_track.get_description().constant_rotation_threshold_angle, ref_track.get_description().constant_rotation_threshold_angle, 0.0F));
		CHECK(rtm::scalar_near_equal(file_track.get_description().constant_translation_threshold, ref_track.get_description().constant_translation_threshold, 0.0F));
		CHECK(rtm::scalar_near_equal(file_track.get_description().constant_scale_threshold, ref_track.get_description().constant_scale_threshold, 0.0F));
		CHECK(file_track.get_num_samples() == ref_track.get_num_samples());
		CHECK(file_track.get_output_index() == ref_track.get_output_index());
		CHECK(file_track.get_sample_rate() == ref_track.get_sample_rate());
		CHECK(file_track.get_type() == ref_track.get_type());
		CHECK(file_track.get_category() == ref_track.get_category());
		CHECK(file_track.get_name() == ref_track.get_name());

		for (uint32_t sample_index = 0; sample_index < num_samples; ++sample_index)
		{
			const rtm::qvvf& ref_sample = ref_track[sample_index];
			const rtm::qvvf& file_sample = file_track[sample_index];
			CHECK(rtm::quat_near_equal(ref_sample.rotation, file_sample.rotation, 0.0F));
			CHECK(rtm::vector_all_near_equal3(ref_sample.translation, file_sample.translation, 0.0F));
			CHECK(rtm::vector_all_near_equal3(ref_sample.scale, file_sample.scale, 0.0F));
		}
	}
#endif
}

TEST_CASE("sjson_track_list_reader_writer float1f", "[io]")
{
	// Only test the reader/writer on non-mobile platforms
#if defined(RTM_SSE2_INTRINSICS) && defined(ACL_USE_SJSON)
	ansi_allocator allocator;

	const uint32_t num_tracks = 3;
	const uint32_t num_samples = 4;
	track_array_float1f track_list(allocator, num_tracks);

	track_desc_scalarf desc0;
	desc0.output_index = 0;
	desc0.precision = 0.001F;
	track_float1f track0 = track_float1f::make_reserve(desc0, allocator, num_samples, 32.0F);
	track0[0] = 1.0F;
	track0[1] = 2.333F;
	track0[2] = 3.123F;
	track0[3] = 4.5F;
	track_list[0] = track0.get_ref();
	track_float1f track1 = track_float1f::make_reserve(desc0, allocator, num_samples, 32.0F);
	track1[0] = 12.0F;
	track1[1] = 21.1231F;
	track1[2] = 3.1444123F;
	track1[3] = 421.5156F;
	track_list[1] = track1.get_ref();
	track_float1f track2 = track_float1f::make_reserve(desc0, allocator, num_samples, 32.0F);
	track2[0] = 11.61F;
	track2[1] = 23313.367F;
	track2[2] = 313.7876F;
	track2[3] = 4441.514F;
	track_list[2] = track2.get_ref();

	const uint32_t filename_size = k_max_filename_size;
	char filename[filename_size] = { 0 };

	error_result error;
	for (uint32_t try_count = 0; try_count < 20; ++try_count)
	{
		get_temporary_filename(filename, filename_size, "list_float1f_");

		// Write the clip to a temporary file
		error = write_track_list(track_list, filename);

		if (error.empty())
			break;	// Everything worked, stop trying
	}
	REQUIRE(error.empty());

	std::FILE* file = nullptr;
	for (uint32_t try_count = 0; try_count < 20; ++try_count)
	{
#ifdef _WIN32
		fopen_s(&file, filename, "rb");
#else
		file = fopen(filename, "rb");
#endif

		if (file != nullptr)
			break;	// File is open, all good

		// Sleep a bit before tring again
		std::this_thread::sleep_for(std::chrono::seconds(1));
	}
	REQUIRE(file != nullptr);

	char sjson_file_buffer[256 * 1024];
	const size_t buffer_size = fread(sjson_file_buffer, 1, get_array_size(sjson_file_buffer), file);
	fclose(file);

	std::remove(filename);

	// Read back the clip
	clip_reader reader(allocator, sjson_file_buffer, buffer_size - 1);

	REQUIRE(reader.get_file_type() == sjson_file_type::raw_track_list);

	sjson_raw_track_list file_track_list;
	const bool success = reader.read_raw_track_list(file_track_list);
	REQUIRE(success);

	CHECK(file_track_list.track_list.get_num_samples_per_track() == track_list.get_num_samples_per_track());
	CHECK(file_track_list.track_list.get_sample_rate() == track_list.get_sample_rate());
	CHECK(file_track_list.track_list.get_num_tracks() == track_list.get_num_tracks());
	CHECK(rtm::scalar_near_equal(file_track_list.track_list.get_duration(), track_list.get_duration(), 1.0E-8F));
	CHECK(file_track_list.track_list.get_track_type() == track_list.get_track_type());
	CHECK(file_track_list.track_list.get_track_category() == track_list.get_track_category());

	for (uint32_t track_index = 0; track_index < num_tracks; ++track_index)
	{
		const track_float1f& ref_track = track_cast<track_float1f>(track_list[track_index]);
		const track_float1f& file_track = track_cast<track_float1f>(file_track_list.track_list[track_index]);

		CHECK(file_track.get_description().output_index == ref_track.get_description().output_index);
		CHECK(rtm::scalar_near_equal(file_track.get_description().precision, ref_track.get_description().precision, 0.0F));
		CHECK(file_track.get_num_samples() == ref_track.get_num_samples());
		CHECK(file_track.get_output_index() == ref_track.get_output_index());
		CHECK(file_track.get_sample_rate() == ref_track.get_sample_rate());
		CHECK(file_track.get_type() == ref_track.get_type());
		CHECK(file_track.get_category() == ref_track.get_category());

		for (uint32_t sample_index = 0; sample_index < num_samples; ++sample_index)
		{
			const float ref_sample = ref_track[sample_index];
			const float file_sample = file_track[sample_index];
			CHECK(rtm::scalar_near_equal(ref_sample, file_sample, 0.0F));
		}
	}
#endif
}

TEST_CASE("sjson_track_list_reader_writer float2f", "[io]")
{
	// Only test the reader/writer on non-mobile platforms
#if defined(RTM_SSE2_INTRINSICS) && defined(ACL_USE_SJSON)
	ansi_allocator allocator;

	const uint32_t num_tracks = 3;
	const uint32_t num_samples = 4;
	track_array_float2f track_list(allocator, num_tracks);

	track_desc_scalarf desc0;
	desc0.output_index = 0;
	desc0.precision = 0.001F;
	track_float2f track0 = track_float2f::make_reserve(desc0, allocator, num_samples, 32.0F);
	track0[0] = rtm::float2f{ 1.0F, 3123.0F };
	track0[1] = rtm::float2f{ 2.333F, 321.13F };
	track0[2] = rtm::float2f{ 3.123F, 81.0F };
	track0[3] = rtm::float2f{ 4.5F, 91.13F };
	track_list[0] = track0.get_ref();
	track_float2f track1 = track_float2f::make_reserve(desc0, allocator, num_samples, 32.0F);
	track1[0] = rtm::float2f{ 12.0F, 91.013F };
	track1[1] = rtm::float2f{ 21.1231F, 911.14F };
	track1[2] = rtm::float2f{ 3.1444123F, 113.44F };
	track1[3] = rtm::float2f{ 421.5156F, 913901.0F };
	track_list[1] = track1.get_ref();
	track_float2f track2 = track_float2f::make_reserve(desc0, allocator, num_samples, 32.0F);
	track2[0] = rtm::float2f{ 11.61F, 90.13F };
	track2[1] = rtm::float2f{ 23313.367F, 13.3F };
	track2[2] = rtm::float2f{ 313.7876F, 931.2F };
	track2[3] = rtm::float2f{ 4441.514F, 913.56F };
	track_list[2] = track2.get_ref();

	const uint32_t filename_size = k_max_filename_size;
	char filename[filename_size] = { 0 };

	error_result error;
	for (uint32_t try_count = 0; try_count < 20; ++try_count)
	{
		get_temporary_filename(filename, filename_size, "list_float2f_");

		// Write the clip to a temporary file
		error = write_track_list(track_list, filename);

		if (error.empty())
			break;	// Everything worked, stop trying
	}
	REQUIRE(error.empty());

	std::FILE* file = nullptr;
	for (uint32_t try_count = 0; try_count < 20; ++try_count)
	{
#ifdef _WIN32
		fopen_s(&file, filename, "rb");
#else
		file = fopen(filename, "rb");
#endif

		if (file != nullptr)
			break;	// File is open, all good

		// Sleep a bit before tring again
		std::this_thread::sleep_for(std::chrono::seconds(1));
	}
	REQUIRE(file != nullptr);

	char sjson_file_buffer[256 * 1024];
	const size_t buffer_size = fread(sjson_file_buffer, 1, get_array_size(sjson_file_buffer), file);
	fclose(file);

	std::remove(filename);

	// Read back the clip
	clip_reader reader(allocator, sjson_file_buffer, buffer_size - 1);

	REQUIRE(reader.get_file_type() == sjson_file_type::raw_track_list);

	sjson_raw_track_list file_track_list;
	const bool success = reader.read_raw_track_list(file_track_list);
	REQUIRE(success);

	CHECK(file_track_list.track_list.get_num_samples_per_track() == track_list.get_num_samples_per_track());
	CHECK(file_track_list.track_list.get_sample_rate() == track_list.get_sample_rate());
	CHECK(file_track_list.track_list.get_num_tracks() == track_list.get_num_tracks());
	CHECK(rtm::scalar_near_equal(file_track_list.track_list.get_duration(), track_list.get_duration(), 1.0E-8F));
	CHECK(file_track_list.track_list.get_track_type() == track_list.get_track_type());
	CHECK(file_track_list.track_list.get_track_category() == track_list.get_track_category());

	for (uint32_t track_index = 0; track_index < num_tracks; ++track_index)
	{
		const track_float2f& ref_track = track_cast<track_float2f>(track_list[track_index]);
		const track_float2f& file_track = track_cast<track_float2f>(file_track_list.track_list[track_index]);

		CHECK(file_track.get_description().output_index == ref_track.get_description().output_index);
		CHECK(rtm::scalar_near_equal(file_track.get_description().precision, ref_track.get_description().precision, 0.0F));
		CHECK(file_track.get_num_samples() == ref_track.get_num_samples());
		CHECK(file_track.get_output_index() == ref_track.get_output_index());
		CHECK(file_track.get_sample_rate() == ref_track.get_sample_rate());
		CHECK(file_track.get_type() == ref_track.get_type());
		CHECK(file_track.get_category() == ref_track.get_category());

		for (uint32_t sample_index = 0; sample_index < num_samples; ++sample_index)
		{
			const rtm::float2f& ref_sample = ref_track[sample_index];
			const rtm::float2f& file_sample = file_track[sample_index];
			CHECK(rtm::vector_all_near_equal2(rtm::vector_load2(&ref_sample), rtm::vector_load2(&file_sample), 0.0F));
		}
	}
#endif
}

TEST_CASE("sjson_track_list_reader_writer float3f", "[io]")
{
	// Only test the reader/writer on non-mobile platforms
#if defined(RTM_SSE2_INTRINSICS) && defined(ACL_USE_SJSON)
	ansi_allocator allocator;

	const uint32_t num_tracks = 3;
	const uint32_t num_samples = 4;
	track_array_float3f track_list(allocator, num_tracks);

	track_desc_scalarf desc0;
	desc0.output_index = 0;
	desc0.precision = 0.001F;
	track_float3f track0 = track_float3f::make_reserve(desc0, allocator, num_samples, 32.0F);
	track0[0] = rtm::float3f{ 1.0F, 3123.0F, 315.13F };
	track0[1] = rtm::float3f{ 2.333F, 321.13F, 31.66F };
	track0[2] = rtm::float3f{ 3.123F, 81.0F, 913.13F };
	track0[3] = rtm::float3f{ 4.5F, 91.13F, 41.135F };
	track_list[0] = track0.get_ref();
	track_float3f track1 = track_float3f::make_reserve(desc0, allocator, num_samples, 32.0F);
	track1[0] = rtm::float3f{ 12.0F, 91.013F, 9991.13F };
	track1[1] = rtm::float3f{ 21.1231F, 911.14F, 825.12351F };
	track1[2] = rtm::float3f{ 3.1444123F, 113.44F, 913.51F };
	track1[3] = rtm::float3f{ 421.5156F, 913901.0F, 184.6981F };
	track_list[1] = track1.get_ref();
	track_float3f track2 = track_float3f::make_reserve(desc0, allocator, num_samples, 32.0F);
	track2[0] = rtm::float3f{ 11.61F, 90.13F, 918.011F };
	track2[1] = rtm::float3f{ 23313.367F, 13.3F, 913.813F };
	track2[2] = rtm::float3f{ 313.7876F, 931.2F, 8123.123F };
	track2[3] = rtm::float3f{ 4441.514F, 913.56F, 813.61F };
	track_list[2] = track2.get_ref();

	const uint32_t filename_size = k_max_filename_size;
	char filename[filename_size] = { 0 };

	error_result error;
	for (uint32_t try_count = 0; try_count < 20; ++try_count)
	{
		get_temporary_filename(filename, filename_size, "list_float3f_");

		// Write the clip to a temporary file
		error = write_track_list(track_list, filename);

		if (error.empty())
			break;	// Everything worked, stop trying
	}
	REQUIRE(error.empty());

	std::FILE* file = nullptr;
	for (uint32_t try_count = 0; try_count < 20; ++try_count)
	{
#ifdef _WIN32
		fopen_s(&file, filename, "rb");
#else
		file = fopen(filename, "rb");
#endif

		if (file != nullptr)
			break;	// File is open, all good

		// Sleep a bit before tring again
		std::this_thread::sleep_for(std::chrono::seconds(1));
	}
	REQUIRE(file != nullptr);

	char sjson_file_buffer[256 * 1024];
	const size_t buffer_size = fread(sjson_file_buffer, 1, get_array_size(sjson_file_buffer), file);
	fclose(file);

	std::remove(filename);

	// Read back the clip
	clip_reader reader(allocator, sjson_file_buffer, buffer_size - 1);

	REQUIRE(reader.get_file_type() == sjson_file_type::raw_track_list);

	sjson_raw_track_list file_track_list;
	const bool success = reader.read_raw_track_list(file_track_list);
	REQUIRE(success);

	CHECK(file_track_list.track_list.get_num_samples_per_track() == track_list.get_num_samples_per_track());
	CHECK(file_track_list.track_list.get_sample_rate() == track_list.get_sample_rate());
	CHECK(file_track_list.track_list.get_num_tracks() == track_list.get_num_tracks());
	CHECK(rtm::scalar_near_equal(file_track_list.track_list.get_duration(), track_list.get_duration(), 1.0E-8F));
	CHECK(file_track_list.track_list.get_track_type() == track_list.get_track_type());
	CHECK(file_track_list.track_list.get_track_category() == track_list.get_track_category());

	for (uint32_t track_index = 0; track_index < num_tracks; ++track_index)
	{
		const track_float3f& ref_track = track_cast<track_float3f>(track_list[track_index]);
		const track_float3f& file_track = track_cast<track_float3f>(file_track_list.track_list[track_index]);

		CHECK(file_track.get_description().output_index == ref_track.get_description().output_index);
		CHECK(rtm::scalar_near_equal(file_track.get_description().precision, ref_track.get_description().precision, 0.0F));
		CHECK(file_track.get_num_samples() == ref_track.get_num_samples());
		CHECK(file_track.get_output_index() == ref_track.get_output_index());
		CHECK(file_track.get_sample_rate() == ref_track.get_sample_rate());
		CHECK(file_track.get_type() == ref_track.get_type());
		CHECK(file_track.get_category() == ref_track.get_category());

		for (uint32_t sample_index = 0; sample_index < num_samples; ++sample_index)
		{
			const rtm::float3f& ref_sample = ref_track[sample_index];
			const rtm::float3f& file_sample = file_track[sample_index];
			CHECK(rtm::vector_all_near_equal3(rtm::vector_load3(&ref_sample), rtm::vector_load3(&file_sample), 0.0F));
		}
	}
#endif
}

TEST_CASE("sjson_track_list_reader_writer float4f", "[io]")
{
	// Only test the reader/writer on non-mobile platforms
#if defined(RTM_SSE2_INTRINSICS) && defined(ACL_USE_SJSON)
	ansi_allocator allocator;

	const uint32_t num_tracks = 3;
	const uint32_t num_samples = 4;
	track_array_float4f track_list(allocator, num_tracks);

	track_desc_scalarf desc0;
	desc0.output_index = 0;
	desc0.precision = 0.001F;
	track_float4f track0 = track_float4f::make_reserve(desc0, allocator, num_samples, 32.0F);
	track0[0] = rtm::float4f{ 1.0F, 3123.0F, 315.13F, 123.31F };
	track0[1] = rtm::float4f{ 2.333F, 321.13F, 31.66F, 7154.1F };
	track0[2] = rtm::float4f{ 3.123F, 81.0F, 913.13F, 9817.8135F };
	track0[3] = rtm::float4f{ 4.5F, 91.13F, 41.135F, 755.12345F };
	track_list[0] = track0.get_ref();
	track_float4f track1 = track_float4f::make_reserve(desc0, allocator, num_samples, 32.0F);
	track1[0] = rtm::float4f{ 12.0F, 91.013F, 9991.13F, 813.97F };
	track1[1] = rtm::float4f{ 21.1231F, 911.14F, 825.12351F, 321.517F };
	track1[2] = rtm::float4f{ 3.1444123F, 113.44F, 913.51F, 6136.613F };
	track1[3] = rtm::float4f{ 421.5156F, 913901.0F, 184.6981F, 41.1254F };
	track_list[1] = track1.get_ref();
	track_float4f track2 = track_float4f::make_reserve(desc0, allocator, num_samples, 32.0F);
	track2[0] = rtm::float4f{ 11.61F, 90.13F, 918.011F, 31.13F };
	track2[1] = rtm::float4f{ 23313.367F, 13.3F, 913.813F, 8997.1F };
	track2[2] = rtm::float4f{ 313.7876F, 931.2F, 8123.123F, 813.76F };
	track2[3] = rtm::float4f{ 4441.514F, 913.56F, 813.61F, 873.612F };
	track_list[2] = track2.get_ref();

	const uint32_t filename_size = k_max_filename_size;
	char filename[filename_size] = { 0 };

	error_result error;
	for (uint32_t try_count = 0; try_count < 20; ++try_count)
	{
		get_temporary_filename(filename, filename_size, "list_float4f_");

		// Write the clip to a temporary file
		error = write_track_list(track_list, filename);

		if (error.empty())
			break;	// Everything worked, stop trying
	}
	REQUIRE(error.empty());

	std::FILE* file = nullptr;
	for (uint32_t try_count = 0; try_count < 20; ++try_count)
	{
#ifdef _WIN32
		fopen_s(&file, filename, "rb");
#else
		file = fopen(filename, "rb");
#endif

		if (file != nullptr)
			break;	// File is open, all good

		// Sleep a bit before tring again
		std::this_thread::sleep_for(std::chrono::seconds(1));
	}
	REQUIRE(file != nullptr);

	char sjson_file_buffer[256 * 1024];
	const size_t buffer_size = fread(sjson_file_buffer, 1, get_array_size(sjson_file_buffer), file);
	fclose(file);

	std::remove(filename);

	// Read back the clip
	clip_reader reader(allocator, sjson_file_buffer, buffer_size - 1);

	REQUIRE(reader.get_file_type() == sjson_file_type::raw_track_list);

	sjson_raw_track_list file_track_list;
	const bool success = reader.read_raw_track_list(file_track_list);
	REQUIRE(success);

	CHECK(file_track_list.track_list.get_num_samples_per_track() == track_list.get_num_samples_per_track());
	CHECK(file_track_list.track_list.get_sample_rate() == track_list.get_sample_rate());
	CHECK(file_track_list.track_list.get_num_tracks() == track_list.get_num_tracks());
	CHECK(rtm::scalar_near_equal(file_track_list.track_list.get_duration(), track_list.get_duration(), 1.0E-8F));
	CHECK(file_track_list.track_list.get_track_type() == track_list.get_track_type());
	CHECK(file_track_list.track_list.get_track_category() == track_list.get_track_category());

	for (uint32_t track_index = 0; track_index < num_tracks; ++track_index)
	{
		const track_float4f& ref_track = track_cast<track_float4f>(track_list[track_index]);
		const track_float4f& file_track = track_cast<track_float4f>(file_track_list.track_list[track_index]);

		CHECK(file_track.get_description().output_index == ref_track.get_description().output_index);
		CHECK(rtm::scalar_near_equal(file_track.get_description().precision, ref_track.get_description().precision, 0.0F));
		CHECK(file_track.get_num_samples() == ref_track.get_num_samples());
		CHECK(file_track.get_output_index() == ref_track.get_output_index());
		CHECK(file_track.get_sample_rate() == ref_track.get_sample_rate());
		CHECK(file_track.get_type() == ref_track.get_type());
		CHECK(file_track.get_category() == ref_track.get_category());

		for (uint32_t sample_index = 0; sample_index < num_samples; ++sample_index)
		{
			const rtm::float4f& ref_sample = ref_track[sample_index];
			const rtm::float4f& file_sample = file_track[sample_index];
			CHECK(rtm::vector_all_near_equal(rtm::vector_load(&ref_sample), rtm::vector_load(&file_sample), 0.0F));
		}
	}
#endif
}

TEST_CASE("sjson_track_list_reader_writer vector4f", "[io]")
{
	// Only test the reader/writer on non-mobile platforms
#if defined(RTM_SSE2_INTRINSICS) && defined(ACL_USE_SJSON)
	ansi_allocator allocator;

	const uint32_t num_tracks = 3;
	const uint32_t num_samples = 4;
	track_array_vector4f track_list(allocator, num_tracks);

	track_desc_scalarf desc0;
	desc0.output_index = 0;
	desc0.precision = 0.001F;
	track_vector4f track0 = track_vector4f::make_reserve(desc0, allocator, num_samples, 32.0F);
	track0[0] = rtm::vector_set(1.0F, 3123.0F, 315.13F, 123.31F);
	track0[1] = rtm::vector_set(2.333F, 321.13F, 31.66F, 7154.1F);
	track0[2] = rtm::vector_set(3.123F, 81.0F, 913.13F, 9817.8135F);
	track0[3] = rtm::vector_set(4.5F, 91.13F, 41.135F, 755.12345F);
	track_list[0] = track0.get_ref();
	track_vector4f track1 = track_vector4f::make_reserve(desc0, allocator, num_samples, 32.0F);
	track1[0] = rtm::vector_set(12.0F, 91.013F, 9991.13F, 813.97F);
	track1[1] = rtm::vector_set(21.1231F, 911.14F, 825.12351F, 321.517F);
	track1[2] = rtm::vector_set(3.1444123F, 113.44F, 913.51F, 6136.613F);
	track1[3] = rtm::vector_set(421.5156F, 913901.0F, 184.6981F, 41.1254F);
	track_list[1] = track1.get_ref();
	track_vector4f track2 = track_vector4f::make_reserve(desc0, allocator, num_samples, 32.0F);
	track2[0] = rtm::vector_set(11.61F, 90.13F, 918.011F, 31.13F);
	track2[1] = rtm::vector_set(23313.367F, 13.3F, 913.813F, 8997.1F);
	track2[2] = rtm::vector_set(313.7876F, 931.2F, 8123.123F, 813.76F);
	track2[3] = rtm::vector_set(4441.514F, 913.56F, 813.61F, 873.612F);
	track_list[2] = track2.get_ref();

	const uint32_t filename_size = k_max_filename_size;
	char filename[filename_size] = { 0 };

	error_result error;
	for (uint32_t try_count = 0; try_count < 20; ++try_count)
	{
		get_temporary_filename(filename, filename_size, "list_vector4f_");

		// Write the clip to a temporary file
		error = write_track_list(track_list, filename);

		if (error.empty())
			break;	// Everything worked, stop trying
	}
	REQUIRE(error.empty());

	std::FILE* file = nullptr;
	for (uint32_t try_count = 0; try_count < 20; ++try_count)
	{
#ifdef _WIN32
		fopen_s(&file, filename, "rb");
#else
		file = fopen(filename, "rb");
#endif

		if (file != nullptr)
			break;	// File is open, all good

		// Sleep a bit before tring again
		std::this_thread::sleep_for(std::chrono::seconds(1));
	}
	REQUIRE(file != nullptr);

	char sjson_file_buffer[256 * 1024];
	const size_t buffer_size = fread(sjson_file_buffer, 1, get_array_size(sjson_file_buffer), file);
	fclose(file);

	std::remove(filename);

	// Read back the clip
	clip_reader reader(allocator, sjson_file_buffer, buffer_size - 1);

	REQUIRE(reader.get_file_type() == sjson_file_type::raw_track_list);

	sjson_raw_track_list file_track_list;
	const bool success = reader.read_raw_track_list(file_track_list);
	REQUIRE(success);

	CHECK(file_track_list.track_list.get_num_samples_per_track() == track_list.get_num_samples_per_track());
	CHECK(file_track_list.track_list.get_sample_rate() == track_list.get_sample_rate());
	CHECK(file_track_list.track_list.get_num_tracks() == track_list.get_num_tracks());
	CHECK(rtm::scalar_near_equal(file_track_list.track_list.get_duration(), track_list.get_duration(), 1.0E-8F));
	CHECK(file_track_list.track_list.get_track_type() == track_list.get_track_type());
	CHECK(file_track_list.track_list.get_track_category() == track_list.get_track_category());

	for (uint32_t track_index = 0; track_index < num_tracks; ++track_index)
	{
		const track_vector4f& ref_track = track_cast<track_vector4f>(track_list[track_index]);
		const track_vector4f& file_track = track_cast<track_vector4f>(file_track_list.track_list[track_index]);

		CHECK(file_track.get_description().output_index == ref_track.get_description().output_index);
		CHECK(rtm::scalar_near_equal(file_track.get_description().precision, ref_track.get_description().precision, 0.0F));
		CHECK(file_track.get_num_samples() == ref_track.get_num_samples());
		CHECK(file_track.get_output_index() == ref_track.get_output_index());
		CHECK(file_track.get_sample_rate() == ref_track.get_sample_rate());
		CHECK(file_track.get_type() == ref_track.get_type());
		CHECK(file_track.get_category() == ref_track.get_category());

		for (uint32_t sample_index = 0; sample_index < num_samples; ++sample_index)
		{
			const rtm::vector4f& ref_sample = ref_track[sample_index];
			const rtm::vector4f& file_sample = file_track[sample_index];
			CHECK(rtm::vector_all_near_equal(ref_sample, file_sample, 0.0F));
		}
	}
#endif
}
