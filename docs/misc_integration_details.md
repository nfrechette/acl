# Other integration considerations

## Useful defines

A small number of defines are exposed and can be used to finetune the behavior.

### RTM_NO_INTRINSICS

This define prevents the usage of intrinsics unless explicitly requested by the integration manually (e.g. you can still defined `RTM_SSE2_INTRINSICS` yourself). Everything will default to pure scalar implementations.

### ACL_USE_POPCOUNT

This enables the usage of the `POPCNT` intrinsics [when available](https://en.wikipedia.org/wiki/Bit_Manipulation_Instruction_Sets) on x86/x64 CPUs. It is currently not possible to determine at compile time when it is supported. For example *Haswell* CPUs have support for AVX2 but not `POPCNT`. The macro is automatically enabled on *Xbox One* but not yet on *PlayStation 4* (even though it is supported, contributions welcome).

### ACL_USE_SJSON

ACL uses `sjson-cpp` to output stats as well as to read/write ASCII human readable clips. Enable this define to use these features and make sure `sjson-cpp/includes` is in the include path.

## Floating point contraction

A compiler can merge a multiply and an add into one FMA instruction. This is floating point contraction. The FMA does not round the product, thus the result can change with the compiler, the flags, and the target. For example, the compressed data or the decompressed values can differ in the last bits between two targets. For the same input, ACL gives the same result on all the targets only when the compiler does not merge these operations. ACL and [Realtime Math](https://nfrechette.github.io/rtm/) have the same behavior.

* With MSVC, the headers set `float_control(precise, on)`. Do not use `/fp:fast` or `/fp:contract`.
* With Clang, the headers disable contraction on x86, x64, AArch64 (Clang 16 and later, Apple Clang 15 and later), and RISC-V (Clang 18 and later). With `-ffp-contract=fast`, LLVM Clang ignores the pragma after it inlines a function. Use `-ffp-contract=on`, `-ffp-contract=off`, or `-ffp-contract=fast-honor-pragmas`.
* With GCC, the headers do not disable contraction. GCC uses `-ffp-contract=fast` by default in C++, and it merges the operations of a function after it inlines the function in the code of the caller. A pragma in the headers does not stop this. Also, GCC does not inline a function when its optimization options are not the same as those of the caller: such a pragma makes the code slower. Compile with `-ffp-contract=off` to get the same results on all the targets.
