#pragma once

#include <string>

//TODO #include "acl/memory.h"
#include "memory.h"

namespace acl
{
	class Variant
	{
	public:
		Variant() : m_type(UNDEFINED), m_is_null(true)
		{
		}

		Variant(std::string s) : m_type(STRING), m_string(s)
		{
		}

		Variant(bool b) : m_type(BOOL), m_bool(b)
		{
		}

		Variant(int i) : m_type(INT), m_int(i)
		{
		}

		Variant(double d) : m_type(DOUBLE), m_double(d)
		{
		}

		static Variant* New(Allocator &a)
		{
			auto v = allocate_type<Variant>(m_allocator);
			return new(v) Variant();
		}

		static Variant* New(Allocator &a, std::string s)
		{
			auto v = allocate_type<Variant>(m_allocator);
			return new(v) Variant(s);
		}

		static Variant* New(Allocator &a, bool b)
		{
			auto v = allocate_type<Variant>(m_allocator);
			return new(v) Variant(b);
		}

		static Variant* New(Allocator &a, int i)
		{
			auto v = allocate_type<Variant>(m_allocator);
			return new(v) Variant(i);
		}

		static Variant* New(Allocator &a, double d)
		{
			auto v = allocate_type<Variant>(m_allocator);
			return new(v) Variant(d);
		}

		bool IsNull()
		{
			return m_is_null;
		}

		std::string TryString(bool &succeeded)
		{
			if (m_type == STRING)
			{
				succeeded = true;
				return m_string;
			}

			succeeded = false;
			return "";
		}

		bool TryBool(bool &succeeded)
		{
			if (m_type == BOOL)
			{
				succeeded = true;
				return m_bool;
			}

			succeeded = false;
			return false;
		}

		int TryInt(bool &succeeded)
		{
			if (m_type == INT)
			{
				succeeded = true;
				return m_int;
			}

			succeeded = false;
			return 0;
		}

		double TryDouble(bool &succeeded)
		{
			if (m_type == DOUBLE)
			{
				succeeded = true;
				return m_double;
			}

			succeeded = false;
			return 0;
		}

	private:
		enum Type
		{
			UNDEFINED,
			STRING,
			BOOL,
			INT,
			DOUBLE
		};

		Type m_type{};
		bool m_is_null{};
		std::string m_string{};
		bool m_bool{};
		int m_int{};
		double m_double{};
	};
}