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

#include "catch2.impl.h"

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
#if !defined(NOMINMAX) && 0			// We leave MIN/MAX macros enabled, they are defined by default and we must handle it
	#define NOMINMAX                // Macros min(a,b) and max(a,b)
#endif
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

	#if defined(_MSC_VER) && !defined(__clang__)
		#pragma warning(push)
		#pragma warning(disable : 5039)	// Pointer to function without noexcept passed to extern "C"
	#endif

	#include <windows.h>
	#include <conio.h>

	#if defined(_MSC_VER) && !defined(__clang__)
		#pragma warning(pop)
	#endif
#endif    // _WIN32

#if defined(ACL_USE_SJSON)
	#include <sjson/parser.h>
	#include <sjson/writer.h>
#endif

#include <acl/core/ansi_allocator.h>
#include <acl/io/clip_reader.h>
#include <acl/io/clip_writer.h>

#include <rtm/vqf.h>
#include <rtm/vqsf.h>
#include <rtm/vqvd.h>
#include <rtm/vqvf.h>
#include <rtm/scalarf.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <thread>

// Only test the reader/writer on non-mobile platforms
#if defined(RTM_SSE2_INTRINSICS) && defined(ACL_USE_SJSON)
	#define ACL_IMPL_ENABLE_IO_UNIT_TESTS
#endif

#define ACL_IMPL_ENABLE_IO_UNIT_TESTS

using namespace acl;

#if defined(ACL_IMPL_ENABLE_IO_UNIT_TESTS)
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

// Disable warning for implicit constructor using deprecated members in sjson_raw_clip and sjson_raw_track_list
#if defined(RTM_COMPILER_MSVC_2015)
	#pragma warning(push)
	#pragma warning(disable : 4996)
#endif

TEST_CASE("sjson_clip_reader_writer", "[io]")
{
#if defined(ACL_IMPL_ENABLE_IO_UNIT_TESTS)
	ansi_allocator allocator;

	const uint32_t num_tracks = 3;
	const uint32_t num_samples = 4;
	track_array_vqvf track_list(allocator, num_tracks);

	track_desc_transformf desc0;
	desc0.output_index = 0;
	desc0.precision = 0.001F;
	track_vqvf track0 = track_vqvf::make_reserve(desc0, allocator, num_samples, 32.0F);
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
	track_vqvf track1 = track_vqvf::make_reserve(desc1, allocator, num_samples, 32.0F);
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

	track_vqvf track2 = track_vqvf::make_reserve(desc1, allocator, num_samples, 32.0F);
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
	settings.scalar_format = scalar_format8::scalarf_variable;

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
		const track_vqvf& ref_track = track_list[track_index];
		const track_vqvf& file_track = track_cast<track_vqvf>(file_clip.track_list[track_index]);

		CHECK(file_track.get_description().output_index == ref_track.get_description().output_index);
		CHECK(file_track.get_description().parent_index == ref_track.get_description().parent_index);
		CHECK(rtm::scalar_near_equal(file_track.get_description().precision, ref_track.get_description().precision, 0.0F));
		CHECK(rtm::scalar_near_equal(file_track.get_description().shell_distance, ref_track.get_description().shell_distance, 0.0F));
		CHECK(file_track.get_num_samples() == ref_track.get_num_samples());
		CHECK(file_track.get_output_index() == ref_track.get_output_index());
		CHECK(file_track.get_sample_rate() == ref_track.get_sample_rate());
		CHECK(file_track.get_type() == ref_track.get_type());
		CHECK(file_track.get_category() == ref_track.get_category());
		CHECK(file_track.get_name() == ref_track.get_name());

		for (uint32_t sample_index = 0; sample_index < num_samples; ++sample_index)
		{
			const rtm::vqvf& ref_sample = ref_track[sample_index];
			const rtm::vqvf& file_sample = file_track[sample_index];
			CHECK(rtm::quat_near_equal(ref_sample.rotation, file_sample.rotation, 0.0F));
			CHECK(rtm::vector_all_near_equal3(ref_sample.translation, file_sample.translation, 0.0F));
			CHECK(rtm::vector_all_near_equal3(ref_sample.scale, file_sample.scale, 0.0F));
		}
	}
#endif
}

#if defined(RTM_COMPILER_MSVC_2015)
	#pragma warning(pop)
#endif

TEST_CASE("sjson_track_list_reader_writer float1f", "[io]")
{
#if defined(ACL_IMPL_ENABLE_IO_UNIT_TESTS)
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
		const track_float1f& ref_track = track_list[track_index];
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

TEST_CASE("sjson_track_list_reader_writer float1f_ex", "[io]")
{
#if defined(ACL_IMPL_ENABLE_IO_UNIT_TESTS)
	ansi_allocator allocator;

	const uint32_t num_tracks = 3;
	const uint32_t num_samples = 4;
	track_array_float1f_ex track_list(allocator, num_tracks);

	track_desc_scalarf desc0;
	desc0.output_index = 0;
	desc0.precision = 0.001F;

	track_float1f_ex track0 = track_float1f_ex::make_reserve(desc0, allocator, num_samples, 32.0F);
	track0[0].set_constant({ 1.0F });
	track0[1].set_constant({ 2.333F });
	track0[2].set_linear({ 3.123F });
	track0[3].set_linear({ 4.5F });
	track_list[0] = track0.get_ref();

	track_float1f_ex track1 = track_float1f_ex::make_reserve(desc0, allocator, num_samples, 32.0F);
	track1[0].set_constant({ 12.0F });
	track1[1].set_constant({ 21.1231F });
	track1[2].set_linear({ 3.1444123F });
	track1[3].set_linear({ 421.5156F });
	track_list[1] = track1.get_ref();

	track_float1f_ex track2 = track_float1f_ex::make_reserve(desc0, allocator, num_samples, 32.0F);
	track2[0].set_constant({ 11.61F });
	track2[1].set_constant({ 23313.367F });
	track2[2].set_linear({ 313.7876F });
	track2[3].set_linear({ 4441.514F });
	track_list[2] = track2.get_ref();

	const uint32_t filename_size = k_max_filename_size;
	char filename[filename_size] = { 0 };

	error_result error;
	for (uint32_t try_count = 0; try_count < 20; ++try_count)
	{
		get_temporary_filename(filename, filename_size, "list_float1f_ex_");

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

	// todo: refactor file roundtrip, and asserts, together and separate

	CHECK(file_track_list.track_list.get_num_samples_per_track() == track_list.get_num_samples_per_track());
	CHECK(file_track_list.track_list.get_sample_rate() == track_list.get_sample_rate());
	CHECK(file_track_list.track_list.get_num_tracks() == track_list.get_num_tracks());
	CHECK(rtm::scalar_near_equal(file_track_list.track_list.get_duration(), track_list.get_duration(), 1.0E-8F));
	CHECK(file_track_list.track_list.get_track_type() == track_list.get_track_type());
	CHECK(file_track_list.track_list.get_track_category() == track_list.get_track_category());

	for (uint32_t track_index = 0; track_index < num_tracks; ++track_index)
	{
		const track_float1f_ex& ref_track = track_list[track_index];
		const track_float1f_ex& file_track = track_cast<track_float1f_ex>(file_track_list.track_list[track_index]);

		CHECK(file_track.get_description().output_index == ref_track.get_description().output_index);
		CHECK(rtm::scalar_near_equal(file_track.get_description().precision, ref_track.get_description().precision, 0.0F));
		CHECK(file_track.get_num_samples() == ref_track.get_num_samples());
		CHECK(file_track.get_output_index() == ref_track.get_output_index());
		CHECK(file_track.get_sample_rate() == ref_track.get_sample_rate());
		CHECK(file_track.get_type() == ref_track.get_type());
		CHECK(file_track.get_category() == ref_track.get_category());

		for (uint32_t sample_index = 0; sample_index < num_samples; ++sample_index)
		{
			const track_extended_sample_t<float> ref_sample = ref_track[sample_index];
			const track_extended_sample_t<float> file_sample = file_track[sample_index];

			CHECK(ref_sample.interpolator == file_sample.interpolator);
			CHECK(rtm::scalar_near_equal(ref_sample.value, file_sample.value, 0.0F));
		}
	}
#endif
}

TEST_CASE("sjson_track_list_reader_writer float2f", "[io]")
{
#if defined(ACL_IMPL_ENABLE_IO_UNIT_TESTS)
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
		const track_float2f& ref_track = track_list[track_index];
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

TEST_CASE("sjson_track_list_reader_writer float2f_ex", "[io]")
{
#if defined(ACL_IMPL_ENABLE_IO_UNIT_TESTS)
	ansi_allocator allocator;

	const uint32_t num_tracks = 3;
	const uint32_t num_samples = 4;
	track_array_float2f_ex track_list(allocator, num_tracks);

	track_desc_scalarf desc0;
	desc0.output_index = 0;
	desc0.precision = 0.001F;
	track_float2f_ex track0 = track_float2f_ex::make_reserve(desc0, allocator, num_samples, 32.0F);
	track0[0].set_constant({ 1.0F, 3123.0F });
	track0[1].set_constant({ 2.333F, 321.13F });
	track0[2].set_linear({ 3.123F, 81.0F });
	track0[3].set_linear({ 4.5F, 91.13F });
	track_list[0] = track0.get_ref();
	track_float2f_ex track1 = track_float2f_ex::make_reserve(desc0, allocator, num_samples, 32.0F);
	track1[0].set_constant({ 12.0F, 91.013F });
	track1[1].set_constant({ 21.1231F, 911.14F });
	track1[2].set_linear({ 3.1444123F, 113.44F });
	track1[3].set_linear({ 421.5156F, 913901.0F });
	track_list[1] = track1.get_ref();
	track_float2f_ex track2 = track_float2f_ex::make_reserve(desc0, allocator, num_samples, 32.0F);
	track2[0].set_constant({ 11.61F, 90.13F });
	track2[1].set_constant({ 23313.367F, 13.3F });
	track2[2].set_linear({ 313.7876F, 931.2F });
	track2[3].set_linear({ 4441.514F, 913.56F });
	track_list[2] = track2.get_ref();

	const uint32_t filename_size = k_max_filename_size;
	char filename[filename_size] = { 0 };

	error_result error;
	for (uint32_t try_count = 0; try_count < 20; ++try_count)
	{
		get_temporary_filename(filename, filename_size, "list_float2f_ex_");

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
		const track_float2f_ex& ref_track = track_list[track_index];
		const track_float2f_ex& file_track = track_cast<track_float2f_ex>(file_track_list.track_list[track_index]);

		CHECK(file_track.get_description().output_index == ref_track.get_description().output_index);
		CHECK(rtm::scalar_near_equal(file_track.get_description().precision, ref_track.get_description().precision, 0.0F));
		CHECK(file_track.get_num_samples() == ref_track.get_num_samples());
		CHECK(file_track.get_output_index() == ref_track.get_output_index());
		CHECK(file_track.get_sample_rate() == ref_track.get_sample_rate());
		CHECK(file_track.get_type() == ref_track.get_type());
		CHECK(file_track.get_category() == ref_track.get_category());

		for (uint32_t sample_index = 0; sample_index < num_samples; ++sample_index)
		{
			const track_extended_sample_t<rtm::float2f> ref_sample = ref_track[sample_index];
			const track_extended_sample_t<rtm::float2f> file_sample = file_track[sample_index];

			CHECK(ref_sample.interpolator == file_sample.interpolator);
			CHECK(rtm::vector_all_near_equal2(rtm::vector_load2(&ref_sample.value), rtm::vector_load2(&file_sample.value), 0.0F));
		}
	}
#endif
}

TEST_CASE("sjson_track_list_reader_writer float3f", "[io]")
{
#if defined(ACL_IMPL_ENABLE_IO_UNIT_TESTS)
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
		const track_float3f& ref_track = track_list[track_index];
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

TEST_CASE("sjson_track_list_reader_writer float3f_ex", "[io]")
{
#if defined(ACL_IMPL_ENABLE_IO_UNIT_TESTS)
	ansi_allocator allocator;

	const uint32_t num_tracks = 3;
	const uint32_t num_samples = 4;
	track_array_float3f_ex track_list(allocator, num_tracks);

	track_desc_scalarf desc0;
	desc0.output_index = 0;
	desc0.precision = 0.001F;
	track_float3f_ex track0 = track_float3f_ex::make_reserve(desc0, allocator, num_samples, 32.0F);
	track0[0].set_constant({ 1.0F, 3123.0F, 315.13F });
	track0[1].set_constant({ 2.333F, 321.13F, 31.66F });
	track0[2].set_linear({ 3.123F, 81.0F, 913.13F });
	track0[3].set_linear({ 4.5F, 91.13F, 41.135F });
	track_list[0] = track0.get_ref();
	track_float3f_ex track1 = track_float3f_ex::make_reserve(desc0, allocator, num_samples, 32.0F);
	track1[0].set_constant({ 12.0F, 91.013F, 9991.13F });
	track1[1].set_constant({ 21.1231F, 911.14F, 825.12351F });
	track1[2].set_linear({ 3.1444123F, 113.44F, 913.51F });
	track1[3].set_linear({ 421.5156F, 913901.0F, 184.6981F });
	track_list[1] = track1.get_ref();
	track_float3f_ex track2 = track_float3f_ex::make_reserve(desc0, allocator, num_samples, 32.0F);
	track2[0].set_constant({ 11.61F, 90.13F, 918.011F });
	track2[1].set_constant({ 23313.367F, 13.3F, 913.813F });
	track2[2].set_linear({ 313.7876F, 931.2F, 8123.123F });
	track2[3].set_linear({ 4441.514F, 913.56F, 813.61F });
	track_list[2] = track2.get_ref();

	const uint32_t filename_size = k_max_filename_size;
	char filename[filename_size] = { 0 };

	error_result error;
	for (uint32_t try_count = 0; try_count < 20; ++try_count)
	{
		get_temporary_filename(filename, filename_size, "list_float3f_ex_");

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
		const track_float3f_ex& ref_track = track_list[track_index];
		const track_float3f_ex& file_track = track_cast<track_float3f_ex>(file_track_list.track_list[track_index]);

		CHECK(file_track.get_description().output_index == ref_track.get_description().output_index);
		CHECK(rtm::scalar_near_equal(file_track.get_description().precision, ref_track.get_description().precision, 0.0F));
		CHECK(file_track.get_num_samples() == ref_track.get_num_samples());
		CHECK(file_track.get_output_index() == ref_track.get_output_index());
		CHECK(file_track.get_sample_rate() == ref_track.get_sample_rate());
		CHECK(file_track.get_type() == ref_track.get_type());
		CHECK(file_track.get_category() == ref_track.get_category());

		for (uint32_t sample_index = 0; sample_index < num_samples; ++sample_index)
		{
			const track_extended_sample_t<rtm::float3f> ref_sample = ref_track[sample_index];
			const track_extended_sample_t<rtm::float3f> file_sample = file_track[sample_index];

			CHECK(ref_sample.interpolator == file_sample.interpolator);
			CHECK(rtm::vector_all_near_equal3(rtm::vector_load3(&ref_sample.value), rtm::vector_load3(&file_sample.value), 0.0F));
		}
	}
#endif
}

TEST_CASE("sjson_track_list_reader_writer float4f", "[io]")
{
#if defined(ACL_IMPL_ENABLE_IO_UNIT_TESTS)
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
		const track_float4f& ref_track = track_list[track_index];
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

TEST_CASE("sjson_track_list_reader_writer float4f_ex", "[io]")
{
#if defined(ACL_IMPL_ENABLE_IO_UNIT_TESTS)
	ansi_allocator allocator;

	const uint32_t num_tracks = 3;
	const uint32_t num_samples = 4;
	track_array_float4f_ex track_list(allocator, num_tracks);

	track_desc_scalarf desc0;
	desc0.output_index = 0;
	desc0.precision = 0.001F;
	track_float4f_ex track0 = track_float4f_ex::make_reserve(desc0, allocator, num_samples, 32.0F);
	track0[0].set_constant({ 1.0F, 3123.0F, 315.13F, 123.31F });
	track0[1].set_constant({ 2.333F, 321.13F, 31.66F, 7154.1F });
	track0[2].set_linear({ 3.123F, 81.0F, 913.13F, 9817.8135F });
	track0[3].set_linear({ 4.5F, 91.13F, 41.135F, 755.12345F });
	track_list[0] = track0.get_ref();
	track_float4f_ex track1 = track_float4f_ex::make_reserve(desc0, allocator, num_samples, 32.0F);
	track1[0].set_constant({ 12.0F, 91.013F, 9991.13F, 813.97F });
	track1[1].set_constant({ 21.1231F, 911.14F, 825.12351F, 321.517F });
	track1[2].set_linear({ 3.1444123F, 113.44F, 913.51F, 6136.613F });
	track1[3].set_linear({ 421.5156F, 913901.0F, 184.6981F, 41.1254F });
	track_list[1] = track1.get_ref();
	track_float4f_ex track2 = track_float4f_ex::make_reserve(desc0, allocator, num_samples, 32.0F);
	track2[0].set_constant({ 11.61F, 90.13F, 918.011F, 31.13F });
	track2[1].set_constant({ 23313.367F, 13.3F, 913.813F, 8997.1F });
	track2[2].set_linear({ 313.7876F, 931.2F, 8123.123F, 813.76F });
	track2[3].set_linear({ 4441.514F, 913.56F, 813.61F, 873.612F });
	track_list[2] = track2.get_ref();

	const uint32_t filename_size = k_max_filename_size;
	char filename[filename_size] = { 0 };

	error_result error;
	for (uint32_t try_count = 0; try_count < 20; ++try_count)
	{
		get_temporary_filename(filename, filename_size, "list_float4f_ex_");

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
		const track_float4f_ex& ref_track = track_list[track_index];
		const track_float4f_ex& file_track = track_cast<track_float4f_ex>(file_track_list.track_list[track_index]);

		CHECK(file_track.get_description().output_index == ref_track.get_description().output_index);
		CHECK(rtm::scalar_near_equal(file_track.get_description().precision, ref_track.get_description().precision, 0.0F));
		CHECK(file_track.get_num_samples() == ref_track.get_num_samples());
		CHECK(file_track.get_output_index() == ref_track.get_output_index());
		CHECK(file_track.get_sample_rate() == ref_track.get_sample_rate());
		CHECK(file_track.get_type() == ref_track.get_type());
		CHECK(file_track.get_category() == ref_track.get_category());

		for (uint32_t sample_index = 0; sample_index < num_samples; ++sample_index)
		{
			const track_extended_sample_t<rtm::float4f> ref_sample = ref_track[sample_index];
			const track_extended_sample_t<rtm::float4f> file_sample = file_track[sample_index];

			CHECK(ref_sample.interpolator == file_sample.interpolator);
			CHECK(rtm::vector_all_near_equal(rtm::vector_load(&ref_sample.value), rtm::vector_load(&file_sample.value), 0.0F));
		}
	}
#endif
}

TEST_CASE("sjson_track_list_reader_writer vector4f", "[io]")
{
#if defined(ACL_IMPL_ENABLE_IO_UNIT_TESTS)
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
		const track_vector4f& ref_track = track_list[track_index];
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

TEST_CASE("sjson_track_list_reader_writer vector4f_ex", "[io]")
{
#if defined(ACL_IMPL_ENABLE_IO_UNIT_TESTS)
	// Suppress GCC warning about may-alias warning being ignored due to type being used as template argument
	#if defined(RTM_COMPILER_GCC)
		#pragma GCC diagnostic push
		#pragma GCC diagnostic ignored "-Wignored-attributes"
	#endif

	using track_extended_sample_vec4f_t = track_extended_sample_t<rtm::vector4f>;

	#if defined(RTM_COMPILER_GCC)
		#pragma GCC diagnostic pop
	#endif

	ansi_allocator allocator;

	const uint32_t num_tracks = 3;
	const uint32_t num_samples = 4;
	track_array_vector4f_ex track_list(allocator, num_tracks);

	track_desc_scalarf desc0;
	desc0.output_index = 0;
	desc0.precision = 0.001F;
	track_vector4f_ex track0 = track_vector4f_ex::make_reserve(desc0, allocator, num_samples, 32.0F);
	track0[0].set_constant({ rtm::vector_set(1.0F, 3123.0F, 315.13F, 123.31F) });
	track0[1].set_constant({ rtm::vector_set(2.333F, 321.13F, 31.66F, 7154.1F) });
	track0[2].set_linear({ rtm::vector_set(3.123F, 81.0F, 913.13F, 9817.8135F) });
	track0[3].set_linear({ rtm::vector_set(4.5F, 91.13F, 41.135F, 755.12345F) });
	track_list[0] = track0.get_ref();
	track_vector4f_ex track1 = track_vector4f_ex::make_reserve(desc0, allocator, num_samples, 32.0F);
	track1[0].set_constant({ rtm::vector_set(12.0F, 91.013F, 9991.13F, 813.97F) });
	track1[1].set_constant({ rtm::vector_set(21.1231F, 911.14F, 825.12351F, 321.517F) });
	track1[2].set_linear({ rtm::vector_set(3.1444123F, 113.44F, 913.51F, 6136.613F) });
	track1[3].set_linear({ rtm::vector_set(421.5156F, 913901.0F, 184.6981F, 41.1254F) });
	track_list[1] = track1.get_ref();
	track_vector4f_ex track2 = track_vector4f_ex::make_reserve(desc0, allocator, num_samples, 32.0F);
	track2[0].set_constant({ rtm::vector_set(11.61F, 90.13F, 918.011F, 31.13F) });
	track2[1].set_constant({ rtm::vector_set(23313.367F, 13.3F, 913.813F, 8997.1F) });
	track2[2].set_linear({ rtm::vector_set(313.7876F, 931.2F, 8123.123F, 813.76F) });
	track2[3].set_linear({ rtm::vector_set(4441.514F, 913.56F, 813.61F, 873.612F) });
	track_list[2] = track2.get_ref();

	const uint32_t filename_size = k_max_filename_size;
	char filename[filename_size] = { 0 };

	error_result error;
	for (uint32_t try_count = 0; try_count < 20; ++try_count)
	{
		get_temporary_filename(filename, filename_size, "list_vector4f_ex_");

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
		const track_vector4f_ex& ref_track = track_list[track_index];
		const track_vector4f_ex& file_track = track_cast<track_vector4f_ex>(file_track_list.track_list[track_index]);

		CHECK(file_track.get_description().output_index == ref_track.get_description().output_index);
		CHECK(rtm::scalar_near_equal(file_track.get_description().precision, ref_track.get_description().precision, 0.0F));
		CHECK(file_track.get_num_samples() == ref_track.get_num_samples());
		CHECK(file_track.get_output_index() == ref_track.get_output_index());
		CHECK(file_track.get_sample_rate() == ref_track.get_sample_rate());
		CHECK(file_track.get_type() == ref_track.get_type());
		CHECK(file_track.get_category() == ref_track.get_category());

		for (uint32_t sample_index = 0; sample_index < num_samples; ++sample_index)
		{
			const track_extended_sample_vec4f_t& ref_sample = ref_track[sample_index];
			const track_extended_sample_vec4f_t& file_sample = file_track[sample_index];

			CHECK(ref_sample.interpolator == file_sample.interpolator);
			CHECK(rtm::vector_all_near_equal(ref_sample.value, file_sample.value, 0.0F));
		}
	}
#endif
}

TEST_CASE("sjson_track_list_reader_writer vqf", "[io]")
{
#if defined(ACL_IMPL_ENABLE_IO_UNIT_TESTS)
	ansi_allocator allocator;

	const uint32_t num_tracks = 3;
	const uint32_t num_samples = 4;
	track_array_vqf track_list(allocator, num_tracks);

	track_desc_transformf desc0;
	desc0.output_index = 0;
	desc0.precision = 0.001F;
	desc0.shell_distance = 0.1233F;
	desc0.default_value = rtm::vqv_set(rtm::quat_from_euler(0.1F, 0.5F, 1.2F), rtm::vector_set(1.0F, 2.0F, 3.0F), rtm::vector_set(1.0F));

	track_desc_transformf desc1;
	desc1.output_index = 2;
	desc1.parent_index = 0;
	desc1.precision = 0.01F;
	desc1.shell_distance = 3.5F;

	track_vqf track0 = track_vqf::make_reserve(desc0, allocator, num_samples, 32.0F);
	track0[0] = rtm::vq_set(rtm::quat_from_euler(0.1F, 0.5F, 1.2F), rtm::vector_set(0.0F, 0.6F, 2.3F));
	track0[1] = rtm::vq_set(rtm::quat_from_euler(1.1F, 1.5F, 1.7F), rtm::vector_set(0.02F, 0.13F, 3.123F));
	track0[2] = rtm::vq_set(rtm::quat_from_euler(2.1F, 2.5F, 2.2F), rtm::vector_set(0.21F, 2.66F, 1.33F));
	track0[3] = rtm::vq_set(rtm::quat_from_euler(3.1F, 3.5F, 3.2F), rtm::vector_set(1.21F, 9.66F, 21.33F));
	track_list[0] = track0.get_ref();

	track_vqf track1 = track_vqf::make_reserve(desc1, allocator, num_samples, 32.0F);
	track1[0] = rtm::vq_set(rtm::quat_from_euler(10.1F, 0.25F, 91.2F), rtm::vector_set(81.0F, 0.6F, 2.33F));
	track1[1] = rtm::vq_set(rtm::quat_from_euler(21.1F, 11.5F, 13.2F), rtm::vector_set(8.02F, 2.13F, 13.13F));
	track1[2] = rtm::vq_set(rtm::quat_from_euler(32.1F, 2.53F, 2.24F), rtm::vector_set(8.21F, 12.66F, 41.33F));
	track1[3] = rtm::vq_set(rtm::quat_from_euler(43.1F, 3.25F, 73.2F), rtm::vector_set(81.21F, 91.66F, 51.33F));
	track_list[1] = track1.get_ref();

	track_vqf track2 = track_vqf::make_reserve(desc0, allocator, num_samples, 32.0F);
	track2.get_description().output_index = 1;
	track2.get_description().parent_index = 1;
	track2[0] = rtm::vq_set(rtm::quat_from_euler(1.1F, 0.5F, 1.2F), rtm::vector_set(-5.0F, 0.0F, 9.0F));
	track2[1] = rtm::vq_set(rtm::quat_from_euler(2.1F, 1.5F, 1.7F), rtm::vector_set(-4.0F, 0.5F, 8.0F));
	track2[2] = rtm::vq_set(rtm::quat_from_euler(3.1F, 2.5F, 2.2F), rtm::vector_set(-3.0F, 1.0F, 7.0F));
	track2[3] = rtm::vq_set(rtm::quat_from_euler(4.1F, 3.5F, 3.2F), rtm::vector_set(-2.0F, 1.5F, 6.0F));
	track_list[2] = track2.get_ref();

	REQUIRE(track_list.is_valid().empty());

	const uint32_t filename_size = k_max_filename_size;
	char filename[filename_size] = { 0 };

	error_result error;
	for (uint32_t try_count = 0; try_count < 20; ++try_count)
	{
		get_temporary_filename(filename, filename_size, "list_vqf_");

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

	// The file has no scale: the type is vqf, and there is no bind scale
	const std::string file_content(sjson_file_buffer, buffer_size);
	CHECK(file_content.find("\"vqf\"") != std::string::npos);
	CHECK(file_content.find("bind_scale") == std::string::npos);

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
	CHECK(file_track_list.track_list.get_track_type() == track_type8::vqf);
	CHECK(file_track_list.track_list.get_track_category() == track_category8::transformf);

	for (uint32_t track_index = 0; track_index < num_tracks; ++track_index)
	{
		const track_vqf& ref_track = track_list[track_index];
		const track_vqf& file_track = track_cast<track_vqf>(file_track_list.track_list[track_index]);

		const track_desc_transformf& ref_desc = ref_track.get_description();
		const track_desc_transformf& file_desc = file_track.get_description();
		CHECK(file_desc.output_index == ref_desc.output_index);
		CHECK(file_desc.parent_index == ref_desc.parent_index);
		CHECK(rtm::scalar_near_equal(file_desc.precision, ref_desc.precision, 0.0F));
		CHECK(rtm::scalar_near_equal(file_desc.shell_distance, ref_desc.shell_distance, 0.0F));
		CHECK(rtm::quat_near_equal(file_desc.default_value.rotation, ref_desc.default_value.rotation, 0.0F));
		CHECK(rtm::vector_all_near_equal3(file_desc.default_value.translation, ref_desc.default_value.translation, 0.0F));
		CHECK(rtm::vector_all_near_equal3(file_desc.default_value.scale, rtm::vector_set(1.0F), 0.0F));
		CHECK(file_track.get_num_samples() == ref_track.get_num_samples());
		CHECK(file_track.get_output_index() == ref_track.get_output_index());
		CHECK(file_track.get_sample_rate() == ref_track.get_sample_rate());
		CHECK(file_track.get_type() == track_type8::vqf);
		CHECK(file_track.get_category() == track_category8::transformf);

		for (uint32_t sample_index = 0; sample_index < num_samples; ++sample_index)
		{
			const rtm::vqf& ref_sample = ref_track[sample_index];
			const rtm::vqf& file_sample = file_track[sample_index];
			CHECK(rtm::quat_near_equal(ref_sample.rotation, file_sample.rotation, 0.0F));
			CHECK(rtm::vector_all_near_equal3(ref_sample.translation, file_sample.translation, 0.0F));
		}
	}
#endif
}

TEST_CASE("sjson_track_list_reader_writer vqsf", "[io]")
{
#if defined(ACL_IMPL_ENABLE_IO_UNIT_TESTS)
	ansi_allocator allocator;

	const uint32_t num_tracks = 3;
	const uint32_t num_samples = 4;
	track_array_vqsf track_list(allocator, num_tracks);

	track_desc_transformf desc0;
	desc0.output_index = 0;
	desc0.precision = 0.001F;
	desc0.shell_distance = 0.1233F;
	desc0.default_value = rtm::vqv_set(rtm::quat_from_euler(0.1F, 0.5F, 1.2F), rtm::vector_set(1.0F, 2.0F, 3.0F), rtm::vector_set(1.5F, 2.0F, 2.5F));

	track_desc_transformf desc1;
	desc1.output_index = 2;
	desc1.parent_index = 0;
	desc1.precision = 0.01F;
	desc1.shell_distance = 3.5F;

	track_vqsf track0 = track_vqsf::make_reserve(desc0, allocator, num_samples, 32.0F);
	track0[0] = rtm::vqs_set(rtm::quat_from_euler(0.1F, 0.5F, 1.2F), rtm::vector_set(0.0F, 0.6F, 2.3F), 1.0F);
	track0[1] = rtm::vqs_set(rtm::quat_from_euler(1.1F, 1.5F, 1.7F), rtm::vector_set(0.02F, 0.13F, 3.123F), 1.5F);
	track0[2] = rtm::vqs_set(rtm::quat_from_euler(2.1F, 2.5F, 2.2F), rtm::vector_set(0.21F, 2.66F, 1.33F), 0.5F);
	track0[3] = rtm::vqs_set(rtm::quat_from_euler(3.1F, 3.5F, 3.2F), rtm::vector_set(1.21F, 9.66F, 21.33F), 2.0F);
	track_list[0] = track0.get_ref();

	track_vqsf track1 = track_vqsf::make_reserve(desc1, allocator, num_samples, 32.0F);
	track1[0] = rtm::vqs_set(rtm::quat_from_euler(10.1F, 0.25F, 91.2F), rtm::vector_set(81.0F, 0.6F, 2.33F), 0.25F);
	track1[1] = rtm::vqs_set(rtm::quat_from_euler(21.1F, 11.5F, 13.2F), rtm::vector_set(8.02F, 2.13F, 13.13F), 3.0F);
	track1[2] = rtm::vqs_set(rtm::quat_from_euler(32.1F, 2.53F, 2.24F), rtm::vector_set(8.21F, 12.66F, 41.33F), 1.25F);
	track1[3] = rtm::vqs_set(rtm::quat_from_euler(43.1F, 3.25F, 73.2F), rtm::vector_set(81.21F, 91.66F, 51.33F), 0.75F);
	track_list[1] = track1.get_ref();

	track_vqsf track2 = track_vqsf::make_reserve(desc0, allocator, num_samples, 32.0F);
	track2.get_description().output_index = 1;
	track2.get_description().parent_index = 1;
	track2[0] = rtm::vqs_set(rtm::quat_from_euler(1.1F, 0.5F, 1.2F), rtm::vector_set(-5.0F, 0.0F, 9.0F), 1.0F);
	track2[1] = rtm::vqs_set(rtm::quat_from_euler(2.1F, 1.5F, 1.7F), rtm::vector_set(-4.0F, 0.5F, 8.0F), 2.5F);
	track2[2] = rtm::vqs_set(rtm::quat_from_euler(3.1F, 2.5F, 2.2F), rtm::vector_set(-3.0F, 1.0F, 7.0F), 0.1F);
	track2[3] = rtm::vqs_set(rtm::quat_from_euler(4.1F, 3.5F, 3.2F), rtm::vector_set(-2.0F, 1.5F, 6.0F), 4.0F);
	track_list[2] = track2.get_ref();

	REQUIRE(track_list.is_valid().empty());

	const uint32_t filename_size = k_max_filename_size;
	char filename[filename_size] = { 0 };

	error_result error;
	for (uint32_t try_count = 0; try_count < 20; ++try_count)
	{
		get_temporary_filename(filename, filename_size, "list_vqsf_");

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

	// The file has a uniform scale: the type is vqsf
	const std::string file_content(sjson_file_buffer, buffer_size);
	CHECK(file_content.find("\"vqsf\"") != std::string::npos);
	CHECK(file_content.find("bind_scale") != std::string::npos);

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
	CHECK(file_track_list.track_list.get_track_type() == track_type8::vqsf);
	CHECK(file_track_list.track_list.get_track_category() == track_category8::transformf);

	for (uint32_t track_index = 0; track_index < num_tracks; ++track_index)
	{
		const track_vqsf& ref_track = track_list[track_index];
		const track_vqsf& file_track = track_cast<track_vqsf>(file_track_list.track_list[track_index]);

		const track_desc_transformf& ref_desc = ref_track.get_description();
		const track_desc_transformf& file_desc = file_track.get_description();
		CHECK(file_desc.output_index == ref_desc.output_index);
		CHECK(file_desc.parent_index == ref_desc.parent_index);
		CHECK(rtm::scalar_near_equal(file_desc.precision, ref_desc.precision, 0.0F));
		CHECK(rtm::scalar_near_equal(file_desc.shell_distance, ref_desc.shell_distance, 0.0F));
		CHECK(rtm::quat_near_equal(file_desc.default_value.rotation, ref_desc.default_value.rotation, 0.0F));
		CHECK(rtm::vector_all_near_equal3(file_desc.default_value.translation, ref_desc.default_value.translation, 0.0F));
		// A VQS track has a uniform scale: the file has the X component of the default scale
		CHECK(rtm::vector_all_near_equal3(file_desc.default_value.scale, rtm::vector_dup_x(ref_desc.default_value.scale), 0.0F));
		CHECK(file_track.get_num_samples() == ref_track.get_num_samples());
		CHECK(file_track.get_output_index() == ref_track.get_output_index());
		CHECK(file_track.get_sample_rate() == ref_track.get_sample_rate());
		CHECK(file_track.get_type() == track_type8::vqsf);
		CHECK(file_track.get_category() == track_category8::transformf);

		for (uint32_t sample_index = 0; sample_index < num_samples; ++sample_index)
		{
			const rtm::vqsf& ref_sample = ref_track[sample_index];
			const rtm::vqsf& file_sample = file_track[sample_index];
			CHECK(rtm::quat_near_equal(ref_sample.rotation, file_sample.rotation, 0.0F));
			CHECK(rtm::vector_all_near_equal(ref_sample.translation_scale, file_sample.translation_scale, 0.0F));
		}
	}
#endif
}

TEST_CASE("sjson_clip_reader transform space", "[io][transform_space]")
{
#if defined(ACL_IMPL_ENABLE_IO_UNIT_TESTS)
	ansi_allocator allocator;

	// A raw clip with one bone, its samples use the default values
	auto make_clip = [](const char* transform_space_line)
	{
		std::string clip;
		clip += "version = 6\n";
		clip += "clip =\n{\n";
		clip += "\tname = \"test\"\n";
		clip += "\tnum_samples = 2\n";
		clip += "\tsample_rate = 30\n";
		clip += transform_space_line;
		clip += "}\n";
		clip += "bones =\n[\n\t{\n\t\tname = \"root\"\n\t\tparent = \"\"\n\t\tvertex_distance = 1.0\n\t}\n]\n";
		clip += "tracks =\n[\n\t{\n\t\tname = \"root\"\n\t}\n]\n";
		return clip;
	};

	{
		// Without the key, the transform space is local
		const std::string clip = make_clip("");
		clip_reader reader(allocator, clip.c_str(), clip.size());
		REQUIRE(reader.get_file_type() == sjson_file_type::raw_clip);

		sjson_raw_clip file_clip;
		REQUIRE(reader.read_raw_clip(file_clip));
		CHECK(file_clip.track_list.get_transform_space() == transform_space_t::local);
		CHECK(file_clip.track_list.get_num_tracks() == 1);
	}

	{
		const std::string clip = make_clip("\ttransform_space = \"local\"\n");
		clip_reader reader(allocator, clip.c_str(), clip.size());

		sjson_raw_clip file_clip;
		REQUIRE(reader.read_raw_clip(file_clip));
		CHECK(file_clip.track_list.get_transform_space() == transform_space_t::local);
	}

	{
		const std::string clip = make_clip("\ttransform_space = \"mesh\"\n");
		clip_reader reader(allocator, clip.c_str(), clip.size());

		sjson_raw_clip file_clip;
		REQUIRE(reader.read_raw_clip(file_clip));
		CHECK(file_clip.track_list.get_transform_space() == transform_space_t::mesh);
	}

	{
		const std::string clip = make_clip("\ttransform_space = \"unknown\"\n");
		clip_reader reader(allocator, clip.c_str(), clip.size());

		sjson_raw_clip file_clip;
		CHECK_FALSE(reader.read_raw_clip(file_clip));
		CHECK(reader.get_error().error == clip_reader_error::InvalidTransformSpace);
	}
#endif
}
