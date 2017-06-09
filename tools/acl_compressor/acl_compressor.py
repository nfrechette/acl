import os
import sys

def parse_argv():
	options = {}
	options['acl'] = ""
	options['stat'] = ""

	for i in range(1, len(sys.argv)):
		value = sys.argv[i]

		# TODO: Strip trailing '/' or '\'
		if value.startswith('-acl='):
			options['acl'] = value[5:].replace('"', '')

		if value.startswith('-stat='):
			options['stat'] = value[6:].replace('"', '')

	return options

def print_usage():
	print('Usage: python acl_compressor.py -acl=<path to directory containing ACL files> -stat=<path to output directory for stats>')

if __name__ == "__main__":
	options = parse_argv()

	debug_exe_path = './x64/Debug/acl_compressor.exe'
	release_exe_path = './x64/Release/acl_compressor.exe'

	debug_exe_timestamp = os.path.getmtime(debug_exe_path)
	release_exe_timestamp = os.path.getmtime(release_exe_path)

	if release_exe_timestamp >= debug_exe_timestamp:
		latest_exe_path = release_exe_path
	else:
		latest_exe_path = debug_exe_path

	acl_dir = options['acl']
	stat_dir = options['stat']

	if not os.path.exists(acl_dir) or not os.path.isdir(acl_dir):
		print('ACL input directory not found: {}'.format(acl_dir))
		print_usage()
		sys.exit(1)

	if not os.path.exists(stat_dir):
		os.makedirs(stat_dir)

	if not os.path.isdir(stat_dir):
		print('The output stat argument must be a directory')
		print_usage()
		sys.exit(1)

	for (dirpath, dirnames, filenames) in os.walk(acl_dir):
		stat_dirname = dirpath.replace(acl_dir, stat_dir)

		for filename in filenames:
			if not filename.endswith('.acl.js'):
				continue

			acl_filename = os.path.join(dirpath, filename)
			stat_filename = os.path.join(stat_dirname, filename.replace('.acl.js', '_stats.txt'))

			if not os.path.exists(stat_dirname):
				os.makedirs(stat_dirname)

			cmd = '{} -acl="{}" -stats="{}"'.format(latest_exe_path, acl_filename, stat_filename)
			cmd = cmd.replace('/', '\\')

			print('Compressing {}...'.format(acl_filename))
			os.system(cmd)
