import os
import sys
from collections import namedtuple

Stats = namedtuple('Stats', 'file name rotation_format raw_size compressed_size ratio max_error compression_time duration num_animated_tracks')

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

def print_stat(stat):
	print('Algorithm: {}, Format: {}, Ratio: {:.2f}, Error: {}'.format(stat.name, stat.rotation_format, stat.ratio, stat.max_error))
	print('')

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

	stat_files = []

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

			stat_files.append(stat_filename)

	if len(stat_files) == 0:
		sys.exit(0)

	print('Aggregating results...')
	print('')

	stats = []
	for stat_filename in stat_files:
		with open(stat_filename, 'r') as file:
			line = file.readline()
			while line != '':
				if len(line.strip()) == 0:
					line = file.readline()
					continue

				name = line.split(': ')[1].strip()
				rotation_format = file.readline().split(': ')[1].strip()
				raw_size = float(file.readline().split(': ')[1].strip())
				compressed_size = float(file.readline().split(': ')[1].strip())
				ratio = file.readline().strip()
				ratio = raw_size / compressed_size
				max_error = float(file.readline().split(': ')[1].strip())
				compression_time = float(file.readline().split(': ')[1].strip())
				duration = float(file.readline().split(': ')[1].strip())
				num_animated_tracks = int(file.readline().split(': ')[1].strip())

				stats.append(Stats(stat_filename, name, rotation_format, raw_size, compressed_size, ratio, max_error, compression_time, duration, num_animated_tracks))
				line = file.readline()

	smallest_error = 100000000.0
	smallest_error_entry = None
	best_ratio = 0.0
	best_ratio_entry = None
	worst_ratio = 100000000.0
	worst_ratio_entry = None
	for stat in stats:
		if stat.max_error < smallest_error:
			smallest_error = stat.max_error
			smallest_error_entry = stat

		if stat.ratio > best_ratio:
			best_ratio = stat.ratio
			best_ratio_entry = stat

		if stat.ratio < worst_ratio:
			worst_ratio = stat.ratio
			worst_ratio_entry = stat

	print('Most accurate: {}'.format(smallest_error_entry.file))
	print_stat(smallest_error_entry)

	print('Best ratio: {}'.format(best_ratio_entry.file))
	print_stat(best_ratio_entry)

	print('Worst ratio: {}'.format(worst_ratio_entry.file))
	print_stat(worst_ratio_entry)
