# External dependencies

## Runtime dependencies

### Realtime Math

[Reamtime Math v2.1.2](https://github.com/nfrechette/rtm/releases/tag/v2.1.2) (MIT License) is used for its optimized math.

## Development dependencies

### Catch2

[Catch2 v2.13.1](https://github.com/catchorg/Catch2/releases/tag/v2.13.1) (Boost Software License v1.0) is used by our [unit tests](../tests). You will only need it if you run the unit tests and it is included as-is without modifications.

### Google Benchmark

[Google Benchmark v1.5.1](https://github.com/google/benchmark/releases/tag/v1.5.1) (Apache License 2.0) is used to benchmark various functions. You will only need it if you run the benchmarks and it is included as-is without modifications.

### sjson-cpp

[sjson-cpp v0.8.1](https://github.com/nfrechette/sjson-cpp/releases/tag/v0.8.1) (MIT License) is used by our [ACL file format](../docs/the_acl_file_format.md) [clip reader](../includes/acl/io/clip_reader.h) and [clip writer](../includes/acl/io/clip_writer.h) as well as by the [acl_compressor](../tools/acl_compressor) tool used for regression testing and profiling. Unless you use our ACL file format at runtime (which you shouldn't), you will not have this dependency included at all.

In fact, to use any of these things you must include `sjson-cpp` relevant headers manually before you include the ACL headers that need them. For convenience, you can use the included version here or your own version as long as the API remains compatible.
