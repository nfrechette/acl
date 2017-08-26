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

#define NOMINMAX
#include <Windows.h>

#include <stdint.h>

namespace acl
{
	class ScopeProfiler
	{
	public:
		ScopeProfiler(uint64_t* output_var = nullptr);
		~ScopeProfiler() { stop(); }

		uint64_t stop();
		uint64_t get_elapsed_cycles() const { return m_end_cycles - m_start_cycles; }

	private:
		ScopeProfiler(const ScopeProfiler&) = delete;
		ScopeProfiler& operator=(const ScopeProfiler&) = delete;

		uint64_t m_start_cycles;
		uint64_t m_end_cycles;

		uint64_t* m_output_var;
	};

	inline double cycles_to_seconds(uint64_t cycles)
	{
		LARGE_INTEGER frequency_cycles_per_sec;
		QueryPerformanceFrequency(&frequency_cycles_per_sec);
		return double(cycles) / double(frequency_cycles_per_sec.QuadPart);
	}

	//////////////////////////////////////////////////////////////////////////

	inline ScopeProfiler::ScopeProfiler(uint64_t* output_var)
	{
		LARGE_INTEGER time_cycles;
		QueryPerformanceCounter(&time_cycles);
		m_start_cycles = time_cycles.QuadPart;
		m_end_cycles = m_start_cycles;
		m_output_var = output_var;
	}

	inline uint64_t ScopeProfiler::stop()
	{
		if (m_end_cycles == m_start_cycles)
		{
			LARGE_INTEGER time_cycles;
			QueryPerformanceCounter(&time_cycles);
			m_end_cycles = time_cycles.QuadPart;

			if (m_output_var != nullptr)
				*m_output_var = get_elapsed_cycles();
		}

		return m_end_cycles - m_start_cycles;
	}
}
