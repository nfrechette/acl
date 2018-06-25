import os
import platform
import subprocess
import sys

def get_platform_compilers():
	if platform.system() == 'Windows':
		return [ '-vs2015', '-vs2017' ]
	elif platform.system() == 'Linux':
		return [ '-gcc5', '-gcc6', '-gcc7', '-clang4', '-clang5' ]
	elif platform.system() == 'Darwin':
		return [ '-osx' ]
	else:
		print('Unknown platform!')
		sys.exit(1)

if __name__ == "__main__":
	os.environ['PYTHONIOENCODING'] = 'utf_8'

	configs = [ '-debug', '-release' ]
	archs = [ '-x86', '-x64' ]
	compilers = get_platform_compilers()
	avx_opts = [ '', '-avx' ]

	cmds = []
	for config in configs:
		for arch in archs:
			for compiler in compilers:
				for w_avx in avx_opts:
					cmds.append('python make.py {} {} {} {} -build -unit_test -regression_test -clean'.format(compiler, arch, config, w_avx))

	root_dir = os.path.join(os.getcwd(), '../..')
	os.chdir(root_dir)

	for cmd in cmds:
		print('Running command: "{}" ...'.format(cmd))
		try:
			subprocess.check_output(cmd)
		except subprocess.CalledProcessError as e:
			print('Failed command: {}'.format(cmd))
			print(e.output.decode(sys.stdout.encoding))
			sys.exit(1)

	print('Done!')
	sys.exit(0)
