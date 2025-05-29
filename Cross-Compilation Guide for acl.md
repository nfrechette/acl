# acl cross compilation guide

## Introduction

This library is verified based on the image of OpenHarmony5.0 Release version on the Orange Pi opi5max development board

## Getting Started

The acl compilation scheme introduced in this chapter is：
* [Download this repository](#download_tag)
* [Preparing OpenHarmony SDK](#sdk_prepare_tag)
* [Compiling Source Code via CMake](#cmake_tag)
* [Verifying Compiled Outputs](#check_binary_tag)
* [Compiling Source Code via configure](#configure_tag)


### Prerequisites


#### Download this repository <a id="download_tag"></a>

1. Clone the project to your local machine

   ```shell
   git clone https://github.com/nfrechette/acl.git
   ```
   
2. In addition to pulling the code here, you also need to enter the acl source code path and execute git submodule update --init --recursive to complete the code.
   
   ```shell
   git submodule update --init --recursive
   ```

#### Preparing OpenHarmony SDK <a id="sdk_prepare_tag"></a>

OpenHarmony provides SDKs for Linux, Windows, and macOS platforms, enabling cross-compilation across these systems. This guide focuses on Linux-based cross-compilation.

1. Download the SDK for your target platform from the [official release channel](https://gitcode.com/openharmony/docs/blob/master/en/release-notes/OpenHarmony-v5.0.1-release.md#acquiring-source-code-from-mirrors).
2. Extract the SDK package:

   ```shell
   owner@ubuntu:~/workspace$ tar -zxvf ohos-sdk-windows_linux-public.tar.gz
   ```

3. Navigate to the SDK's Linux directory and extract all toolchain packages:
   ```shell
   owner@ubuntu:~/workspace$ cd ohos_sdk/linux
   owner@ubuntu:~/workspace/ohos-sdk/linux$ for i in *.zip;do unzip ${i};done                  
   owner@ubuntu:~/workspace/ohos-sdk/linux$ ls
   ets-linux-x64-5.0.2.123-Release.zip             #ArkTS compiler tools
   js-linux-x64-5.0.2.123-Release.zip              #JS compiler tools
   native-linux-x64-5.0.2.123-Release.zip          #C/C++ cross-compilation tools
   previewer-linux-x64-5.0.2.123-Release.zip       #App preview tools
   toolchains-linux-x64-5.0.2.123-Release.zip      #Utilities (e.g., signing tool, device connector)
   ```
   
### Compiling CMake Projects <a id="cmake_tag"></a>

1. Create a build directory:

   In order not to pollute the source code directory files, we recommend creating a compilation directory in the third-party library source directory to generate the configuration files that need to be compiled. In this use case, we create a build directory in the acl directory:

   ```shell
   owner@ubuntu:~/workspace$ cd acl                                   # Enter the acl directory
   owner@ubuntu:~/workspace/acl$ mkdir build && cd build              # Create and enter build directory
   owner@ubuntu:~/workspace/acl/build$
   ```

2. Configure cross-compilation parameters and generate Makefile. In this use case, we use the cmake tool and configuration file in the SDK for compilation (the corresponding path is: SDKPATH=~/workspace/ohos-sdk/linux/native/build-tools/cmake). The reference command is as follows:

   ```shell
   owner@ubuntu:~/workspace/acl/build$ {SDKPATH}/bin/cmake -DCMAKE_TOOLCHAIN_FILE=/workspace/ohos-sdk/linux/native/build/cmake/ohos.toolchain.cmake -DCMAKE_INSTALL_PREFIX={INSTALL_PATH} -DOHOS_ARCH=arm64-v8a .. -L             # Execute the cmake command. After cmake is successfully executed, a Makefile file is generated in the current directory.
   ```

   **Notes:** 
   - Use the CMake executable from the SDK, **NOT** your system's default CMake.
   - Key parameters:
   1) CMAKE_TOOLCHAIN_FILE: Path to the cross-compilation configuration file in the SDK.
   2) CMAKE_INSTALL_PREFIX: Installation path for compiled outputs.
   3) OHOS_ARCH: Target architecture (`arm64-v8a` for 64-bit, `armeabi-v7a` for 32-bit).
   4) -L: Display configurable items in cmake

3. Execute compilation:

   ```shell
   owner@ubuntu:~/workspace/acl/build$ make                 
   ```

4. Install outputs:

   After successful compilation, we can execute make install to install the compiled binary files and header files to the installation path configured by cmake:

   ```shell
   owner@ubuntu:~/workspace/acl/build$ make install                
   ```

5. Verify compiled binaries <a id="check_binary_tag"></a>

   After successful compilation, a bin directory is generated under the configuration and installation path of the tripartite library. After entering the bin directory, we can view the properties of the file through the file command to determine whether the cross-compilation is successful. The following information shows that the compiled binary output is an aarch64 architecture file, which means that the cross-compilation is successful:

   ```shell
   owner@ubuntu:~/workspace/{INSTALL_PATH}/bin$ file acl_compressor    
    
   acl_compressor: ELF 64-bit LSB shared object, ARM aarch64, version 1 (SYSV), dynamically linked, interpreter/lib/ld-musl-aarch64
   so.1, BuildID[sha1]=c0aaff0b401feef924f074a6cb7d19b5958f74f5, with debug_info, not stripped
   ```

### Compiling configure-Based Projects <a id="configure_tag"></a>

1. Review configuration options:
   ```shell
   owner@ubuntu:~/workspace/acl$ ./configure --help
   ```

2. Set cross-compilation environment variables (for 64-bit ARM):
   ```shell
   export OHOS_SDK=/home/owner/tools/OHOS_SDK/ohos-sdk/linux/
   export AS=${OHOS_SDK}/native/llvm/bin/llvm-as
   export CC="${OHOS_SDK}/native/llvm/bin/clang --target=aarch64-linux-ohos"
   export CXX="${OHOS_SDK}/native/llvm/bin/clang++ --target=aarch64-linux-ohos"
   export LD=${OHOS_SDK}/native/llvm/bin/ld.lld
   export STRIP=${OHOS_SDK}/native/llvm/bin/llvm-strip
   export RANLIB=${OHOS_SDK}/native/llvm/bin/llvm-ranlib
   export OBJDUMP=${OHOS_SDK}/native/llvm/bin/llvm-objdump
   export OBJCOPY=${OHOS_SDK}/native/llvm/bin/llvm-objcopy
   export NM=${OHOS_SDK}/native/llvm/bin/llvm-nm
   export AR=${OHOS_SDK}/native/llvm/bin/llvm-ar
   export CFLAGS="-fPIC -D__MUSL__=1"      # For 32-bit: add "-march=armv7a"
   export CXXFLAGS="-fPIC -D__MUSL__=1"    # For 32-bit: add "-march=armv7a"
   ```

3. Run configure with cross-compilation parameters:
   ```shell
   owner@ubuntu:~/workspace/acl$ ./configure --prefix=/home/owner/workspace/acl --host=aarch64-linux
   ```

4. Compile and install:
   ```shell
   owner@ubuntu:~/workspace/acl$ make
   owner@ubuntu:~/workspace/acl$ make install
   ```

