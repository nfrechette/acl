# coding: utf-8

from __future__ import print_function

import argparse
import json
import multiprocessing
import multiprocessing.pool
import os
import platform
import re
import shlex
import shutil
import subprocess
import sys
import threading
import time
import urllib.request
import zipfile

# The current test data version in use
REGRESSION_TEST_DATA_SOURCE_URL = 'https://github.com/nfrechette/acl-test-data/releases/download/v4/'
CURRENT_REGRESSION_TEST_DATA_FILENAME = 'acl_regression_tests_v4.zip'

# The current decompression data version in use
current_decomp_data = 'decomp_data_v8'

def is_host_cpu_arm64():
	return platform.machine() == 'arm64' or platform.machine() == 'aarch64'

def is_host_cpu_armv7():
	# armv8l is a 32-bit ARM user space on a 64-bit ARM processor
	return platform.machine() == 'armv7l' or platform.machine() == 'armv8l'

def is_host_cpu_riscv64():
	return platform.machine() == 'riscv64'

def parse_argv():
	parser = argparse.ArgumentParser(add_help=False)

	actions = parser.add_argument_group(title='Actions', description='If no action is specified, on Windows, OS X, and Linux the solution/make files are generated.  Multiple actions can be used simultaneously.')
	actions.add_argument('-build', action='store_true')
	actions.add_argument('-clean', action='store_true')
	actions.add_argument('-clean_only', action='store_true')
	actions.add_argument('-unit_test', action='store_true')
	actions.add_argument('-regression_test', action='store_true')
	actions.add_argument('-bench', action='store_true')
	actions.add_argument('-run_bench', action='store_true')
	actions.add_argument('-pull_bench', action='store_true')	# Android only
	actions.add_argument('-convert', help='Input/Output directory to convert')
	actions.add_argument('-analyze', action='store_true', help='Run the clang static analyzer on the public headers')

	target = parser.add_argument_group(title='Target')
	target.add_argument('-compiler', choices=['vs2015', 'vs2017', 'vs2019', 'vs2022', 'vs2026', 'vs2019-clang', 'vs2022-clang', 'vs2026-clang', 'android', 'clang4', 'clang5', 'clang6', 'clang7', 'clang8', 'clang9', 'clang10', 'clang11', 'clang12', 'clang13', 'clang14', 'clang15', 'clang16', 'clang17', 'clang18', 'clang19', 'clang20', 'clang21', 'clang22', 'clang23', 'gcc4.9', 'gcc5', 'gcc6', 'gcc7', 'gcc8', 'gcc9', 'gcc10', 'gcc11', 'gcc12', 'gcc13', 'gcc14', 'gcc15', 'osx', 'ios', 'emscripten'], help='Defaults to the host system\'s default compiler')
	target.add_argument('-config', choices=['Debug', 'Release'], type=str.capitalize)
	target.add_argument('-cpu', choices=['x86', 'x64', 'armv7', 'arm64', 'arm64ec', 'riscv64', 'wasm'], help='Defaults to the host system\'s architecture')
	target.add_argument('-cpp_version', choices=['11', '14', '17', '20'], help='Defaults to C++11')

	misc = parser.add_argument_group(title='Miscellaneous')
	misc.add_argument('-avx', dest='use_avx', action='store_true', help='Compile using AVX instructions on Windows, OS X, and Linux')
	misc.add_argument('-avx2', dest='use_avx2', action='store_true', help='Compile using AVX2 instructions on Windows and Linux')
	misc.add_argument('-pop', dest='use_popcnt', action='store_true', help='Compile using the POPCNT instruction')
	misc.add_argument('-nosimd', dest='use_simd', action='store_false', help='Compile without SIMD instructions')
	misc.add_argument('-simd', dest='use_simd', action='store_true', help='Compile with default SIMD instructions')
	misc.add_argument('-nosjson', dest='use_sjson', action='store_false', help='Compile without SJSON support')
	misc.add_argument('-allwarnings', dest='allwarnings', action='store_true', help='Enable all warnings, MSVC only')
	misc.add_argument('-num_threads', type=int, help='No. to use while compiling and regressing')
	misc.add_argument('-tests_matching', help='Only run tests whose names match this regex')
	misc.add_argument('-fast_math', action='store_true', help='Compile the include validation with fast math')
	misc.add_argument('-ci', action='store_true', help='Whether or not this is a Continuous Integration build')
	misc.add_argument('-help', action='help', help='Display this usage information')

	num_threads = multiprocessing.cpu_count()
	if platform.system() == 'Linux' and sys.version_info >= (3, 4):
		num_threads = len(os.sched_getaffinity(0))
	if not num_threads or num_threads == 0:
		num_threads = 4

	parser.set_defaults(build=False, clean=False, clean_only=False, unit_test=False, regression_test=False, bench=False, run_bench=False, pull_bench=False, analyze=False,
		compiler=None, config='Release', cpu=None, cpp_version='11', use_avx=False, use_avx2=False, use_popcnt=False, use_simd=True, use_sjson=True, allwarnings=False,
		num_threads=num_threads, tests_matching='', fast_math=False)

	args = parser.parse_args()

	is_arm64_cpu = is_host_cpu_arm64()
	is_armv7_cpu = is_host_cpu_armv7()
	is_riscv64_cpu = is_host_cpu_riscv64()

	# Sanitize and validate our options
	if (args.use_avx or args.use_avx2) and not args.use_simd:
		print('SIMD is explicitly disabled, AVX and AVX2 will not be used')
		args.use_avx = False
		args.use_avx2 = False

	if args.compiler == 'android':
		if not args.cpu:
			args.cpu = 'arm64'

		if platform.system() != 'Windows':
			print('Android is only supported on Windows')
			sys.exit(1)

		if args.use_avx or args.use_avx2:
			print('AVX and AVX2 are not supported on Android')
			sys.exit(1)

		if args.cpu not in ['armv7', 'arm64']:
			print('{} cpu architecture not in supported list [armv7, arm64] for Android'.format(args.cpu))
			sys.exit(1)
	elif args.compiler == 'ios':
		if not args.cpu:
			args.cpu = 'arm64'

		if platform.system() != 'Darwin':
			print('iOS is only supported on OS X')
			sys.exit(1)

		if args.use_avx or args.use_avx2:
			print('AVX and AVX2 are not supported on iOS')
			sys.exit(1)

		if args.unit_test:
			print('Unit tests cannot run from the command line on iOS')
			sys.exit(1)

		if args.regression_test:
			print('Regression tests cannot run from the command line on iOS')
			sys.exit(1)

		if args.cpu not in ['arm64']:
			print('{} cpu architecture not in supported list [arm64] for iOS'.format(args.cpu))
			sys.exit(1)
	elif args.compiler == 'emscripten':
		if not args.cpu:
			args.cpu = 'wasm'

		if platform.system() != 'Darwin' and platform.system() != 'Linux':
			print('Emscripten is only supported on OS X and Linux')
			sys.exit(1)

		if args.use_avx or args.use_avx2:
			print('AVX and AVX2 are not supported on Emscripten')
			sys.exit(1)

		if args.cpu not in ['wasm']:
			print('{} cpu architecture not in supported list [wasm] for Emscripten'.format(args.cpu))
			sys.exit(1)
	else:
		if not args.cpu:
			if is_arm64_cpu:
				args.cpu = 'arm64'
			elif is_armv7_cpu:
				args.cpu = 'armv7'
			elif is_riscv64_cpu:
				args.cpu = 'riscv64'
			else:
				args.cpu = 'x64'

	if args.cpu == 'arm64':
		is_arm_supported = False

		# Cross compilation
		if args.compiler in ['vs2017', 'vs2019', 'vs2022', 'vs2026', 'ios', 'android']:
			is_arm_supported = True

		# Native compilation
		if platform.system() in ['Darwin', 'Linux'] and is_arm64_cpu:
			is_arm_supported = True

		if not is_arm_supported:
			print('arm64 is only supported with VS2017, VS2019, VS2022, VS2026, OS X (M* processors), Linux, Android, and iOS')
			sys.exit(1)
	elif args.cpu == 'arm64ec':
		if args.compiler not in ['vs2019', 'vs2022', 'vs2026']:
			print('arm64ec is only supported with VS2019, VS2022, and VS2026')
			sys.exit(1)
	elif args.cpu == 'armv7':
		is_armv7_supported = False

		# Cross compilation
		if args.compiler == 'android':
			is_armv7_supported = True

		# Native compilation
		if platform.system() == 'Linux' and is_armv7_cpu:
			is_armv7_supported = True

		if not is_armv7_supported:
			print('armv7 is only supported with Linux and Android')
			sys.exit(1)
	elif args.cpu == 'riscv64':
		# RTM has no SIMD code for RISC-V, so riscv64 uses the scalar code path
		if platform.system() != 'Linux' or not is_riscv64_cpu:
			print('riscv64 is only supported with a native Linux riscv64 host')
			print('To cross-compile for riscv64, use CMake with a RISC-V toolchain file')
			sys.exit(1)

		if args.use_avx or args.use_avx2 or args.use_popcnt:
			print('AVX, AVX2, and POPCNT are not supported on riscv64')
			sys.exit(1)
	elif args.cpu == 'wasm' and args.compiler != 'emscripten':
		print('wasm is only supported with Emscripten')
		sys.exit(1)

	if args.analyze:
		# The static analyzer reads compile_commands.json, which only the Makefile and Ninja generators write
		if args.compiler in ['vs2015', 'vs2017', 'vs2019', 'vs2022', 'vs2026', 'vs2019-clang', 'vs2022-clang', 'vs2026-clang', 'android', 'osx', 'ios', 'emscripten'] or (args.compiler and args.compiler.startswith('gcc')):
			print('The static analyzer is only supported with clang on Linux and with the default compiler on OS X')
			sys.exit(1)

	if platform.system() == 'Darwin' and args.cpu == 'x86':
		result = subprocess.check_output(['xcodebuild', '-version']).decode("utf-8")
		if 'Xcode 11' in result:
			print('Versions of Xcode 11 and up no longer support x86')
			sys.exit(1)

	return args

def get_generator(compiler, cpu):
	if compiler == None:
		return None

	if platform.system() == 'Windows':
		if compiler == 'vs2015':
			if cpu == 'x86':
				return 'Visual Studio 14'
			elif cpu == 'x64':
				return 'Visual Studio 14 Win64'
		elif compiler == 'vs2017':
			if cpu == 'x86':
				return 'Visual Studio 15'
			elif cpu == 'x64':
				return 'Visual Studio 15 Win64'
			elif cpu == 'arm64':
				# VS2017 ARM/ARM64 support only works with cmake 3.13 and up and the architecture must be specified with
				# the -A cmake switch
				return 'Visual Studio 15 2017'
		elif compiler == 'vs2019' or compiler == 'vs2019-clang':
			return 'Visual Studio 16 2019'
		elif compiler == 'vs2022' or compiler == 'vs2022-clang':
			return 'Visual Studio 17 2022'
		elif compiler == 'vs2026' or compiler == 'vs2026-clang':
			# VS2026 support needs cmake 4.2 and up
			return 'Visual Studio 18 2026'
		elif compiler == 'android':
			# For Android, we use the default generator since we don't build with CMake
			return None
	elif platform.system() == 'Darwin':
		if compiler == 'osx' or compiler == 'ios':
			return 'Xcode'
		elif compiler == 'emscripten':
			# Emscripten uses the default generator
			return None
	elif platform.system() == 'Linux':
		if compiler == 'emscripten':
			# Emscripten uses the default generator
			return None

		return 'Unix Makefiles'

	print('Unknown compiler: {}'.format(compiler))
	print('See help with: python make.py -help')
	sys.exit(1)

def get_architecture(compiler, cpu):
	if compiler == None:
		return None

	if platform.system() == 'Windows':
		if compiler == 'vs2017' and cpu == 'arm64':
			return 'ARM64'

		is_modern_vs = compiler in ['vs2019', 'vs2019-clang', 'vs2022', 'vs2022-clang', 'vs2026', 'vs2026-clang']
		if is_modern_vs:
			if cpu == 'x86':
				return 'Win32'
			else:
				return cpu

	# This compiler/cpu pair does not need the architecture switch
	return None

def get_toolchain(compiler, cmake_script_dir):
	if platform.system() == 'Windows' and compiler == 'android':
		return os.path.join(cmake_script_dir, 'Toolchain-Android.cmake')
	elif platform.system() == 'Darwin' and compiler == 'ios':
		return os.path.join(cmake_script_dir, 'Toolchain-iOS.cmake')

	# No toolchain
	return None

def set_compiler_env(compiler):
	if platform.system() != 'Linux':
		return

	if compiler == 'emscripten':
		# Nothing to do for Emscripten
		return

	# The C and C++ compiler executables for each compiler
	linux_compilers = {
		'clang4':  ('clang-4.0', 'clang++-4.0'),
		'clang5':  ('clang-5.0', 'clang++-5.0'),
		'clang6':  ('clang-6.0', 'clang++-6.0'),
		'clang7':  ('clang-7', 'clang++-7'),
		'clang8':  ('clang-8', 'clang++-8'),
		'clang9':  ('clang-9', 'clang++-9'),
		'clang10': ('clang-10', 'clang++-10'),
		'clang11': ('clang-11', 'clang++-11'),
		'clang12': ('clang-12', 'clang++-12'),
		'clang13': ('clang-13', 'clang++-13'),
		'clang14': ('clang-14', 'clang++-14'),
		'clang15': ('clang-15', 'clang++-15'),
		'clang16': ('clang-16', 'clang++-16'),
		'clang17': ('clang-17', 'clang++-17'),
		'clang18': ('clang-18', 'clang++-18'),
		'clang19': ('clang-19', 'clang++-19'),
		'clang20': ('clang-20', 'clang++-20'),
		'clang21': ('clang-21', 'clang++-21'),
		'clang22': ('clang-22', 'clang++-22'),
		'clang23': ('clang-23', 'clang++-23'),
		'gcc4.9':  ('gcc-4.9', 'g++-4.9'),
		'gcc5':    ('gcc-5', 'g++-5'),
		'gcc6':    ('gcc-6', 'g++-6'),
		'gcc7':    ('gcc-7', 'g++-7'),
		'gcc8':    ('gcc-8', 'g++-8'),
		'gcc9':    ('gcc-9', 'g++-9'),
		'gcc10':   ('gcc-10', 'g++-10'),
		'gcc11':   ('gcc-11', 'g++-11'),
		'gcc12':   ('gcc-12', 'g++-12'),
		'gcc13':   ('gcc-13', 'g++-13'),
		'gcc14':   ('gcc-14', 'g++-14'),
		'gcc15':   ('gcc-15', 'g++-15'),
	}

	if compiler not in linux_compilers:
		print('Unknown compiler: {}'.format(compiler))
		print('See help with: python make.py -help')
		sys.exit(1)

	os.environ['CC'], os.environ['CXX'] = linux_compilers[compiler]

def do_generate_solution(build_dir, cmake_script_dir, regression_test_data_dir, decomp_data_dir,
						 is_cross_compiling, args):
	compiler = args.compiler
	cpu = args.cpu
	config = args.config

	if compiler:
		set_compiler_env(compiler)

	extra_switches = ['--no-warn-unused-cli']
	extra_switches.append('-DCPU_INSTRUCTION_SET:STRING={}'.format(cpu))
	extra_switches.append('-DCMAKE_CXX_STANDARD:STRING={}'.format(args.cpp_version))

	if is_cross_compiling:
		extra_switches.append('-DIS_CROSS_COMPILING:BOOL=true')

	if args.use_avx:
		print('Enabling AVX usage')
		extra_switches.append('-DUSE_AVX_INSTRUCTIONS:BOOL=true')

	if args.use_avx2:
		print('Enabling AVX2 usage')
		extra_switches.append('-DUSE_AVX2_INSTRUCTIONS:BOOL=true')

	if args.use_popcnt:
		print('Enabling POPCOUNT usage')
		extra_switches.append('-DUSE_POPCNT_INSTRUCTIONS:BOOL=true')

	if not args.use_simd:
		print('Disabling SIMD instruction usage')
		extra_switches.append('-DUSE_SIMD_INSTRUCTIONS:BOOL=false')

	if not args.use_sjson:
		print('Disabling SJSON support')
		extra_switches.append('-DUSE_SJSON:BOOL=false')

	if args.allwarnings and compiler.startswith('vs'):
		print('Enabling all warnings')
		extra_switches.append('-DENABLE_ALL_WARNINGS:BOOL=true')

	if args.fast_math:
		print('Enabling fast math for the include validation')
		extra_switches.append('-DUSE_FAST_MATH:BOOL=true')

	if args.bench:
		extra_switches.append('-DBUILD_BENCHMARK_EXE:BOOL=true')

	if args.analyze:
		extra_switches.append('-DCMAKE_EXPORT_COMPILE_COMMANDS:BOOL=true')

	# Native Windows uses a multi-config generator that does not use CMAKE_BUILD_TYPE
	# MSYS2 and Cygwin use a single-config generator
	is_native_windows = platform.system() == 'Windows' and os.path.sep == '\\'
	if not is_native_windows:
		extra_switches.append('-DCMAKE_BUILD_TYPE={}'.format(config.upper()))

	if platform.system() == 'Darwin' and compiler == 'ios' and args.ci:
		# Disable code signing for CI iOS builds since we just test compilation
		extra_switches.append('-DCMAKE_XCODE_ATTRIBUTE_CODE_SIGNING_REQUIRED=NO')
		extra_switches.append('-DCMAKE_XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED=NO')

	toolchain = get_toolchain(compiler, cmake_script_dir)
	if toolchain:
		extra_switches.append('-DCMAKE_TOOLCHAIN_FILE={}'.format(toolchain))

	if regression_test_data_dir:
		extra_switches.append('-DTEST_DATA_DIR:STRING={}'.format(regression_test_data_dir))

	if decomp_data_dir:
		extra_switches.append('-DDECOMP_DATA_DIR:STRING={}'.format(decomp_data_dir))

	# Generate IDE solution
	print('Generating build files ...')
	if compiler == 'emscripten':
		cmake_cmd = ['emcmake', 'cmake', '..', '-DCMAKE_INSTALL_PREFIX={}'.format(build_dir)] + extra_switches
	else:
		cmake_generator = get_generator(compiler, cpu)
		if not cmake_generator:
			print('Using default generator')
		else:
			generator_suffix = ''
			if compiler == 'vs2019-clang' or compiler == 'vs2022-clang' or compiler == 'vs2026-clang':
				extra_switches.extend(['-T', 'ClangCL'])
				generator_suffix = 'Clang CL'

			print('Using generator: {} {}'.format(cmake_generator, generator_suffix))
			extra_switches.extend(['-G', cmake_generator])

		cmake_arch = get_architecture(compiler, cpu)
		if cmake_arch:
			print('Using architecture: {}'.format(cmake_arch))
			extra_switches.extend(['-A', cmake_arch])

		cmake_cmd = ['cmake', '..', '-DCMAKE_INSTALL_PREFIX={}'.format(build_dir)] + extra_switches

	# Do not use a shell: each switch goes to cmake as one argument
	result = subprocess.call(cmake_cmd)
	if result != 0:
		sys.exit(result)

def do_build(args):
	config = args.config

	print('Building ...')
	cmake_cmd = ['cmake', '--build', '.']
	if platform.system() == 'Windows':
		if args.compiler == 'android':
			cmake_cmd += ['--config', config]
		else:
			if os.path.sep == '\\':
				# Native Windows
				cmake_cmd += ['--config', config, '--target', 'INSTALL']
			else:
				# MSYS2 or Cygwin
				cmake_cmd += ['--config', config, '--target', 'install']
	elif platform.system() == 'Darwin':
		if args.compiler == 'ios':
			cmake_cmd += ['--config', config]
		else:
			cmake_cmd += ['--config', config, '--target', 'install']
	else:
		cmake_cmd += ['--target', 'install']

	# Do not use a shell: the arguments go to cmake as they are
	result = subprocess.call(cmake_cmd)
	if result != 0:
		sys.exit(result)

def do_convert(test_data_dir, args):
	if sys.version_info < (3, 4):
		print('Python 3.4 or higher needed to run conversion')
		sys.exit(1)

	if not os.path.exists(args.convert):
		print('Input/Output conversion directory not found: {}'.format(args.convert))
		sys.exit(1)

	# Validate that our regression testing tool is present
	if args.compiler == 'emscripten':
		compressor_exe_path = './bin/acl_compressor.js'
	elif platform.system() == 'Windows':
		compressor_exe_path = './bin/acl_compressor.exe'
	else:
		compressor_exe_path = './bin/acl_compressor'

	compressor_exe_path = os.path.abspath(compressor_exe_path)
	if not os.path.exists(compressor_exe_path):
		print('Compressor exe not found: {}'.format(compressor_exe_path))
		sys.exit(1)

	# Grab all the test clips
	conversion_clips = []
	for (dirpath, dirnames, filenames) in os.walk(args.convert):
		for filename in filenames:
			if not filename.endswith('.acl.sjson'):
				continue

			clip_filename = os.path.join(dirpath, filename)
			conversion_clips.append(clip_filename)

	# Grab the raw config
	config_dir = os.path.join(test_data_dir, 'configs')
	config_filename = os.path.join(config_dir, 'uniformly_sampled_raw.config.sjson')

	print('Converting SJSON clips in {} ...'.format(args.convert))
	conversion_failed = False
	for clip_filename in conversion_clips:
		output_filename = clip_filename.replace('.acl.sjson', '.acl')

		if args.compiler == 'emscripten':
			cmd = 'node "{}" -acl="{}" -config="{}" -out="{}"'.format(compressor_exe_path, clip_filename, config_filename, output_filename)
		else:
			cmd = '"{}" -acl="{}" -config="{}" -out="{}"'.format(compressor_exe_path, clip_filename, config_filename, output_filename)

		if platform.system() == 'Windows':
			cmd = cmd.replace('/', '\\')

		result = subprocess.call(cmd, shell=True)

		if result != 0:
			print('Failed to run conversion for clip: {}'.format(clip_filename))
			print(cmd)
			conversion_failed = True

	print('Done!')

	if conversion_failed:
		sys.exit(1)

def do_tests_android(build_dir, args):
	# Switch our working directory to where we built everything
	working_dir = os.path.join(build_dir, 'tests', 'main_android')
	os.chdir(working_dir)

	gradlew_exe = os.path.join(working_dir, 'gradlew.bat')

	# We uninstall first and then install
	if args.config == 'Debug':
		install_cmd = 'uninstallAll installDebug'
	elif args.config == 'Release':
		install_cmd = 'uninstallAll installRelease'

	# Install our app
	test_cmd = '"{}" {}'.format(gradlew_exe, install_cmd)
	result = subprocess.call(test_cmd, shell=True)
	if result != 0:
		sys.exit(result)

	# Execute through ADB
	run_cmd = 'adb shell am start -n "com.acl.unit_tests/com.acl.unit_tests.MainActivity" -a android.intent.action.MAIN -c android.intent.category.LAUNCHER'
	result = subprocess.call(run_cmd, shell=True)
	if result != 0:
		sys.exit(result)

	# Restore working directory
	os.chdir(build_dir)

def do_tests_cmake(args):
	ctest_cmd = ['ctest', '--output-on-failure', '--parallel', str(args.num_threads)]

	if platform.system() == 'Windows' or platform.system() == 'Darwin':
		ctest_cmd += ['-C', args.config]
	if args.tests_matching:
		ctest_cmd += ['--tests-regex', args.tests_matching]

	# Do not use a shell: a regex such as 'a|b' must go to ctest as it is
	result = subprocess.call(ctest_cmd)
	if result != 0:
		sys.exit(result)

def do_tests(build_dir, args):
	print('Running unit tests ...')

	if args.compiler == 'android':
		do_tests_android(build_dir, args)
	else:
		do_tests_cmake(args)

def format_elapsed_time(elapsed_time):
	hours, rem = divmod(elapsed_time, 3600)
	minutes, seconds = divmod(rem, 60)
	return '{:0>2}h {:0>2}m {:05.2f}s'.format(int(hours), int(minutes), seconds)

def print_progress(iteration, total, prefix='', suffix='', decimals = 1, bar_length = 40):
	# Taken from https://stackoverflow.com/questions/3173320/text-progress-bar-in-the-console
	# With minor tweaks
	"""
	Call in a loop to create terminal progress bar
	@params:
		iteration   - Required  : current iteration (Int)
		total       - Required  : total iterations (Int)
		prefix      - Optional  : prefix string (Str)
		suffix      - Optional  : suffix string (Str)
		decimals    - Optional  : positive number of decimals in percent complete (Int)
		bar_length  - Optional  : character length of bar (Int)
	"""
	str_format = "{0:." + str(decimals) + "f}"
	percents = str_format.format(100 * (iteration / float(total)))
	filled_length = int(round(bar_length * iteration / float(total)))
	bar = '█' * filled_length + '-' * (bar_length - filled_length)

	# We need to clear any previous line we might have to ensure we have no visual artifacts
	# Note that if this function is called too quickly, the text might flicker
	terminal_width = 80
	sys.stdout.write('{}\r'.format(' ' * terminal_width))
	sys.stdout.flush()

	sys.stdout.write('%s |%s| %s%s %s\r' % (prefix, bar, percents, '%', suffix)),
	sys.stdout.flush()

	if iteration == total:
		sys.stdout.write('\n')

def do_prepare_regression_test_data(test_data_dir, args):
	print('Preparing regression test data ...')

	regression_test_data_dir = os.path.join(test_data_dir, 'regression_test_data')
	current_test_data_zip = os.path.join(test_data_dir, CURRENT_REGRESSION_TEST_DATA_FILENAME)

	# Validate that our regression test data is present
	if not os.path.exists(current_test_data_zip):
		print('Regression test data not found: {}'.format(current_test_data_zip))

		# Delete any stale data we might have
		if os.path.exists(regression_test_data_dir):
			shutil.rmtree(regression_test_data_dir)

		# Attempt to download it from GitHub
		source_url = REGRESSION_TEST_DATA_SOURCE_URL + CURRENT_REGRESSION_TEST_DATA_FILENAME
		print('Downloading regression test data from: ' + source_url)

		try:
			filename, headers = urllib.request.urlretrieve(source_url, CURRENT_REGRESSION_TEST_DATA_FILENAME)
			shutil.copyfile(filename, current_test_data_zip)
		except:
			print('Failed to download regression tests')
			return None

	# If it hasn't been decompressed yet, do so now
	needs_decompression = not os.path.exists(regression_test_data_dir)
	if needs_decompression:
		print('Decompressing {} ...'.format(current_test_data_zip))
		with zipfile.ZipFile(current_test_data_zip, 'r') as zip_ref:
			zip_ref.extractall(regression_test_data_dir)

	# Grab all the test clips
	regression_clips = []
	for (dirpath, dirnames, filenames) in os.walk(regression_test_data_dir):
		for filename in filenames:
			if not filename.endswith('.acl') and not filename.endswith('.acl.sjson'):
				continue

			clip_filename = os.path.join(dirpath, filename)
			regression_clips.append((clip_filename, os.path.getsize(clip_filename)))

	if len(regression_clips) == 0:
		print('No regression clips found')
		sys.exit(1)

	print('Found {} regression clips'.format(len(regression_clips)))

	# Grab all the test configurations
	test_configs = []
	test_config_dir = os.path.join(test_data_dir, 'configs')
	if os.path.exists(test_config_dir):
		for (dirpath, dirnames, filenames) in os.walk(test_config_dir):
			for filename in filenames:
				if not filename.endswith('.config.sjson'):
					continue

				config_filename = os.path.join(dirpath, filename)
				test_configs.append((config_filename, filename))

	if len(test_configs) == 0:
		print('No regression configurations found')
		sys.exit(1)

	print('Found {} regression configurations'.format(len(test_configs)))

	# Sort the configs by name for consistency
	test_configs.sort(key=lambda entry: entry[1])

	# Sort clips by size to test larger clips first, it parallelizes better
	regression_clips.sort(key=lambda entry: entry[1], reverse=True)

	# Write our metadata file
	with open(os.path.join(regression_test_data_dir, 'metadata.sjson'), 'w') as metadata_file:
		print('configs = [', file = metadata_file)
		for config_filename, _ in test_configs:
			print('\t"{}"'.format(os.path.relpath(config_filename, test_config_dir)), file = metadata_file)
		print(']', file = metadata_file)
		print('', file = metadata_file)
		print('clips = [', file = metadata_file)
		for clip_filename, _ in regression_clips:
			print('\t"{}"'.format(os.path.relpath(clip_filename, regression_test_data_dir)), file = metadata_file)
		print(']', file = metadata_file)
		print('', file = metadata_file)

	return regression_test_data_dir

def do_prepare_decompression_test_data(test_data_dir, args):
	print('Preparing decompression test data ...')

	current_data_zip = os.path.join(test_data_dir, '{}.zip'.format(current_decomp_data))

	# Validate that our regression test data is present
	if not os.path.exists(current_data_zip):
		print('Decompression test data not found: {}'.format(current_data_zip))
		return

	# If it hasn't been decompressed yet, do so now
	current_data_dir = os.path.join(test_data_dir, current_decomp_data)
	needs_decompression = not os.path.exists(current_data_dir)
	if needs_decompression:
		print('Decompressing {} ...'.format(current_data_zip))
		with zipfile.ZipFile(current_data_zip, 'r') as zip_ref:
			zip_ref.extractall(test_data_dir)

	# Grab all the test clips
	clips = []
	for (dirpath, dirnames, filenames) in os.walk(current_data_dir):
		for filename in filenames:
			if not filename.endswith('.acl'):
				continue

			clip_filename = os.path.join(dirpath, filename)
			clips.append(clip_filename)

	if len(clips) == 0:
		print('No decompression clips found')
		return

	print('Found {} decompression clips'.format(len(clips)))

	# Sort clips by name for determinism
	clips.sort()

	# Write our metadata file
	with open(os.path.join(current_data_dir, 'metadata.sjson'), 'w') as metadata_file:
		print('clip_dir = "{}"'.format(current_data_dir), file = metadata_file)
		print('', file = metadata_file)
		print('clips = [', file = metadata_file)
		for clip_filename in clips:
			print('\t"{}"'.format(os.path.relpath(clip_filename, current_data_dir)), file = metadata_file)
		print(']', file = metadata_file)
		print('', file = metadata_file)

	return current_data_dir

def do_regression_tests_android(build_dir, args):
	# Switch our working directory to where we built everything
	working_dir = os.path.join(build_dir, 'tools', 'regression_tester_android')
	os.chdir(working_dir)

	gradlew_exe = os.path.join(working_dir, 'gradlew.bat')

	# We uninstall first and then install
	if args.config == 'Debug':
		install_cmd = 'uninstallAll installDebug'
	elif args.config == 'Release':
		install_cmd = 'uninstallAll installRelease'

	# Install our app
	test_cmd = '"{}" {}'.format(gradlew_exe, install_cmd)
	result = subprocess.call(test_cmd, shell=True)
	if result != 0:
		sys.exit(result)

	# Execute through ADB
	run_cmd = 'adb shell am start -n "com.acl.regression_tests/com.acl.regression_tests.MainActivity" -a android.intent.action.MAIN -c android.intent.category.LAUNCHER'
	result = subprocess.call(run_cmd, shell=True)
	if result != 0:
		sys.exit(result)

	# Restore working directory
	os.chdir(build_dir)

def do_regression_tests_cmake(test_data_dir, regression_test_data_dir, args):
	if sys.version_info < (3, 4):
		print('Python 3.4 or higher needed to run regression tests')
		sys.exit(1)

	import queue

	# Validate that our regression testing tool is present
	if args.compiler == 'emscripten':
		compressor_exe_path = './bin/acl_compressor.js'
	elif platform.system() == 'Windows':
		compressor_exe_path = './bin/acl_compressor.exe'
	else:
		compressor_exe_path = './bin/acl_compressor'

	compressor_exe_path = os.path.abspath(compressor_exe_path)
	if not os.path.exists(compressor_exe_path):
		print('Compressor exe not found: {}'.format(compressor_exe_path))
		sys.exit(1)

	# Grab all the test clips
	regression_clips = []
	for (dirpath, dirnames, filenames) in os.walk(regression_test_data_dir):
		for filename in filenames:
			if not filename.endswith('.acl') and not filename.endswith('.acl.sjson'):
				continue

			clip_filename = os.path.join(dirpath, filename)
			regression_clips.append((clip_filename, os.path.getsize(clip_filename)))

	# Grab all the test configurations
	test_configs = []
	test_config_dir = os.path.join(test_data_dir, 'configs')
	if os.path.exists(test_config_dir):
		for (dirpath, dirnames, filenames) in os.walk(test_config_dir):
			for filename in filenames:
				if not filename.endswith('.config.sjson'):
					continue

				config_filename = os.path.join(dirpath, filename)
				test_configs.append((config_filename, filename))

	# Sort the configs by name for consistency
	test_configs.sort(key=lambda entry: entry[1])

	# Sort clips by size to test larger clips first, it parallelizes better
	regression_clips.sort(key=lambda entry: entry[1], reverse=True)

	# Iterate over every clip and configuration and perform the regression testing
	for config_filename, _ in test_configs:
		print('Performing regression tests for configuration: {}'.format(os.path.basename(config_filename)))
		regression_start_time = time.perf_counter()

		cmd_queue = queue.Queue()
		completed_queue = queue.Queue()
		failed_queue = queue.Queue()
		failure_lock = threading.Lock()
		for clip_filename, _ in regression_clips:
			if args.compiler == 'emscripten':
				cmd = 'node "{}" -acl="{}" -test -config="{}"'.format(compressor_exe_path, clip_filename, config_filename)
			else:
				cmd = '"{}" -acl="{}" -test -config="{}"'.format(compressor_exe_path, clip_filename, config_filename)

			if platform.system() == 'Windows':
				cmd = cmd.replace('/', '\\')

			cmd_queue.put((clip_filename, cmd))

		# Add a marker to terminate the threads
		for i in range(args.num_threads):
			cmd_queue.put(None)

		def run_clip_regression_test(cmd_queue, completed_queue, failed_queue, failure_lock):
			while True:
				entry = cmd_queue.get()
				if entry is None:
					return

				(clip_filename, cmd) = entry

				result = subprocess.call(cmd, shell=True)

				if result != 0:
					failed_queue.put((clip_filename, cmd))
					failure_lock.acquire()
					# Print the exit code in hexadecimal too. Windows reports a crash with an NTSTATUS value, for example 0xC0000005.
					print('Failed to run regression test for clip: {} (exit code {}, 0x{:08X})'.format(clip_filename, result, result & 0xFFFFFFFF))
					print(cmd)
					failure_lock.release()

				completed_queue.put(clip_filename)

		threads = [ threading.Thread(target = run_clip_regression_test, args = (cmd_queue, completed_queue, failed_queue, failure_lock)) for _i in range(args.num_threads) ]
		for thread in threads:
			thread.daemon = True
			thread.start()

		if not args.ci:
			print_progress(0, len(regression_clips), 'Testing clips:', '{} / {}'.format(0, len(regression_clips)))
		try:
			while True:
				for thread in threads:
					thread.join(1.0)

				if not args.ci:
					num_processed = completed_queue.qsize()
					print_progress(num_processed, len(regression_clips), 'Testing clips:', '{} / {}'.format(num_processed, len(regression_clips)))

				all_threads_done = True
				for thread in threads:
					if thread.is_alive():
						all_threads_done = False

				if all_threads_done:
					break
		except KeyboardInterrupt:
			sys.exit(1)

		regression_testing_failed = not failed_queue.empty()

		regression_end_time = time.perf_counter()
		print('Done in {}'.format(format_elapsed_time(regression_end_time - regression_start_time)))

		if regression_testing_failed:
			sys.exit(1)

def do_regression_tests(build_dir, test_data_dir, regression_test_data_dir, args):
	print('Running regression tests ...')

	if args.compiler == 'android':
		do_regression_tests_android(build_dir, args)
	else:
		do_regression_tests_cmake(test_data_dir, regression_test_data_dir, args)

def do_run_bench_android(build_dir, args):
	# Switch our working directory to where we built everything
	working_dir = os.path.join(build_dir, 'tools', 'acl_decompressor', 'main_android')
	os.chdir(working_dir)

	gradlew_exe = os.path.join(working_dir, 'gradlew.bat')

	# We uninstall first and then install
	if args.config == 'Debug':
		install_cmd = 'uninstallAll installDebug'
	elif args.config == 'Release':
		install_cmd = 'uninstallAll installRelease'

	# Install our app
	test_cmd = '"{}" {}'.format(gradlew_exe, install_cmd)
	result = subprocess.call(test_cmd, shell=True)
	if result != 0:
		sys.exit(result)

	# Execute through ADB
	run_cmd = 'adb shell am start -n "com.acl.decompressor/com.acl.decompressor.MainActivity" -a android.intent.action.MAIN -c android.intent.category.LAUNCHER'
	result = subprocess.call(run_cmd, shell=True)
	if result != 0:
		sys.exit(result)

	# Restore working directory
	os.chdir(build_dir)

def do_pull_bench_android(build_dir):
	# Grab the android directory we wrote the results to
	output = str(subprocess.check_output('adb logcat -s acl -e "Benchmark results will be written to:" -m 1 -d'))
	matches = re.search(r'Benchmark results will be written to: ([/\.\w]+)', output)
	if matches == None:
		print('Failed to find Android source directory from ADB')
		android_src_dir = '/storage/emulated/0/Android/data/com.acl.decompressor/files'
		print('{} will be used instead'.format(android_src_dir))
	else:
		android_src_dir = matches.group(1)

	# Grab the benchmark results from the android device
	dst_filename = os.path.join(build_dir, 'benchmark_results.json')
	src_filename = '{}/benchmark_results.json'.format(android_src_dir)
	cmd = 'adb pull "{}" "{}"'.format(src_filename, dst_filename)
	os.system(cmd)

def do_run_bench_native(build_dir, test_data_dir):
	if platform.system() == 'Windows':
		bench_exe = os.path.join(os.getcwd(), 'bin/acl_decompressor.exe')
	else:
		bench_exe = os.path.join(os.getcwd(), 'bin/acl_decompressor')

	current_data_dir = os.path.join(test_data_dir, current_decomp_data)
	metadata_filename = os.path.join(current_data_dir, 'metadata.sjson')
	benchmark_output_filename = os.path.join(build_dir, 'benchmark_results.json')
	bench_cmd = '{} -metadata="{}" --benchmark_out={} --benchmark_out_format=json'.format(bench_exe, metadata_filename, benchmark_output_filename)

	result = subprocess.call(bench_cmd, shell=True)
	if result != 0:
		sys.exit(result)

def do_run_bench(build_dir, test_data_dir, args):
	if args.compiler == 'ios':
		return	# Not supported on iOS

	print('Running benchmark ...')

	if args.compiler == 'android':
		do_run_bench_android(build_dir, args)
	else:
		do_run_bench_native(build_dir, test_data_dir)

def get_analyzer_array_bound_checker(compiler_exe):
	# The array bound checker is not enabled by default
	# Clang 21 and later name it security.ArrayBound, older versions name it alpha.security.ArrayBoundV2
	checkers = [('-analyzer-checker-help', 'security.ArrayBound'), ('-analyzer-checker-help-alpha', 'alpha.security.ArrayBoundV2')]
	for help_switch, checker in checkers:
		try:
			output = subprocess.check_output([compiler_exe, '-cc1', help_switch], stderr=subprocess.STDOUT).decode('utf-8', 'replace')
		except (OSError, subprocess.CalledProcessError):
			continue

		if re.search(r'^\s*{}\s'.format(re.escape(checker)), output, re.MULTILINE):
			return checker

	return None

def get_analyzer_command(entry, analyzer_switches):
	if 'arguments' in entry:
		compile_args = entry['arguments']
	else:
		compile_args = shlex.split(entry['command'], posix=(os.name != 'nt'))

	# We remove the object file, the dependency file, and '-c' (an unused argument with -Werror is an error)
	analyzer_cmd = []
	skip_next_arg = False
	for arg in compile_args:
		if skip_next_arg:
			skip_next_arg = False
		elif arg in ['-o', '-MF', '-MT', '-MQ']:
			skip_next_arg = True
		elif arg not in ['-c', '-MD', '-MMD']:
			analyzer_cmd.append(arg)

	analyzer_cmd.extend(analyzer_switches)
	analyzer_cmd.extend(['-o', os.devnull])
	return analyzer_cmd

def run_analyzer(entry, analyzer_switches):
	analyzer_cmd = get_analyzer_command(entry, analyzer_switches)
	process = subprocess.Popen(analyzer_cmd, cwd=entry['directory'], stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
	output = process.communicate()[0].decode('utf-8', 'replace')
	return (process.returncode, output)

def split_analyzer_line(line):
	# A diagnostic line has the form 'filename:line:column: message'
	# We do not use a regex: a lazy match on the filename makes the runtime of the regex more than linear
	# A filename on Windows has a ':' after the drive letter, thus we split the line and the column from the right
	location, separator, message = line.partition(': ')
	parts = location.rsplit(':', 2)
	if not separator or len(parts) != 3 or not parts[0] or not parts[1].isdigit() or not parts[2].isdigit():
		return None

	return (parts[0], int(parts[1]), int(parts[2]), message)

def is_analyzer_line_of_kind(line, kind):
	# The kind is 'warning' or 'note'
	parts = split_analyzer_line(line)
	return parts is not None and parts[3].startswith(kind + ': ')

def get_analyzer_line_location(line):
	# Returns the filename, the line, and the column
	return split_analyzer_line(line)[0:3]

def normalize_analyzer_line(line, root_dir):
	# Print the paths relative to the root directory, for example 'includes/acl/core/bitset.h:171:33: warning: ...'
	parts = split_analyzer_line(line)
	if not parts or not os.path.isabs(parts[0]):
		return line

	filename = os.path.realpath(parts[0])
	if filename.startswith(root_dir + os.sep):
		filename = os.path.relpath(filename, root_dir)
	return '{}:{}:{}: {}'.format(filename, parts[1], parts[2], parts[3])

def parse_analyzer_findings(output, root_dir):
	# A finding is a warning line and the source and note lines that follow it
	findings = []
	finding = None
	for line in output.splitlines():
		if is_analyzer_line_of_kind(line, 'warning'):
			finding = [line]
			findings.append(finding)
		elif line.startswith('In file included from') or re.match(r'^\d+ warnings? generated\.$', line):
			finding = None
		elif finding is not None:
			finding.append(line)

	return [[normalize_analyzer_line(line, root_dir) for line in finding] for finding in findings]

def escape_github_command_data(value):
	return value.replace('%', '%25').replace('\r', '%0D').replace('\n', '%0A')

def escape_github_command_property(value):
	return escape_github_command_data(value).replace(':', '%3A').replace(',', '%2C')

def print_github_annotation(finding):
	# GitHub Actions shows this line as an annotation on the line of the finding
	# The message is the warning and its notes, without the source lines
	filename, line_number, column, warning = split_analyzer_line(finding[0])
	filename = filename.replace(os.sep, '/')

	# The warning has the form 'warning: text [checker]', the checker is optional
	text = warning[len('warning: '):]
	checker = None
	if text.endswith(']') and ' [' in text:
		text, _, checker = text[:-1].rpartition(' [')

	title = 'Static analyzer ({})'.format(checker) if checker else 'Static analyzer'
	notes = [line for line in finding[1:] if is_analyzer_line_of_kind(line, 'note')]
	message = '\n'.join([text] + notes)

	print('::error file={},line={},col={},title={}::{}'.format(
		escape_github_command_property(filename), line_number, column,
		escape_github_command_property(title), escape_github_command_data(message)))

def collect_analyzer_findings(entries, results, root_dir):
	# Each header is analyzed once for each cpp file that includes it, we keep each finding once
	# The headers of the compiler are analyzed too (e.g. the intrinsic headers), we ignore the findings in them
	is_failure = False
	unique_findings = {}
	ignored_findings = set()
	for entry, (result, output) in zip(entries, results):
		if result != 0:
			print('The static analyzer failed on {}:'.format(entry['file']))
			print(output)
			is_failure = True
			continue

		for finding in parse_analyzer_findings(output, root_dir):
			# A finding in the repository has a relative path
			if os.path.isabs(get_analyzer_line_location(finding[0])[0]):
				ignored_findings.add(finding[0])
			else:
				unique_findings.setdefault(finding[0], finding)

	return (is_failure, unique_findings, ignored_findings)

def do_analyze(build_dir, args):
	print('Running the static analyzer ...')

	compile_commands_filename = os.path.join(build_dir, 'compile_commands.json')
	if not os.path.exists(compile_commands_filename):
		print('{} does not exist. The static analyzer needs a Makefile or Ninja generator.'.format(compile_commands_filename))
		sys.exit(1)

	with open(compile_commands_filename, 'r') as compile_commands_file:
		entries = json.load(compile_commands_file)

	# The include validation has one cpp file for each public header
	# We analyze the functions in the headers too, because a header-only library has no other functions to analyze
	entries = [entry for entry in entries if 'validate_includes' in entry['file'].replace('\\', '/').split('/')]
	if not entries:
		print('No include validation file is in {}'.format(compile_commands_filename))
		sys.exit(1)

	compiler_exe = get_analyzer_command(entries[0], [])[0]
	compiler_version = subprocess.check_output([compiler_exe, '--version']).decode('utf-8', 'replace')
	if 'clang' not in compiler_version.lower():
		print('The static analyzer needs clang. The compiler is: {}'.format(compiler_version.splitlines()[0]))
		sys.exit(1)

	analyzer_switches = ['--analyze', '--analyzer-output', 'text', '-Xclang', '-analyzer-opt-analyze-headers']

	# The asserts are removed by default, we enable them to let the analyzer use the conditions that they check
	# When an assert fails, it calls abort() and the analyzer stops on that path
	analyzer_switches.extend(['-DACL_ON_ASSERT_ABORT', '-DRTM_ON_ASSERT_ABORT'])

	# The code writes its buffers with a pointer that it moves after each write, the last move is never read
	# This checker finds no memory errors, we disable it
	analyzer_switches.extend(['-Xclang', '-analyzer-disable-checker=deadcode.DeadStores'])

	array_bound_checker = get_analyzer_array_bound_checker(compiler_exe)
	if array_bound_checker:
		print('Enabling checker: {}'.format(array_bound_checker))
		analyzer_switches.extend(['-Xclang', '-analyzer-checker={}'.format(array_bound_checker)])
	else:
		print('This compiler has no array bound checker. Only the default checkers are used.')

	thread_pool = multiprocessing.pool.ThreadPool(int(args.num_threads))
	results = thread_pool.map(lambda entry: run_analyzer(entry, analyzer_switches), entries)
	thread_pool.close()
	thread_pool.join()

	root_dir = os.path.realpath(os.path.dirname(build_dir))
	is_failure, unique_findings, ignored_findings = collect_analyzer_findings(entries, results, root_dir)

	# Sort the findings by file, line, and column
	# On GitHub Actions, we also print each finding as an annotation
	is_github_actions = os.environ.get('GITHUB_ACTIONS') == 'true'
	for warning_line in sorted(unique_findings.keys(), key=get_analyzer_line_location):
		print('\n'.join(unique_findings[warning_line]))
		if is_github_actions:
			print_github_annotation(unique_findings[warning_line])

	if ignored_findings:
		print('The static analyzer ignored {} issues in files outside of the repository'.format(len(ignored_findings)))

	print('The static analyzer found {} issues in {} files'.format(len(unique_findings), len(entries)))
	if is_failure or unique_findings:
		sys.exit(1)

if __name__ == "__main__":
	args = parse_argv()

	build_dir = os.path.join(os.getcwd(), 'build')
	test_data_dir = os.path.join(os.getcwd(), 'test_data')
	cmake_script_dir = os.path.join(os.getcwd(), 'cmake')

	is_clean_requested = args.clean or args.clean_only
	if is_clean_requested and os.path.exists(build_dir):
		print('Cleaning previous build ...')
		shutil.rmtree(build_dir)

	if args.clean_only:
		sys.exit(0)

	if not os.path.exists(build_dir):
		os.makedirs(build_dir)

	os.chdir(build_dir)

	print('Using config: {}'.format(args.config))
	print('Using cpu: {}'.format(args.cpu))
	if args.compiler:
		print('Using compiler: {}'.format(args.compiler))
	print('Using C++-{}'.format(args.cpp_version))
	print('Using {} threads'.format(args.num_threads))

	# Check if we are cross-compiling
	is_cross_compiling = False
	if args.compiler == 'ios' or args.compiler == 'android':
		is_cross_compiling = True
	elif args.cpu == 'x86' or args.cpu == 'x64':
		is_cross_compiling = is_host_cpu_arm64()
	elif args.cpu == 'armv7':
		is_cross_compiling = not is_host_cpu_armv7()
	elif args.cpu == 'arm64' or args.cpu == 'arm64ec':
		is_cross_compiling = not is_host_cpu_arm64()

	# We always prepare this since for Android/iOS the data is included as part of the executable
	regression_test_data_dir = do_prepare_regression_test_data(test_data_dir, args)
	decomp_data_dir = do_prepare_decompression_test_data(test_data_dir, args)

	# Make sure 'make' runs with all available cores
	os.environ['MAKEFLAGS'] = '-j{}'.format(args.num_threads)

	do_generate_solution(build_dir, cmake_script_dir, regression_test_data_dir, decomp_data_dir, is_cross_compiling, args)

	if args.build:
		do_build(args)

	if args.convert and not is_cross_compiling:
		do_convert(test_data_dir, args)

	if args.unit_test and (not is_cross_compiling or args.compiler == 'android'):
		do_tests(build_dir, args)

	if args.regression_test and regression_test_data_dir and (not is_cross_compiling or args.compiler == 'android'):
		do_regression_tests(build_dir, test_data_dir, regression_test_data_dir, args)

	if args.run_bench and (not is_cross_compiling or args.compiler == 'android'):
		do_run_bench(build_dir, test_data_dir, args)

	if args.pull_bench and (not is_cross_compiling or args.compiler == 'android'):
		do_pull_bench_android(build_dir)

	if args.analyze:
		do_analyze(build_dir, args)

	sys.exit(0)
