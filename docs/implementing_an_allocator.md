# Implementing an allocator

The interface is very simple and provided in [**acl/core/iallocator.h**](../includes/acl/core/iallocator.h). ACL also provides an implementation that uses the system `malloc` and `free` through [**acl/core/ansi_allocator.h**](../includes/acl/core/ansi_allocator.h). You can use it as is or for inspiration to implement your own.

Only two functions are exposed and required by the interface:

*  `allocate(size_t size, size_t alignment)`
*  `deallocate(void* ptr, size_t size)`

The `deallocate` function will be provided with the same size used to allocate the memory.

There is no global allocator instance to set or use. Instead, every function that might allocate memory takes an explicit allocator argument. This avoids the need for global state which might impede thread safety and helps keep the library 100% headers.
