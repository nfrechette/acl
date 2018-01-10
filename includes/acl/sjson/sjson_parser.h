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

#include "acl/core/string_view.h"
#include "acl/sjson/sjson_parser_error.h"

#include <cctype>
#include <cmath>
#include <stdint.h>

namespace acl
{
	/* SJSON is a simplified form of JSON created by Autodesk.  It is documented here:
	   http://help.autodesk.com/view/Stingray/ENU/?guid=__stingray_help_managing_content_sjson_html

	   This parser is intended to accept only pure SJSON, and it will fail if given a JSON file,
	   unlike the Autodesk Stingray parser.

	   The following are not yet supported:
	     - null literals

	     - unescaping characters within strings; the returned string view will be
		   exactly as the string appears in the JSON.
    */
	class SJSONParser
	{
	public:
		SJSONParser(const char* input, size_t input_length)
			: m_input(input)
			, m_input_length(input_length)
			, m_state(input, input_length)
		{
		}

		bool object_begins() { return read_opening_brace(); }
		bool object_begins(const char* having_name) { return read_key(having_name) && read_equal_sign() && object_begins(); }
		bool object_ends() { return read_closing_brace(); }

		bool array_begins() { return read_opening_bracket(); }
		bool array_begins(const char* having_name) { return read_key(having_name) && read_equal_sign() && read_opening_bracket(); }
		bool array_ends() { return read_closing_bracket(); }

		bool try_array_begins(const char* having_name)
		{
			State s = save_state();

			if (!array_begins(having_name))
			{
				restore_state(s);
				return false;
			}

			return true;
		}

		bool try_array_ends()
		{
			State s = save_state();

			if (!array_ends())
			{
				restore_state(s);
				return false;
			}

			return true;
		}

		bool read(const char* key, StringView& value) { return read_key(key) && read_equal_sign() && read_string(value); }
		bool read(const char* key, bool& value) { return read_key(key) && read_equal_sign() && read_bool(value); }
		bool read(const char* key, double& value) { return read_key(key) && read_equal_sign() && read_double(value); }

		bool read(const char* key, double* values, uint32_t num_elements)
		{
			return read_key(key) && read_equal_sign() && read_opening_bracket() && read(values, num_elements) && read_closing_bracket();
		}

		bool read(const char* key, StringView* values, uint32_t num_elements)
		{
			return read_key(key) && read_equal_sign() && read_opening_bracket() && read(values, num_elements) && read_closing_bracket();
		}

		bool read(double* values, uint32_t num_elements)
		{
			if (num_elements == 0)
				return true;

			for (uint32_t i = 0; i < num_elements; ++i)
			{
				if (!read_double(values[i]) || (i < (num_elements - 1) && !read_comma()))
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
				if (!read_string(values[i]) || (i < (num_elements - 1) && !read_comma()))
					return false;
			}

			return true;
		}

		bool try_read(const char* key, StringView& value)
		{
			State s = save_state();

			if (!read(key, value))
			{
				restore_state(s);
				value = nullptr;
				return false;
			}

			return true;
		}

		bool try_read(const char* key, double* values, uint32_t num_elements)
		{
			State s = save_state();

			if (!read(key, values, num_elements))
			{
				restore_state(s);

				for (uint32_t i = 0; i < num_elements; ++i)
					values[i] = 0.0;

				return false;
			}

			return true;
		}

		bool try_read(const char* key, StringView* values, uint32_t num_elements)
		{
			State s = save_state();

			if (!read(key, values, num_elements))
			{
				restore_state(s);

				for (uint32_t i = 0; i < num_elements; ++i)
					values[i] = StringView();

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
				set_error(SJSONParserError::UnexpectedContentAtEnd);
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

		SJSONParserError get_error() { return m_state.error; }

		struct State
		{
			State(const char* input, size_t input_length)
				: offset()
				, line(1)
				, column(1)
				, symbol(input_length > 0 ? input[0] : '\0')
				, error()
			{
			}

			size_t offset;
			uint32_t line;
			uint32_t column;
			char symbol;

			SJSONParserError error;
		};

		State save_state() const { return m_state; }
		void restore_state(const State& s) { m_state = s; }
		void reset_state() { m_state = State(m_input, m_input_length); }

	private:
		static constexpr size_t k_max_number_length = 64;

		const char* m_input;
		const size_t m_input_length;
		State m_state;

		bool read_equal_sign()      { return read_symbol('=', SJSONParserError::EqualSignExpected); }
		bool read_opening_brace()   { return read_symbol('{', SJSONParserError::OpeningBraceExpected); }
		bool read_closing_brace()   { return read_symbol('}', SJSONParserError::ClosingBraceExpected); }
		bool read_opening_bracket() { return read_symbol('[', SJSONParserError::OpeningBracketExpected); }
		bool read_closing_bracket() { return read_symbol(']', SJSONParserError::ClosingBracketExpected); }
		bool read_comma()		    { return read_symbol(',', SJSONParserError::CommaExpected); }

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

		bool read_comment()
		{
			if (eof())
			{
				set_error(SJSONParserError::InputTruncated);
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
						set_error(SJSONParserError::InputTruncated);
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
				set_error(SJSONParserError::CommentBeginsIncorrectly);
				return false;
			}
		}

		bool read_key(const char* having_name)
		{
			if (!skip_comments_and_whitespace_fail_if_eof())
				return false;

			State start_of_key = save_state();
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
				set_error(SJSONParserError::IncorrectKey);
				return false;
			}

			return true;
		}

		bool read_string(StringView& value)
		{
			if (!skip_comments_and_whitespace_fail_if_eof())
				return false;

			if (m_state.symbol != '"')
			{
				set_error(SJSONParserError::QuotationMarkExpected);
				return false;
			}

			advance();

			size_t start_offset = m_state.offset;
			size_t end_offset;

			while (true)
			{
				if (eof())
				{
					set_error(SJSONParserError::InputTruncated);
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
				}

				advance();
			}

			value = StringView(m_input + start_offset, end_offset - start_offset + 1);
			return true;
		}

		bool read_unquoted_key(StringView& value)
		{
			if (eof())
			{
				set_error(SJSONParserError::InputTruncated);
				return false;
			}

			size_t start_offset = m_state.offset;
			size_t end_offset;

			while (true)
			{
				if (eof())
				{
					end_offset = m_state.offset - 1;
					break;
				}

				if (m_state.symbol == '"')
				{
					set_error(SJSONParserError::CannotUseQuotationMarkInUnquotedString);
					return false;
				}

				if (m_state.symbol == '=')
				{
					if (m_state.offset == start_offset)
					{
						set_error(SJSONParserError::KeyExpected);
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

			State start_of_literal = save_state();

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
			set_error(SJSONParserError::TrueOrFalseExpected);
			return false;
		}

		bool read_double(double& value)
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
				set_error(SJSONParserError::NumberExpected);
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
					set_error(SJSONParserError::InvalidNumber);
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
				set_error(SJSONParserError::NumberIsTooLong);
				return false;
			}

			std::memcpy(slice, m_input + start_offset, length);
			slice[length] = '\0';

			char* last_used_symbol = nullptr;
			value = std::strtod(slice, &last_used_symbol);

			if (last_used_symbol != slice + length)
			{
				set_error(SJSONParserError::NumberCouldNotBeConverted);
				return false;
			}

			return true;
		}

		bool skip_comments_and_whitespace_fail_if_eof()
		{
			if (!skip_comments_and_whitespace())
				return false;

			if (eof())
			{
				set_error(SJSONParserError::InputTruncated);
				return false;
			}

			return true;
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
