#include "test_vector4_impl.h"

TEST_CASE("vector4 32 vector_mix<A|B|C|D,_,_,_>", "[math][vector4]")
{
	test_vector4_vector_mix_impl<Vector4_32, float, VectorMix::A>(1.0e-6f);
	test_vector4_vector_mix_impl<Vector4_32, float, VectorMix::B>(1.0e-6f);
	test_vector4_vector_mix_impl<Vector4_32, float, VectorMix::C>(1.0e-6f);
	test_vector4_vector_mix_impl<Vector4_32, float, VectorMix::D>(1.0e-6f);
}
