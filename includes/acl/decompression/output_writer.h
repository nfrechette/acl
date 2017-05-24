#pragma once

#include "acl/math/quat_32.h"
#include "acl/math/vector4_32.h"

#include <stdint.h>

namespace acl
{
	// We use a struct like this to allow an arbitrary format on the end user side.
	// Since our decode function is templated on this type implemented by the user,
	// the callbacks can trivially be inlined.
	struct OutputWriter
	{
		// TODO: use constexpr flags to control if we extract rotation only, translation only, etc.

		void WriteBoneRotation(uint32_t bone_index, const Quat_32& rotation)
		{
		}

		void WriteBoneTranslation(uint32_t bone_index, const Vector4_32& translation)
		{
		}
	};
}
