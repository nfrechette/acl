# Handling asserts

When integrating ACL, it is very easy to hook up asserts of your own. By default, the library uses the standard C++ `assert(..);` but it is stripped out in configurations other than **Debug**. To override the library asserts and plug in your own, simply define the appropriate asserts before including any ACL header.

*  `ACL_NO_ERROR_CHECKS`: This macro, if defined, disabled all asserts regardless of the configuration.
*  `ACL_ASSERT`: This macro handles recoverable assertions. Skipping these is safe in the sense that the library should handle these cases and not crash. The behavior might not end up being what you expect or want.
*  `ACL_ENSURE`: This macro handles fatal assertions. Skipping these is **NOT** safe.

These macros are defined and implemented in [**acl/core/error.h**](https://github.com/nfrechette/acl/blob/develop/includes/acl/core/error.h).
