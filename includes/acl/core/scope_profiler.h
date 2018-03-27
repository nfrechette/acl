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

#include <chrono>
#include <cstdint>

namespace acl
{
	////////////////////////////////////////////////////////////////////////////////
	// A scope activated profiler.
	////////////////////////////////////////////////////////////////////////////////
	class ScopeProfiler
	{
	public:
		////////////////////////////////////////////////////////////////////////////////
		// Creates and starts a scope profiler and automatically starts it.
		ScopeProfiler();

		////////////////////////////////////////////////////////////////////////////////
		// Destroys a scope profiler and automatically stops it.
		~ScopeProfiler();

		////////////////////////////////////////////////////////////////////////////////
		// Manually stops the profiler.
		void stop();

		////////////////////////////////////////////////////////////////////////////////
		// Returns the elapsed time in nanoseconds since the profiler was started.
		std::chrono::nanoseconds get_elapsed_time() const { return std::chrono::duration_cast<std::chrono::nanoseconds>(m_end_time - m_start_time); }

		////////////////////////////////////////////////////////////////////////////////
		// Returns the elapsed time in milliseconds since the profiler was started.
		double get_elapsed_milliseconds() const { return std::chrono::duration<double, std::chrono::milliseconds::period>(get_elapsed_time()).count(); }

		////////////////////////////////////////////////////////////////////////////////
		// Returns the elapsed time in seconds since the profiler was started.
		double get_elapsed_seconds() const { return std::chrono::duration<double, std::chrono::seconds::period>(get_elapsed_time()).count(); }

	private:
		ScopeProfiler(const ScopeProfiler&) = delete;
		ScopeProfiler& operator=(const ScopeProfiler&) = delete;

		// The time at which the profiler started.
		std::chrono::time_point<std::chrono::high_resolution_clock>		m_start_time;

		// The time at which the profiler stopped.
		std::chrono::time_point<std::chrono::high_resolution_clock>		m_end_time;
	};

	//////////////////////////////////////////////////////////////////////////

	inline ScopeProfiler::ScopeProfiler()
	{
		m_start_time = m_end_time = std::chrono::high_resolution_clock::now();
	}

	inline ScopeProfiler::~ScopeProfiler()
	{
		// Nothing to do, really
		//stop();
	}

	inline void ScopeProfiler::stop()
	{
		if (m_start_time == m_end_time)
			m_end_time = std::chrono::high_resolution_clock::now();
	}
}
