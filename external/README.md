# External dependencies

Good news! There are no external dependencies needed by this library at runtime unless you use the [ACL file format](../docs/the_acl_file_format.md)!

## Catch

[Catch 1.9.6](https://github.com/catchorg/Catch2/releases/tag/v1.9.6) is used by our [unit tests](../tests). You will only need it if you run the unit tests and it is included as-is without modifications.

## sjson-cpp

[sjson-cpp 0.4.0-develop](https://github.com/nfrechette/sjson-cpp/releases/tag/v0.4.0) is used by our ACL file format [clip reader](../includes/acl/io/clip_reader.h) and [clip writer](../includes/acl/io/clip_writer.h) as well as by the [acl_compressor](../tools/acl_compressor) tool used for regression testing and profiling. Unless you use our ACL file format at runtime (which you shouldn't), you will not have this dependency included at all.

In fact, to use any of these things you must include `sjson-cpp` relevant headers manually before you include the ACL headers that need them. For convenience, you can use the included version here or your own version as long as the API remains compatible.
