# Getting started

In order to contribute to ACL and use the various tools provided for development and research, you will first need to setup your environment.

## Setting up your environment

### Windows, Linux, and OS X

1. Install *CMake 3.2* or higher, *Python 3*, and the proper compiler for your platform
2. Generate the IDE solution with: `python make.py`  
   The solution is generated under `./build`  
   Note that if you do not have CMake in your `PATH`, you should define the `ACL_CMAKE_HOME` environment variable to something like `C:\Program Files\CMake`.
3. Build the IDE solution with: `python make.py -build`
4. Run the unit tests with: `python make.py -unit_test`
5. Run the regression tests with: `python make.py -regression_test`

On all three platforms, *AVX* support can be enabled by using the `-avx` switch.

### Android

For *Android*, the steps are identical to *Windows, Linux, and OS X* but you also need to install *NVIDIA CodeWorks 1R5* (or higher).

Note that it is not currently possible to run the unit tests or the regression tests with scripts, you will need to run them manually from Visual Studio:

*  Open the Visual Studio solution
*  Build and run on your device

Note that Android builds have never been tested on an emulator and that if you cannot code sign the APK, you will need to change the project ANT settings to use the debug configuration.

*We currently only support NVIDIA CodeWorks as that is what is used by Unreal 4 to build. Contributions welcome to also support the NDK natively with CMake*

### iOS

For *iOS*, the steps are identical to the other platforms but due to code signing, you will need to perform the builds from *Xcode* manually. Note that this is only an issue if you attempt to use the tools or the unit tests locally.

In order to run these manually:

*  Open the *Xcode* project for the appropriate tool
*  In the project settings, enable automatic code signing and select your development team
*  Build and run on your device

Note that *iOS* builds have never been tested on an emulator.

## Generating the stats and graphs

See [here](graph_generation.md) to find out how the various statistics and graphs we track are generated.
