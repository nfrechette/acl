import os
import sys
from collections import namedtuple

Stats = namedtuple('Stats', 'filename name rotation_format translation_format range_reduction raw_size compressed_size ratio max_error compression_time duration num_animated_tracks')
RunStats = namedtuple('RunStats', 'name total_raw_size total_compressed_size max_error num_runs')

def parse_argv():
	options = {}
	options['acl'] = ""
	options['stats'] = ""
	options['csv'] = False
	options['refresh'] = False

	for i in range(1, len(sys.argv)):
		value = sys.argv[i]

		# TODO: Strip trailing '/' or '\'
		if value.startswith('-acl='):
			options['acl'] = value[5:].replace('"', '')

		if value.startswith('-stats='):
			options['stats'] = value[7:].replace('"', '')

		if value == '-csv':
			options['csv'] = True

		if value == '-refresh':
			options['refresh'] = True

	if options['acl'] == None:
		print('ACL input directory not found')
		print_usage()
		sys.exit(1)

	if options['stats'] == None:
		print('Stat output directory not found')
		print_usage()
		sys.exit(1)

	return options

def print_usage():
	print('Usage: python acl_compressor.py -acl=<path to directory containing ACL files> -stats=<path to output directory for stats> [-csv] [-refresh]')

def print_stat(stat):
	print('Algorithm: {}, Format: [{}, {}, {}], Ratio: {:.2f}, Error: {}'.format(stat.name, stat.rotation_format, stat.translation_format, stat.range_reduction, stat.ratio, stat.max_error))
	print('')

def bytes_to_mb(size_in_bytes):
	return size_in_bytes / (1024.0 * 1024.0)

def sanitize_csv_entry(entry):
	return entry.replace(', ', ' ').replace(',', '_')

def output_csv(stat_dir):
	csv_filename = os.path.join(stat_dir, 'stats.csv')
	print('Generating CSV file {}...'.format(csv_filename))
	print()
	file = open(csv_filename, 'w')
	print('Algorithm Name, Rotation Format, Translation Format, Range Reduction, Raw Size, Compressed Size, Compression Ratio, Compression Time, Clip Duration, Num Animated Tracks', file = file)
	for stat in stats:
		rotation_format = sanitize_csv_entry(stat.rotation_format)
		translation_format = sanitize_csv_entry(stat.translation_format)
		range_reduction = sanitize_csv_entry(stat.range_reduction)
		print('{}, {}, {}, {}, {}, {}, {}, {}, {}, {}'.format(stat.name, rotation_format, translation_format, range_reduction, stat.raw_size, stat.compressed_size, stat.ratio, stat.compression_time, stat.duration, stat.num_animated_tracks), file = file)
	file.close()

if __name__ == "__main__":
	options = parse_argv()

	compressor_exe_path = '../../build/bin/acl_compressor.exe'

	acl_dir = options['acl']
	stat_dir = options['stats']
	refresh = options['refresh']

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

			stat_files.append(stat_filename)

			if os.path.exists(stat_filename) and os.path.isfile(stat_filename) and not refresh:
				continue

			if not os.path.exists(stat_dirname):
				os.makedirs(stat_dirname)

			cmd = '{} -acl="{}" -stats="{}"'.format(compressor_exe_path, acl_filename, stat_filename)
			cmd = cmd.replace('/', '\\')

			print('Compressing {}...'.format(acl_filename))
			os.system(cmd)

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

				parsed_stats = []
				while len(line.strip()) != 0:
					parsed_stats.append(line.strip().split(': '))
					line = file.readline()

				name = next(x[1] for x in parsed_stats if x[0] == 'Clip algorithm')
				rotation_format = next(x[1] for x in parsed_stats if x[0] == 'Clip rotation format')
				translation_format = next(x[1] for x in parsed_stats if x[0] == 'Clip translation format')
				range_reduction = next(x[1] for x in parsed_stats if x[0] == 'Clip range reduction')
				raw_size = int(next(x[1] for x in parsed_stats if x[0] == 'Clip raw size (bytes)'))
				compressed_size = int(next(x[1] for x in parsed_stats if x[0] == 'Clip compressed size (bytes)'))
				ratio = float(raw_size) / float(compressed_size)
				max_error = float(next(x[1] for x in parsed_stats if x[0] == 'Clip max error'))
				compression_time = float(next(x[1] for x in parsed_stats if x[0] == 'Clip compression time (s)'))
				duration = float(next(x[1] for x in parsed_stats if x[0] == 'Clip duration (s)'))
				num_animated_tracks = int(next(x[1] for x in parsed_stats if x[0] == 'Clip num animated tracks'))

				stats.append(Stats(stat_filename, name, rotation_format, translation_format, range_reduction, raw_size, compressed_size, ratio, max_error, compression_time, duration, num_animated_tracks))

	print('Found {} runs'.format(len(stats)))
	print()

	if options['csv']:
		output_csv(stat_dir)

	# Aggregate per run type
	print('Stats per run type:')
	run_types = {}
	for stat in stats:
		key = stat.rotation_format + stat.translation_format + stat.range_reduction
		if not key in run_types:
			run_types[key] = RunStats('{}, {}, {}'.format(stat.rotation_format, stat.translation_format, stat.range_reduction), 0, 0, 0.0, 0)
		run_stats = run_types[key]
		raw_size = stat.raw_size + run_stats.total_raw_size
		compressed_size = stat.compressed_size + run_stats.total_compressed_size
		max_error = max(stat.max_error, run_stats.max_error)
		run_types[key] = RunStats(run_stats.name, raw_size, compressed_size, max_error, run_stats.num_runs + 1)

	run_types_by_size = sorted(run_types.values(), key = lambda entry: entry.total_compressed_size)
	for run_stats in run_types_by_size:
		ratio = float(run_stats.total_raw_size) / float(run_stats.total_compressed_size)
		print('Raw {:.2f} MB, Compressed {:.2f} MB, Ratio [{:.2f} : 1], Max error [{:.4f}] Run type: {}'.format(bytes_to_mb(run_stats.total_raw_size), bytes_to_mb(run_stats.total_compressed_size), ratio, run_stats.max_error, run_stats.name))
	print()

	# Find outliers
	best_error = 100000000.0
	best_error_entry = None
	worst_error = -100000000.0
	worst_error_entry = None
	worst_variable_error = -100000000.0
	worst_variable_error_entry = None
	best_ratio = 0.0
	best_ratio_entry = None
	worst_ratio = 100000000.0
	worst_ratio_entry = None
	for stat in stats:
		if stat.max_error < best_error:
			best_error = stat.max_error
			best_error_entry = stat

		if stat.max_error > worst_error:
			worst_error = stat.max_error
			worst_error_entry = stat

		if stat.max_error > worst_variable_error and stat.rotation_format.endswith('Variable') and stat.translation_format.endswith('Variable'):
			worst_variable_error = stat.max_error
			worst_variable_error_entry = stat

		if stat.ratio > best_ratio:
			best_ratio = stat.ratio
			best_ratio_entry = stat

		if stat.ratio < worst_ratio:
			worst_ratio = stat.ratio
			worst_ratio_entry = stat

	print('Most accurate: {}'.format(best_error_entry.filename))
	print_stat(best_error_entry)

	print('Least accurate: {}'.format(worst_error_entry.filename))
	print_stat(worst_error_entry)

	print('Least accurate variable: {}'.format(worst_variable_error_entry.filename))
	print_stat(worst_variable_error_entry)

	print('Best ratio: {}'.format(best_ratio_entry.filename))
	print_stat(best_ratio_entry)

	print('Worst ratio: {}'.format(worst_ratio_entry.filename))
	print_stat(worst_ratio_entry)
