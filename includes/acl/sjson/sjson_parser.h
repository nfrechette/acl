#pragma once

#include "sjson_parser_error.h"

#include <cctype>
#include <cmath>
#include <cstring>

namespace acl
{
	class SJSONParser
	{
	public:
		SJSONParser(const char* const input, int input_length)
			: m_input(input),
			  m_input_length(input_length)
		{
			init_state();
		}

		bool object_begins()
		{
			return read_opening_brace();
		}

		bool object_begins(const char* const having_name)
		{
			return read_key(having_name) && read_equal_sign() && object_begins();
		}

		bool object_ends()
		{
			return read_closing_brace();
		}

		bool array_begins(const char* const having_name)
		{
			return read_key(having_name) && read_equal_sign() && read_opening_bracket();
		}

		bool array_begins()
		{
			return read_opening_bracket();
		}

		bool peek_if_array_ends() 
		{
			State s = save_state();
			m_state.peeking = true;
			bool result = array_ends();
			restore_state(s);
			return result;
		}

		bool array_ends()
		{
			return read_closing_bracket();
		}

		bool read(const char* const key, const char*& value, int& length)
		{
			return read_key(key) && read_equal_sign() && read_string(value, length);
		}

		bool read(const char* const key, bool& value)
		{
			return read_key(key) && read_equal_sign() && read_bool(value);
		}

		bool read(const char* const key, double& value)
		{
			return read_key(key) && read_equal_sign() && read_double(value);
		}

		bool read(const char* const key, int& value)
		{
			return read_key(key) && read_equal_sign() && read_int(value);
		}

		bool read(const char* const key, double* const values, int num_elements)
		{
			return read_key(key) && read_equal_sign() && read_opening_bracket() && read(values, num_elements) && read_closing_bracket();
		}

		bool read(double* const values, int num_elements)
		{
			for (int i = 0; i < num_elements; ++i)
			{
				if (!read_double(values[i]) || i < num_elements - 1 && !read_comma())
				{
					return false;
				}
			}

			return true;
		}

		bool remainder_is_comments_and_whitespace()
		{
			if (!skip_comments_and_whitespace())
			{
				return false;
			}

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
				{
					return true;
				}

				if (std::isspace(m_state.symbol))
				{
					advance();
					continue;
				}

				if (m_state.symbol == '/')
				{
					advance();

					if (!read_comment())
					{
						return false;
					}
				}

				return true;
			}
		}

		void get_position(int& line, int& column)
		{
			line = m_state.line;
			column = m_state.column;
		}

		bool eof()
		{
			return m_state.offset >= m_input_length;
		}

		SJSONParserError get_error()
		{
			return m_state.error;
		}

		struct State
		{
			int offset{};
			int line{ 1 };
			int column{ 1 };
			char symbol;
			bool peeking{};

			SJSONParserError error{};
		};

		State save_state()
		{
			return m_state;
		}

		void restore_state(State& s)
		{
			m_state = s;

			// If large file support is added, seek to the old position in the input file.
		}

		bool strings_equal(const char* const nul_terminated, const char* const unterminated, const int unterminated_length)
		{
			return std::strlen(nul_terminated) == unterminated_length &&
				std::memcmp(nul_terminated, unterminated, unterminated_length) == 0;
		}

	private:
		static int constexpr MAX_NUMBER_LENGTH = 64;

		void set_error(int error)
		{
			m_state.error.error = error;
			m_state.error.line = m_state.line;
			m_state.error.column = m_state.column;
		}

		bool read_equal_sign()
		{
			return read_symbol('=', SJSONParserError::EqualSignExpected);
		}

		bool read_opening_brace()
		{
			return read_symbol('{', SJSONParserError::OpeningBraceExpected);
		}

		bool read_closing_brace()
		{
			return read_symbol('}', SJSONParserError::ClosingBraceExpected);
		}

		bool read_opening_bracket()
		{
			return read_symbol('[', SJSONParserError::OpeningBracketExpected);
		}

		bool read_closing_bracket()
		{
			return read_symbol(']', SJSONParserError::ClosingBracketExpected);
		}

		bool read_comma()
		{
			return read_symbol(',', SJSONParserError::CommaExpected);
		}

		bool read_symbol(char expected, int reason_if_other_found)
		{
			if (!skip_comments_and_whitespace_fail_if_eof())
			{
				return false;
			}

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
				{
					advance();
				}

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

		bool read_key(const char* const having_name)
		{
			if (!skip_comments_and_whitespace_fail_if_eof())
			{
				return false;
			}

			State start_of_key = save_state();

			const char* actual;
			int length;

			if (m_state.symbol == '"')
			{
				if (!read_string(actual, length))
				{
					return false;
				}
			}
			else
			{
				if (!read_unquoted_key(actual, length))
				{
					return false;
				}
			}

			if (!strings_equal(having_name, actual, length))
			{
				restore_state(start_of_key);
				set_error(SJSONParserError::IncorrectKey);
				return false;
			}

			return true;
		}

		bool read_string(const char*& value, int& length)
		{
			if (!skip_comments_and_whitespace_fail_if_eof())
			{
				return false;
			}

			if (m_state.symbol != '"')
			{
				set_error(SJSONParserError::QuotationMarkExpected);
				return false;
			}

			advance();

			int start_offset = m_state.offset;
			int end_offset;

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

			value = m_input + start_offset;
			length = end_offset - start_offset + 1;
			return true;
		}

		bool read_unquoted_key(const char*& value, int& length)
		{
			if (eof())
			{
				set_error(SJSONParserError::InputTruncated);
				return false;
			}

			int start_offset = m_state.offset;
			int end_offset;

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

			value = m_input + start_offset;
			length = end_offset - start_offset + 1;
			return true;
		}

		// TODO: document that the literal null is not supported
		bool read_bool(bool& value)
		{
			if (!skip_comments_and_whitespace_fail_if_eof())
			{
				return false;
			}

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
			{
				return false;
			}

			int start_offset = m_state.offset;
			int end_offset;

			if (m_state.symbol == '-')
			{
				advance();
			}

			if (m_state.symbol == '0')
			{
				advance();
			}
			else if (std::isdigit(m_state.symbol))
			{
				while (std::isdigit(m_state.symbol))
				{
					advance();
				}
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
				{
					advance();
				}
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
				{
					advance();
				}
			}
			
			end_offset = m_state.offset - 1;
			int length = end_offset - start_offset + 1;

			char slice[MAX_NUMBER_LENGTH + 1];
			if (length >= MAX_NUMBER_LENGTH)
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

		bool read_int(int& value)
		{
			double d;

			if (!read_double(d))
			{
				return false;
			}

			double integer_portion;

			if (std::modf(d, &integer_portion) != 0.0)
			{
				set_error(SJSONParserError::IntegerExpected);
				return false;
			}

			value = static_cast<int>(integer_portion);
			return true;
		}

		bool skip_comments_and_whitespace_fail_if_eof()
		{
			if (!skip_comments_and_whitespace())
			{
				return false;
			}

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
			{
				return false;
			}

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

		void init_state()
		{
			m_state.symbol = m_input_length > 0 ? m_input[0] : '\0';
		}

		const char* const m_input;
		const int m_input_length;

		State m_state;
		State m_saved_state;
	};
}