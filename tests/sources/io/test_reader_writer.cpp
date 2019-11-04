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

#include <catch.hpp>

// Enable allocation tracking
#define ACL_ALLOCATOR_TRACK_NUM_ALLOCATIONS
#define ACL_ALLOCATOR_TRACK_ALL_ALLOCATIONS

#include <sjson/parser.h>
#include <sjson/writer.h>

#include <acl/core/ansi_allocator.h>
#include <acl/io/clip_reader.h>
#include <acl/io/clip_writer.h>
#include <acl/math/math.h>
#include <acl/math/scalar_32.h>

#include <cstdio>
#include <cstdlib>

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

TEST_CASE("sjson_reader_writer", "[io]")
{
	// Only test the reader/writer on non-mobile platforms
#if defined(ACL_SSE2_INTRINSICS)
	ANSIAllocator allocator;

	const uint16_t num_bones = 3;
	RigidBone bones[num_bones];
	bones[0].name = String(allocator, "root");
	bones[0].vertex_distance = 4.0F;
	bones[0].parent_index = k_invalid_bone_index;
	bones[0].bind_transform = transform_identity_64();

	bones[1].name = String(allocator, "bone1");
	bones[1].vertex_distance = 3.0F;
	bones[1].parent_index = 0;
	bones[1].bind_transform = transform_set(quat_from_axis_angle(vector_set(0.0, 1.0, 0.0), k_pi_64 * 0.5), vector_set(3.2, 8.2, 5.1), vector_set(1.0));

	bones[2].name = String(allocator, "bone2");
	bones[2].vertex_distance = 2.0F;
	bones[2].parent_index = 1;
	bones[2].bind_transform = transform_set(quat_from_axis_angle(vector_set(0.0, 0.0, 1.0), k_pi_64 * 0.25), vector_set(6.3, 9.4, 1.5), vector_set(1.0));

	RigidSkeleton skeleton(allocator, bones, num_bones);

	const uint32_t num_samples = 4;
	AnimationClip clip(allocator, skeleton, num_samples, 30.0F, String(allocator, "test_clip"));

	AnimatedBone* animated_bones = clip.get_bones();
	animated_bones[0].output_index = 0;
	animated_bones[0].rotation_track.set_sample(0, quat_from_axis_angle(vector_set(0.0, 1.0, 0.0), k_pi_64 * 0.1));
	animated_bones[0].rotation_track.set_sample(1, quat_from_axis_angle(vector_set(0.0, 1.0, 0.0), k_pi_64 * 0.2));
	animated_bones[0].rotation_track.set_sample(2, quat_from_axis_angle(vector_set(0.0, 1.0, 0.0), k_pi_64 * 0.3));
	animated_bones[0].rotation_track.set_sample(3, quat_from_axis_angle(vector_set(0.0, 1.0, 0.0), k_pi_64 * 0.4));
	animated_bones[0].translation_track.set_sample(0, vector_set(3.2, 1.4, 9.4));
	animated_bones[0].translation_track.set_sample(1, vector_set(3.3, 1.5, 9.5));
	animated_bones[0].translation_track.set_sample(2, vector_set(3.4, 1.6, 9.6));
	animated_bones[0].translation_track.set_sample(3, vector_set(3.5, 1.7, 9.7));
	animated_bones[0].scale_track.set_sample(0, vector_set(1.0, 1.5, 1.1));
	animated_bones[0].scale_track.set_sample(1, vector_set(1.1, 1.6, 1.2));
	animated_bones[0].scale_track.set_sample(2, vector_set(1.2, 1.7, 1.3));
	animated_bones[0].scale_track.set_sample(3, vector_set(1.3, 1.8, 1.4));

	animated_bones[1].output_index = 2;
	animated_bones[1].rotation_track.set_sample(0, quat_from_axis_angle(vector_set(0.0, 1.0, 0.0), k_pi_64 * 1.1));
	animated_bones[1].rotation_track.set_sample(1, quat_from_axis_angle(vector_set(0.0, 1.0, 0.0), k_pi_64 * 1.2));
	animated_bones[1].rotation_track.set_sample(2, quat_from_axis_angle(vector_set(0.0, 1.0, 0.0), k_pi_64 * 1.3));
	animated_bones[1].rotation_track.set_sample(3, quat_from_axis_angle(vector_set(0.0, 1.0, 0.0), k_pi_64 * 1.4));
	animated_bones[1].translation_track.set_sample(0, vector_set(5.2, 2.4, 13.4));
	animated_bones[1].translation_track.set_sample(1, vector_set(5.3, 2.5, 13.5));
	animated_bones[1].translation_track.set_sample(2, vector_set(5.4, 2.6, 13.6));
	animated_bones[1].translation_track.set_sample(3, vector_set(5.5, 2.7, 13.7));
	animated_bones[1].scale_track.set_sample(0, vector_set(2.0, 0.5, 4.1));
	animated_bones[1].scale_track.set_sample(1, vector_set(2.1, 0.6, 4.2));
	animated_bones[1].scale_track.set_sample(2, vector_set(2.2, 0.7, 4.3));
	animated_bones[1].scale_track.set_sample(3, vector_set(2.3, 0.8, 4.4));

	animated_bones[2].output_index = 1;
	animated_bones[2].rotation_track.set_sample(0, quat_from_axis_angle(vector_set(0.0, 0.0, 1.0), k_pi_64 * 0.7));
	animated_bones[2].rotation_track.set_sample(1, quat_from_axis_angle(vector_set(0.0, 0.0, 1.0), k_pi_64 * 0.8));
	animated_bones[2].rotation_track.set_sample(2, quat_from_axis_angle(vector_set(0.0, 0.0, 1.0), k_pi_64 * 0.9));
	animated_bones[2].rotation_track.set_sample(3, quat_from_axis_angle(vector_set(0.0, 0.0, 1.0), k_pi_64 * 0.4));
	animated_bones[2].translation_track.set_sample(0, vector_set(1.2, 123.4, 11.4));
	animated_bones[2].translation_track.set_sample(1, vector_set(1.3, 123.5, 11.5));
	animated_bones[2].translation_track.set_sample(2, vector_set(1.4, 123.6, 11.6));
	animated_bones[2].translation_track.set_sample(3, vector_set(1.5, 123.7, 11.7));
	animated_bones[2].scale_track.set_sample(0, vector_set(4.0, 2.5, 3.1));
	animated_bones[2].scale_track.set_sample(1, vector_set(4.1, 2.6, 3.2));
	animated_bones[2].scale_track.set_sample(2, vector_set(4.2, 2.7, 3.3));
	animated_bones[2].scale_track.set_sample(3, vector_set(4.3, 2.8, 3.4));

	CompressionSettings settings;
	settings.constant_rotation_threshold_angle = 32.23F;
	settings.constant_scale_threshold = 1.123F;
	settings.constant_translation_threshold = 0.124F;
	settings.error_threshold = 0.23F;
	settings.level = CompressionLevel8::High;
	settings.range_reduction = RangeReductionFlags8::Rotations | RangeReductionFlags8::Scales;
	settings.rotation_format = RotationFormat8::QuatDropW_48;
	settings.scale_format = VectorFormat8::Vector3_96;
	settings.translation_format = VectorFormat8::Vector3_32;
	settings.segmenting.enabled = false;
	settings.segmenting.ideal_num_samples = 23;
	settings.segmenting.max_num_samples = 123;
	settings.segmenting.range_reduction = RangeReductionFlags8::Translations;

#ifdef _WIN32
	const uint32_t clip_filename_size = MAX_PATH;
	char clip_filename[clip_filename_size] = { 0 };

	DWORD result = GetTempPathA(clip_filename_size, clip_filename);
	REQUIRE(result != 0);

	char id[1024];
	snprintf(id, get_array_size(id), "%u", std::rand());
	strcat_s(clip_filename, clip_filename_size, id);
	strcat_s(clip_filename, clip_filename_size, ".acl.sjson");
#else
	const uint32_t clip_filename_size = 1024;
	char clip_filename[clip_filename_size] = { 0 };
	std::strcat(clip_filename, "/tmp/");
	char id[1024];
	snprintf(id, get_array_size(id), "%u", std::rand());
	std::strcat(clip_filename, id);
	std::strcat(clip_filename, ".acl.sjson");
#endif

	// Write the clip to a temporary file
	const char* error = write_acl_clip(skeleton, clip, AlgorithmType8::UniformlySampled, settings, clip_filename);
	REQUIRE(error == nullptr);

	std::FILE* file = nullptr;

#ifdef _WIN32
	fopen_s(&file, clip_filename, "rb");
#else
	file = fopen(clip_filename, "rb");
#endif

	REQUIRE(file != nullptr);

	char sjson_file_buffer[256 * 1024];
	const size_t clip_size = fread(sjson_file_buffer, 1, get_array_size(sjson_file_buffer), file);
	fclose(file);

	std::remove(clip_filename);

	// Read back the clip
	ClipReader reader(allocator, sjson_file_buffer, clip_size - 1);

	REQUIRE(reader.get_file_type() == sjson_file_type::raw_clip);

	sjson_raw_clip file_clip;
	const bool success = reader.read_raw_clip(file_clip);
	REQUIRE(success);

	CHECK(file_clip.algorithm_type == AlgorithmType8::UniformlySampled);
	CHECK(file_clip.has_settings);
	CHECK(file_clip.settings.get_hash() == settings.get_hash());
	CHECK(file_clip.skeleton->get_num_bones() == num_bones);
	CHECK(file_clip.clip->get_num_bones() == num_bones);
	CHECK(file_clip.clip->get_name() == clip.get_name());
	CHECK(scalar_near_equal(file_clip.clip->get_duration(), clip.get_duration(), 1.0E-8F));
	CHECK(file_clip.clip->get_num_samples() == clip.get_num_samples());
	CHECK(file_clip.clip->get_sample_rate() == clip.get_sample_rate());

	for (uint16_t bone_index = 0; bone_index < num_bones; ++bone_index)
	{
		const RigidBone& src_bone = skeleton.get_bone(bone_index);
		const RigidBone& file_bone = file_clip.skeleton->get_bone(bone_index);
		CHECK(src_bone.name == file_bone.name);
		CHECK(src_bone.vertex_distance == file_bone.vertex_distance);
		CHECK(src_bone.parent_index == file_bone.parent_index);
		CHECK(quat_near_equal(src_bone.bind_transform.rotation, file_bone.bind_transform.rotation, 0.0));
		CHECK(vector_all_near_equal3(src_bone.bind_transform.translation, file_bone.bind_transform.translation, 0.0));
		CHECK(vector_all_near_equal3(src_bone.bind_transform.scale, file_bone.bind_transform.scale, 0.0));

		const AnimatedBone& src_animated_bone = clip.get_animated_bone(bone_index);
		const AnimatedBone& file_animated_bone = file_clip.clip->get_animated_bone(bone_index);
		//REQUIRE(src_animated_bone.output_index == file_animated_bone.output_index);

		for (uint32_t sample_index = 0; sample_index < num_samples; ++sample_index)
		{
			CHECK(quat_near_equal(src_animated_bone.rotation_track.get_sample(sample_index), file_animated_bone.rotation_track.get_sample(sample_index), 0.0));
			CHECK(vector_all_near_equal3(src_animated_bone.translation_track.get_sample(sample_index), file_animated_bone.translation_track.get_sample(sample_index), 0.0));
			CHECK(vector_all_near_equal3(src_animated_bone.scale_track.get_sample(sample_index), file_animated_bone.scale_track.get_sample(sample_index), 0.0));
		}
	}
#endif
}
