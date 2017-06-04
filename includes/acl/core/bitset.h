#pragma once

#include "acl/core/error.h"

namespace acl
{
	inline void bitset_reset(uint32_t* bitset, uint32_t size, bool value)
	{
		uint32_t mask = value ? 0xFFFFFFFF : 0x00000000;

		for (uint32_t offset = 0; offset < size; ++offset)
			bitset[offset] = mask;
	}

	inline void bitset_set(uint32_t* bitset, uint32_t size, uint32_t bit_offset, bool value)
	{
		ACL_ENSURE(bit_offset < (size * 32), "Invalid bit offset: %u > %u", bit_offset, size * 32);

		uint32_t offset = bit_offset / 32;
		uint32_t mask = 1 << (31 - (bit_offset - offset));

		if (value)
			bitset[offset] |= mask;
		else
			bitset[offset] &= ~mask;
	}

	inline bool bitset_test(const uint32_t* bitset, uint32_t size, uint32_t bit_offset)
	{
		ACL_ENSURE(bit_offset < (size * 32), "Invalid bit offset: %u > %u", bit_offset, size * 32);

		uint32_t offset = bit_offset / 32;
		uint32_t mask = 1 << (31 - (bit_offset - offset));

		return (bitset[offset] & mask) != 0;
	}
}
