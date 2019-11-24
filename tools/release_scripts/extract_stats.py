import os
import platform
import subprocess
import sys

def print_usage():
	print('Usage: python extract_stats.py -acl=<path/to/raw_clips_directory> -version=<1.0.0-test>')

def parse_argv():
	options = {}
	options['acl'] = ''
	options['version'] = None

	for i in range(1, len(sys.argv)):
		value = sys.argv[i]

		# TODO: Strip trailing '/' or '\'
		if value.startswith('-acl='):
			options['acl'] = value[len('-acl='):].replace('"', '')
			options['acl'] = os.path.expanduser(options['acl'])

		if value.startswith('-version='):
			options['version'] = value[len('-version='):]

	if not os.path.exists(options['acl']) or not os.path.isdir(options['acl']):
		print('ACL input directory not found: {}'.format(options['acl']))
		print_usage()
		sys.exit(1)

	if options['version'] is None or len(options['version']) == 0:
		print('-version missing')
		sys.exit(1)

	return options

def get_python_exe_name():
	if platform.system() == 'Windows':
		return 'python'
	else:
		return 'python3'

def safe_create_dir(dir_path):
	if os.path.exists(dir_path):
		print('Directory already exists: {}'.format(dir_path))
		return False

	os.makedirs(dir_path)
	return True

def run_cmd(cmd, output_log):
	try:
		args = cmd.split(' ')
		result = subprocess.check_output(args)
		with open(output_log, 'w') as log_file:
			log_file.write(result.decode('utf-8'))
	except subprocess.CalledProcessError as e:
		print('Failed command: {}'.format(cmd))
		print(e.output.decode(sys.stdout.encoding))
		sys.exit(1)

if __name__ == "__main__":
	if sys.version_info < (3, 4):
		print('Python 3.4 or higher needed to run this script')
		sys.exit(1)

	options = parse_argv()

	os.environ['PYTHONIOENCODING'] = 'utf_8'

	python_exe = get_python_exe_name()
	acl_raw = options['acl']
	version = options['version']

	summary_output_dir = '{}-{}-summary'.format(acl_raw, version)
	error_output_dir = '{}-{}-error'.format(acl_raw, version)

	high_output_dir = '{}-{}-high'.format(acl_raw, version)
	highest_output_dir = '{}-{}-highest'.format(acl_raw, version)

	cmds = []
	if safe_create_dir(summary_output_dir):
		output_log = os.path.join(summary_output_dir, 'output.log')
		cmds.append(('{} acl_compressor.py -acl="{}" -stats="{}" -csv_summary -parallel=14 -no_progress_bar -level=medium'.format(python_exe, acl_raw, summary_output_dir, version), output_log))
	if safe_create_dir(high_output_dir):
		output_log = os.path.join(high_output_dir, 'output.log')
		cmds.append(('{} acl_compressor.py -acl="{}" -stats="{}" -csv_summary -parallel=14 -no_progress_bar -level=high'.format(python_exe, acl_raw, high_output_dir, version), output_log))
	if safe_create_dir(highest_output_dir):
		output_log = os.path.join(highest_output_dir, 'output.log')
		cmds.append(('{} acl_compressor.py -acl="{}" -stats="{}" -csv_summary -parallel=14 -no_progress_bar -level=highest'.format(python_exe, acl_raw, highest_output_dir, version), output_log))
	if safe_create_dir(error_output_dir):
		output_log = os.path.join(error_output_dir, 'output.log')
		cmds.append(('{} acl_compressor.py -acl="{}" -stats="{}" -csv_error -parallel=30 -no_progress_bar -level=medium -stat_exhaustive'.format(python_exe, acl_raw, error_output_dir, version), output_log))

	root_dir = os.path.join(os.getcwd(), '../acl_compressor')
	os.chdir(root_dir)

	for (cmd, output_log) in cmds:
		print('Running command: "{}" ...'.format(cmd))
		run_cmd(cmd, output_log)

	print('Done!')
	sys.exit(0)
