#pragma once

#include "acl/compressed_clip.h"

namespace acl
{
	inline CompressedClip* make_compressed_clip(void* buffer, uint32_t size, AlgorithmType type)
	{
		return new(buffer) CompressedClip(size, type);
	}
}
