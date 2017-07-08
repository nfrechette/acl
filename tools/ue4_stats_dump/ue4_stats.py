import os
import sys
from collections import namedtuple

Stats = namedtuple('Stats', 'filename name rotation_format translation_format raw_size compressed_size ratio max_error duration')
RunStats = namedtuple('RunStats', 'name total_raw_size total_compressed_size max_error num_runs')

def parse_argv():
	options = {}
	options['stats'] = ""
	options['csv'] = False

	for i in range(1, len(sys.argv)):
		value = sys.argv[i]

		# TODO: Strip trailing '/' or '\'
		if value.startswith('-stats='):
			options['stats'] = value[7:].replace('"', '')

		if value == '-csv':
			options['csv'] = True

	if options['stats'] == None:
		print('Stat input directory not found')
		print_usage()
		sys.exit(1)

	return options

def print_usage():
	print('Usage: python ue4_stats.py -stats=<path to input directory for stats> [-csv]')

def print_stat(stat):
	print('Algorithm: {}, Format: [{}, {}], Ratio: {:.2f}, Error: {}'.format(stat.name, stat.rotation_format, stat.translation_format, stat.ratio, stat.max_error))
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
	print('Algorithm Name, Rotation Format, Translation Format, Raw Size, Compressed Size, Compression Ratio, Clip Duration, Max Error', file = file)
	for stat in stats:
		clean_name = sanitize_csv_entry(stat.name)
		print('{}, {}, {}, {}, {}, {}, {}, {}'.format(clean_name, stat.rotation_format, stat.translation_format, stat.raw_size, stat.compressed_size, stat.ratio, stat.duration, stat.max_error), file = file)
	file.close()

if __name__ == "__main__":
	options = parse_argv()

	stat_dir = options['stats']

	if not os.path.exists(stat_dir) or not os.path.isdir(stat_dir):
		print('Stats input directory not found: {}'.format(stat_dir))
		print_usage()
		sys.exit(1)

	stat_files = []

	for (dirpath, dirnames, filenames) in os.walk(stat_dir):
		for filename in filenames:
			if not filename.endswith('.txt'):
				continue

			stat_filename = os.path.join(dirpath, filename)
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

				parsed_stats = []
				while len(line.strip()) != 0:
					parsed_stats.append(line.strip().split(': '))
					line = file.readline()

				name = next(x[1] for x in parsed_stats if x[0] == 'Clip algorithm')
				rotation_format = next(x[1] for x in parsed_stats if x[0] == 'Clip rotation format')
				translation_format = next(x[1] for x in parsed_stats if x[0] == 'Clip translation format')
				raw_size = int(next(x[1] for x in parsed_stats if x[0] == 'Clip raw size (bytes)'))
				compressed_size = int(next(x[1] for x in parsed_stats if x[0] == 'Clip compressed size (bytes)'))
				ratio = float(raw_size) / float(compressed_size)
				max_error = float(next(x[1] for x in parsed_stats if x[0] == 'Clip max error'))
				duration = float(next(x[1] for x in parsed_stats if x[0] == 'Clip duration (s)'))

				stats.append(Stats(stat_filename, name, rotation_format, translation_format, raw_size, compressed_size, ratio, max_error, duration))

	print('Found {} runs'.format(len(stats)))
	print()

	if options['csv']:
		output_csv(stat_dir)

	# Aggregate per run type
	print('Stats per run type:')
	run_types = {}
	for stat in stats:
		key = stat.name + stat.rotation_format + stat.translation_format
		if not key in run_types:
			run_types[key] = RunStats('{}, {}, {}'.format(stat.name, stat.rotation_format, stat.translation_format), 0, 0, 0.0, 0)
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

	print('Best ratio: {}'.format(best_ratio_entry.filename))
	print_stat(best_ratio_entry)

	print('Worst ratio: {}'.format(worst_ratio_entry.filename))
	print_stat(worst_ratio_entry)
