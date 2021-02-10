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

#include <acl/math/quat_packing.h>

using namespace acl;
using namespace rtm;

TEST_CASE("quat packing math", "[math][quat][packing]")
{
	struct UnalignedBuffer
	{
		uint32_t padding0;
		uint16_t padding1;
		uint8_t buffer[250];
	};
	static_assert((offsetof(UnalignedBuffer, buffer) % 2) == 0, "Minimum packing alignment is 2");

	const quatf quat0 = quat_set(0.39564531008956383F, 0.044254239301713752F, 0.22768840967675355F, 0.88863059760894492F);

	{
		UnalignedBuffer tmp0;
		pack_quat_128(quat0, &tmp0.buffer[0]);
		quatf quat1 = unpack_quat_128(&tmp0.buffer[0]);
		CHECK((float)quat_get_x(quat0) == (float)quat_get_x(quat1));
		CHECK((float)quat_get_y(quat0) == (float)quat_get_y(quat1));
		CHECK((float)quat_get_z(quat0) == (float)quat_get_z(quat1));
		CHECK((float)quat_get_w(quat0) == (float)quat_get_w(quat1));
	}

	{
		UnalignedBuffer tmp0;
		pack_quat_96(quat0, &tmp0.buffer[0]);
		quatf quat1 = unpack_quat_96_unsafe(&tmp0.buffer[0]);
		CHECK((float)quat_get_x(quat0) == (float)quat_get_x(quat1));
		CHECK((float)quat_get_y(quat0) == (float)quat_get_y(quat1));
		CHECK((float)quat_get_z(quat0) == (float)quat_get_z(quat1));
		CHECK(scalar_near_equal((float)quat_get_w(quat0), (float)quat_get_w(quat1), 1.0E-4F));
	}

	{
		UnalignedBuffer tmp0;
		pack_quat_48(quat0, &tmp0.buffer[0]);
		quatf quat1 = unpack_quat_48(&tmp0.buffer[0]);
		CHECK(scalar_near_equal((float)quat_get_x(quat0), (float)quat_get_x(quat1), 1.0E-4F));
		CHECK(scalar_near_equal((float)quat_get_y(quat0), (float)quat_get_y(quat1), 1.0E-4F));
		CHECK(scalar_near_equal((float)quat_get_z(quat0), (float)quat_get_z(quat1), 1.0E-4F));
		CHECK(scalar_near_equal((float)quat_get_w(quat0), (float)quat_get_w(quat1), 1.0E-4F));
	}

	{
		UnalignedBuffer tmp0;
		pack_quat_32(quat0, &tmp0.buffer[0]);
		quatf quat1 = unpack_quat_32(&tmp0.buffer[0]);
		CHECK(scalar_near_equal((float)quat_get_x(quat0), (float)quat_get_x(quat1), 1.0E-3F));
		CHECK(scalar_near_equal((float)quat_get_y(quat0), (float)quat_get_y(quat1), 1.0E-3F));
		CHECK(scalar_near_equal((float)quat_get_z(quat0), (float)quat_get_z(quat1), 1.0E-3F));
		CHECK(scalar_near_equal((float)quat_get_w(quat0), (float)quat_get_w(quat1), 1.0E-3F));
	}

	CHECK(get_packed_rotation_size(rotation_format8::quatf_full) == 16);
	CHECK(get_packed_rotation_size(rotation_format8::quatf_drop_w_full) == 12);

	CHECK(get_range_reduction_rotation_size(rotation_format8::quatf_full) == 32);
	CHECK(get_range_reduction_rotation_size(rotation_format8::quatf_drop_w_full) == 24);
	CHECK(get_range_reduction_rotation_size(rotation_format8::quatf_drop_w_variable) == 24);
}
