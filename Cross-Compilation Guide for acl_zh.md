# acl交叉编译指导

## 简介

本库是在香橙派opi5max开发板上基于OpenHarmony5.0 Release版本的镜像验证

## 入门指南

本章介绍的acl编译方案为：
* [下载本仓库](#download_tag)
* [OpenHarmony SDK 准备](#sdk_prepare_tag)
* [CMake方式编译源码](#cmake_tag)
* [如何进行测试验证编译产物](#check_binary_tag)


### 编译前准备


#### 下载本仓库 <a id="download_tag"></a>

1. 将项目克隆到本地

   ```shell
   git clone https://github.com/nfrechette/acl
   ```

2. 在这里拉完代码之外，还要进入acl源码路径执行git submodule update --init --recursive，代码才是完整的
   
   ```shell
   git submodule update --init --recursive
   ```

#### OpenHarmony SDK 准备 <a id="sdk_prepare_tag"></a>

OpenHarmony提供 linux/windwos以及mac平台的几种SDK，开发者可以在linux、windwos以及mac平台上进行交叉编译。本文以linux平台为主进行交叉编译的讲解。

1. 从 OpenHarmony SDK [官方发布渠道](https://gitcode.com/openharmony/docs/blob/master/zh-cn/release-notes/OpenHarmony-v5.0.2-release.md#%E4%BB%8E%E9%95%9C%E5%83%8F%E7%AB%99%E7%82%B9%E8%8E%B7%E5%8F%96) 下载对应版本的SDK。

2. 解压SDK

   ```shell
   owner@ubuntu:~/workspace$ tar -zxvf ohos-sdk-windows_linux-public.tar.gz
   ```

3. 进入到sdk的linux目录，解压对应工具包：

   ```shell
   owner@ubuntu:~/workspace$ cd ohos_sdk/linux
   owner@ubuntu:~/workspace/ohos-sdk/linux$ for i in *.zip;do unzip ${i};done                   
   owner@ubuntu:~/workspace/ohos-sdk/linux$ ls
   ets-linux-x64-5.0.2.123-Release.zip             #arkts 编译工具
   js-linux-x64-5.0.2.123-Release.zip              #js 编译工具
   native-linux-x64-5.0.2.123-Release.zip          #c/c++ 交叉编译工具
   previewer-linux-x64-5.0.2.123-Release.zip       #应用预览工具
   toolchains-linux-x64-5.0.2.123-Release.zip      #实用工具，如应用签名工具，设备连接工具
   ```

### CMake 项目编译构建 <a id="cmake_tag"></a>

1. 新建编译目录

   为了不污染源码目录文件，我们推荐在三方库源码目录新建一个编译目录，用于生成需要编译的配置文件，本用例中我们在acl目录下新建一个build目录：

   ```shell
   owner@ubuntu:~/workspace$ cd acl                                   # 进入acl目录
   owner@ubuntu:~/workspace/acl$ mkdir build && cd build              # 创建编译目录并进入到编译目录
   owner@ubuntu:~/workspace/acl/build$
   ```

2. 配置交叉编译参数，生成Makefile，本用例中，我们采用SDK中的cmake工具以及配置文件进行编译（对应的路径为：SDKPATH=~/workspace/ohos-sdk/linux/native/build-tools/cmake），参考命令如下：

   ```shell
   owner@ubuntu:~/workspace/acl/build$ {SDKPATH}/bin/cmake -DCMAKE_TOOLCHAIN_FILE=/workspace/ohos-sdk/linux/native/build/cmake/ohos.toolchain.cmake  -DCMAKE_CXX_FLAGS="-Wno-unused-command-line-argument" -DCMAKE_INSTALL_PREFIX={INSTALL_PATH} -DHILOG_LIB_PATH={HILOG_LIB_PATH} -DOHOS_ARCH=arm64-v8a .. -L             # 执行cmake命令,执行完cmake成功后在当前目录生成Makefile文件
   ```

   **注意这里执行的 cmake 必须是 SDK 内的 cmake，不是你自己系统上原有的 cmake 。否则会不识别参数OHOS_ARCH。**

   参数说明：
   1) CMAKE_TOOLCHAIN_FILE: 交叉编译置文件路径，必须设置成工具链中的配置文件。
   2) CMAKE_INSTALL_PREFIX: 配置安装三方库路径。
   3) OHOS_ARCH: 配置交叉编译的CPU架构，一般为arm64-v8a(编译64位的三方库)、armeabi-v7a(编译32位的三方库)，本示例中我们设置编译64位的源码库。
   4) -L: 显示cmake中可配置项目

3. 执行编译

   cmake执行成功后，在build目录下生成了Makefile，我们就可以直接执行make对源代码进行编译了：

   ```shell
   owner@ubuntu:~/workspace/acl/build$ make                  # 执行make命令进行编译，执行完毕后本文件夹可以看到编译后结果
   ```

4. 执行安装命令

   编译成功后，我们可以执行make install将编译好的二进制文件以及头文件安装到cmake配置的安装路径下：

   ```shell
   owner@ubuntu:~/workspace/acl/build$ make install                # 执行安装命令
   ```

5. 查看编译后文件属性 <a id="check_binary_tag"></a>

   编译成功后，在三方库的配置安装路径下会生成一个bin目录。进入bin目录后，我们可以通过file命令查看文件的属性来判断交叉编译是否成功。如下信息显示编译后的二进制输出是一个aarch64架构的文件，即表示交叉编译成功：

   ```shell
   owner@ubuntu:~/workspace/{INSTALL_PATH}/bin$ file acl_compressor     # 查看文件属性命令
   
   acl_compressor: ELF 64-bit LSB shared object, ARM aarch64, version 1 (SYSV), dynamically linked, interpreter/lib/ld-musl-aarch64
   so.1, BuildID[sha1]=c0aaff0b401feef924f074a6cb7d19b5958f74f5, with debug_info, not stripped
   ```   

