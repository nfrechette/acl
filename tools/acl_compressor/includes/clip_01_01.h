#pragma once
#include "acl/compression/skeleton.h"
#include "acl/math/quat_64.h"
#include "acl/math/vector4_64.h"

namespace clip_01_01
{
	extern acl::RigidBone bones[];
	extern uint16_t num_bones;
	static constexpr uint16_t num_samples = 688;
	static constexpr uint16_t sample_rate = 30;

	extern uint16_t rotation_track_bone_index[];
	extern uint32_t num_rotation_tracks;
	extern acl::Quat_64 rotation_tracks[][688];

	extern uint16_t translation_track_bone_index[];
	extern uint32_t num_translation_tracks;
	extern acl::Vector4_64 translation_tracks[][688];
}

