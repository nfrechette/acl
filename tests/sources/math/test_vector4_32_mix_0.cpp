#include "test_vector4_impl.h"

TEST_CASE("vector4 32 vector_mix<X|Y|Z|W,_,_,_>", "[math][vector4]")
{
	test_vector4_vector_mix_impl<Vector4_32, float, VectorMix::X>(1.0e-6f);
	test_vector4_vector_mix_impl<Vector4_32, float, VectorMix::Y>(1.0e-6f);
	test_vector4_vector_mix_impl<Vector4_32, float, VectorMix::Z>(1.0e-6f);
	test_vector4_vector_mix_impl<Vector4_32, float, VectorMix::W>(1.0e-6f);
}
