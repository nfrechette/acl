#pragma once

//TODO #include "acl/memory.h"
//#include "acl/sjson/variant.h"

#include "memory.h"
#include "variant.h"

#include <stack>
#include <cctype>
#include <iterator>
#include <string>
#include <vector>

namespace acl
{
	class SJSONParser
	{
	public:
		SJSONParser(Allocator& allocator, std::istreambuf_iterator<char> input)
			: m_allocator(&allocator)
			, m_input(input)
		{
		}

	protected:
		virtual void onKey(std::string key) = 0;
		virtual void onValue(std::string key, Variant* v) = 0;
		virtual void onValue(std::string key, std::vector<Variant*> a) = 0;
		virtual void onBeginObject() = 0;
		virtual void onEndObject() = 0;

	private:
		enum Symbol
		{
			WHITESPACE,
			OPENING_BRACE,
			CLOSING_BRACE,
			QUOTATION_MARK,
			EQUALS,
			COLON,
			COMMA,
			OPENING_BRACKET,
			CLOSING_BRACKET,
			BACKSLASH,
			SLASH,
			ASTERISK,
			END_OF_LINE,
			A,
			E,
			F,
			L,
			N,
			R,
			S,
			T,
			U,
			LETTER,
			MINUS,
			DECIMAL_DIGIT,
			CONTROL
		};

		void begin()
		{
			while (!m_eof && !m_failed)
			{
				if (found(SLASH))
				{
					comment();
				}
				else if (found(OPENING_BRACE))
				{
					explicitObject();
				}
				else
				{
					implicitObject();
				}
			}
		}

		void comment()
		{
			if (m_eof)
			{
				fail("This is starting to look like a comment, but then the file ends");
			}
			else if (found(SLASH))
			{
				singleLineComment();
			}
			else if (found(ASTERISK))
			{
				multiLineComment();
			}
			else
			{
				fail("A comment must begin with either // or /*");
			}
		}

		void singleLineComment()
		{
			while (!found(END_OF_LINE) && !m_eof)
			{
			}
		}

		void multiLineComment()
		{
			bool wasAsterisk = false;

			while (!m_failed)
			{
				if (m_eof)
				{
					fail("The file ends before the comment does");
				}
				else if (found(ASTERISK))
				{
					wasAsterisk = true;
				}
				else if (wasAsterisk && found(SLASH))
				{
					break;
				}
				else
				{
					wasAsterisk = false;
				}
			}
		}

		void explicitObject()
		{
			bool readValue = false;

			while (!m_failed)
			{
				if (m_eof)
				{
					fail("The file ends before the object does; a closing brace is required");
				}
				else if (found(CLOSING_BRACE))
				{
					break;
				}
				else
				{
					commonObject(readValue);
				}
			}
		}

		void implicitObject()
		{
			bool readValue = false;

			while (!m_eof && !m_failed)
			{
				if (found(CLOSING_BRACE))
				{
					fail("A closing brace is not allowed, because the object doesn't start with one");
				}
				else
				{
					commonObject(readValue);
				}
			}
		}

		void commonObject(bool &readValue)
		{
			if (found(OPENING_BRACE))
			{
				fail("An object cannot be defined without a name");
			}
			else if (found(QUOTATION_MARK))
			{
				quotedKeyValue();
				readValue = true;
			}
			else if (found(EQUALS) || found(COLON))
			{
				fail("A value cannot be defined without a name");
			}
			else if (found(COMMA))
			{
				if (!readValue)
				{
					fail("A comma in an object must follow a key-value pair");
				}
				else
				{
					readValue = false;
				}
			}
			else if (found(WHITESPACE))
			{
			}
			else if (found(SLASH))
			{
				comment();
			}
			else
			{
				unquotedKeyValue();
				readValue = true;
			}
		}

		void quotedKeyValue()
		{
			m_key = string();

			value(false);

			m_key.clear();
		}

		void unquotedKeyValue()
		{
			bool readDelimiter;

			m_key = unquotedKey(readDelimiter);

			value(readDelimiter);

			m_key.clear();
		}

		std::string unquotedKey(bool &readDelimiter)
		{
			std::string result;
			readDelimiter = false;

			while (!m_failed)
			{
				if (m_eof)
				{
					break;
				}
				else if (found(QUOTATION_MARK))
				{
					fail("This quotation mark must be escaped");
				}
				else if (found(CONTROL))
				{
					fail("Control characters are not allowed in strings; use backslash escapes instead");
				}
				else if (found(BACKSLASH))
				{
					result += quotedCharacter();
				}
				else if (found(WHITESPACE))
				{
					break;
				}
				else if (found(EQUALS) || found(COLON))
				{
					readDelimiter = true;
					break;
				}
				else
				{
					result += *m_input;
				}
			}

			return result;
		}

		Variant* value(bool readDelimiter)
		{
			while (true)
			{
				if (m_eof)
				{
					fail("The file ends before the value does");
					return nullptr;
				}
				else if (found(WHITESPACE))
				{
				}
				else if (found(EQUALS) || found(COLON))
				{
					if (readDelimiter)
					{
						fail("There must be only one equal sign or colon between the key and value");
						return nullptr;
					}
					else
					{
						readDelimiter = true;
					}
				}
				else if (found(QUOTATION_MARK))
				{
					return Variant.New(string());
				}
				else if (found(MINUS, false) || found(DECIMAL_DIGIT, false))
				{
					return number();
				}
				else if (found(OPENING_BRACE))
				{
					explicitObject();
					return nullptr;
				}
				else if (found(OPENING_BRACKET))
				{
					array();
					return nullptr;
				}
				else
				{
					return nonNumericLiteral();
				}
			}
		}

		void array()
		{
			m_array.clear();

			while (!m_failed)
			{
				if (m_eof)
				{
					fail("The file ends before the array does");
				}
				else if (found(COMMA) && m_array.empty())
				{
					fail("A comma in an array must follow a value");
				}
				else if (found(CLOSING_BRACKET))
				{
					break;
				}
				else
				{
					m_array.push_back(value(true));
				}
			}
		}

		Variant* nonNumericLiteral()
		{
			if (found(T))
			{
				if (m_eof || !found(R)) goto error;
				if (m_eof || !found(U)) goto error;
				if (m_eof || !found(E)) goto error;

				return Variant.New(true);
			}
			
			if (found(F))
			{
				if (m_eof || !found(A)) goto error;
				if (m_eof || !found(L)) goto error;
				if (m_eof || !found(S)) goto error;
				if (m_eof || !found(E)) goto error;

				return Variant.New(false);
			}
			
			if (found(N))
			{
				if (m_eof || !found(U)) goto error;
				if (m_eof || !found(L)) goto error;
				if (m_eof || !found(L)) goto error;

				return Variant.New();
			}

error:
			fail("The only non-numeric literals allowed are true, false, and null.");
			return nullptr;
		}

		std::string string()
		{
			std::string result;

			while (!m_failed)
			{
				if (m_eof)
				{
					fail("The file ends before the string does");
				}
				else if (found(QUOTATION_MARK))
				{
					break;
				}
				else if (found(CONTROL))
				{
					fail("Control characters are not allowed in strings; use backslash escapes instead");
				}
				else if (found(BACKSLASH))
				{
					result += quotedCharacter();
				}
				else
				{
					result += *m_input;
				}
			}

			return result;
		}

		std::string quotedCharacter()
		{
			std::string result;

			if (m_eof)
			{
				fail("The file ends before the escaped character does");
			}
			else if (found(QUOTATION_MARK))
			{
				result = "\"";
			}
			else if (found(BACKSLASH))
			{
				result = "\\";
			}
			else if (found(SLASH))
			{
				result = "/";
			}
			else if (found(N))
			{
				result = "\n";
			}
			else if (found(R))
			{
				result = "\r";
			}
			else if (found(T))
			{
				result = "\t";
			}
			else
			{
				fail("Unrecognized or unsupported escape sequence");
			}

			return result;
		}

		Variant* number()
		{
			std::string text;

			// Cheat and lean on the standard library, rather than following the JSON specification.
			while (!m_eof &&
				!found(WHITESPACE) &&
				!found(COMMA) &&
				!found(CLOSING_BRACE, false) &&
				!found(CLOSING_BRACKET, false))
			{
				text += *m_input;
			}

			try
			{
				if (text.find(".") != std::string::npos)
				{
					// TODO - parse with std::stod
				}
				else
				{
					// TODO - parse with std::stoi
				}
			}
			catch()
			{
				fail("Incorrectly formatted number");
				return nullptr;
			}
		}

		std::istreambuf_iterator<unsigned char> m_input;
		std::istreambuf_iterator<unsigned char> m_sentinel;

		int m_line{};
		int m_column{};

		bool m_eof{};

		bool found(Symbol s, bool advanceIfFound = true)
		{
			bool matches = false;

			switch (s)
			{
			case WHITESPACE:
				matches = std::isspace(*m_input);
				break;
			case OPENING_BRACE:
				matches = *m_input == '{';
				break;
			case CLOSING_BRACE:
				matches = *m_input == '}';
				break;
			case QUOTATION_MARK:
				matches = *m_input == '"';
				break;
			case EQUALS:
				matches = *m_input == '=';
				break;
			case COLON:
				matches = *m_input == ':';
				break;
			case COMMA:
				matches = *m_input == ',';
				break;
			case END_OF_LINE:
				matches = *m_input == '\n';
				break;
			case OPENING_BRACKET:
				matches = *m_input == '[';
				break;
			case CLOSING_BRACKET:
				matches = *m_input == ']';
				break;
			case BACKSLASH:
				matches = *m_input == '\\';
				break;
			case SLASH:
				matches = *m_input == '/';
				break;
			case ASTERISK:
				matches = *m_input == '*';
				break;
			case A:
				matches = *m_input == 'a';
				break;
			case E:
				matches = *m_input == 'e';
				break;
			case F:
				matches = *m_input == 'f';
				break;
			case L:
				matches = *m_input == 'l';
				break;
			case N:
				matches = *m_input == 'n';
				break;
			case R:
				matches = *m_input == 'r';
				break;
			case S:
				matches = *m_input == 's';
				break;
			case T:
				matches = *m_input == 't';
				break;
			case U:
				matches = *m_input == 'u';
				break;
			case LETTER:
				matches = std::isalpha(*m_input);
				break;
			case MINUS:
				matches = *m_input == '-';
				break;
			case DECIMAL_DIGIT:
				matches = std::isdigit(*m_input);
				break;
			case CONTROL:
				matches = std::iscntrl(*m_input);
				break;
			}

			if (matches && advanceIfFound)
			{
				readNext();
			}
			
			return matches;
		}

		void readNext()
		{
			++m_input;

			if (m_input == m_sentinel)
			{
				m_eof = true;
				return;
			}

			if (*m_input == '\n')
			{
				++m_line;
				m_column = 0;
			}
			else
			{
				m_column++;
			}
		}

		Allocator* m_allocator;
			
		bool m_failed{};
		std::string m_failure_reason{};
		int m_failed_at_line{};
		int m_failed_at_column{};

		void fail(std::string reason)
		{
			if (!m_failed)
			{
				m_failed = true;
				m_failure_reason = reason;
				m_failed_at_line = m_line;
				m_failed_at_column = m_column;
			}
		}

		std::string m_key{};
		std::vector<Variant*> m_array;
	};
}