#include "test_vector4_impl.h"

TEST_CASE("vector4 64 vector_mix<X|Y|Z|W,_,_,_>", "[math][vector4]")
{
	test_vector4_vector_mix_impl<Vector4_64, double, VectorMix::X>(1.0e-9);
	test_vector4_vector_mix_impl<Vector4_64, double, VectorMix::Y>(1.0e-9);
	test_vector4_vector_mix_impl<Vector4_64, double, VectorMix::Z>(1.0e-9);
	test_vector4_vector_mix_impl<Vector4_64, double, VectorMix::W>(1.0e-9);
}
