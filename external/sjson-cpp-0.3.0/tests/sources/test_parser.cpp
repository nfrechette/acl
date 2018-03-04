////////////////////////////////////////////////////////////////////////////////
// The MIT License (MIT)
//
// Copyright (c) 2018 Nicholas Frechette, Cody Jones, and sjson-cpp contributors
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

#include <catch.hpp>

#include "error_exceptions.h"
#include <sjson/parser.h>

using namespace sjson;

static Parser parser_from_c_str(const char* c_str)
{
	return Parser(c_str, std::strlen(c_str));
}

TEST_CASE("Parser Misc", "[parser]")
{
	{
		Parser parser = parser_from_c_str("");
		REQUIRE(parser.eof());
		REQUIRE(parser.is_valid());
	}

	{
		Parser parser = parser_from_c_str("");
		REQUIRE(parser.remainder_is_comments_and_whitespace());
		REQUIRE(parser.eof());
		REQUIRE(parser.is_valid());
	}

	{
		Parser parser = parser_from_c_str("     ");
		REQUIRE(parser.remainder_is_comments_and_whitespace());
		REQUIRE(parser.eof());
		REQUIRE(parser.is_valid());
	}

	{
		Parser parser = parser_from_c_str("// lol \\n     ");
		REQUIRE(parser.remainder_is_comments_and_whitespace());
		REQUIRE(parser.eof());
		REQUIRE(parser.is_valid());
	}

	{
		Parser parser = parser_from_c_str("\"key-one\" = true");
		bool value = false;
		REQUIRE(parser.read("key-one", value));
		REQUIRE(value == true);
		REQUIRE(parser.eof());
		REQUIRE(parser.is_valid());
	}

	{
		Parser parser = parser_from_c_str("key = /* bar */ true");
		bool value = false;
		REQUIRE(parser.read("key", value));
		REQUIRE(value == true);
		REQUIRE(parser.eof());
		REQUIRE(parser.is_valid());
	}

	{
		Parser parser = parser_from_c_str("key = /* bar * true");
		bool value = false;
		REQUIRE_FALSE(parser.read("key", value));
		REQUIRE_FALSE(parser.is_valid());
	}

	{
		Parser parser = parser_from_c_str("key = // bar \ntrue");
		bool value = false;
		REQUIRE(parser.read("key", value));
		REQUIRE(value == true);
		REQUIRE(parser.eof());
		REQUIRE(parser.is_valid());
	}

	{
		Parser parser = parser_from_c_str("key /* bar */ = true");
		bool value = false;
		REQUIRE(parser.read("key", value));
		REQUIRE(value == true);
		REQUIRE(parser.eof());
		REQUIRE(parser.is_valid());
	}

	{
		Parser parser = parser_from_c_str("/* bar */ key = true");
		bool value = false;
		REQUIRE(parser.read("key", value));
		REQUIRE(value == true);
		REQUIRE(parser.eof());
		REQUIRE(parser.is_valid());
	}
}

TEST_CASE("Parser Bool Reading", "[parser]")
{
	{
		Parser parser = parser_from_c_str("key = true");
		bool value = false;
		REQUIRE(parser.read("key", value));
		REQUIRE(value == true);
		REQUIRE(parser.eof());
		REQUIRE(parser.is_valid());
	}

	{
		Parser parser = parser_from_c_str("key = false");
		bool value = true;
		REQUIRE(parser.read("key", value));
		REQUIRE(value == false);
		REQUIRE(parser.eof());
		REQUIRE(parser.is_valid());
	}

	{
		Parser parser = parser_from_c_str("bad_key = 0");
		bool value = true;
		REQUIRE_FALSE(parser.try_read("key", value, false));
		REQUIRE(value == false);
		REQUIRE_FALSE(parser.eof());
		REQUIRE(parser.is_valid());
	}

	{
		Parser parser = parser_from_c_str("key = true");
		bool value = false;
		REQUIRE(parser.try_read("key", value, false));
		REQUIRE(value == true);
		REQUIRE(parser.eof());
		REQUIRE(parser.is_valid());
	}
}

TEST_CASE("Parser String Reading", "[parser]")
{
	{
		Parser parser = parser_from_c_str("key = \"Quoted string\"");
		StringView value;
		REQUIRE(parser.read("key", value));
		REQUIRE(value == "Quoted string");
		REQUIRE(parser.eof());
		REQUIRE(parser.is_valid());
	}

	{
		// Note: Escaped quotes \" are left escaped within the StringView because we do not allocate memory
		Parser parser = parser_from_c_str("key = \"Quoted \\\" string\"");
		StringView value;
		REQUIRE(parser.read("key", value));
		REQUIRE(value == "Quoted \\\" string");
		REQUIRE(parser.eof());
		REQUIRE(parser.is_valid());
	}

	{
		Parser parser = parser_from_c_str("key = \"New\\nline\"");
		StringView value;
		REQUIRE(parser.read("key", value));
		REQUIRE(value == "New\\nline");
		REQUIRE(parser.eof());
		REQUIRE(parser.is_valid());
	}

	{
		Parser parser = parser_from_c_str("key = \"Tab\\tulator\"");
		StringView value;
		REQUIRE(parser.read("key", value));
		REQUIRE(value == "Tab\\tulator");
		REQUIRE(parser.eof());
		REQUIRE(parser.is_valid());
	}

	{
		Parser parser = parser_from_c_str("key = \"Tab\\tulator\"");
		StringView value;
		REQUIRE(parser.read("key", value));
		REQUIRE(value == "Tab\\tulator");
		REQUIRE(parser.eof());
		REQUIRE(parser.is_valid());
	}

	{
		Parser parser = parser_from_c_str("bad_key = 0");
		StringView value;
		REQUIRE_FALSE(parser.try_read("key", value, "default"));
		REQUIRE(value == "default");
		REQUIRE_FALSE(parser.eof());
		REQUIRE(parser.is_valid());
	}

	{
		Parser parser = parser_from_c_str("key = \"good\"");
		StringView value;
		REQUIRE(parser.try_read("key", value, "default"));
		REQUIRE(value == "good");
		REQUIRE(parser.eof());
		REQUIRE(parser.is_valid());
	}

	{
		Parser parser = parser_from_c_str("key = \"bad");
		StringView value;
		REQUIRE_FALSE(parser.read("key", value));
		REQUIRE_FALSE(parser.is_valid());
	}

	{
		Parser parser = parser_from_c_str("key = bad");
		StringView value;
		REQUIRE_FALSE(parser.read("key", value));
		REQUIRE_FALSE(parser.is_valid());
	}
}

TEST_CASE("Parser Number Reading", "[parser]")
{
	{
		Parser parser = parser_from_c_str("key = 123.456789");
		double value;
		REQUIRE(parser.read("key", value));
		REQUIRE(value == 123.456789);
		REQUIRE(parser.eof());
		REQUIRE(parser.is_valid());
	}

	{
		Parser parser = parser_from_c_str("key = -123");
		int8_t value;
		REQUIRE(parser.read("key", value));
		REQUIRE(value == -123);
		REQUIRE(parser.eof());
		REQUIRE(parser.is_valid());
	}

	{
		Parser parser = parser_from_c_str("key = 123");
		uint8_t value;
		REQUIRE(parser.read("key", value));
		REQUIRE(value == 123);
		REQUIRE(parser.eof());
		REQUIRE(parser.is_valid());
	}

	{
		Parser parser = parser_from_c_str("key = -1234");
		int16_t value;
		REQUIRE(parser.read("key", value));
		REQUIRE(value == -1234);
		REQUIRE(parser.eof());
		REQUIRE(parser.is_valid());
	}

	{
		Parser parser = parser_from_c_str("key = 1234");
		uint16_t value;
		REQUIRE(parser.read("key", value));
		REQUIRE(value == 1234);
		REQUIRE(parser.eof());
		REQUIRE(parser.is_valid());
	}

	{
		Parser parser = parser_from_c_str("key = -123456");
		int32_t value;
		REQUIRE(parser.read("key", value));
		REQUIRE(value == -123456);
		REQUIRE(parser.eof());
		REQUIRE(parser.is_valid());
	}

	{
		Parser parser = parser_from_c_str("key = 123456");
		uint32_t value;
		REQUIRE(parser.read("key", value));
		REQUIRE(value == 123456);
		REQUIRE(parser.eof());
		REQUIRE(parser.is_valid());
	}

	{
		Parser parser = parser_from_c_str("key = -1234567890123456");
		int64_t value;
		REQUIRE(parser.read("key", value));
		REQUIRE(value == -1234567890123456ll);
		REQUIRE(parser.eof());
		REQUIRE(parser.is_valid());
	}

	{
		Parser parser = parser_from_c_str("key = 1234567890123456");
		uint64_t value;
		REQUIRE(parser.read("key", value));
		REQUIRE(value == 1234567890123456ull);
		REQUIRE(parser.eof());
		REQUIRE(parser.is_valid());
	}

	{
		Parser parser = parser_from_c_str("bad_key = \"bad\"");
		double value = 0.0;
		REQUIRE_FALSE(parser.try_read("key", value, 1.0));
		REQUIRE(value == 1.0);
		REQUIRE_FALSE(parser.eof());
		REQUIRE(parser.is_valid());
	}

	{
		Parser parser = parser_from_c_str("key = 2.0");
		double value = 0.0;
		REQUIRE(parser.try_read("key", value, 1.0));
		REQUIRE(value == 2.0);
		REQUIRE(parser.eof());
		REQUIRE(parser.is_valid());
	}
}

TEST_CASE("Parser Array Reading", "[parser]")
{
	{
		Parser parser = parser_from_c_str("key = [ 123.456789, 456.789, 151.091 ]");
		double value[3] = { 0.0, 0.0, 0.0 };
		REQUIRE(parser.read("key", value, 3));
		REQUIRE(value[0] == 123.456789);
		REQUIRE(value[1] == 456.789);
		REQUIRE(value[2] == 151.091);
		REQUIRE(parser.eof());
		REQUIRE(parser.is_valid());
	}

	{
		Parser parser = parser_from_c_str("key = [ \"123.456789\", \"456.789\", \"151.091\" ]");
		StringView value[3];
		REQUIRE(parser.read("key", value, 3));
		REQUIRE(value[0] == "123.456789");
		REQUIRE(value[1] == "456.789");
		REQUIRE(value[2] == "151.091");
		REQUIRE(parser.eof());
		REQUIRE(parser.is_valid());
	}

	{
		Parser parser = parser_from_c_str("bad_key = \"bad\"");
		double value[3] = { 0.0, 0.0, 0.0 };
		REQUIRE_FALSE(parser.try_read("key", value, 3, 1.0));
		REQUIRE(value[0] == 1.0);
		REQUIRE(value[1] == 1.0);
		REQUIRE(value[2] == 1.0);
		REQUIRE_FALSE(parser.eof());
		REQUIRE(parser.is_valid());
	}

	{
		Parser parser = parser_from_c_str("key = [ 123.456789, 456.789, 151.091 ]");
		double value[3] = { 0.0, 0.0, 0.0 };
		REQUIRE(parser.try_read("key", value, 3, 1.0));
		REQUIRE(value[0] == 123.456789);
		REQUIRE(value[1] == 456.789);
		REQUIRE(value[2] == 151.091);
		REQUIRE(parser.eof());
		REQUIRE(parser.is_valid());
	}

	{
		Parser parser = parser_from_c_str("bad_key = \"bad\"");
		StringView value[3];
		REQUIRE_FALSE(parser.try_read("key", value, 3, "default"));
		REQUIRE(value[0] == "default");
		REQUIRE(value[1] == "default");
		REQUIRE(value[2] == "default");
		REQUIRE_FALSE(parser.eof());
		REQUIRE(parser.is_valid());
	}

	{
		Parser parser = parser_from_c_str("key = [ \"123.456789\", \"456.789\", \"151.091\" ]");
		StringView value[3];
		REQUIRE(parser.try_read("key", value, 3, "default"));
		REQUIRE(value[0] == "123.456789");
		REQUIRE(value[1] == "456.789");
		REQUIRE(value[2] == "151.091");
		REQUIRE(parser.eof());
		REQUIRE(parser.is_valid());
	}

#if 0
	{
		Parser parser = parser_from_c_str("key = [ 123.456789, \"456.789\", false, [ 1.0, true ], { key0 = 1.0, key1 = false } ]");

		REQUIRE(parser.array_begins("key"));
		// TODO
		REQUIRE(parser.array_ends());
		REQUIRE(parser.eof());
		REQUIRE(parser.is_valid());
	}
#endif
}

TEST_CASE("Parser Null Reading", "[parser]")
{
	{
		Parser parser = parser_from_c_str("key = null");
		bool value_bool = false;
		REQUIRE_FALSE(parser.try_read("key", value_bool, true));
		REQUIRE(value_bool == true);
		double value_dbl = 0.0;
		REQUIRE_FALSE(parser.try_read("key", value_dbl, 1.0));
		REQUIRE(value_dbl == 1.0);
		REQUIRE(parser.eof());
		REQUIRE(parser.is_valid());
	}
}
