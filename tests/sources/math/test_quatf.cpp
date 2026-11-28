////////////////////////////////////////////////////////////////////////////////
// The MIT License (MIT)
//
// Copyright (c) 2026 Nicholas Frechette & Animation Compression Library contributors
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

#include <acl/math/quatf.h>

#include <rtm/macros.h>
#include <rtm/quatf.h>
#include <rtm/scalarf.h>
#include <rtm/vector4f.h>

#include <cstdint>

using namespace acl;

TEST_CASE("quat_mul_vector3_x4", "[math][quat]")
{
	const rtm::quatf rotations[] =
	{
		rtm::quat_identity(),
		rtm::quat_from_axis_angle(rtm::vector_set(0.0F, 0.0F, 1.0F), 1.5707963F),
		rtm::quat_from_euler(0.4F, 1.1F, -0.6F),
		rtm::quat_from_euler(-2.1F, 0.3F, 2.8F),
	};

	const rtm::vector4f vector0 = rtm::vector_set(1.0F, 0.0F, 0.0F);
	const rtm::vector4f vector1 = rtm::vector_set(0.0F, 2.0F, 0.0F);
	const rtm::vector4f vector2 = rtm::vector_set(0.0F, 0.0F, 3.0F);
	const rtm::vector4f vector3 = rtm::vector_set(4.5F, 0.6F, -0.9F);
	const rtm::vector4f vectors[4] = { vector0, vector1, vector2, vector3 };

	rtm::vector4f vectors_xxxx;
	rtm::vector4f vectors_yyyy;
	rtm::vector4f vectors_zzzz;
	RTM_MATRIXF_TRANSPOSE_4X3(vector0, vector1, vector2, vector3, vectors_xxxx, vectors_yyyy, vectors_zzzz);

	for (const rtm::quatf& rotation : rotations)
	{
		rtm::vector4f result_xxxx;
		rtm::vector4f result_yyyy;
		rtm::vector4f result_zzzz;
		acl_impl::quat_mul_vector3_x4(vectors_xxxx, vectors_yyyy, vectors_zzzz, rotation, result_xxxx, result_yyyy, result_zzzz);

		const float result_x[4] = { rtm::vector_get_x(result_xxxx), rtm::vector_get_y(result_xxxx), rtm::vector_get_z(result_xxxx), rtm::vector_get_w(result_xxxx) };
		const float result_y[4] = { rtm::vector_get_x(result_yyyy), rtm::vector_get_y(result_yyyy), rtm::vector_get_z(result_yyyy), rtm::vector_get_w(result_yyyy) };
		const float result_z[4] = { rtm::vector_get_x(result_zzzz), rtm::vector_get_y(result_zzzz), rtm::vector_get_z(result_zzzz), rtm::vector_get_w(result_zzzz) };

		for (uint32_t vector_index = 0; vector_index < 4; ++vector_index)
		{
			const rtm::vector4f expected = rtm::quat_mul_vector3(vectors[vector_index], rotation);
			CHECK(rtm::scalar_near_equal(result_x[vector_index], rtm::vector_get_x(expected), 1.0E-4F));
			CHECK(rtm::scalar_near_equal(result_y[vector_index], rtm::vector_get_y(expected), 1.0E-4F));
			CHECK(rtm::scalar_near_equal(result_z[vector_index], rtm::vector_get_z(expected), 1.0E-4F));
		}
	}
}

TEST_CASE("dropped component encoding", "[math][quat]")
{
	// W has the value 0: a rotation that drops W has the same bits as with a format that always drops W
	CHECK(acl_impl::encode_dropped_component(3) == 0);

	for (uint32_t component = 0; component < 4; ++component)
	{
		const uint32_t encoded_component = acl_impl::encode_dropped_component(component);
		CHECK(encoded_component < 4);
		CHECK(acl_impl::decode_dropped_component(encoded_component) == component);
	}
}

TEST_CASE("quat_get_largest_component", "[math][quat]")
{
	CHECK(acl_impl::quat_get_largest_component(rtm::quat_identity()) == 3);
	CHECK(acl_impl::quat_get_largest_component(rtm::quat_set(-0.9F, 0.1F, 0.2F, 0.3F)) == 0);
	CHECK(acl_impl::quat_get_largest_component(rtm::quat_set(0.1F, 0.8F, -0.2F, 0.3F)) == 1);
	CHECK(acl_impl::quat_get_largest_component(rtm::quat_set(0.1F, 0.2F, -0.8F, 0.3F)) == 2);

	// W wins a tie
	CHECK(acl_impl::quat_get_largest_component(rtm::quat_set(-0.70710677F, 0.0F, 0.0F, 0.70710677F)) == 3);
}

namespace
{
	// Returns true if both rotations are equal, or if one is the negation of the other
	bool are_same_rotation(rtm::quatf_arg0 lhs, rtm::quatf_arg1 rhs, float threshold)
	{
		return rtm::quat_near_equal(lhs, rhs, threshold) || rtm::quat_near_equal(lhs, rtm::quat_neg(rhs), threshold);
	}

	const rtm::quatf k_drop_test_rotations[] =
	{
		rtm::quat_identity(),
		rtm::quat_set(0.39564531F, 0.04425424F, 0.22768841F, 0.88863060F),
		rtm::quat_set(-0.9238795F, 0.3826834F, 0.0F, 0.0F),
		rtm::quat_set(0.0F, -0.70710677F, 0.70710677F, 0.0F),
		rtm::quat_set(-4.3E-17F, 0.70710677F, -0.70710677F, 4.3E-17F),
		rtm::quat_set(-0.0607873F, 0.70208073F, -0.14128874F, -0.69528770F),
		rtm::quat_set(-0.00876705F, 0.97749585F, -0.19043812F, 0.000001F),
	};
}

TEST_CASE("quat_drop_component", "[math][quat]")
{
	for (const rtm::quatf& rotation_raw : k_drop_test_rotations)
	{
		const rtm::quatf rotation = rtm::quat_normalize(rotation_raw);

		for (uint32_t component = 0; component < 4; ++component)
		{
			const rtm::vector4f packed = acl_impl::quat_drop_component(rotation, component);

			// The dropped component is positive or zero
			CHECK(rtm::vector_get_w(packed) >= 0.0F);

			// Moving the components back gives the same rotation, maybe negated
			const rtm::quatf restored = acl_impl::quat_restore_dropped_component(rtm::vector_to_quat(packed), component);
			CHECK(are_same_rotation(restored, rotation, 0.0F));

			// The scalar function that rebuilds the dropped component gives the same rotation
			const rtm::quatf rebuilt = acl_impl::quat_restore_dropped_component(acl_impl::quat_from_positive_w_stable(packed), component);
			CHECK(are_same_rotation(rebuilt, rotation, 1.0E-3F));
		}

		// When we drop the largest component, the rebuild is precise even when W is near 0
		const uint32_t largest_component = acl_impl::quat_get_largest_component(rotation);
		const rtm::vector4f packed = acl_impl::quat_drop_component(rotation, largest_component);
		const rtm::quatf rebuilt = acl_impl::quat_restore_dropped_component(acl_impl::quat_from_positive_w_stable(packed), largest_component);
		CHECK(are_same_rotation(rebuilt, rotation, 1.0E-6F));
	}
}

TEST_CASE("quat_restore_dropped_component_x4", "[math][quat]")
{
	// Each lane drops another component
	const rtm::quatf rotations[4] =
	{
		rtm::quat_normalize(k_drop_test_rotations[1]),
		rtm::quat_normalize(k_drop_test_rotations[2]),
		rtm::quat_normalize(k_drop_test_rotations[3]),
		rtm::quat_normalize(k_drop_test_rotations[5]),
	};
	const uint32_t components[4] = { 3, 0, 1, 2 };

	rtm::vector4f packed[4];
	for (uint32_t lane = 0; lane < 4; ++lane)
		packed[lane] = acl_impl::quat_drop_component(rotations[lane], components[lane]);

	rtm::vector4f xxxx;
	rtm::vector4f yyyy;
	rtm::vector4f zzzz;
	rtm::vector4f wwww;
	RTM_MATRIXF_TRANSPOSE_4X4(packed[0], packed[1], packed[2], packed[3], xxxx, yyyy, zzzz, wwww);

	// The format byte of each sub-track: bits [0, 4] hold the bit rate, bits [5, 6] the encoded index, and bit 7 is reserved.
	// The masks must ignore the bits of the bit rate and the reserved bit.
	const uint8_t format_per_track_data[4] =
	{
		uint8_t(19 | (acl_impl::encode_dropped_component(components[0]) << 5) | 0x80),
		uint8_t(31 | (acl_impl::encode_dropped_component(components[1]) << 5)),
		uint8_t(0 | (acl_impl::encode_dropped_component(components[2]) << 5) | 0x80),
		uint8_t(7 | (acl_impl::encode_dropped_component(components[3]) << 5)),
	};

	rtm::mask4f drops_x;
	rtm::mask4f drops_y;
	rtm::mask4f drops_z;
	acl_impl::get_dropped_component_masks_x4(&format_per_track_data[0], drops_x, drops_y, drops_z);

	acl_impl::quat_restore_dropped_component_x4(xxxx, yyyy, zzzz, wwww, drops_x, drops_y, drops_z);

	rtm::vector4f restored[4];
	RTM_MATRIXF_TRANSPOSE_4X4(xxxx, yyyy, zzzz, wwww, restored[0], restored[1], restored[2], restored[3]);

	for (uint32_t lane = 0; lane < 4; ++lane)
	{
		const rtm::quatf expected = acl_impl::quat_restore_dropped_component(rtm::vector_to_quat(packed[lane]), components[lane]);
		CHECK(rtm::quat_near_equal(rtm::vector_to_quat(restored[lane]), expected, 0.0F));
	}
}

TEST_CASE("dropped component in float bits", "[math][quat]")
{
	const rtm::quatf rotation = rtm::quat_normalize(k_drop_test_rotations[5]);

	rtm::vector4f encoded[4];
	for (uint32_t component = 0; component < 4; ++component)
	{
		const rtm::vector4f packed = acl_impl::quat_drop_component(rotation, component);
		encoded[component] = acl_impl::encode_dropped_component_in_floats(packed, component);

		// W sets no bit: the floats are the same as with a format that always drops W
		if (component == 3)
			CHECK(rtm::vector_all_equal(encoded[component], packed));

		uint32_t decoded_component = ~0U;
		const rtm::vector4f decoded = acl_impl::decode_dropped_component_from_floats(encoded[component], decoded_component);
		CHECK(decoded_component == component);
		CHECK(rtm::vector_all_equal(decoded, packed));
	}

	// The SIMD version gives the same result, lane i drops component i
	rtm::vector4f xxxx;
	rtm::vector4f yyyy;
	rtm::vector4f zzzz;
	rtm::vector4f wwww;
	RTM_MATRIXF_TRANSPOSE_4X4(encoded[0], encoded[1], encoded[2], encoded[3], xxxx, yyyy, zzzz, wwww);

	rtm::mask4f drops_x;
	rtm::mask4f drops_y;
	rtm::mask4f drops_z;
	acl_impl::decode_dropped_component_from_floats_x4(xxxx, yyyy, drops_x, drops_y, drops_z);

	CHECK(rtm::mask_all_equal(drops_x, rtm::mask_set(true, false, false, false)));
	CHECK(rtm::mask_all_equal(drops_y, rtm::mask_set(false, true, false, false)));
	CHECK(rtm::mask_all_equal(drops_z, rtm::mask_set(false, false, true, false)));

	for (uint32_t lane = 0; lane < 4; ++lane)
	{
		const rtm::vector4f packed = acl_impl::quat_drop_component(rotation, lane);
		const float lane_x = rtm::vector_get_component(xxxx, rtm::component4(lane));
		const float lane_y = rtm::vector_get_component(yyyy, rtm::component4(lane));
		CHECK(lane_x == float(rtm::vector_get_x(packed)));
		CHECK(lane_y == float(rtm::vector_get_y(packed)));
	}
}
