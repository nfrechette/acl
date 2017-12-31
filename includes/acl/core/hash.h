#pragma once

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

#include <stdint.h>
#include <type_traits>

namespace acl
{
	namespace hash_impl
	{
		template <typename ResultType, ResultType OffsetBasis, ResultType Prime>
		class fnv1a_impl final
		{
		public:
			constexpr fnv1a_impl() noexcept
				: m_state(OffsetBasis)
			{}

			void update(const void* data, size_t size) noexcept
			{
				const uint8_t* cdata = static_cast<const uint8_t*>(data);
				ResultType acc = m_state;
				for (size_t i = 0; i < size; ++i)
				{
					const ResultType next = cdata[i];
					acc = (acc ^ next) * Prime;
				}
				m_state = acc;
			}

			constexpr ResultType digest() const noexcept { return m_state; }

		private:
			static_assert(std::is_unsigned<ResultType>::value, "need unsigned integer");

			ResultType m_state;
		};
	}

	using fnv1a_32 = hash_impl::fnv1a_impl<uint32_t, 2166136261u, 16777619u>;

	using fnv1a_64 = hash_impl::fnv1a_impl<uint64_t, 14695981039346656037ull, 1099511628211ull>;

	inline uint32_t hash32(const void* buffer, size_t buffer_size)
	{
		fnv1a_32 hashfn = fnv1a_32();
		hashfn.update(buffer, buffer_size);
		return hashfn.digest();
	}

	template<typename ElementType>
	inline uint32_t hash32(const ElementType& element) { return hash32(&element, sizeof(ElementType)); }

	inline uint32_t hash32(const char* str) { return hash32(str, std::strlen(str)); }

	inline uint64_t hash64(const void* buffer, size_t buffer_size)
	{
		fnv1a_64 hashfn = fnv1a_64();
		hashfn.update(buffer, buffer_size);
		return hashfn.digest();
	}

	template<typename ElementType>
	inline uint64_t hash64(const ElementType& element) { return hash64(&element, sizeof(ElementType)); }

	inline uint64_t hash64(const char* str) { return hash64(str, std::strlen(str)); }

	inline uint32_t hash_combine(uint32_t hash_a, uint32_t hash_b) { return (hash_a ^ hash_b) * 16777619u; }
	inline uint64_t hash_combine(uint64_t hash_a, uint64_t hash_b) { return (hash_a ^ hash_b) * 1099511628211ull; }
}
