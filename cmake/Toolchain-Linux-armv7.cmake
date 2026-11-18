# Cross-compile for armv7 (armhf) on a Linux host that is not armv7.
# make.py selects the compiler: the GCC cross compiler (arm-linux-gnueabihf-g++-XX) or Clang.
# Clang uses the headers, the libraries, and the binutils of the GCC cross toolchain.

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR armv7)

# CMake gives the target to Clang only, GCC ignores it.
# With this target, Clang uses the same defaults as the native armhf compilers: armv7-a, VFPv3-D16, and no NEON.
set(CMAKE_CXX_COMPILER_TARGET arm-linux-gnueabihf)

# qemu-arm runs the armv7 programs: the discovery of the unit tests and the unit tests.
# QEMU finds the armv7 dynamic loader and the armv7 libraries in the directory of the GCC cross toolchain.
set(CMAKE_CROSSCOMPILING_EMULATOR qemu-arm -L /usr/arm-linux-gnueabihf)
