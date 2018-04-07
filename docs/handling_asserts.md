# Handling asserts

This library uses a simple system to handle asserts. Asserts are fatal and must terminate otherwise the behavior is undefined if execution continues.

A total of 4 behaviors are supported:

*  We can print to `stderr` and `abort`
*  We can `throw` and exception
*  We can call a custom function
*  Do nothing and strip the check at compile time (**default behavior**)

Everything necessary is implemented in [**acl/core/error.h**](../includes/acl/core/error.h).

## Aborting

In order to enable the aborting behavior, simply define the macro `ACL_ON_ASSERT_ABORT`:

`#define ACL_ON_ASSERT_ABORT`

## Throwing

In order to enable the throwing behavior, simply define the macro `ACL_ON_ASSERT_THROW`:

`#define ACL_ON_ASSERT_THROW`

Note that the type of the exception thrown is `std::runtime_error`.

## Custom function

In order to enable the custom function calling behavior, define the macro `ACL_ON_ASSERT_CUSTOM` with the name of the function to call.

`#define ACL_ON_ASSERT_CUSTOM on_custom_assert_impl`

Note that the function signature is as follow: `void on_custom_assert_impl(const char* expression, int line, const char* file, const char* format, ...) {}`

## No checks

By default if no macro mentioned above is defined, all asserts will be stripped at compile time.
