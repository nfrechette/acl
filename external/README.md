# External dependencies

## Runtime dependencies

### Realtime Math

[Reamtime Math v1.1.0-develop](https://github.com/nfrechette/rtm/releases/tag/v1.1.0) (MIT License) is used for some math types and functions. Its usage is currently limited but a full transition to use it exclusively will occur for ACL v2.0. Needed only by the SJSON IO reader/writer and the scalar track compression/decompression API.

## Development dependencies

### Catch2

[Catch2 v2.11.0](https://github.com/catchorg/Catch2/releases/tag/v2.11.0) (Boost Software License v1.0) is used by our [unit tests](../tests). You will only need it if you run the unit tests and it is included as-is without modifications.

### sjson-cpp

[sjson-cpp v0.7.0](https://github.com/nfrechette/sjson-cpp/releases/tag/v0.7.0) (MIT License) is used by our [ACL file format](../docs/the_acl_file_format.md) [clip reader](../includes/acl/io/clip_reader.h) and [clip writer](../includes/acl/io/clip_writer.h) as well as by the [acl_compressor](../tools/acl_compressor) tool used for regression testing and profiling. Unless you use our ACL file format at runtime (which you shouldn't), you will not have this dependency included at all.

In fact, to use any of these things you must include `sjson-cpp` relevant headers manually before you include the ACL headers that need them. For convenience, you can use the included version here or your own version as long as the API remains compatible.
