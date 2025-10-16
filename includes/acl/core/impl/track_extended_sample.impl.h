#pragma once

////////////////////////////////////////////////////////////////////////////////
// The MIT License (MIT)
//
// Copyright (c) 2025 Nicholas Frechette & Animation Compression Library contributors
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

// Included only once from track_extended_sample.h

#include "acl/version.h"
#include "acl/core/error.h"
#include "acl/core/sample_interpolator.h"
#include "acl/core/impl/compiler_utils.h"

#include <cstdint>

ACL_IMPL_FILE_PRAGMA_PUSH

namespace acl
{
	ACL_IMPL_VERSION_NAMESPACE_BEGIN

	template<typename sample_type_t>
	inline void track_extended_sample_t<sample_type_t>::set_constant(const sample_constant_data_t<sample_type_t>& new_data)
	{
		interpolator = sample_interpolator_t::constant;
		data.constant = new_data;
	}

	template<typename sample_type_t>
	inline const sample_constant_data_t<sample_type_t>& track_extended_sample_t<sample_type_t>::get_constant() const
	{
		ACL_ASSERT(interpolator == sample_interpolator_t::constant, "Attempt to access an extended sample as constant when it isn't");
		return data.constant;
	}

	template<typename sample_type_t>
	inline void track_extended_sample_t<sample_type_t>::set_linear(const sample_linear_data_t<sample_type_t>& new_data)
	{
		interpolator = sample_interpolator_t::linear;
		data.linear = new_data;
	}

	template<typename sample_type_t>
	inline const sample_linear_data_t<sample_type_t>& track_extended_sample_t<sample_type_t>::get_linear() const
	{
		ACL_ASSERT(interpolator == sample_interpolator_t::linear, "Attempt to access an extended sample as linear when it isn't");
		return data.linear;
	}

	template<typename sample_type_t>
	inline sample_type_t track_extended_sample_t<sample_type_t>::get_value() const
	{
		switch (interpolator)
		{
		case sample_interpolator_t::linear:
			return data.linear.value;
		case sample_interpolator_t::constant:
			return data.constant.value;
		default:
			ACL_ASSERT(false, "Invalid interpolator");
			return sample_type_t{};
		}
	}

	ACL_IMPL_VERSION_NAMESPACE_END
}

ACL_IMPL_FILE_PRAGMA_POP
