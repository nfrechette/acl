import os
import platform
import shutil
import subprocess
import sys

def parse_argv():
	options = {}
	options['build'] = False
	options['clean'] = False
	options['test'] = False
	options['use_avx'] = False
	options['compiler'] = None
	options['config'] = 'Release'
	options['cpu'] = 'x64'

	for i in range(1, len(sys.argv)):
		value = sys.argv[i]
		value_upper = value.upper()

		if value == '-build':
			options['build'] = True

		if value == '-clean':
			options['clean'] = True

		if value == '-test':
			options['test'] = True

		if value == '-avx':
			options['use_avx'] = True

		# TODO: Refactor to use the form: -compiler=vs2015
		if value == '-vs2015':
			options['compiler'] = 'vs2015'

		if value == '-vs2017':
			options['compiler'] = 'vs2017'

		if value == '-clang4':
			options['compiler'] = 'clang4'

		if value == '-clang5':
			options['compiler'] = 'clang5'

		if value == '-gcc5':
			options['compiler'] = 'gcc5'

		if value == '-xcode':
			options['compiler'] = 'xcode'

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
	elif platform.system() == 'Darwin':
		if compiler == 'xcode':
			return 'Xcode'
	else:
		return 'Unix Makefiles'

	print('Unknown compiler: {}'.format(compiler))
	sys.exit(1)

def set_compiler_env(compiler):
	if not platform.system() == 'Windows' and not platform.system() == 'Darwin':
		if compiler == 'clang4':
			os.environ['CC'] = 'clang-4.0'
			os.environ['CXX'] = 'clang++-4.0'
		elif compiler == 'clang5':
			os.environ['CC'] = 'clang-5.0'
			os.environ['CXX'] = 'clang++-5.0'
		elif compiler == 'gcc5':
			os.environ['CC'] = 'gcc-5'
			os.environ['CXX'] = 'g++-5'
		else:
			print('Unknown compiler: {}'.format(compiler))
			sys.exit(1)

if __name__ == "__main__":
	options = parse_argv()
	cmake_exe, ctest_exe = get_cmake_exes()
	compiler = options['compiler']
	cpu = options['cpu']
	config = options['config']

	print('Using config: {}'.format(config))

	# Set the ACL_CMAKE_HOME environment variable to point to CMake
	# otherwise we assume it is already in the user PATH
	if 'ACL_CMAKE_HOME' in os.environ:
		cmake_home = os.environ['ACL_CMAKE_HOME']
		cmake_exe = os.path.join(cmake_home, 'bin', cmake_exe)
		ctest_exe = os.path.join(cmake_home, 'bin', ctest_exe)

	build_dir = os.path.join(os.getcwd(), 'build')

	if options['clean'] and os.path.exists(build_dir):
		print('Cleaning previous build ...')
		shutil.rmtree(build_dir)

	if not os.path.exists(build_dir):
		os.makedirs(build_dir)

	os.chdir(build_dir)

	if options['build'] or not options['test']:
		print('Using cpu: {}'.format(cpu))
		if not compiler == None:
			print('Using compiler: {}'.format(compiler))

		if not compiler == None:
			set_compiler_env(compiler)

		extra_switches = []
		if not platform.system() == 'Windows':
			extra_switches.append('-DCPU_INSTRUCTION_SET:STRING={}'.format(cpu))

		if options['use_avx']:
			print('Enabling AVX usage')
			extra_switches.append('-DUSE_AVX_INSTRUCTIONS:BOOL=true')

		# Generate IDE solution
		print('Generating build files ...')
		cmake_cmd = '"{}" .. -DCMAKE_INSTALL_PREFIX="{}" {}'.format(cmake_exe, build_dir, ' '.join(extra_switches))
		cmake_generator = get_generator(compiler, cpu)
		if cmake_generator == None:
			print('Using default generator')
		else:
			print('Using generator: {}'.format(cmake_generator))
			cmake_cmd += ' -G "{}"'.format(cmake_generator)

		cmake_cmd += ' -DCMAKE_BUILD_TYPE={}'.format(config.upper())

		result = subprocess.call(cmake_cmd, shell=True)
		if result != 0:
			sys.exit(result)

	if options['build']:
		print('Building ...')
		cmake_cmd = '"{}" --build .'.format(cmake_exe)
		if platform.system() == 'Windows':
			cmake_cmd += ' --config {} --target INSTALL'.format(config)
		elif platform.system() == 'Darwin':
			cmake_cmd += ' --config {}'.format(config)
		else:
			cmake_cmd += ' --target install'

		result = subprocess.call(cmake_cmd, shell=True)
		if result != 0:
			sys.exit(result)
	
	if options['test']:
		print('Running unit tests ...')
		ctest_cmd = '"{}" --output-on-failure'.format(ctest_exe)
		if platform.system() == 'Windows' or platform.system() == 'Darwin':
			ctest_cmd += ' -C {}'.format(config)

		result = subprocess.call(ctest_cmd, shell=True)
		if result != 0:
			sys.exit(result)
