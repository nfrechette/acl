# Configuration macros

ACL is a header-only library, thus macros configure it. Define a macro on the command line of the compiler (for example `-DACL_USE_POPCOUNT`), or before you include the first ACL header. [acl/config.h](https://github.com/nfrechette/acl/blob/develop/includes/acl/config.h) lists the macros with more details.

All the C++ files of a program must use the same macros. Otherwise, the same function can have different definitions in two files, and the behavior is undefined.

ACL uses [Realtime Math](https://nfrechette.github.io/rtm/) (RTM) for its math. The RTM macros (for example `RTM_NO_INTRINSICS` or `RTM_ON_ASSERT_THROW`) also apply to the code of ACL.

## Asserts

By default, ACL removes its asserts at compile time. Define one of these macros to keep them. An assert is fatal: if the program continues after an assert fails, the behavior is undefined. See [handling asserts](handling_asserts.md).

| Macro | Effect |
| --- | --- |
| `ACL_ON_ASSERT_ABORT` | A failed assert prints its message to `stderr` and calls `std::abort()`. |
| `ACL_ON_ASSERT_THROW` | A failed assert throws an `acl::runtime_assert` exception. |
| `ACL_ON_ASSERT_CUSTOM` | A failed assert calls the function that the macro names. Its signature is `void on_assert(const char* expression, int line, const char* file, const char* format, ...)`. |
| `ACL_ASSERT` | With `ACL_ON_ASSERT_CUSTOM`, define `ACL_ASSERT(expression, format, ...)` to replace the complete assert implementation. |
| `ACL_ASSERT_STRING_FORMAT_SPECIFIER` | The format specifier that the assert messages use for a `const char*` argument. The default is `"%s"`. With a custom function that expects wide strings, MSVC can use `"%hs"`. |

When one of the first three macros is defined, ACL defines `ACL_HAS_ASSERT_CHECKS`. Your code can test it.

## Hardware features

ACL detects the SIMD instruction sets with RTM. These macros enable x86 and x64 instructions that RTM does not detect.

| Macro | Effect |
| --- | --- |
| `ACL_USE_POPCOUNT` | Use the `popcnt` and `lzcnt` instructions to count bits. If the processor does not support them, the behavior is undefined. Windows 11 requires them. ACL defines this macro on Xbox One. |
| `ACL_BMI_INTRINSICS` | Use the BMI instructions. If the processor does not support them, the behavior is undefined. Windows 11 requires them. ACL defines this macro on Xbox One, when the compiler defines `__BMI__` (for example with `-mbmi`), and with MSVC when AVX is enabled. |
| `ACL_NO_8_WIDE_AVX` | Do not use the 8-wide AVX instructions when AVX is enabled. On older processors that split each 8-wide instruction in two (for example AMD Jaguar), the 4-wide code can be faster. |

Both `ACL_USE_POPCOUNT` and `ACL_BMI_INTRINSICS` stop the compilation on a processor that is not x86 or x64.

## Decompression performance

| Macro | Effect |
| --- | --- |
| `ACL_NO_EARLY_PREFETCHING` | Do not start the memory prefetches as early as possible during decompression. ACL starts them early on all platforms except x86 and x64. On a processor with a high clock rate, the code can be faster without early prefetches. |

## Clip files

| Macro | Effect |
| --- | --- |
| `ACL_USE_SJSON` | Compile the clip reader and the clip writer in `acl/io/`. They read and write the [ACL file format](the_acl_file_format.md). The [sjson-cpp](https://github.com/nfrechette/sjson-cpp) headers must be on the include path. Without this macro, the headers in `acl/io/` are empty. |

## Namespace and versions

| Macro | Effect |
| --- | --- |
| `ACL_ENABLE_VERSION_NAMESPACE` | Put the code of ACL in a versioned namespace in `acl` (for example `acl::v2199`). By default, the namespace is inline: client code still uses `acl::`. Two versions of ACL can then be in the same program. |
| `ACL_NO_INLINE_NAMESPACE` | With `ACL_ENABLE_VERSION_NAMESPACE`, the versioned namespace is not inline: client code must use the versioned name. |
| `ACL_NO_VERSION_NAMESPACE` | Do not use a versioned namespace, even when `ACL_ENABLE_VERSION_NAMESPACE` is defined. ACL defines this macro with Visual Studio 2015. |
| `ACL_NO_DEPRECATION` | Do not show deprecation warnings when your code uses deprecated API. |
| `ACL_USE_CPP11_STD_MEMORY_ORDER` | With C++20 or later, use the C++11 names of `std::memory_order`. Use it when a modern compiler uses an older standard library that does not have the C++20 names. |

## Allocator debugging

These macros control the checks of `acl::ansi_allocator`. The sanitizing and the tracking are active only when an assert macro is defined.

| Macro | Effect |
| --- | --- |
| `ACL_NO_ALLOCATOR_SANITIZING` | Do not fill new memory with `0xCD` and freed memory with `0xFE`. These patterns help to find uninitialized memory and the use of freed memory. |
| `ACL_NO_ALLOCATOR_TRACKING` | Do not count the allocations and the deallocations. The count helps to find memory leaks and memory that is freed twice. |
| `ACL_ALLOCATOR_TRACK_ALL_ALLOCATIONS` | Record each allocation, to find memory leaks and memory that is freed twice. Use it only to debug: it is slow. |
