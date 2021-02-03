@echo off

REM Unpack arguments
SET WORKER_IMAGE=%1
SET PLATFORM=%2
SET CONFIG=%3
SET TOOLCHAIN=%4
SET PYTHON_PATH=%5

echo Worker image: %WORKER_IMAGE%
echo Platform: %PLATFORM%
echo Config: %CONFIG%
echo Toolchain: %TOOLCHAIN%
echo Python path: %PYTHON_PATH%

REM Convert the build image and toolchain into our compiler string
IF /i %TOOLCHAIN%==msvc GOTO :msvc
IF /i %TOOLCHAIN%==clang GOTO :clang

echo Unknown toolchain: %TOOLCHAIN%
exit /B 1

:msvc
IF /i %WORKER_IMAGE%=="Visual Studio 2015" SET COMPILER=vs2015
IF /i %WORKER_IMAGE%=="Visual Studio 2017" SET COMPILER=vs2017
IF /i %WORKER_IMAGE%=="Visual Studio 2019" SET COMPILER=vs2019
IF /i %WORKER_IMAGE%=="Previous Visual Studio 2019" SET COMPILER=vs2019
GOTO :next

:clang
IF /i %WORKER_IMAGE%=="Visual Studio 2019" SET COMPILER=vs2019-clang
IF /i %WORKER_IMAGE%=="Previous Visual Studio 2019" SET COMPILER=vs2019-clang

REM HACK!!! Disable clang build for now with appveyor since vcpkg breaks the compiler detection of cmake
REM Fake build success
echo VS2019 clang build is disabled for now due to issues with vcpkg breaking compiler detection with cmake
exit /B 0

GOTO :next

:next
REM Set our switch if we need to run unit tests
SET UNIT_TEST_FLAG=-unit_test
IF /i %PLATFORM%==arm64 SET UNIT_TEST_FLAG=

REM If PYTHON_PATH isn't set, assume it is in PATH
IF NOT DEFINED PYTHON_PATH SET PYTHON_PATH=python.exe

REM Build and run unit tests
%PYTHON_PATH% make.py -build %UNIT_TEST_FLAG% -compiler %COMPILER% -config %CONFIG% -cpu %PLATFORM% -clean
IF NOT %ERRORLEVEL% EQU 0 GOTO :build_failure

%PYTHON_PATH% make.py -build %UNIT_TEST_FLAG% -compiler %COMPILER% -config %CONFIG% -cpu %PLATFORM% -nosimd
IF NOT %ERRORLEVEL% EQU 0 GOTO :build_failure

REM Done!
exit /B 0

:build_failure
echo Build failed!
exit /B 1
