# Getting started

In order to contribute to ACL and use the various tools provided for development and research, you will first need to setup your environment.

## Setting up your environment

### Windows, Linux, and OS X for x86 and x64

1. Install *CMake 3.2* or higher (*3.14* for Visual Studio 2019, or *3.10* on OS X with *Xcode 10*), *Python 2.7 or 3*, and the proper compiler for your platform.
2. Execute `git submodule update --init` to get the files of external submodules (e.g. Catch2).
3. Generate the IDE solution with: `python make.py`  
   The solution is generated under `./build`
4. Build the IDE solution with: `python make.py -build`
5. Run the unit tests with: `python make.py -unit_test`
6. Run the regression tests with: `python make.py -regression_test`
7. Run the clang static analyzer on the public headers with: `python make.py -analyze`

On all three platforms, *AVX* support can be enabled by using the `-avx` switch.

The `-analyze` switch runs the clang static analyzer on the include validation files, one file for each public header. It analyzes the functions in the headers and enables the array bound checker. It needs clang and a Makefile or Ninja generator: use the default compiler on OS X, or `-compiler clang19` (or a later version) on Linux. It prints each issue one time and returns an error code if it finds an issue. On GitHub Actions, it also prints each issue as an annotation on the line of the issue. The other switches apply: for example, `-nosimd` analyzes the scalar code path.

### Windows ARM64

For *Windows on ARM64*, the steps are identical to *x86 and x64* but you will need *CMake 3.13 or higher* and you must provide the architecture on the command line: `python make.py -compiler vs2017 -cpu arm64`

### Linux ARMv7

For native *Linux on ARMv7*, the steps are identical to *x86 and x64* and `make.py` selects the `armv7` architecture automatically. You can generate build files with: `python make.py -cpu armv7`

With SIMD enabled, the build adds `-mfpu=neon` because the Linux `armhf` compilers do not enable NEON by default.

### Linux riscv64

For native *Linux on riscv64*, the steps are identical to *x86 and x64* and `make.py` selects the `riscv64` architecture automatically. You can generate build files with: `python make.py -cpu riscv64`

RTM has no SIMD code for RISC-V, so the build uses the scalar code path. The `-avx`, `-avx2`, and `-pop` switches are not supported with `riscv64`.

`make.py` does not cross-compile for `riscv64`. To cross-compile, use CMake directly with a RISC-V compiler and toolchain file.

### Android

For *Android*, the steps are identical to *Windows, Linux, and OS X* but you also need to install *Android NDK 21* (or higher). The build uses `gradle` and `-unit_test` as well as `-regression_test` will deploy and run on the device when executed (make sure that the `adb` executable is in your `PATH` for this to work).

*Android Studio v3.5* can be used to launch and debug. After running *CMake* to build and generate everything, the *Android Studio* projects can be found under the `./build` directory.

### iOS

For *iOS*, the steps are identical to the other platforms but due to code signing, you will need to perform the builds from *Xcode* manually. Note that this is only an issue if you attempt to use the tools or the unit tests locally.

In order to run these manually:

*  Open the *Xcode* project for the appropriate tool
*  In the project settings, enable automatic code signing and select your development team
*  Build and run on your device

Note that *iOS* builds have never been tested on an emulator.

### Emscripten

Emscripten support currently only has been tested on OS X and Linux. To use it, make sure to install a recent version of Emscripten SDK 1.39.11+.

By default, `make.py` compiles the Emscripten unit tests and the `acl_compressor` tool with WebAssembly SIMD (`-msimd128`). To test the scalar code path, use `-nosimd`. The unit tests run with Node.js, which must support WebAssembly SIMD (version 16.4 or later).

## Commit message format

This library uses the [angular.js message format](https://github.com/angular/angular.js/blob/master/DEVELOPERS.md#commits) and it is enforced with commit linting through every pull request.

## Generating the stats and graphs

See [here](graph_generation.md) to find out how the various statistics and graphs we track are generated.
