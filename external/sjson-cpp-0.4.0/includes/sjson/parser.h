#pragma once

////////////////////////////////////////////////////////////////////////////////
// The MIT License (MIT)
//
// Copyright (c) 2017 Nicholas Frechette, Cody Jones, and sjson-cpp contributors
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

// This define allows external libraries using sjson-cpp to detect if it has already be included as a dependency
#if !defined(SJSON_CPP_PARSER)
#define SJSON_CPP_PARSER
#endif

#include "parser_error.h"
#include "parser_state.h"
#include "string_view.h"

#include <cctype>
#include <cmath>
#include <cstring>
#include <cstdint>
#include <algorithm>

#if defined(__ANDROID__)
	#include <stdlib.h>
#else
	#include <cstdlib>
#endif

namespace sjson
{
	namespace impl
	{
		inline unsigned long long int strtoull(const char* str, char** endptr, int base)
		{
#if defined(__ANDROID__)
			return ::strtoull(str, endptr, base);
#else
			return std::strtoull(str, endptr, base);
#endif
		}

		inline long long int strtoll(const char* str, char** endptr, int base)
		{
#if defined(__ANDROID__)
			return ::strtoll(str, endptr, base);
#else
			return std::strtoll(str, endptr, base);
#endif
		}

		inline float strtof(const char* str, char** endptr)
		{
#if defined(__ANDROID__)
			return ::strtof(str, endptr);
#else
			return std::strtof(str, endptr);
#endif
		}
	}

	class Parser
	{
	public:
		Parser(const char* input, size_t input_length)
			: m_input(input)
			, m_input_length(input_length)
			, m_state(input, input_length)
		{
			skip_bom();
		}

		// Prevent copying to reduce to avoid potential mistakes
		Parser(const Parser& other) = delete;
		Parser& operator=(const Parser& other) = delete;

		Parser(Parser&& other)
			: m_input(other.m_input)
			, m_input_length(other.m_input_length)
			, m_state(other.m_state)
		{}

		Parser& operator=(Parser&& other)
		{
			m_input = other.m_input;
			m_input_length = other.m_input_length;
			m_state = other.m_state;

			return *this;
		}

		bool object_begins() { return read_opening_brace(); }
		bool object_begins(const char* having_name) { return read_key(having_name) && read_equal_sign() && object_begins(); }
		bool object_ends() { return read_closing_brace(); }

		bool try_object_begins(const char* having_name)
		{
			ParserState s = save_state();

			if (!object_begins(having_name))
			{
				restore_state(s);
				return false;
			}

			return true;
		}

		bool try_object_ends()
		{
			ParserState s = save_state();

			if (!object_ends())
			{
				restore_state(s);
				return false;
			}

			return true;
		}

		bool array_begins() { return read_opening_bracket(); }
		bool array_begins(const char* having_name) { return read_key(having_name) && read_equal_sign() && read_opening_bracket(); }
		bool array_ends() { return read_closing_bracket(); }

		bool try_array_begins(const char* having_name)
		{
			ParserState s = save_state();

			if (!array_begins(having_name))
			{
				restore_state(s);
				return false;
			}

			return true;
		}

		bool try_array_ends()
		{
			ParserState s = save_state();

			if (!array_ends())
			{
				restore_state(s);
				return false;
			}

			return true;
		}

		// TODO: To support 'null' value entries (e.g. foo = null), these functions should take as an argument an Option
		// e.g. bool read(const char* key, Option<bool>& value)
		// The return value tells us whether or not we successfully parsed our key/value pair
		// The option returned tells us whether we parsed an actual value or a null literal
		// The caller is responsible for interpreting the meaning of this
		// TODO: These should all be renamed to try_read since on failure they restore the state prior to the parsing attempt
		// TODO: Return the actual parsing error instead of a bool, maybe introduce an ErrorType that coerces to bool, true == success?
		// TODO: Rename this for a cleaner API, try_read_string, try_read_bool, try_read_double, try_read_integer, etc

		// The StringView returned is a raw view of the SJSON buffer. Anything escaped within is thus returned as-is.
		// Only the start/end quote is stripped from the returned string.
		// e.g.: some_key = "this is an \"escaped\" string within another string"
		//  StringView start ^                                             end ^
		bool read(const char* key, StringView& value) { return read_key(key) && read_equal_sign() && read_string(value); }
		bool read(const char* key, bool& value) { return read_key(key) && read_equal_sign() && read_bool(value); }
		bool read(const char* key, double& value) { return read_key(key) && read_equal_sign() && read_double(&value, nullptr); }
		bool read(const char* key, float& value) { return read_key(key) && read_equal_sign() && read_double(nullptr, &value); }
		bool read(const char* key, int8_t& value) { return read_key(key) && read_equal_sign() && read_integer(value); }
		bool read(const char* key, uint8_t& value) { return read_key(key) && read_equal_sign() && read_integer(value); }
		bool read(const char* key, int16_t& value) { return read_key(key) && read_equal_sign() && read_integer(value); }
		bool read(const char* key, uint16_t& value) { return read_key(key) && read_equal_sign() && read_integer(value); }
		bool read(const char* key, int32_t& value) { return read_key(key) && read_equal_sign() && read_integer(value); }
		bool read(const char* key, uint32_t& value) { return read_key(key) && read_equal_sign() && read_integer(value); }
		bool read(const char* key, int64_t& value) { return read_key(key) && read_equal_sign() && read_integer(value); }
		bool read(const char* key, uint64_t& value) { return read_key(key) && read_equal_sign() && read_integer(value); }

		bool read(const char* key, double* values, uint32_t num_elements)
		{
			return read_key(key) && read_equal_sign() && read_opening_bracket() && read(values, num_elements) && read_closing_bracket();
		}

		bool read(const char* key, StringView* values, uint32_t num_elements)
		{
			return read_key(key) && read_equal_sign() && read_opening_bracket() && read(values, num_elements) && read_closing_bracket();
		}

		bool try_read(const char* key, StringView& value, const char* default_value)
		{
			ParserState s = save_state();

			if (read_key(key) && read_equal_sign())
			{
				if (try_read_null())
				{
					value = default_value;
					return false;
				}

				if (read_string(value))
					return true;
			}

			restore_state(s);
			value = default_value;
			return false;
		}

		bool try_read(const char* key, bool& value, bool default_value)
		{
			ParserState s = save_state();

			if (read_key(key) && read_equal_sign())
			{
				if (try_read_null())
				{
					value = default_value;
					return false;
				}

				if (read_bool(value))
					return true;
			}

			restore_state(s);
			value = default_value;
			return false;
		}

		bool try_read(const char* key, double& value, double default_value)
		{
			ParserState s = save_state();

			if (read_key(key) && read_equal_sign())
			{
				if (try_read_null())
				{
					value = default_value;
					return false;
				}

				if (read_double(&value, nullptr))
					return true;
			}

			restore_state(s);
			value = default_value;
			return false;
		}

		bool try_read(const char* key, float& value, float default_value)
		{
			ParserState s = save_state();

			if (read_key(key) && read_equal_sign())
			{
				if (try_read_null())
				{
					value = default_value;
					return false;
				}

				if (read_double(nullptr, &value))
					return true;
			}

			restore_state(s);
			value = default_value;
			return false;
		}

		bool try_read(const char* key, double* values, uint32_t num_elements, double default_value)
		{
			ParserState s = save_state();

			if (read_key(key) && read_equal_sign())
			{
				if (try_read_null())
				{
					std::fill(values, values + num_elements, default_value);
					return false;
				}

				if (read_opening_bracket() && read(values, num_elements) && read_closing_bracket())
					return true;
			}

			restore_state(s);
			std::fill(values, values + num_elements, default_value);
			return false;
		}

		bool try_read(const char* key, StringView* values, uint32_t num_elements, const char* default_value)
		{
			ParserState s = save_state();

			if (read_key(key) && read_equal_sign())
			{
				if (try_read_null())
				{
					std::fill(values, values + num_elements, default_value);
					return false;
				}

				if (read_opening_bracket() && read(values, num_elements) && read_closing_bracket())
					return true;
			}

			restore_state(s);
			std::fill(values, values + num_elements, default_value);
			return false;
		}

		bool read(double* values, uint32_t num_elements)
		{
			if (num_elements == 0)
				return true;

			for (uint32_t i = 0; i < num_elements; ++i)
			{
				if (!read_double(&values[i], nullptr))
					return false;

				if (i < (num_elements - 1) && !read_comma())
					return false;
			}

			return true;
		}

		bool read(StringView* values, uint32_t num_elements)
		{
			if (num_elements == 0)
				return true;

			for (uint32_t i = 0; i < num_elements; ++i)
			{
				if (!read_string(values[i]))
					return false;

				if (i < (num_elements - 1) && !read_comma())
					return false;
			}

			return true;
		}

		bool remainder_is_comments_and_whitespace()
		{
			if (!skip_comments_and_whitespace())
				return false;

			if (!eof())
			{
				set_error(ParserError::UnexpectedContentAtEnd);
				return false;
			}

			return true;
		}

		bool skip_comments_and_whitespace()
		{
			while (true)
			{
				if (eof())
					return true;

				if (std::isspace(m_state.symbol))
				{
					advance();
					continue;
				}

				if (m_state.symbol == '/')
				{
					advance();

					if (!read_comment())
						return false;

					continue;
				}

				return true;
			}
		}

		void get_position(uint32_t& line, uint32_t& column)
		{
			line = m_state.line;
			column = m_state.column;
		}

		bool eof() { return m_state.offset >= m_input_length; }

		ParserError get_error() { return m_state.error; }
		bool is_valid() const { return m_state.error.error == ParserError::None; }

		ParserState save_state() const { return m_state; }
		void restore_state(const ParserState& s) { m_state = s; }
		void reset_state() { m_state = ParserState(m_input, m_input_length); }

	private:
		static constexpr size_t k_max_number_length = 64;

		const char* m_input;
		size_t m_input_length;
		ParserState m_state;

		bool read_equal_sign()		{ return read_symbol('=', ParserError::EqualSignExpected); }
		bool read_opening_brace()	{ return read_symbol('{', ParserError::OpeningBraceExpected); }
		bool read_closing_brace()	{ return read_symbol('}', ParserError::ClosingBraceExpected); }
		bool read_opening_bracket()	{ return read_symbol('[', ParserError::OpeningBracketExpected); }
		bool read_closing_bracket()	{ return read_symbol(']', ParserError::ClosingBracketExpected); }
		bool read_comma()			{ return read_symbol(',', ParserError::CommaExpected); }

		bool read_symbol(char expected, int32_t reason_if_other_found)
		{
			if (!skip_comments_and_whitespace_fail_if_eof())
				return false;

			if (m_state.symbol == expected)
			{
				advance();
				return true;
			}

			set_error(reason_if_other_found);
			return false;
		}

		// This function assumes that the first '/' character has already been consumed
		// e.g.:      //        or   /*
		// symbol:     ^              ^
		// TODO: Fix this
		bool read_comment()
		{
			if (eof())
			{
				set_error(ParserError::InputTruncated);
				return false;
			}

			if (m_state.symbol == '/')
			{
				while (!eof() && m_state.symbol != '\n')
					advance();

				return true;
			}
			else if (m_state.symbol == '*')
			{
				advance();

				bool wasAsterisk = false;

				while (true)
				{
					if (eof())
					{
						set_error(ParserError::InputTruncated);
						return false;
					}
					else if (m_state.symbol == '*')
					{
						advance();
						wasAsterisk = true;
					}
					else if (wasAsterisk && m_state.symbol == '/')
					{
						advance();
						return true;
					}
					else
					{
						advance();
						wasAsterisk = false;
					}
				}
			}
			else
			{
				set_error(ParserError::CommentBeginsIncorrectly);
				return false;
			}
		}

		bool read_key(const char* having_name)
		{
			if (!skip_comments_and_whitespace_fail_if_eof())
				return false;

			ParserState start_of_key = save_state();
			StringView actual;

			if (m_state.symbol == '"')
			{
				if (!read_string(actual))
					return false;
			}
			else
			{
				if (!read_unquoted_key(actual))
					return false;
			}

			if (actual != having_name)
			{
				restore_state(start_of_key);
				set_error(ParserError::IncorrectKey);
				return false;
			}

			return true;
		}

		// The StringView value returned is a raw view of the SJSON buffer. Nothing is unescaped:
		// escaped quotation marks will remain, escaped unicode sequences will remain, etc.
		// It is the responsibility of the caller to handle this in a meaningful way.
		bool read_string(StringView& value)
		{
			if (!skip_comments_and_whitespace_fail_if_eof())
				return false;

			if (m_state.symbol != '"')
			{
				set_error(ParserError::QuotationMarkExpected);
				return false;
			}

			advance();

			size_t start_offset = m_state.offset;
			size_t end_offset;

			while (true)
			{
				if (eof())
				{
					set_error(ParserError::InputTruncated);
					return false;
				}

				if (m_state.symbol == '"')
				{
					end_offset = m_state.offset - 1;
					advance();
					break;
				}

				if (m_state.symbol == '\\')
				{
					// Strings are returned as slices of the input, so escape sequences cannot be un-escaped.
					// Assume the escape sequence is valid and skip over it.
					advance();

					if (m_state.symbol == 'u')
					{
						advance();

						// This is an escaped unicode character, skip the 4 bytes that follow
						advance();
						advance();
						advance();
						advance();
					}
					else
					{
						advance();
					}
				}
				else
				{
					advance();
				}
			}

			value = StringView(m_input + start_offset, end_offset - start_offset + 1);
			return true;
		}

		// Unquoted keys do not support escaped unicode literals or any form of escaping
		// e.g. foo_\u0066_bar = "this is an invalid key"
		bool read_unquoted_key(StringView& value)
		{
			if (eof())
			{
				set_error(ParserError::InputTruncated);
				return false;
			}

			size_t start_offset = m_state.offset;
			size_t end_offset;

			while (true)
			{
				if (eof())
				{
					set_error(ParserError::InputTruncated);
					return false;
				}

				if (m_state.symbol == '"')
				{
					set_error(ParserError::CannotUseQuotationMarkInUnquotedString);
					return false;
				}

				if (m_state.symbol == '=')
				{
					if (m_state.offset == start_offset)
					{
						set_error(ParserError::KeyExpected);
						return false;
					}

					end_offset = m_state.offset - 1;
					break;
				}

				if (std::isspace(m_state.symbol))
				{
					end_offset = m_state.offset - 1;
					advance();
					break;
				}

				advance();
			}

			value = StringView(m_input + start_offset, end_offset - start_offset + 1);
			return true;
		}

		bool read_bool(bool& value)
		{
			if (!skip_comments_and_whitespace_fail_if_eof())
				return false;

			ParserState start_of_literal = save_state();

			if (m_state.symbol == 't')
			{
				advance();

				if (m_state.symbol == 'r' && advance() &&
					m_state.symbol == 'u' && advance() &&
					m_state.symbol == 'e' && advance())
				{
					value = true;
					return true;
				}
			}
			else if (m_state.symbol == 'f')
			{
				advance();

				if (m_state.symbol == 'a' && advance() &&
					m_state.symbol == 'l' && advance() &&
					m_state.symbol == 's' && advance() &&
					m_state.symbol == 'e' && advance())
				{
					value = false;
					return true;
				}
			}

			restore_state(start_of_literal);
			set_error(ParserError::TrueOrFalseExpected);
			return false;
		}

		bool read_double(double* dbl_value, float* flt_value)
		{
			if (!skip_comments_and_whitespace_fail_if_eof())
				return false;

			size_t start_offset = m_state.offset;
			size_t end_offset;

			if (m_state.symbol == '-')
				advance();

			if (m_state.symbol == '0')
			{
				advance();
			}
			else if (std::isdigit(m_state.symbol))
			{
				while (std::isdigit(m_state.symbol))
					advance();
			}
			else
			{
				set_error(ParserError::NumberExpected);
				return false;
			}

			if (m_state.symbol == '.')
			{
				advance();

				while (std::isdigit(m_state.symbol))
					advance();
			}

			if (m_state.symbol == 'e' || m_state.symbol == 'E')
			{
				advance();

				if (m_state.symbol == '+' || m_state.symbol == '-')
				{
					advance();
				}
				else if (!std::isdigit(m_state.symbol))
				{
					set_error(ParserError::InvalidNumber);
					return false;
				}

				while (std::isdigit(m_state.symbol))
					advance();
			}

			end_offset = m_state.offset - 1;
			size_t length = end_offset - start_offset + 1;

			char slice[k_max_number_length + 1];
			if (length >= k_max_number_length)
			{
				set_error(ParserError::NumberIsTooLong);
				return false;
			}

			std::memcpy(slice, m_input + start_offset, length);
			slice[length] = '\0';

			char* last_used_symbol = nullptr;
			if (dbl_value != nullptr)
				*dbl_value = std::strtod(slice, &last_used_symbol);
			else
				*flt_value = impl::strtof(slice, &last_used_symbol);

			if (last_used_symbol != slice + length)
			{
				set_error(ParserError::NumberCouldNotBeConverted);
				return false;
			}

			return true;
		}

		template<typename IntegralType>
		bool read_integer(IntegralType& value)
		{
			if (!skip_comments_and_whitespace_fail_if_eof())
				return false;

			size_t start_offset = m_state.offset;
			size_t end_offset;
			int base = 10;

			if (m_state.symbol == '-')
				advance();

			if (m_state.symbol == '0')
			{
				advance();

				if (m_state.symbol == 'x' || m_state.symbol == 'X')
				{
					advance();
					base = 16;

					while (is_hex_digit(m_state.symbol))
						advance();
				}
				else
				{
					base = 8;

					while (std::isdigit(m_state.symbol))
						advance();
				}
			}
			else if (std::isdigit(m_state.symbol))
			{
				while (std::isdigit(m_state.symbol))
					advance();
			}
			else
			{
				set_error(ParserError::NumberExpected);
				return false;
			}

			if (m_state.symbol == '.')
			{
				set_error(ParserError::NumberExpected);
				return false;
			}

			end_offset = m_state.offset - 1;
			size_t length = end_offset - start_offset + 1;

			char slice[k_max_number_length + 1];
			if (length >= k_max_number_length)
			{
				set_error(ParserError::NumberIsTooLong);
				return false;
			}

			std::memcpy(slice, m_input + start_offset, length);
			slice[length] = '\0';

			char* last_used_symbol = nullptr;
			if (std::is_unsigned<IntegralType>::value)
			{
				const uint64_t raw_value = impl::strtoull(slice, &last_used_symbol, base);
				value = static_cast<IntegralType>(raw_value);

				if (static_cast<uint64_t>(value) != raw_value)
				{
					set_error(ParserError::NumberCouldNotBeConverted);
					return false;
				}
			}
			else
			{
				const int64_t raw_value = impl::strtoll(slice, &last_used_symbol, base);
				value = static_cast<IntegralType>(raw_value);

				if (static_cast<int64_t>(value) != raw_value)
				{
					set_error(ParserError::NumberCouldNotBeConverted);
					return false;
				}
			}

			if (last_used_symbol != slice + length)
			{
				set_error(ParserError::NumberCouldNotBeConverted);
				return false;
			}

			return true;
		}

		static bool is_hex_digit(char value)
		{
			return std::isdigit(value)
				|| value == 'a' || value == 'A'
				|| value == 'b' || value == 'B'
				|| value == 'c' || value == 'C'
				|| value == 'd' || value == 'D'
				|| value == 'e' || value == 'E'
				|| value == 'f' || value == 'F';
		}

		// Attempts to read a 'null' literal.
		// Returns true on success and the state is advanced otherwise
		// the state remains unchanged and the function returns false.
		bool try_read_null()
		{
			ParserState old_state = save_state();

			skip_comments_and_whitespace();

			if (m_state.symbol == 'n')
			{
				advance();

				if (m_state.symbol == 'u' && advance() &&
					m_state.symbol == 'l' && advance() &&
					m_state.symbol == 'l' && advance())
				{
					return true;
				}
			}

			restore_state(old_state);
			return false;
		}

		bool skip_comments_and_whitespace_fail_if_eof()
		{
			if (!skip_comments_and_whitespace())
				return false;

			if (eof())
			{
				set_error(ParserError::InputTruncated);
				return false;
			}

			return true;
		}

		void skip_bom()
		{
			ParserState initial_state = save_state();
			bool skipped_bom = false;

			if (m_state.symbol == char(uint8_t(0xEF)))
			{
				advance();
				if (m_state.symbol == char(uint8_t(0xBB)))
				{
					advance();
					if (m_state.symbol == char(uint8_t(0xBF)))
					{
						advance();
						skipped_bom = true;
					}
				}
			}

			if (!skipped_bom)
				restore_state(initial_state);
		}

		bool advance()
		{
			if (eof())
				return false;

			m_state.offset++;

			if (eof())
			{
				m_state.symbol = '\0';
			}
			else
			{
				m_state.symbol = m_input[m_state.offset];

				if (m_state.symbol == '\n')
				{
					++m_state.line;
					m_state.column = 1;
				}
				else
				{
					m_state.column++;
				}
			}

			return true;
		}

		void set_error(int32_t error)
		{
			m_state.error.error = error;
			m_state.error.line = m_state.line;
			m_state.error.column = m_state.column;
		}
	};
}
