import os
import subprocess
import sys

def parse_argv():
	options = {}
	options['build'] = False

	for i in range(1, len(sys.argv)):
		value = sys.argv[i]

		if value == '-build':
			options['build'] = True

	return options

if __name__ == "__main__":
	options = parse_argv()

	# Set the ACL_CMAKE_HOME environment variable to point to CMake
	if 'ACL_CMAKE_HOME' in os.environ:
		cmake_home = os.environ['ACL_CMAKE_HOME']
		cmake_exe = os.path.join(cmake_home, 'bin/cmake.exe')
	else:
		# Assume cmake.exe is already in the user PATH
		cmake_exe = 'cmake.exe'

	build_dir = os.path.join(os.getcwd(), 'build')

	if not os.path.exists(build_dir):
		os.makedirs(build_dir)

	os.chdir(build_dir)

	# Generate IDE solution
	cmake_cmd = '"{}" .. -DCMAKE_INSTALL_PREFIX="{}" -G "Visual Studio 14 Win64"'.format(cmake_exe, build_dir)
	subprocess.call(cmake_cmd, shell=True)

	if options['build']:
		cmake_cmd = '"{}" --build . --config Release --target INSTALL'.format(cmake_exe)
		subprocess.call(cmake_cmd, shell=True)
