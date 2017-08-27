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

#include "acl/core/error.h"

#include <functional>
#include <cstdio>
#include <stdint.h>

namespace acl
{
	// TODO: Cleanup the locking stuff, wrap it in #ifdef to strip when asserts are disabled

	class SJSONWriter;
	class SJSONArrayWriter;
	class SJSONObjectWriter;

	class SJSONStreamWriter
	{
	public:
		virtual ~SJSONStreamWriter() {}

		virtual void write(const void* buffer, size_t buffer_size) = 0;

		void write(const char* str) { write(str, std::strlen(str)); }
	};

	class SJSONFileStreamWriter final : public SJSONStreamWriter
	{
	public:
		SJSONFileStreamWriter(std::FILE* file)
			: m_file(file)
		{}

		virtual void write(const void* buffer, size_t buffer_size) override
		{
			fprintf(m_file, reinterpret_cast<const char*>(buffer));
		}

	private:
		std::FILE* m_file;
	};

	class SJSONArrayWriter
	{
	public:
		void push_value(const char* value);
		void push_value(bool value);
		void push_value(double value);
		void push_value(float value) { push_value(double(value)); }
		void push_value(int8_t value) { push_signed_integer(value); }
		void push_value(uint8_t value) { push_unsigned_integer(value); }
		void push_value(int16_t value) { push_signed_integer(value); }
		void push_value(uint16_t value) { push_unsigned_integer(value); }
		void push_value(int32_t value) { push_signed_integer(value); }
		void push_value(uint32_t value) { push_unsigned_integer(value); }
		void push_value(int64_t value) { push_signed_integer(value); }
		void push_value(uint64_t value) { push_unsigned_integer(value); }

		void push_object(std::function<void(SJSONObjectWriter& object)> writer_fun);
		void push_array(std::function<void(SJSONArrayWriter& array_writer)> writer_fun);

		void push_newline();

	private:
		SJSONArrayWriter(SJSONStreamWriter& stream_writer, uint32_t indent_level);

		SJSONArrayWriter(const SJSONArrayWriter&) = delete;
		SJSONArrayWriter& operator=(const SJSONArrayWriter&) = delete;

		void push_signed_integer(int64_t value);
		void push_unsigned_integer(uint64_t value);
		void write_indentation();

		SJSONStreamWriter& m_stream_writer;
		uint32_t m_indent_level;
		bool m_is_empty;
		bool m_is_locked;
		bool m_is_newline;

		friend SJSONObjectWriter;
	};

	class SJSONObjectWriter
	{
	public:
		void insert_value(const char* key, const char* value);
		void insert_value(const char* key, bool value);
		void insert_value(const char* key, double value);
		void insert_value(const char* key, float value) { insert_value(key, double(value)); }
		void insert_value(const char* key, int8_t value) { insert_signed_integer(key, value); }
		void insert_value(const char* key, uint8_t value) { insert_unsigned_integer(key, value); }
		void insert_value(const char* key, int16_t value) { insert_signed_integer(key, value); }
		void insert_value(const char* key, uint16_t value) { insert_unsigned_integer(key, value); }
		void insert_value(const char* key, int32_t value) { insert_signed_integer(key, value); }
		void insert_value(const char* key, uint32_t value) { insert_unsigned_integer(key, value); }
		void insert_value(const char* key, int64_t value) { insert_signed_integer(key, value); }
		void insert_value(const char* key, uint64_t value) { insert_unsigned_integer(key, value); }

		void insert_object(const char* key, std::function<void(SJSONObjectWriter& object_writer)> writer_fun);
		void insert_array(const char* key, std::function<void(SJSONArrayWriter& array_writer)> writer_fun);

		void insert_newline();

		// Implement operator[] for convenience
		class ValueRef
		{
		public:
			ValueRef(ValueRef&& other);
			~ValueRef();

			void operator=(const char* value);
			void operator=(bool value);
			void operator=(double value);
			void operator=(float value) { *this = double(value); }
			void operator=(int8_t value) { assign_signed_integer(value); }
			void operator=(uint8_t value) { assign_unsigned_integer(value); }
			void operator=(int16_t value) { assign_signed_integer(value); }
			void operator=(uint16_t value) { assign_unsigned_integer(value); }
			void operator=(int32_t value) { assign_signed_integer(value); }
			void operator=(uint32_t value) { assign_unsigned_integer(value); }
			void operator=(int64_t value) { assign_signed_integer(value); }
			void operator=(uint64_t value) { assign_unsigned_integer(value); }

			void operator=(std::function<void(SJSONObjectWriter& object_writer)> writer_fun);
			void operator=(std::function<void(SJSONArrayWriter& array_writer)> writer_fun);

		private:
			ValueRef(SJSONObjectWriter& object_writer, const char* key);

			ValueRef(const ValueRef&) = delete;
			ValueRef& operator=(const ValueRef&) = delete;

			void assign_signed_integer(int64_t value);
			void assign_unsigned_integer(uint64_t value);

			SJSONObjectWriter* m_object_writer;
			bool m_is_empty;
			bool m_is_locked;

			friend SJSONObjectWriter;
		};

		ValueRef operator[](const char* key) { return ValueRef(*this, key); }

	protected:
		SJSONObjectWriter(SJSONStreamWriter& stream_writer, uint32_t indent_level);

		SJSONObjectWriter(const SJSONObjectWriter&) = delete;
		SJSONObjectWriter& operator=(const SJSONObjectWriter&) = delete;

		void insert_signed_integer(const char* key, int64_t value);
		void insert_unsigned_integer(const char* key, uint64_t value);
		void write_indentation();

		SJSONStreamWriter& m_stream_writer;
		uint32_t m_indent_level;
		bool m_is_locked;
		bool m_has_live_value_ref;

		friend SJSONArrayWriter;
	};

	class SJSONWriter : public SJSONObjectWriter
	{
	public:
		SJSONWriter(SJSONStreamWriter& stream_writer);

	private:
		SJSONWriter(const SJSONWriter&) = delete;
		SJSONWriter& operator=(const SJSONWriter&) = delete;
	};

	//////////////////////////////////////////////////////////////////////////

	inline SJSONObjectWriter::SJSONObjectWriter(SJSONStreamWriter& stream_writer, uint32_t indent_level)
		: m_stream_writer(stream_writer)
		, m_indent_level(indent_level)
		, m_is_locked(false)
		, m_has_live_value_ref(false)
	{}

	inline void SJSONObjectWriter::insert_value(const char* key, const char* value)
	{
		ACL_ENSURE(!m_is_locked, "Cannot insert SJSON value in locked object");
		ACL_ENSURE(!m_has_live_value_ref, "Cannot insert SJSON value in object when it has a live ValueRef");

		write_indentation();

		m_stream_writer.write(key);
		m_stream_writer.write(" = \"");
		m_stream_writer.write(value);
		m_stream_writer.write("\"\n");
	}

	inline void SJSONObjectWriter::insert_value(const char* key, bool value)
	{
		ACL_ENSURE(!m_is_locked, "Cannot insert SJSON value in locked object");
		ACL_ENSURE(!m_has_live_value_ref, "Cannot insert SJSON value in object when it has a live ValueRef");

		write_indentation();

		m_stream_writer.write(key);
		m_stream_writer.write(" = ");

		char buffer[256];
		size_t length = snprintf(buffer, sizeof(buffer), "%s\n", value ? "true" : "false");
		ACL_ENSURE(length > 0 && length < sizeof(buffer), "Failed to insert SJSON value: [%s = %s]", key, value);
		m_stream_writer.write(buffer, length);
	}

	inline void SJSONObjectWriter::insert_value(const char* key, double value)
	{
		ACL_ENSURE(!m_is_locked, "Cannot insert SJSON value in locked object");
		ACL_ENSURE(!m_has_live_value_ref, "Cannot insert SJSON value in object when it has a live ValueRef");

		write_indentation();

		m_stream_writer.write(key);
		m_stream_writer.write(" = ");

		char buffer[256];
		size_t length = snprintf(buffer, sizeof(buffer), "%.10f\n", value);
		ACL_ENSURE(length > 0 && length < sizeof(buffer), "Failed to insert SJSON value: [%s = %.10f]", key, value);
		m_stream_writer.write(buffer, length);
	}

	inline void SJSONObjectWriter::insert_signed_integer(const char* key, int64_t value)
	{
		ACL_ENSURE(!m_is_locked, "Cannot insert SJSON value in locked object");
		ACL_ENSURE(!m_has_live_value_ref, "Cannot insert SJSON value in object when it has a live ValueRef");

		write_indentation();

		m_stream_writer.write(key);
		m_stream_writer.write(" = ");

		char buffer[256];
		size_t length = snprintf(buffer, sizeof(buffer), "%lld\n", value);
		ACL_ENSURE(length > 0 && length < sizeof(buffer), "Failed to insert SJSON value: [%s = %lld]", key, value);
		m_stream_writer.write(buffer, length);
	}

	inline void SJSONObjectWriter::insert_unsigned_integer(const char* key, uint64_t value)
	{
		ACL_ENSURE(!m_is_locked, "Cannot insert SJSON value in locked object");
		ACL_ENSURE(!m_has_live_value_ref, "Cannot insert SJSON value in object when it has a live ValueRef");

		write_indentation();

		m_stream_writer.write(key);
		m_stream_writer.write(" = ");

		char buffer[256];
		size_t length = snprintf(buffer, sizeof(buffer), "%llu\n", value);
		ACL_ENSURE(length > 0 && length < sizeof(buffer), "Failed to insert SJSON value: [%s = %llu]", key, value);
		m_stream_writer.write(buffer, length);
	}

	inline void SJSONObjectWriter::insert_object(const char* key, std::function<void(SJSONObjectWriter& object_writer)> writer_fun)
	{
		ACL_ENSURE(!m_is_locked, "Cannot insert SJSON object in locked object");
		ACL_ENSURE(!m_has_live_value_ref, "Cannot insert SJSON object in object when it has a live ValueRef");

		write_indentation();

		m_stream_writer.write(key);
		m_stream_writer.write(" = {\n");
		m_is_locked = true;

		SJSONObjectWriter object_writer(m_stream_writer, m_indent_level + 1);
		writer_fun(object_writer);

		m_is_locked = false;
		write_indentation();

		m_stream_writer.write("}\n");
	}

	inline void SJSONObjectWriter::insert_array(const char* key, std::function<void(SJSONArrayWriter& array_writer)> writer_fun)
	{
		ACL_ENSURE(!m_is_locked, "Cannot insert SJSON array in locked object");
		ACL_ENSURE(!m_has_live_value_ref, "Cannot insert SJSON array in object when it has a live ValueRef");

		write_indentation();

		m_stream_writer.write(key);
		m_stream_writer.write(" = [ ");
		m_is_locked = true;

		SJSONArrayWriter array_writer(m_stream_writer, m_indent_level + 1);
		writer_fun(array_writer);

		if (array_writer.m_is_newline)
		{
			write_indentation();
			m_stream_writer.write("]\n");
		}
		else
			m_stream_writer.write(" ]\n");

		m_is_locked = false;
	}

	inline void SJSONObjectWriter::write_indentation()
	{
		for (uint32_t level = 0; level < m_indent_level; ++level)
			m_stream_writer.write("\t");
	}

	inline void SJSONObjectWriter::insert_newline()
	{
		ACL_ENSURE(!m_is_locked, "Cannot insert newline in locked object");
		ACL_ENSURE(!m_has_live_value_ref, "Cannot insert newline in object when it has a live ValueRef");

		m_stream_writer.write("\n");
	}

	inline SJSONObjectWriter::ValueRef::ValueRef(SJSONObjectWriter& object_writer, const char* key)
		: m_object_writer(&object_writer)
		, m_is_empty(true)
		, m_is_locked(false)
	{
		ACL_ENSURE(!object_writer.m_is_locked, "Cannot insert SJSON value in locked object");
		ACL_ENSURE(!object_writer.m_has_live_value_ref, "Cannot insert SJSON value in object when it has a live ValueRef");

		object_writer.write_indentation();
		object_writer.m_stream_writer.write(key);
		object_writer.m_stream_writer.write(" = ");
		object_writer.m_has_live_value_ref = true;
		object_writer.m_is_locked = true;
	}

	inline SJSONObjectWriter::ValueRef::ValueRef(ValueRef&& other)
		: m_object_writer(other.m_object_writer)
		, m_is_empty(other.m_is_empty)
		, m_is_locked(other.m_is_locked)
	{
		other.m_object_writer = nullptr;
	}

	inline SJSONObjectWriter::ValueRef::~ValueRef()
	{
		if (m_object_writer != nullptr)
		{
			ACL_ENSURE(!m_is_empty, "ValueRef has no associated value");
			ACL_ENSURE(!m_is_locked, "ValueRef is locked");
			ACL_ENSURE(m_object_writer->m_has_live_value_ref, "Expected a live ValueRef to be present");
			ACL_ENSURE(m_object_writer->m_is_locked, "Expected object writer to be locked");

			m_object_writer->m_has_live_value_ref = false;
			m_object_writer->m_is_locked = false;
		}
	}

	inline void SJSONObjectWriter::ValueRef::operator=(const char* value)
	{
		ACL_ENSURE(m_is_empty, "Cannot write multiple values within a ValueRef");
		ACL_ENSURE(m_object_writer != nullptr, "ValueRef not initialized");
		ACL_ENSURE(!m_is_locked, "Cannot assign a value when locked");

		m_object_writer->m_stream_writer.write("\"");
		m_object_writer->m_stream_writer.write(value);
		m_object_writer->m_stream_writer.write("\"\n");
		m_is_empty = false;
	}

	inline void SJSONObjectWriter::ValueRef::operator=(bool value)
	{
		ACL_ENSURE(m_is_empty, "Cannot write multiple values within a ValueRef");
		ACL_ENSURE(m_object_writer != nullptr, "ValueRef not initialized");
		ACL_ENSURE(!m_is_locked, "Cannot assign a value when locked");

		char buffer[256];
		size_t length = snprintf(buffer, sizeof(buffer), "%s\n", value ? "true" : "false");
		ACL_ENSURE(length > 0 && length < sizeof(buffer), "Failed to assign SJSON value: %s", value);
		m_object_writer->m_stream_writer.write(buffer, length);
		m_is_empty = false;
	}

	inline void SJSONObjectWriter::ValueRef::operator=(double value)
	{
		ACL_ENSURE(m_is_empty, "Cannot write multiple values within a ValueRef");
		ACL_ENSURE(m_object_writer != nullptr, "ValueRef not initialized");
		ACL_ENSURE(!m_is_locked, "Cannot assign a value when locked");

		char buffer[256];
		size_t length = snprintf(buffer, sizeof(buffer), "%.10f\n", value);
		ACL_ENSURE(length > 0 && length < sizeof(buffer), "Failed to assign SJSON value: %.10f", value);
		m_object_writer->m_stream_writer.write(buffer, length);
		m_is_empty = false;
	}

	inline void SJSONObjectWriter::ValueRef::operator=(std::function<void(SJSONObjectWriter& object_writer)> writer_fun)
	{
		ACL_ENSURE(m_is_empty, "Cannot write multiple values within a ValueRef");
		ACL_ENSURE(m_object_writer != nullptr, "ValueRef not initialized");
		ACL_ENSURE(!m_is_locked, "Cannot assign a value when locked");

		m_object_writer->m_stream_writer.write("{\n");
		m_is_locked = true;

		SJSONObjectWriter object_writer(m_object_writer->m_stream_writer, m_object_writer->m_indent_level + 1);
		writer_fun(object_writer);

		m_is_locked = false;
		m_object_writer->write_indentation();
		m_object_writer->m_stream_writer.write("}\n");
		m_is_empty = false;
	}

	inline void SJSONObjectWriter::ValueRef::operator=(std::function<void(SJSONArrayWriter& array_writer)> writer_fun)
	{
		ACL_ENSURE(m_is_empty, "Cannot write multiple values within a ValueRef");
		ACL_ENSURE(m_object_writer != nullptr, "ValueRef not initialized");
		ACL_ENSURE(!m_is_locked, "Cannot assign a value when locked");

		m_object_writer->m_stream_writer.write("[ ");
		m_is_locked = true;

		SJSONArrayWriter array_writer(m_object_writer->m_stream_writer, m_object_writer->m_indent_level + 1);
		writer_fun(array_writer);

		if (array_writer.m_is_newline)
		{
			m_object_writer->write_indentation();
			m_object_writer->m_stream_writer.write("]\n");
		}
		else
			m_object_writer->m_stream_writer.write(" ]\n");

		m_is_locked = false;
		m_is_empty = false;
	}

	inline void SJSONObjectWriter::ValueRef::assign_signed_integer(int64_t value)
	{
		ACL_ENSURE(m_is_empty, "Cannot write multiple values within a ValueRef");
		ACL_ENSURE(m_object_writer != nullptr, "ValueRef not initialized");
		ACL_ENSURE(!m_is_locked, "Cannot assign a value when locked");

		char buffer[256];
		size_t length = snprintf(buffer, sizeof(buffer), "%lld\n", value);
		ACL_ENSURE(length > 0 && length < sizeof(buffer), "Failed to assign SJSON value: %lld", value);
		m_object_writer->m_stream_writer.write(buffer, length);
		m_is_empty = false;
	}

	inline void SJSONObjectWriter::ValueRef::assign_unsigned_integer(uint64_t value)
	{
		ACL_ENSURE(m_is_empty, "Cannot write multiple values within a ValueRef");
		ACL_ENSURE(m_object_writer != nullptr, "ValueRef not initialized");
		ACL_ENSURE(!m_is_locked, "Cannot assign a value when locked");

		char buffer[256];
		size_t length = snprintf(buffer, sizeof(buffer), "%llu\n", value);
		ACL_ENSURE(length > 0 && length < sizeof(buffer), "Failed to assign SJSON value: %llu", value);
		m_object_writer->m_stream_writer.write(buffer, length);
		m_is_empty = false;
	}

	//////////////////////////////////////////////////////////////////////////

	inline SJSONArrayWriter::SJSONArrayWriter(SJSONStreamWriter& stream_writer, uint32_t indent_level)
		: m_stream_writer(stream_writer)
		, m_indent_level(indent_level)
		, m_is_empty(true)
		, m_is_locked(false)
		, m_is_newline(false)
	{}

	inline void SJSONArrayWriter::push_value(const char* value)
	{
		ACL_ENSURE(!m_is_locked, "Cannot push SJSON value in locked array");

		if (!m_is_empty && !m_is_newline)
			m_stream_writer.write(", ");

		if (m_is_newline)
			write_indentation();

		m_stream_writer.write("\"");
		m_stream_writer.write(value);
		m_stream_writer.write("\"");
		m_is_empty = false;
		m_is_newline = false;
	}

	inline void SJSONArrayWriter::push_value(bool value)
	{
		ACL_ENSURE(!m_is_locked, "Cannot push SJSON value in locked array");

		if (!m_is_empty && !m_is_newline)
			m_stream_writer.write(", ");

		if (m_is_newline)
			write_indentation();

		char buffer[256];
		size_t length = snprintf(buffer, sizeof(buffer), "%s", value ? "true" : "false");
		ACL_ENSURE(length > 0 && length < sizeof(buffer), "Failed to push SJSON value: %s", value);
		m_stream_writer.write(buffer, length);
		m_is_empty = false;
		m_is_newline = false;
	}

	inline void SJSONArrayWriter::push_value(double value)
	{
		ACL_ENSURE(!m_is_locked, "Cannot push SJSON value in locked array");

		if (!m_is_empty && !m_is_newline)
			m_stream_writer.write(", ");

		if (m_is_newline)
			write_indentation();

		char buffer[256];
		size_t length = snprintf(buffer, sizeof(buffer), "%.10f", value);
		ACL_ENSURE(length > 0 && length < sizeof(buffer), "Failed to push SJSON value: %.10f", value);
		m_stream_writer.write(buffer, length);
		m_is_empty = false;
		m_is_newline = false;
	}

	inline void SJSONArrayWriter::push_signed_integer(int64_t value)
	{
		ACL_ENSURE(!m_is_locked, "Cannot push SJSON value in locked array");

		if (!m_is_empty && !m_is_newline)
			m_stream_writer.write(", ");

		if (m_is_newline)
			write_indentation();

		char buffer[256];
		size_t length = snprintf(buffer, sizeof(buffer), "%lld", value);
		ACL_ENSURE(length > 0 && length < sizeof(buffer), "Failed to push SJSON value: %lld", value);
		m_stream_writer.write(buffer, length);
		m_is_empty = false;
		m_is_newline = false;
	}

	inline void SJSONArrayWriter::push_unsigned_integer(uint64_t value)
	{
		ACL_ENSURE(!m_is_locked, "Cannot push SJSON value in locked array");

		if (!m_is_empty && !m_is_newline)
			m_stream_writer.write(", ");

		if (m_is_newline)
			write_indentation();

		char buffer[256];
		size_t length = snprintf(buffer, sizeof(buffer), "%llu", value);
		ACL_ENSURE(length > 0 && length < sizeof(buffer), "Failed to push SJSON value: %llu", value);
		m_stream_writer.write(buffer, length);
		m_is_empty = false;
		m_is_newline = false;
	}

	inline void SJSONArrayWriter::push_object(std::function<void(SJSONObjectWriter& object_writer)> writer_fun)
	{
		ACL_ENSURE(!m_is_locked, "Cannot push SJSON object in locked array");

		if (!m_is_empty && !m_is_newline)
			m_stream_writer.write(",\n");

		write_indentation();
		m_stream_writer.write("{\n");
		m_is_locked = true;

		SJSONObjectWriter object_writer(m_stream_writer, m_indent_level + 1);
		writer_fun(object_writer);

		write_indentation();
		m_stream_writer.write("}\n");

		m_is_locked = false;
		m_is_empty = false;
		m_is_newline = true;
	}

	inline void SJSONArrayWriter::push_array(std::function<void(SJSONArrayWriter& array_writer)> writer_fun)
	{
		ACL_ENSURE(!m_is_locked, "Cannot push SJSON array in locked array");

		if (!m_is_empty && !m_is_newline)
			m_stream_writer.write(", ");

		if (m_is_newline)
			write_indentation();

		m_stream_writer.write("[ ");
		m_is_locked = true;

		SJSONArrayWriter array_writer(m_stream_writer, m_indent_level);
		writer_fun(array_writer);

		m_is_locked = false;
		m_stream_writer.write(" ]");
		m_is_empty = false;
		m_is_newline = false;
	}

	inline void SJSONArrayWriter::push_newline()
	{
		ACL_ENSURE(!m_is_locked, "Cannot insert newline in locked array");

		m_stream_writer.write("\n");
		m_is_newline = true;
	}

	inline void SJSONArrayWriter::write_indentation()
	{
		for (uint32_t level = 0; level < m_indent_level; ++level)
			m_stream_writer.write("\t");
	}

	//////////////////////////////////////////////////////////////////////////

	inline SJSONWriter::SJSONWriter(SJSONStreamWriter& stream_writer)
		: SJSONObjectWriter(stream_writer, 0)
	{}
}
