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

	for i in range(1, len(sys.argv)):
		value = sys.argv[i]

		if value == '-build':
			options['build'] = True

		if value == '-clean':
			options['clean'] = True

		if value == '-test':
			options['test'] = True

		if value == '-avx':
			options['use_avx'] = True

		if value == '-clang4':
			options['compiler'] = 'clang4'

		if value == '-clang5':
			options['compiler'] = 'clang5'

		if value == '-gcc5':
			options['compiler'] = 'gcc5'

	return options

def get_cmake_exes():
	if platform.system() == 'Windows':
		return ('cmake.exe', 'ctest.exe')
	else:
		return ('cmake', 'ctest')

def get_generator():
	if platform.system() == 'Windows':
		return 'Visual Studio 14 Win64'
	else:
		return 'Unix Makefiles'

def set_compiler(compiler):
	if compiler == 'clang4':
		os.environ['CC'] = 'clang-4.0'
		os.environ['CXX'] = 'clang++-4.0'
	elif compiler == 'clang5':
		os.environ['CC'] = 'clang-5.0'
		os.environ['CXX'] = 'clang++-5.0'
	elif compiler == 'gcc5':
		os.environ['CC'] = 'gcc-5'
		os.environ['CXX'] = 'g++-5'

if __name__ == "__main__":
	options = parse_argv()
	cmake_exe, ctest_exe = get_cmake_exes()
	cmake_generator = get_generator()

	# Set the ACL_CMAKE_HOME environment variable to point to CMake
	# otherwise we assume it is already in the user PATH
	if 'ACL_CMAKE_HOME' in os.environ:
		cmake_home = os.environ['ACL_CMAKE_HOME']
		cmake_exe = os.path.join(cmake_home, 'bin', cmake_exe)
		ctest_exe = os.path.join(cmake_home, 'bin', ctest_exe)

	build_dir = os.path.join(os.getcwd(), 'build')

	if options['clean']:
		print('Cleaning previous build ...')
		shutil.rmtree(build_dir)

	if not os.path.exists(build_dir):
		os.makedirs(build_dir)

	os.chdir(build_dir)

	if options['compiler'] != None:
		set_compiler(options['compiler'])

	extra_switches = []
	if options['use_avx']:
		print('Enabling AVX usage')
		extra_switches.append("-DUSE_AVX_INSTRUCTIONS:BOOL=true")

	# Generate IDE solution
	print('Generating build files for: {}'.format(cmake_generator))
	cmake_cmd = '"{}" .. -DCMAKE_INSTALL_PREFIX="{}" {} -G "{}"'.format(cmake_exe, build_dir, '.'.join(extra_switches), cmake_generator)
	subprocess.call(cmake_cmd, shell=True)

	if options['build']:
		print('Building ...')
		cmake_cmd = '"{}" --build . --config Release'.format(cmake_exe)
		if platform.system() == 'Windows':
			cmake_cmd += ' --target INSTALL'

		subprocess.call(cmake_cmd, shell=True)
	
	if options['test']:
		print('Running unit tests ...')
		ctest_cmd = '"{}" --output-on-failure'.format(ctest_exe)
		subprocess.call(ctest_cmd, shell=True)
