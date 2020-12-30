import os
import platform
import subprocess
import sys

def get_platform_compilers():
	if platform.system() == 'Windows':
		return [ 'vs2015', 'vs2017', 'vs2019' ]
	elif platform.system() == 'Linux':
		return [ 'gcc5', 'gcc6', 'gcc7', 'gcc8', 'gcc9', 'gcc10', 'clang4', 'clang5', 'clang6', 'clang7', 'clang8', 'clang9', 'clang10', 'clang11' ]
	elif platform.system() == 'Darwin':
		return [ 'osx' ]
	else:
		print('Unknown platform!')
		sys.exit(1)

def get_python_exe_name():
	if platform.system() == 'Windows':
		return 'python'
	else:
		return 'python3'

if __name__ == "__main__":
	if sys.version_info < (3, 4):
		print('Python 3.4 or higher needed to run this script')
		sys.exit(1)

	os.environ['PYTHONIOENCODING'] = 'utf_8'

	configs = [ 'debug', 'release' ]
	archs = [ 'x86', 'x64' ]
	compilers = get_platform_compilers()
	simd_opts = [ '', '-avx', '-nosimd', '-nosjson' ]
	python_exe = get_python_exe_name()

	cmd_args = []
	for config in configs:
		for arch in archs:
			for compiler in compilers:
				for simd in simd_opts:
					args = [python_exe, 'make.py', '-compiler', compiler, '-cpu', arch, '-config', config, simd, '-build', '-unit_test', '-clean']
					if config != 'debug':
						# Regression testing is too slow in debug and not really necessary
						args.append('-regression_test')
					cmd_args.append([x for x in args if x])

	if platform.system() == 'Windows':
		for config in configs:
			# Windows ARM
			args = [python_exe, 'make.py', '-compiler', 'vs2017', '-cpu', 'arm64', '-config', config, '-build', '-clean']
			cmd_args.append([x for x in args if x])
			args = [python_exe, 'make.py', '-compiler', 'vs2017', '-cpu', 'arm64', '-config', config, '-build', '-clean', '-nosimd']
			cmd_args.append([x for x in args if x])
			args = [python_exe, 'make.py', '-compiler', 'vs2019', '-cpu', 'arm64', '-config', config, '-build', '-clean']
			cmd_args.append([x for x in args if x])
			args = [python_exe, 'make.py', '-compiler', 'vs2019', '-cpu', 'arm64', '-config', config, '-build', '-clean', '-nosimd']
			cmd_args.append([x for x in args if x])
	elif platform.system() == 'Darwin':
		for config in configs:
			# iOS
			args = [python_exe, 'make.py', '-compiler', 'ios', '-config', config, '-build', '-clean']
			cmd_args.append([x for x in args if x])
			args = [python_exe, 'make.py', '-compiler', 'ios', '-config', config, '-build', '-clean', '-nosimd']
			cmd_args.append([x for x in args if x])

	if platform.system() == 'Darwin' or platform.system() == 'Linux':
		for config in configs:
			# Emscripten
			args = [python_exe, 'make.py', '-compiler', 'emscripten', '-config', config, '-build', '-unit_test', '-clean']
			if config != 'debug':
				# Regression testing is too slow in debug and not really necessary
				args.append('-regression_test')
			cmd_args.append([x for x in args if x])

	root_dir = os.path.join(os.getcwd(), '../..')
	os.chdir(root_dir)

	for args in cmd_args:
		print('Running command: "{}" ...'.format(" ".join(args)))
		try:
			subprocess.check_output(args)
		except subprocess.CalledProcessError as e:
			print('Failed command: {}'.format(" ".join(args)))
			print(e.output.decode(sys.stdout.encoding))
			sys.exit(1)

	print('Done!')
	sys.exit(0)
