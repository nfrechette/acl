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

#include "clip_01_01.h"

#include <Windows.h>
#include <cstring>
#include <cstdio>

//#define ACL_RUN_UNIT_TESTS

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

#ifdef ACL_RUN_UNIT_TESTS
static acl::Vector4_64 quat_rotate_scalar(const acl::Quat_64& rotation, const acl::Vector4_64& vector)
{
	using namespace acl;
	// (q.W*q.W-qv.qv)v + 2(qv.v)qv + 2 q.W (qv x v)
	Vector4_64 qv = vector_set(quat_get_x(rotation), quat_get_y(rotation), quat_get_z(rotation));
	Vector4_64 vOut = vector_mul(vector_cross3(qv, vector), 2.0 * quat_get_w(rotation));
	vOut = vector_add(vOut, vector_mul(vector, (quat_get_w(rotation) * quat_get_w(rotation)) - vector_dot(qv, qv)));
	vOut = vector_add(vOut, vector_mul(qv, 2.0 * vector_dot(qv, vector)));
	return vOut;
}

static acl::Quat_64 quat_mul_scalar(const acl::Quat_64& lhs, const acl::Quat_64& rhs)
{
	using namespace acl;
	double lhs_raw[4] = { quat_get_x(lhs), quat_get_y(lhs), quat_get_z(lhs), quat_get_w(lhs) };
	double rhs_raw[4] = { quat_get_x(rhs), quat_get_y(rhs), quat_get_z(rhs), quat_get_w(rhs) };

	double x = (rhs_raw[3] * lhs_raw[0]) + (rhs_raw[0] * lhs_raw[3]) + (rhs_raw[1] * lhs_raw[2]) - (rhs_raw[2] * lhs_raw[1]);
	double y = (rhs_raw[3] * lhs_raw[1]) - (rhs_raw[0] * lhs_raw[2]) + (rhs_raw[1] * lhs_raw[3]) + (rhs_raw[2] * lhs_raw[0]);
	double z = (rhs_raw[3] * lhs_raw[2]) + (rhs_raw[0] * lhs_raw[1]) - (rhs_raw[1] * lhs_raw[0]) + (rhs_raw[2] * lhs_raw[3]);
	double w = (rhs_raw[3] * lhs_raw[3]) - (rhs_raw[0] * lhs_raw[0]) - (rhs_raw[1] * lhs_raw[1]) - (rhs_raw[2] * lhs_raw[2]);

	return quat_set(x, y, z, w);
}

static void run_unit_tests()
{
	using namespace acl;

	constexpr double threshold = 1e-6;

	{
		Quat_64 quat0 = quat_from_euler(deg2rad(30.0), deg2rad(-45.0), deg2rad(90.0));
		Quat_64 quat1 = quat_from_euler(deg2rad(45.0), deg2rad(60.0), deg2rad(120.0));
		Quat_64 result = quat_mul(quat0, quat1);
		Quat_64 result_ref = quat_mul_scalar(quat0, quat1);
		ensure(quat_near_equal(result, result_ref, threshold));

		quat0 = quat_set(0.39564531008956383, 0.044254239301713752, 0.22768840967675355, 0.88863059760894492);
		quat1 = quat_set(1.0, 0.0, 0.0, 0.0);
		result = quat_mul(quat0, quat1);
		result_ref = quat_mul_scalar(quat0, quat1);
		ensure(quat_near_equal(result, result_ref, threshold));
	}

	{
		const Quat_64 test_rotations[] = {
			quat_identity_64(),
			quat_from_euler(deg2rad(30.0), deg2rad(-45.0), deg2rad(90.0)),
			quat_from_euler(deg2rad(45.0), deg2rad(60.0), deg2rad(120.0)),
			quat_from_euler(deg2rad(0.0), deg2rad(180.0), deg2rad(45.0)),
			quat_from_euler(deg2rad(-120.0), deg2rad(-90.0), deg2rad(0.0)),
			quat_from_euler(deg2rad(-0.01), deg2rad(0.02), deg2rad(-0.03)),
		};

		const Vector4_64 test_vectors[] = {
			vector_zero_64(),
			vector_set(1.0, 0.0, 0.0),
			vector_set(0.0, 1.0, 0.0),
			vector_set(0.0, 0.0, 1.0),
			vector_set(45.0, -60.0, 120.0),
			vector_set(-45.0, 60.0, -120.0),
			vector_set(0.57735026918962576451, 0.57735026918962576451, 0.57735026918962576451),
			vector_set(-1.0, 0.0, 0.0),
		};

		for (size_t quat_index = 0; quat_index < (sizeof(test_rotations) / sizeof(Quat_64)); ++quat_index)
		{
			const Quat_64& rotation = test_rotations[quat_index];
			for (size_t vector_index = 0; vector_index < (sizeof(test_vectors) / sizeof(Vector4_64)); ++vector_index)
			{
				const Vector4_64& vector = test_vectors[vector_index];
				Vector4_64 result = quat_rotate(rotation, vector);
				Vector4_64 result_ref = quat_rotate_scalar(rotation, vector);
				ensure(vector_near_equal(result, result_ref, threshold));
			}
		}
	}

	{
		Quat_64 rotation = quat_set(0.39564531008956383, 0.044254239301713752, 0.22768840967675355, 0.88863059760894492);
		Vector4_64 axis_ref = vector_set(1.0, 0.0, 0.0);
		axis_ref = quat_rotate(rotation, axis_ref);
		double angle_ref = deg2rad(57.0);
		Quat_64 result = quat_from_axis_angle(axis_ref, angle_ref);
		Vector4_64 axis;
		double angle;
		quat_to_axis_angle(result, axis, angle);
		ensure(vector_near_equal(axis, axis_ref, threshold));
		ensure(scalar_near_equal(angle, angle_ref, threshold));
	}
}
#endif

int main(int argc, char** argv)
{
	using namespace acl;

#ifdef ACL_RUN_UNIT_TESTS
	run_unit_tests();
#endif

	const Options options = parse_options(argc, argv);

	Allocator allocator;

	// Initialize our skeleton
	RigidSkeleton skeleton(allocator, &clip_01_01::bones[0], clip_01_01::num_bones);

	// Populate our clip with our raw samples
	AnimationClip clip(allocator, skeleton, clip_01_01::num_samples, clip_01_01::sample_rate);
	AnimatedBone* clip_bones = clip.get_bones();

	for (uint16_t bone_index = 0; bone_index < clip_01_01::num_bones; ++bone_index)
	{
		uint32_t rotation_track_index = ~0u;
		for (uint32_t track_bone_index = 0; track_bone_index < clip_01_01::num_rotation_tracks; ++track_bone_index)
		{
			if (clip_01_01::rotation_track_bone_index[track_bone_index] == bone_index)
			{
				rotation_track_index = track_bone_index;
				break;
			}
		}

		uint32_t translation_track_index = ~0u;
		for (uint32_t track_bone_index = 0; track_bone_index < clip_01_01::num_translation_tracks; ++track_bone_index)
		{
			if (clip_01_01::translation_track_bone_index[track_bone_index] == bone_index)
			{
				translation_track_index = track_bone_index;
				break;
			}
		}

		for (uint32_t sample_index = 0; sample_index < clip_01_01::num_samples; ++sample_index)
		{
			Quat_64 rotation = rotation_track_index != ~0u ? clip_01_01::rotation_tracks[rotation_track_index][sample_index] : quat_identity_64();
			clip_bones[bone_index].rotation_track.set_sample(sample_index, rotation);

			Vector4_64 translation = translation_track_index != ~0u ? clip_01_01::translation_tracks[translation_track_index][sample_index] : vector_zero_64();
			clip_bones[bone_index].translation_track.set_sample(sample_index, translation);
		}
	}

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
