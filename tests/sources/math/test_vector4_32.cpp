#include "test_vector4_impl.h"

TEST_CASE("vector4 32 math", "[math][vector4]")
{
	test_vector4_impl<Vector4_32, Quat_32, float>(vector_zero_32(), quat_identity_32(), 1.0e-6f);

	const Vector4_32 src = vector_set(-2.65f, 2.996113f, 0.68123521f, -5.9182f);
	const Vector4_64 dst = vector_cast(src);
	REQUIRE(scalar_near_equal(vector_get_x(dst), -2.65, 1.0e-6));
	REQUIRE(scalar_near_equal(vector_get_y(dst), 2.996113, 1.0e-6));
	REQUIRE(scalar_near_equal(vector_get_z(dst), 0.68123521, 1.0e-6));
	REQUIRE(scalar_near_equal(vector_get_w(dst), -5.9182, 1.0e-6));
}
