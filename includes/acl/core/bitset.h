#pragma once

#include "acl/core/error.h"

namespace acl
{
	constexpr uint32_t get_bitset_size(uint32_t num_bits)
	{
		return (num_bits + 32 - 1) / 32;
	}

	constexpr uint32_t get_bitset_num_bits(uint32_t size)
	{
		return size * 32;
	}

	inline void bitset_reset(uint32_t* bitset, uint32_t size, bool value)
	{
		const uint32_t mask = value ? 0xFFFFFFFF : 0x00000000;

		for (uint32_t offset = 0; offset < size; ++offset)
			bitset[offset] = mask;
	}

	inline void bitset_set(uint32_t* bitset, uint32_t size, uint32_t bit_offset, bool value)
	{
		ACL_ENSURE(bit_offset < get_bitset_num_bits(size), "Invalid bit offset: %u >= %u", bit_offset, get_bitset_num_bits(size));

		const uint32_t offset = bit_offset / 32;
		const uint32_t mask = 1 << (31 - (bit_offset % 32));

		if (value)
			bitset[offset] |= mask;
		else
			bitset[offset] &= ~mask;
	}

	inline bool bitset_test(const uint32_t* bitset, uint32_t size, uint32_t bit_offset)
	{
		ACL_ENSURE(bit_offset < get_bitset_num_bits(size), "Invalid bit offset: %u >= %u", bit_offset, get_bitset_num_bits(size));

		const uint32_t offset = bit_offset / 32;
		const uint32_t mask = 1 << (31 - (bit_offset % 32));

		return (bitset[offset] & mask) != 0;
	}

	inline uint32_t bitset_count_set_bits(const uint32_t* bitset, uint32_t size)
	{
		// TODO: Use popcount instruction if available
		uint32_t num_set_bits = 0;
		for (uint32_t offset = 0; offset < size; ++offset)
		{
			uint32_t value = bitset[offset];
			value = value - ((value >> 1) & 0x55555555);
			value = (value & 0x33333333) + ((value >> 2) & 0x33333333);
			num_set_bits += (((value + (value >> 4)) & 0x0F0F0F0F) * 0x01010101) >> 24;
		}

		return num_set_bits;
	}
}
