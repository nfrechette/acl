# Implementing an allocator

At the moment, the decompression makes no allocations. As such, for decompression it is not necessary to implement or provide an allocator.

On the other hand, the compression performs a number of allocations and must be provided an allocator. The interface is very simple and provided in [**acl/core/memory.h**](https://github.com/nfrechette/acl/blob/develop/includes/acl/core/memory.h).

Only two functions are exposed and required:

*  `allocate(size_t size, size_t alignment)`
*  `deallocate(void* ptr, size_t size)`

The `deallocate` function will be provided with the same size used to allocate the memory.

There is no global allocator instance to set or use. Instead, every function that might allocate memory takes an explicit allocator argument. This avoids the need for global state which might impede thread safety and helps keep the library 100% headers.
