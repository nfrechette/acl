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

#include <catch2/catch.hpp>

#include <acl/core/memory_utils.h>

#include <cstdint>
#include <cstring>
#include <limits>

using namespace acl;

TEST_CASE("misc tests", "[core][memory]")
{
	size_t num_powers_of_two = 0;
	for (size_t i = 0; i <= 65536; ++i)
	{
		if (is_power_of_two(i))
			num_powers_of_two++;
	}

	CHECK(num_powers_of_two == 17);
	CHECK(is_power_of_two(1) == true);
	CHECK(is_power_of_two(2) == true);
	CHECK(is_power_of_two(4) == true);
	CHECK(is_power_of_two(8) == true);
	CHECK(is_power_of_two(16) == true);
	CHECK(is_power_of_two(32) == true);
	CHECK(is_power_of_two(64) == true);
	CHECK(is_power_of_two(128) == true);
	CHECK(is_power_of_two(256) == true);
	CHECK(is_power_of_two(512) == true);
	CHECK(is_power_of_two(1024) == true);
	CHECK(is_power_of_two(2048) == true);
	CHECK(is_power_of_two(4096) == true);
	CHECK(is_power_of_two(8192) == true);
	CHECK(is_power_of_two(16384) == true);
	CHECK(is_power_of_two(32768) == true);
	CHECK(is_power_of_two(65536) == true);

	CHECK(is_alignment_valid<int32_t>(0) == false);
	CHECK(is_alignment_valid<int32_t>(4) == true);
	CHECK(is_alignment_valid<int32_t>(8) == true);
	CHECK(is_alignment_valid<int32_t>(2) == false);
	CHECK(is_alignment_valid<int32_t>(5) == false);
	CHECK(is_alignment_valid<int64_t>(8) == true);
	CHECK(is_alignment_valid<int64_t>(16) == true);

	struct alignas(8) Tmp
	{
		int32_t padding;	// Aligned to 8 bytes
		int32_t value;		// Aligned to 4 bytes
	};
	Tmp tmp;
	CHECK(is_aligned_to(&tmp.padding, 8) == true);
	CHECK(is_aligned_to(&tmp.value, 4) == true);
	CHECK(is_aligned_to(&tmp.value, 2) == true);
	CHECK(is_aligned_to(&tmp.value, 1) == true);
	CHECK(is_aligned_to(&tmp.value, 8) == false);

	CHECK(is_aligned_to(4, 4) == true);
	CHECK(is_aligned_to(4, 2) == true);
	CHECK(is_aligned_to(4, 1) == true);
	CHECK(is_aligned_to(4, 8) == false);
	CHECK(is_aligned_to(6, 4) == false);
	CHECK(is_aligned_to(6, 2) == true);
	CHECK(is_aligned_to(6, 1) == true);

	CHECK(is_aligned_to(align_to(5, 4), 4) == true);
	CHECK(align_to(5, 4) == 8);
	CHECK(is_aligned_to(align_to(8, 4), 4) == true);
	CHECK(align_to(8, 4) == 8);

	void* ptr = (void*)0x00000000;
	CHECK(align_to(ptr, 4) == (void*)0x00000000);
	CHECK(align_to(ptr, 8) == (void*)0x00000000);
	ptr = (void*)0x00000001;
	CHECK(align_to(ptr, 4) == (void*)0x00000004);
	CHECK(align_to(ptr, 8) == (void*)0x00000008);
	ptr = (void*)0x00000004;
	CHECK(align_to(ptr, 4) == (void*)0x00000004);
	CHECK(align_to(ptr, 8) == (void*)0x00000008);

	struct alignas(8) Align8
	{
		float tmp[2];
	};

	constexpr size_t padding0 = get_required_padding<float, Align8>();
	constexpr size_t padding1 = get_required_padding<uint8_t, Align8>();
	CHECK(padding0 == 4);
	CHECK(padding1 == 7);

	int32_t array[8];
	CHECK(get_array_size(array) == (sizeof(array) / sizeof(array[0])));
}

TEST_CASE("raw memory support", "[core][memory]")
{
	uint8_t buffer[1024];
	uint8_t* ptr = &buffer[32];
	CHECK(add_offset_to_ptr<uint8_t>(ptr, 23) == ptr + 23);
	CHECK(add_offset_to_ptr<uint8_t>(ptr, 64) == ptr + 64);

	uint16_t value16 = 0x04FE;
	CHECK(byte_swap(value16) == 0xFE04);

	uint32_t value32 = 0x04FE78AB;
	CHECK(byte_swap(value32) == 0xAB78FE04);

	uint64_t value64 = uint64_t(0x04FE78AB0098DC56ULL);
	CHECK(byte_swap(value64) == uint64_t(0x56DC9800AB78FE04ULL));

	uint8_t unaligned_value_buffer[5] = { 0x00, 0x00, 0x00, 0x00, 0x00 };
	std::memcpy(&unaligned_value_buffer[1], &value32, sizeof(uint32_t));
	CHECK(unaligned_load<uint32_t>(&unaligned_value_buffer[1]) == value32);
}

TEST_CASE("memcpy_bits", "[core][memory]")
{
	uint64_t dest = uint64_t(~0ULL);
	uint64_t src = byte_swap(uint64_t(0x5555555555555555ULL));
	memcpy_bits(&dest, 1, &src, 0, 64 - 3);
	CHECK(dest == byte_swap(uint64_t(0xAAAAAAAAAAAAAAABULL)));

	dest = byte_swap(uint64_t(0x0F00FF0000000000ULL));
	src = byte_swap(uint64_t(0x3800000000000000ULL));
	memcpy_bits(&dest, 0, &src, 2, 5);
	CHECK(dest == byte_swap(uint64_t(0xE700FF0000000000ULL)));

	dest = byte_swap(uint64_t(0x0F00FF0000000000ULL));
	src = byte_swap(uint64_t(0x3800000000000000ULL));
	memcpy_bits(&dest, 1, &src, 2, 5);
	CHECK(dest == byte_swap(uint64_t(0x7300FF0000000000ULL)));

	dest = 0;
	src = uint64_t(~0ULL);
	memcpy_bits(&dest, 1, &src, 0, 7);
	CHECK(dest == byte_swap(uint64_t(0x7F00000000000000ULL)));

	memcpy_bits(&dest, 8, &src, 0, 8);
	CHECK(dest == byte_swap(uint64_t(0x7FFF000000000000ULL)));

	memcpy_bits(&dest, 0, &src, 0, 64);
	CHECK(dest == uint64_t(~0ULL));

	dest = 0;
	src = uint64_t(~0ULL);
	memcpy_bits(&dest, 0, &src, 0, 0);
	CHECK(dest == 0);

	memcpy_bits(&dest, 0, nullptr, 0, 0);
	CHECK(dest == 0);

	memcpy_bits(nullptr, 0, &src, 0, 0);
	CHECK(dest == 0);
}

enum class UnsignedEnum : uint32_t
{
	ZERO = 0,
	U16_MAX = std::numeric_limits<uint16_t>::max(),
	U32_MAX = std::numeric_limits<uint32_t>::max(),
};

enum class SignedEnum : int32_t
{
	I32_MIN = std::numeric_limits<int32_t>::min(),
	I16_MIN = std::numeric_limits<int16_t>::min(),
	I16_MAX = std::numeric_limits<int16_t>::max(),
	I32_MAX = std::numeric_limits<int32_t>::max(),
};

TEST_CASE("safe_static_cast from unsigned enum", "[core][memory]")
{
	CHECK(safe_static_cast<uint32_t>(UnsignedEnum::ZERO) == 0);
	CHECK(safe_static_cast<uint32_t>(UnsignedEnum::U16_MAX) == std::numeric_limits<uint16_t>::max());
	CHECK(safe_static_cast<uint32_t>(UnsignedEnum::U32_MAX) == std::numeric_limits<uint32_t>::max());

	CHECK(safe_static_cast<int32_t>(UnsignedEnum::ZERO) == 0);
	CHECK(safe_static_cast<int32_t>(UnsignedEnum::U16_MAX));
	CHECK_THROWS(safe_static_cast<int32_t>(UnsignedEnum::U32_MAX));

	CHECK(safe_static_cast<uint16_t>(UnsignedEnum::ZERO) == 0);
	CHECK(safe_static_cast<uint16_t>(UnsignedEnum::U16_MAX) == std::numeric_limits<uint16_t>::max());
	CHECK_THROWS(safe_static_cast<uint16_t>(UnsignedEnum::U32_MAX));

	CHECK(safe_static_cast<int16_t>(UnsignedEnum::ZERO) == 0);
	CHECK_THROWS(safe_static_cast<int16_t>(UnsignedEnum::U16_MAX));
	CHECK_THROWS(safe_static_cast<int16_t>(UnsignedEnum::U32_MAX));
}

TEST_CASE("safe_static_cast from signed enum", "[core][memory]")
{
	CHECK_THROWS(safe_static_cast<uint32_t>(SignedEnum::I32_MIN));
	CHECK_THROWS(safe_static_cast<uint32_t>(SignedEnum::I16_MIN));
	CHECK(safe_static_cast<uint32_t>(SignedEnum::I16_MAX) == safe_static_cast<uint32_t>(std::numeric_limits<int16_t>::max()));
	CHECK(safe_static_cast<uint32_t>(SignedEnum::I32_MAX) == safe_static_cast<uint32_t>(std::numeric_limits<int32_t>::max()));

	CHECK(safe_static_cast<int32_t>(SignedEnum::I32_MIN) == std::numeric_limits<int32_t>::min());
	CHECK(safe_static_cast<int32_t>(SignedEnum::I16_MIN) == std::numeric_limits<int16_t>::min());
	CHECK(safe_static_cast<int32_t>(SignedEnum::I16_MAX) == std::numeric_limits<int16_t>::max());
	CHECK(safe_static_cast<int32_t>(SignedEnum::I32_MAX) == std::numeric_limits<int32_t>::max());

	CHECK_THROWS(safe_static_cast<uint16_t>(SignedEnum::I32_MIN));
	CHECK_THROWS(safe_static_cast<uint16_t>(SignedEnum::I16_MIN));
	CHECK(safe_static_cast<uint16_t>(SignedEnum::I16_MAX) == std::numeric_limits<int16_t>::max());
	CHECK_THROWS(safe_static_cast<uint16_t>(SignedEnum::I32_MAX));

	CHECK_THROWS(safe_static_cast<int16_t>(SignedEnum::I32_MIN));
	CHECK(safe_static_cast<int16_t>(SignedEnum::I16_MIN) == std::numeric_limits<int16_t>::min());
	CHECK(safe_static_cast<int16_t>(SignedEnum::I16_MAX) == std::numeric_limits<int16_t>::max());
	CHECK_THROWS(safe_static_cast<int16_t>(SignedEnum::I32_MAX));
}

TEST_CASE("safe_static_cast from signed int", "[core][memory]")
{
	CHECK_THROWS(safe_static_cast<uint32_t>(std::numeric_limits<int32_t>::min()));
	CHECK_THROWS(safe_static_cast<uint32_t>(std::numeric_limits<int16_t>::min()));
	CHECK(safe_static_cast<uint32_t>(std::numeric_limits<int16_t>::max()) == safe_static_cast<uint32_t>(std::numeric_limits<int16_t>::max()));
	CHECK(safe_static_cast<uint32_t>(std::numeric_limits<int32_t>::max()) == safe_static_cast<uint32_t>(std::numeric_limits<int32_t>::max()));

	CHECK(safe_static_cast<int32_t>(std::numeric_limits<int32_t>::min()) == std::numeric_limits<int32_t>::min());
	CHECK(safe_static_cast<int32_t>(std::numeric_limits<int16_t>::min()) == std::numeric_limits<int16_t>::min());
	CHECK(safe_static_cast<int32_t>(std::numeric_limits<int16_t>::max()) == std::numeric_limits<int16_t>::max());
	CHECK(safe_static_cast<int32_t>(std::numeric_limits<int32_t>::max()) == std::numeric_limits<int32_t>::max());

	CHECK_THROWS(safe_static_cast<uint16_t>(std::numeric_limits<int32_t>::min()));
	CHECK_THROWS(safe_static_cast<uint16_t>(std::numeric_limits<int16_t>::min()));
	CHECK(safe_static_cast<uint16_t>(std::numeric_limits<int16_t>::max()) == std::numeric_limits<int16_t>::max());
	CHECK_THROWS(safe_static_cast<uint16_t>(std::numeric_limits<int32_t>::max()));

	CHECK_THROWS(safe_static_cast<int16_t>(std::numeric_limits<int32_t>::min()));
	CHECK(safe_static_cast<int16_t>(std::numeric_limits<int16_t>::min()) == std::numeric_limits<int16_t>::min());
	CHECK(safe_static_cast<int16_t>(std::numeric_limits<int16_t>::max()) == std::numeric_limits<int16_t>::max());
	CHECK_THROWS(safe_static_cast<int16_t>(std::numeric_limits<int32_t>::max()));
}

TEST_CASE("safe_static_cast from unsigned int", "[core][memory]")
{
	CHECK(safe_static_cast<uint32_t>(0U) == 0);
	CHECK(safe_static_cast<uint32_t>(std::numeric_limits<uint16_t>::max()) == std::numeric_limits<uint16_t>::max());
	CHECK(safe_static_cast<uint32_t>(std::numeric_limits<uint32_t>::max()) == std::numeric_limits<uint32_t>::max());

	CHECK(safe_static_cast<int32_t>(0U) == 0);
	CHECK(safe_static_cast<int32_t>(std::numeric_limits<uint16_t>::max()) == std::numeric_limits<uint16_t>::max());
	CHECK_THROWS(safe_static_cast<int32_t>(std::numeric_limits<uint32_t>::max()));

	CHECK(safe_static_cast<uint16_t>(0U) == 0);
	CHECK(safe_static_cast<uint16_t>(std::numeric_limits<uint16_t>::max()) == std::numeric_limits<uint16_t>::max());
	CHECK_THROWS(safe_static_cast<uint16_t>(std::numeric_limits<uint32_t>::max()));

	CHECK(safe_static_cast<int16_t>(0U) == 0);
	CHECK_THROWS(safe_static_cast<int16_t>(std::numeric_limits<uint16_t>::max()));
	CHECK_THROWS(safe_static_cast<int16_t>(std::numeric_limits<uint32_t>::max()));
}

TEST_CASE("safe_static_cast from double", "[core][memory]")
{
	// Use volatile to avoid a constant being truncated warning
	volatile double value = -std::numeric_limits<double>::max();
	CHECK_THROWS(safe_static_cast<float>(value));
	value = std::numeric_limits<double>::max();
	CHECK_THROWS(safe_static_cast<float>(value));

	CHECK(safe_static_cast<float>(-std::numeric_limits<float>::max()) == -std::numeric_limits<float>::max());
	CHECK(safe_static_cast<float>(std::numeric_limits<float>::max()) == std::numeric_limits<float>::max());
}
