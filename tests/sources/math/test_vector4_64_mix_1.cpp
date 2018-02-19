#include "test_vector4_impl.h"

TEST_CASE("vector4 64 vector_mix<A|B|C|D,_,_,_>", "[math][vector4]")
{
	test_vector4_vector_mix_impl<Vector4_64, double, VectorMix::A>(1.0e-9);
	test_vector4_vector_mix_impl<Vector4_64, double, VectorMix::B>(1.0e-9);
	test_vector4_vector_mix_impl<Vector4_64, double, VectorMix::C>(1.0e-9);
	test_vector4_vector_mix_impl<Vector4_64, double, VectorMix::D>(1.0e-9);
}
