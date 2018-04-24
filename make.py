import os
import platform
import queue
import shutil
import subprocess
import sys
import threading
import time
import zipfile

# The current test data version in used
current_test_data = 'test_data_v1'
current_decomp_data = 'decomp_data_v1'

def parse_argv():
	options = {}
	options['build'] = False
	options['clean'] = False
	options['unit_test'] = False
	options['regression_test'] = False
	options['use_avx'] = False
	options['compiler'] = None
	options['config'] = 'Release'
	options['cpu'] = 'x64'
	options['num_threads'] = 4
	options['print_help'] = False

	for i in range(1, len(sys.argv)):
		value = sys.argv[i]
		value_upper = value.upper()

		if value == '-build':
			options['build'] = True

		if value == '-clean':
			options['clean'] = True

		if value == '-unit_test':
			options['unit_test'] = True

		if value == '-regression_test':
			options['regression_test'] = True

		if value == '-help':
			options['print_help'] = True

		if value == '-avx':
			options['use_avx'] = True

		# TODO: Refactor to use the form: -compiler=vs2015
		if value == '-vs2015':
			options['compiler'] = 'vs2015'

		if value == '-vs2017':
			options['compiler'] = 'vs2017'

		if value == '-android':
			options['compiler'] = 'android'

		if value == '-clang4':
			options['compiler'] = 'clang4'

		if value == '-clang5':
			options['compiler'] = 'clang5'

		if value == '-gcc5':
			options['compiler'] = 'gcc5'

		if value == '-gcc6':
			options['compiler'] = 'gcc6'

		if value == '-gcc7':
			options['compiler'] = 'gcc7'

		if value == '-osx':
			options['compiler'] = 'osx'

		if value == '-ios':
			options['compiler'] = 'ios'

		# TODO: Refactor to use the form: -config=Release
		if value_upper == '-DEBUG':
			options['config'] = 'Debug'

		if value_upper == '-RELEASE':
			options['config'] = 'Release'

		# TODO: Refactor to use the form: -cpu=x86
		if value == '-x86':
			options['cpu'] = 'x86'

		if value == '-x64':
			options['cpu'] = 'x64'

	# Sanitize and validate our options
	if options['compiler'] == 'android':
		options['cpu'] = 'armv7-a'

		if not platform.system() == 'Windows':
			print('Android is only supported on Windows')
			sys.exit(1)

		if options['use_avx']:
			print('AVX is not supported on Android')
			sys.exit(1)

		if options['unit_test']:
			print('Unit tests cannot run from the command line on Android')
			sys.exit(1)

	if options['compiler'] == 'ios':
		options['cpu'] = 'arm64'

		if not platform.system() == 'Darwin':
			print('iOS is only supported on OS X')
			sys.exit(1)

		if options['use_avx']:
			print('AVX is not supported on iOS')
			sys.exit(1)

		if options['unit_test']:
			print('Unit tests cannot run from the command line on iOS')
			sys.exit(1)

	return options

def get_cmake_exes():
	if platform.system() == 'Windows':
		return ('cmake.exe', 'ctest.exe')
	else:
		return ('cmake', 'ctest')

def get_generator(compiler, cpu):
	if compiler == None:
		return None

	if platform.system() == 'Windows':
		if compiler == 'vs2015':
			if cpu == 'x86':
				return 'Visual Studio 14'
			else:
				return 'Visual Studio 14 Win64'
		elif compiler == 'vs2017':
			if cpu == 'x86':
				return 'Visual Studio 15'
			else:
				return 'Visual Studio 15 Win64'
		elif compiler == 'android':
			return 'Visual Studio 14'
	elif platform.system() == 'Darwin':
		if compiler == 'osx' or compiler == 'ios':
			return 'Xcode'
	else:
		return 'Unix Makefiles'

	print('Unknown compiler: {}'.format(compiler))
	print('See help with: python make.py -help')
	sys.exit(1)

def get_toolchain(compiler):
	if platform.system() == 'Windows' and compiler == 'android':
		return 'Toolchain-Android.cmake'
	elif platform.system() == 'Darwin' and compiler == 'ios':
		return 'Toolchain-iOS.cmake'

	# No toolchain
	return None

def set_compiler_env(compiler, options):
	if platform.system() == 'Linux':
		os.environ['MAKEFLAGS'] = '-j{}'.format(options['num_threads'])
		if compiler == 'clang4':
			os.environ['CC'] = 'clang-4.0'
			os.environ['CXX'] = 'clang++-4.0'
		elif compiler == 'clang5':
			os.environ['CC'] = 'clang-5.0'
			os.environ['CXX'] = 'clang++-5.0'
		elif compiler == 'gcc5':
			os.environ['CC'] = 'gcc-5'
			os.environ['CXX'] = 'g++-5'
		elif compiler == 'gcc6':
			os.environ['CC'] = 'gcc-6'
			os.environ['CXX'] = 'g++-6'
		elif compiler == 'gcc7':
			os.environ['CC'] = 'gcc-7'
			os.environ['CXX'] = 'g++-7'
		else:
			print('Unknown compiler: {}'.format(compiler))
			print('See help with: python make.py -help')
			sys.exit(1)

def do_generate_solution(cmake_exe, build_dir, cmake_script_dir, options):
	compiler = options['compiler']
	cpu = options['cpu']
	config = options['config']

	if not compiler == None:
		set_compiler_env(compiler, options)

	extra_switches = ['--no-warn-unused-cli']
	if not platform.system() == 'Windows':
		extra_switches.append('-DCPU_INSTRUCTION_SET:STRING={}'.format(cpu))

	if options['use_avx']:
		print('Enabling AVX usage')
		extra_switches.append('-DUSE_AVX_INSTRUCTIONS:BOOL=true')

	if not platform.system() == 'Windows' and not platform.system() == 'Darwin':
		extra_switches.append('-DCMAKE_BUILD_TYPE={}'.format(config.upper()))

	toolchain = get_toolchain(compiler)
	if not toolchain == None:
		extra_switches.append('-DCMAKE_TOOLCHAIN_FILE={}'.format(os.path.join(cmake_script_dir, toolchain)))

	if options['regression_test']:
		extra_switches.append('-DREGRESSION_TESTING:BOOL=true')

	# Generate IDE solution
	print('Generating build files ...')
	cmake_cmd = '"{}" .. -DCMAKE_INSTALL_PREFIX="{}" {}'.format(cmake_exe, build_dir, ' '.join(extra_switches))
	cmake_generator = get_generator(compiler, cpu)
	if cmake_generator == None:
		print('Using default generator')
	else:
		print('Using generator: {}'.format(cmake_generator))
		cmake_cmd += ' -G "{}"'.format(cmake_generator)

	result = subprocess.call(cmake_cmd, shell=True)
	if result != 0:
		sys.exit(result)

def do_build(cmake_exe, options):
	config = options['config']

	print('Building ...')
	cmake_cmd = '"{}" --build .'.format(cmake_exe)
	if platform.system() == 'Windows':
		if options['compiler'] == 'android':
			cmake_cmd += ' --config {}'.format(config)
		else:
			cmake_cmd += ' --config {} --target INSTALL'.format(config)
	elif platform.system() == 'Darwin':
		if options['compiler'] == 'ios':
			cmake_cmd += ' --config {}'.format(config)
		else:
			cmake_cmd += ' --config {} --target install'.format(config)
	else:
		cmake_cmd += ' --target install'

	result = subprocess.call(cmake_cmd, shell=True)
	if result != 0:
		sys.exit(result)

def do_tests(ctest_exe, options):
	config = options['config']

	print('Running unit tests ...')
	ctest_cmd = '"{}" --output-on-failure'.format(ctest_exe)
	if platform.system() == 'Windows' or platform.system() == 'Darwin':
		ctest_cmd += ' -C {}'.format(config)

	result = subprocess.call(ctest_cmd, shell=True)
	if result != 0:
		sys.exit(result)

def format_elapsed_time(elapsed_time):
	hours, rem = divmod(elapsed_time, 3600)
	minutes, seconds = divmod(rem, 60)
	return '{:0>2}h {:0>2}m {:05.2f}s'.format(int(hours), int(minutes), seconds)

def print_progress(iteration, total, prefix='', suffix='', decimals = 1, bar_length = 50):
	# Taken from https://stackoverflow.com/questions/3173320/text-progress-bar-in-the-console
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

	if platform.system() == 'Darwin':
		# On OS X, \r doesn't appear to work properly in the terminal
		print('{}{} |{}| {}{} {}'.format('\b' * 100, prefix, bar, percents, '%', suffix), end='')
	else:
		sys.stdout.write('\r%s |%s| %s%s %s' % (prefix, bar, percents, '%', suffix)),

	if iteration == total:
		print('')

	sys.stdout.flush()

def do_prepare_regression_test_data(test_data_dir, options):
	print('Preparing regression test data ...')

	current_test_data_zip = os.path.join(test_data_dir, '{}.zip'.format(current_test_data))

	# Validate that our regression test data is present
	if not os.path.exists(current_test_data_zip):
		print('Regression test data not found: {}'.format(current_test_data_zip))
		sys.exit(1)

	# If it hasn't been decompressed yet, do so now
	current_test_data_dir = os.path.join(test_data_dir, current_test_data)
	needs_decompression = not os.path.exists(current_test_data_dir)
	if needs_decompression:
		print('Decompressing {} ...'.format(current_test_data_zip))
		with zipfile.ZipFile(current_test_data_zip, 'r') as zip_ref:
			zip_ref.extractall(test_data_dir)

	# Grab all the test clips
	regression_clips = []
	for (dirpath, dirnames, filenames) in os.walk(current_test_data_dir):
		for filename in filenames:
			if not filename.endswith('.acl.sjson'):
				continue

			clip_filename = os.path.join(dirpath, filename)
			regression_clips.append(clip_filename)

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
				test_configs.append(config_filename)

	if len(test_configs) == 0:
		print('No regression configurations found')
		sys.exit(1)

	print('Found {} regression configurations'.format(len(test_configs)))

	if needs_decompression:
		with open(os.path.join(current_test_data_dir, 'metadata.sjson'), 'w') as metadata_file:
			print('configs = [', file = metadata_file)
			for config_filename in test_configs:
				print('\t"{}"'.format(os.path.relpath(config_filename, test_config_dir)), file = metadata_file)
			print(']', file = metadata_file)
			print('', file = metadata_file)
			print('clips = [', file = metadata_file)
			for clip_filename in regression_clips:
				print('\t"{}"'.format(os.path.relpath(clip_filename, current_test_data_dir)), file = metadata_file)
			print(']', file = metadata_file)
			print('', file = metadata_file)

def do_prepare_decompression_test_data(test_data_dir, options):
	print('Preparing decompression test data ...')

	current_data_zip = os.path.join(test_data_dir, '{}.zip'.format(current_decomp_data))

	# Validate that our regression test data is present
	if not os.path.exists(current_data_zip):
		print('Decompression test data not found: {}'.format(current_data_zip))
		sys.exit(1)

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
			if not filename.endswith('.acl.bin'):
				continue

			clip_filename = os.path.join(dirpath, filename)
			clips.append(clip_filename)

	if len(clips) == 0:
		print('No decompression clips found')
		sys.exit(1)

	print('Found {} decompression clips'.format(len(clips)))

	# Grab all the test configurations
	configs = []
	config_dir = os.path.join(test_data_dir, 'configs')
	if os.path.exists(config_dir):
		for (dirpath, dirnames, filenames) in os.walk(config_dir):
			for filename in filenames:
				if not filename.endswith('.config.sjson'):
					continue

				if not filename == 'uniformly_sampled_quant_var_2.config.sjson':
					continue

				config_filename = os.path.join(dirpath, filename)
				configs.append(config_filename)

	if len(configs) == 0:
		print('No decompression configurations found')
		sys.exit(1)

	print('Found {} decompression configurations'.format(len(configs)))

	if needs_decompression:
		with open(os.path.join(current_data_dir, 'metadata.sjson'), 'w') as metadata_file:
			print('configs = [', file = metadata_file)
			for config_filename in configs:
				print('\t"{}"'.format(os.path.relpath(config_filename, config_dir)), file = metadata_file)
			print(']', file = metadata_file)
			print('', file = metadata_file)
			print('clips = [', file = metadata_file)
			for clip_filename in clips:
				print('\t"{}"'.format(os.path.relpath(clip_filename, current_data_dir)), file = metadata_file)
			print(']', file = metadata_file)
			print('', file = metadata_file)

def do_regression_tests(ctest_exe, test_data_dir, options):
	print('Running regression tests ...')

	# Validate that our regression testing tool is present
	if platform.system() == 'Windows':
		compressor_exe_path = './bin/acl_compressor.exe'
	else:
		compressor_exe_path = './bin/acl_compressor'

	compressor_exe_path = os.path.abspath(compressor_exe_path)
	if not os.path.exists(compressor_exe_path):
		print('Compressor exe not found: {}'.format(compressor_exe_path))
		sys.exit(1)

	# Grab all the test clips
	regression_clips = []
	current_test_data_dir = os.path.join(test_data_dir, current_test_data)
	for (dirpath, dirnames, filenames) in os.walk(current_test_data_dir):
		for filename in filenames:
			if not filename.endswith('.acl.sjson'):
				continue

			clip_filename = os.path.join(dirpath, filename)
			regression_clips.append(clip_filename)

	# Grab all the test configurations
	test_configs = []
	test_config_dir = os.path.join(test_data_dir, 'configs')
	if os.path.exists(test_config_dir):
		for (dirpath, dirnames, filenames) in os.walk(test_config_dir):
			for filename in filenames:
				if not filename.endswith('.config.sjson'):
					continue

				config_filename = os.path.join(dirpath, filename)
				test_configs.append(config_filename)

	# Iterate over every clip and configuration and perform the regression testing
	for config_filename in test_configs:
		print('Performing regression tests for configuration: {}'.format(os.path.basename(config_filename)))
		regression_start_time = time.clock()

		cmd_queue = queue.Queue()
		completed_queue = queue.Queue()
		failed_queue = queue.Queue()
		failure_lock = threading.Lock()
		for clip_filename in regression_clips:
			cmd = '{} -acl="{}" -test -config="{}"'.format(compressor_exe_path, clip_filename, config_filename)
			if platform.system() == 'Windows':
				cmd = cmd.replace('/', '\\')

			cmd_queue.put((clip_filename, cmd))

		# Add a marker to terminate the threads
		for i in range(options['num_threads']):
			cmd_queue.put(None)

		def run_clip_regression_test(cmd_queue, completed_queue, failed_queue, failure_lock):
			while True:
				entry = cmd_queue.get()
				if entry is None:
					return

				(clip_filename, cmd) = entry

				result = os.system(cmd)

				if result != 0:
					failed_queue.put((clip_filename, cmd))
					failure_lock.acquire()
					print('Failed to run regression test for clip: {}'.format(clip_filename))
					print(cmd)
					failure_lock.release()

				completed_queue.put(clip_filename)

		threads = [ threading.Thread(target = run_clip_regression_test, args = (cmd_queue, completed_queue, failed_queue, failure_lock)) for _i in range(options['num_threads']) ]
		for thread in threads:
			thread.daemon = True
			thread.start()

		print_progress(0, len(regression_clips), 'Testing clips:', '{} / {}'.format(0, len(regression_clips)))
		try:
			while True:
				for thread in threads:
					thread.join(1.0)

				num_processed = completed_queue.qsize()
				print_progress(num_processed, len(regression_clips), 'Testing clips:', '{} / {}'.format(num_processed, len(regression_clips)))

				all_threads_done = True
				for thread in threads:
					if thread.isAlive():
						all_threads_done = False

				if all_threads_done:
					break
		except KeyboardInterrupt:
			sys.exit(1)

		regression_testing_failed = not failed_queue.empty()

		regression_end_time = time.clock()
		print('Done in {}'.format(format_elapsed_time(regression_end_time - regression_start_time)))

		if regression_testing_failed:
			sys.exit(1)

def print_help():
	print('Usage: python make.py [actions] [cpu arch] [compiler] [config] [misc]')
	print()
	print('Actions:')
	print('  If no action is specified, on Windows, OS X, and Linux the solution/make files are generated.')
	print('  Multiple actions can be used simultaneously.')
	print('  -build: Builds the solution.')
	print('  -clean: Cleans the build directory.')
	print('  -unit_test: Runs the unit tests.')
	print('  -regression_test: Runs the regression tests.')
	print()
	print('CPU Architecture:')
	print('  Only supported for Windows, OS X, and Linux. Defaults to the host system architecture.')
	print('  Only a single architecture argument must be used.')
	print('  -x86: Builds an x86 executable.')
	print('  -x64: Builds an x64 executable.')
	print()
	print('Compiler:')
	print('  Defaults to the host system\'s default compiler.')
	print('  Only a single compiler argument must be used.')
	print('  -vs2015: Uses Visual Studio 2015.')
	print('  -vs2017: Uses Visual Studio 2017.')
	print('  -osx: Uses X Code for OS X.')
	print('  -gcc5: Uses GCC 5.')
	print('  -gcc6: Uses GCC 6.')
	print('  -gcc7: Uses GCC 7.')
	print('  -clang4: Uses clang 4.')
	print('  -clang5: Uses clang 5.')
	print('  -android: Uses NVIDIA CodeWorks.')
	print('  -ios: Uses X Code for iOS.')
	print()
	print('Config:')
	print('  Defaults to Release.')
	print('  Only a single config argument mus tbe used.')
	print('  -debug: Uses the Debug configuration to build and test.')
	print('  -release: Uses the Release configuration to build and test.')
	print()
	print('Miscelanous:')
	print('  -avx: On Windows, OS X, and Linux AVX support will be enabled.')
	print('  -help: Prints this help message.')

if __name__ == "__main__":
	options = parse_argv()
	if options['print_help']:
		print_help()
		sys.exit(0)

	cmake_exe, ctest_exe = get_cmake_exes()
	compiler = options['compiler']
	cpu = options['cpu']
	config = options['config']

	# Set the ACL_CMAKE_HOME environment variable to point to CMake
	# otherwise we assume it is already in the user PATH
	if 'ACL_CMAKE_HOME' in os.environ:
		cmake_home = os.environ['ACL_CMAKE_HOME']
		cmake_exe = os.path.join(cmake_home, 'bin', cmake_exe)
		ctest_exe = os.path.join(cmake_home, 'bin', ctest_exe)

	build_dir = os.path.join(os.getcwd(), 'build')
	test_data_dir = os.path.join(os.getcwd(), 'test_data')
	cmake_script_dir = os.path.join(os.getcwd(), 'cmake')

	if options['clean'] and os.path.exists(build_dir):
		print('Cleaning previous build ...')
		shutil.rmtree(build_dir)

	if not os.path.exists(build_dir):
		os.makedirs(build_dir)

	os.chdir(build_dir)

	print('Using config: {}'.format(config))
	print('Using cpu: {}'.format(cpu))
	if not compiler == None:
		print('Using compiler: {}'.format(compiler))

	do_prepare_regression_test_data(test_data_dir, options)
	do_prepare_decompression_test_data(test_data_dir, options)

	do_generate_solution(cmake_exe, build_dir, cmake_script_dir, options)

	if options['build']:
		do_build(cmake_exe, options)

	if options['unit_test']:
		do_tests(ctest_exe, options)

	if options['regression_test'] and not options['compiler'] == 'android' and not options['compiler'] == 'ios':
		do_regression_tests(ctest_exe, test_data_dir, options)
