#include "test_vector4_impl.h"

TEST_CASE("vector4 64 math", "[math][vector4]")
{
	test_vector4_impl<Vector4_64, Quat_64, double>(vector_zero_64(), quat_identity_64(), 1.0e-9);

	const Vector4_64 src = vector_set(-2.65, 2.996113, 0.68123521, -5.9182);
	const Vector4_32 dst = vector_cast(src);
	REQUIRE(scalar_near_equal(vector_get_x(dst), -2.65f, 1.0e-6f));
	REQUIRE(scalar_near_equal(vector_get_y(dst), 2.996113f, 1.0e-6f));
	REQUIRE(scalar_near_equal(vector_get_z(dst), 0.68123521f, 1.0e-6f));
	REQUIRE(scalar_near_equal(vector_get_w(dst), -5.9182f, 1.0e-6f));
}
