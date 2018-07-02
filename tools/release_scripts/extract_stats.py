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

	if options['version'] == None or len(options['version']) == 0:
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
	options = parse_argv()

	os.environ['PYTHONIOENCODING'] = 'utf_8'

	python_exe = get_python_exe_name()
	acl_raw = options['acl']
	version = options['version']

	summary_output_dir = '{}-{}-summary'.format(acl_raw, version)
	detailed_output_dir = '{}-{}-detailed'.format(acl_raw, version)
	error_output_dir = '{}-{}-error'.format(acl_raw, version)

	cmds = []
	if safe_create_dir(summary_output_dir):
		summary_log = os.path.join(summary_output_dir, 'output.log')
		cmds.append(('{} acl_compressor.py -acl="{}" -stats="{}" -csv_summary -parallel=4 -no_progress_bar'.format(python_exe, acl_raw, summary_output_dir, version), summary_log))
	if safe_create_dir(detailed_output_dir):
		detailed_log = os.path.join(detailed_output_dir, 'output.log')
		cmds.append(('{} acl_compressor.py -acl="{}" -stats="{}" -csv_bit_rate -csv_animated_size -parallel=10 -no_progress_bar -stat_detailed'.format(python_exe, acl_raw, detailed_output_dir, version), detailed_log))
	if safe_create_dir(error_output_dir):
		error_log = os.path.join(error_output_dir, 'output.log')
		cmds.append(('{} acl_compressor.py -acl="{}" -stats="{}" -csv_error -parallel=10 -no_progress_bar -stat_exhaustive'.format(python_exe, acl_raw, error_output_dir, version), error_log))

	root_dir = os.path.join(os.getcwd(), '../acl_compressor')
	os.chdir(root_dir)

	for (cmd, output_log) in cmds:
		print('Running command: "{}" ...'.format(cmd))
		run_cmd(cmd, output_log)

	print('Done!')
	sys.exit(0)
