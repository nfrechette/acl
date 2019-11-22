# Other integration considerations

## Useful defines

A small number of defines are exposed and can be used to finetune the behavior.

### RTM_NO_INTRINSICS

This define prevents the usage of intrinsics unless explicitly requested by the integration manually (e.g. you can still defined `RTM_SSE2_INTRINSICS` yourself). Everything will default to pure scalar implementations.

### ACL_USE_POPCOUNT

This enables the usage of the `POPCNT` intrinsics [when available](https://en.wikipedia.org/wiki/Bit_Manipulation_Instruction_Sets) on x86/x64 CPUs. It is currently not possible to determine at compile time when it is supported. For example *Haswell* CPUs have support for AVX2 but not `POPCNT`. The macro is automatically enabled on *Xbox One* but not yet on *PlayStation 4* (even though it is supported, contributions welcome).

