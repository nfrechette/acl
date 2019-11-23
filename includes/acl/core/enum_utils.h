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

#include "acl/core/impl/compiler_utils.h"

#include <type_traits>

ACL_IMPL_FILE_PRAGMA_PUSH

// This macro defines common operators for manipulating bit flags

#define ACL_IMPL_ENUM_FLAGS_OPERATORS(EnumType) \
	constexpr EnumType operator|(EnumType lhs, EnumType rhs) \
	{ \
		typedef typename std::underlying_type<EnumType>::type IntegralType; \
		typedef typename std::make_unsigned<IntegralType>::type RawType; \
		return static_cast<EnumType>(static_cast<RawType>(lhs) | static_cast<RawType>(rhs)); \
	} \
	inline void operator|=(EnumType& lhs, EnumType rhs) \
	{ \
		typedef typename std::underlying_type<EnumType>::type IntegralType; \
		typedef typename std::make_unsigned<IntegralType>::type RawType; \
		lhs = static_cast<EnumType>(static_cast<RawType>(lhs) | static_cast<RawType>(rhs)); \
	} \
	constexpr EnumType operator&(EnumType lhs, EnumType rhs) \
	{ \
		typedef typename std::underlying_type<EnumType>::type IntegralType; \
		typedef typename std::make_unsigned<IntegralType>::type RawType; \
		return static_cast<EnumType>(static_cast<RawType>(lhs) & static_cast<RawType>(rhs)); \
	} \
	inline void operator&=(EnumType& lhs, EnumType rhs) \
	{ \
		typedef typename std::underlying_type<EnumType>::type IntegralType; \
		typedef typename std::make_unsigned<IntegralType>::type RawType; \
		lhs = static_cast<EnumType>(static_cast<RawType>(lhs) & static_cast<RawType>(rhs)); \
	} \
	constexpr EnumType operator^(EnumType lhs, EnumType rhs) \
	{ \
		typedef typename std::underlying_type<EnumType>::type IntegralType; \
		typedef typename std::make_unsigned<IntegralType>::type RawType; \
		return static_cast<EnumType>(static_cast<RawType>(lhs) ^ static_cast<RawType>(rhs)); \
	} \
	inline void operator^=(EnumType& lhs, EnumType rhs) \
	{ \
		typedef typename std::underlying_type<EnumType>::type IntegralType; \
		typedef typename std::make_unsigned<IntegralType>::type RawType; \
		lhs = static_cast<EnumType>(static_cast<RawType>(lhs) ^ static_cast<RawType>(rhs)); \
	} \
	constexpr EnumType operator~(EnumType rhs) \
	{ \
		typedef typename std::underlying_type<EnumType>::type IntegralType; \
		typedef typename std::make_unsigned<IntegralType>::type RawType; \
		return static_cast<EnumType>(~static_cast<RawType>(rhs)); \
	}

namespace acl
{
	////////////////////////////////////////////////////////////////////////////////
	// Returns true if any of the requested flags are set.
	template<typename EnumType>
	constexpr bool are_any_enum_flags_set(EnumType flags, EnumType flags_to_test)
	{
		typedef typename std::underlying_type<EnumType>::type IntegralType;
		return static_cast<IntegralType>(flags & flags_to_test) != 0;
	}

	////////////////////////////////////////////////////////////////////////////////
	// Returns true if all of the requested flags are set.
	template<typename EnumType>
	constexpr bool are_all_enum_flags_set(EnumType flags, EnumType flags_to_test)
	{
		typedef typename std::underlying_type<EnumType>::type IntegralType;
		return static_cast<IntegralType>(flags & flags_to_test) == static_cast<IntegralType>(flags_to_test);
	}
}

ACL_IMPL_FILE_PRAGMA_POP
