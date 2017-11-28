import os
import sys

# This script depends on a SJSON parsing package:
# https://pypi.python.org/pypi/SJSON/1.1.0
# https://shelter13.net/projects/SJSON/
# https://bitbucket.org/Anteru/sjson/src
import sjson


def parse_argv():
	options = {}
	options['stats'] = ""
	options['csv_summary'] = False
	options['csv_error'] = False

	for i in range(1, len(sys.argv)):
		value = sys.argv[i]

		# TODO: Strip trailing '/' or '\'
		if value.startswith('-stats='):
			options['stats'] = value[7:].replace('"', '')

		if value == '-csv_summary':
			options['csv_summary'] = True

		if value == '-csv_error':
			options['csv_error'] = True

	if options['stats'] == None:
		print('Stat input directory not found')
		print_usage()
		sys.exit(1)

	return options

def print_usage():
	print('Usage: python ue4_stats.py -stats=<path to input directory for stats> [-csv_summary] [-csv_error]')

def print_stat(stat):
	print('Algorithm: {}, Format: [{}, {}], Ratio: {:.2f}, Error: {}'.format(stat['algorithm_name'], stat['rotation_format'], stat['translation_format'], stat['acl_compression_ratio'], stat['max_error']))
	print('')

def bytes_to_mb(size_in_bytes):
	return size_in_bytes / (1024.0 * 1024.0)

def format_elapsed_time(elapsed_time):
	hours, rem = divmod(elapsed_time, 3600)
	minutes, seconds = divmod(rem, 60)
	return '{:0>2}h {:0>2}m {:05.2f}s'.format(int(hours), int(minutes), seconds)

def sanitize_csv_entry(entry):
	return entry.replace(', ', ' ').replace(',', '_')

def output_csv_summary(stat_dir, stats):
	csv_filename = os.path.join(stat_dir, 'stats_summary.csv')
	print('Generating CSV file {} ...'.format(csv_filename))
	print()
	file = open(csv_filename, 'w')
	print('Algorithm Name, Raw Size, Compressed Size, Compression Ratio, Clip Duration, Max Error', file = file)
	for stat in stats:
		clean_name = sanitize_csv_entry(stat['desc'])
		print('{}, {}, {}, {}, {}, {}'.format(clean_name, stat['acl_raw_size'], stat['compressed_size'], stat['acl_compression_ratio'], stat['duration'], stat['max_error']), file = file)
	file.close()

def output_csv_error(stat_dir, stats):
	csv_filename = os.path.join(stat_dir, 'stats_error.csv')
	print('Generating CSV file {} ...'.format(csv_filename))
	print()
	file = open(csv_filename, 'w')
	print('Clip Name, Key Frame, Bone Index, Error', file = file)
	for stat in stats:
		name = stat['clip_name']
		key_frame = 0
		for frame_errors in stat['error_per_frame_and_bone']:
			bone_index = 0
			for bone_error in frame_errors:
				print('{}, {}, {}, {}'.format(name, key_frame, bone_index, bone_error), file = file)
				bone_index += 1

			key_frame += 1
	file.close()

def print_progress(iteration, total, prefix='', suffix='', decimals = 1, bar_length = 50):
	# Taken from https://stackoverflow.com/questions/3173320/text-progress-bar-in-the-console
	"""
	Call in a loop to create terminal progress bar
	@params:
		iteration   - Required  : current iteration (Int)
		total       - Required  : total iterations (Int)
		prefix      - Optional  : prefix string (Str)
		suffix      - Optional  : suffix string (Str)
		decimals    - Optional  : positive number of decimals in percent complete (Int)
		bar_length  - Optional  : character length of bar (Int)
	"""
	str_format = "{0:." + str(decimals) + "f}"
	percents = str_format.format(100 * (iteration / float(total)))
	filled_length = int(round(bar_length * iteration / float(total)))
	bar = '█' * filled_length + '-' * (bar_length - filled_length)

	sys.stdout.write('\r%s |%s| %s%s %s' % (prefix, bar, percents, '%', suffix)),

	if iteration == total:
		sys.stdout.write('\n')
	sys.stdout.flush()

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
			if not filename.endswith('.sjson'):
				continue

			stat_filename = os.path.join(dirpath, filename)
			stat_files.append(stat_filename)

	if len(stat_files) == 0:
		sys.exit(0)

	stats = []
	num_stat_file_processed = 0
	print_progress(0, len(stat_files), 'Aggregating results:', '{} / {}'.format(num_stat_file_processed, len(stat_files)))
	for stat_filename in stat_files:
		with open(stat_filename, 'r') as file:
			file_data = sjson.loads(file.read())
			file_data['filename'] = stat_filename
			file_data['desc'] = '{} {} {}'.format(file_data['algorithm_name'], file_data['rotation_format'], file_data['translation_format'])
			file_data['clip_name'] = os.path.splitext(os.path.basename(stat_filename))[0]

			if not options['csv_error']:
				file_data['error_per_frame_and_bone'] = []

			stats.append(file_data)
			num_stat_file_processed += 1
			print_progress(num_stat_file_processed, len(stat_files), 'Aggregating results:', '{} / {}'.format(num_stat_file_processed, len(stat_files)))

	print()

	if options['csv_summary']:
		output_csv_summary(stat_dir, stats)

	if options['csv_error']:
		output_csv_error(stat_dir, stats)

	# Aggregate per run type
	print('Stats per run type:')
	run_types = {}
	total_run_types = {}
	total_run_types['desc'] = 'Total'
	total_run_types['total_raw_size'] = 0
	total_run_types['total_compressed_size'] = 0
	total_run_types['total_compression_time'] = 0.0
	total_run_types['max_error'] = 0.0
	total_run_types['num_runs'] = 0
	for stat in stats:
		key = stat['desc']
		if not key in run_types:
			run_stats = {}
			run_stats['desc'] = key
			run_stats['total_raw_size'] = 0
			run_stats['total_compressed_size'] = 0
			run_stats['total_compression_time'] = 0.0
			run_stats['max_error'] = 0.0
			run_stats['num_runs'] = 0
			run_types[key] = run_stats

		run_stats = run_types[key]
		run_stats['total_raw_size'] += stat['acl_raw_size']
		run_stats['total_compressed_size'] += stat['compressed_size']
		run_stats['total_compression_time'] += stat['compression_time']
		run_stats['max_error'] = max(stat['max_error'], run_stats['max_error'])
		run_stats['num_runs'] += 1

		total_run_types['total_raw_size'] += stat['acl_raw_size']
		total_run_types['total_compressed_size'] += stat['compressed_size']
		total_run_types['total_compression_time'] += stat['compression_time']
		total_run_types['max_error'] = max(stat['max_error'], total_run_types['max_error'])
		total_run_types['num_runs'] += 1

	run_types_by_size = sorted(run_types.values(), key = lambda entry: entry['total_compressed_size'])
	for run_stats in run_types_by_size:
		ratio = float(run_stats['total_raw_size']) / float(run_stats['total_compressed_size'])
		print('Raw {:.2f} MB, Compressed {:.2f} MB, Elapsed {}, Ratio [{:.2f} : 1], Max error [{:.4f}] Run type: {}'.format(bytes_to_mb(run_stats['total_raw_size']), bytes_to_mb(run_stats['total_compressed_size']), format_elapsed_time(run_stats['total_compression_time']), ratio, run_stats['max_error'], run_stats['desc']))

	print()
	print('Total:')
	ratio = float(total_run_types['total_raw_size']) / float(total_run_types['total_compressed_size'])
	print('Raw {:.2f} MB, Compressed {:.2f} MB, Elapsed {}, Ratio [{:.2f} : 1], Max error [{:.4f}]'.format(bytes_to_mb(total_run_types['total_raw_size']), bytes_to_mb(total_run_types['total_compressed_size']), format_elapsed_time(total_run_types['total_compression_time']), ratio, total_run_types['max_error']))
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
		if stat['max_error'] < best_error:
			best_error = stat['max_error']
			best_error_entry = stat

		if stat['max_error'] > worst_error:
			worst_error = stat['max_error']
			worst_error_entry = stat

		if stat['acl_compression_ratio'] > best_ratio:
			best_ratio = stat['acl_compression_ratio']
			best_ratio_entry = stat

		if stat['acl_compression_ratio'] < worst_ratio:
			worst_ratio = stat['acl_compression_ratio']
			worst_ratio_entry = stat

	print('Most accurate: {}'.format(best_error_entry['filename']))
	print_stat(best_error_entry)

	print('Least accurate: {}'.format(worst_error_entry['filename']))
	print_stat(worst_error_entry)

	print('Best ratio: {}'.format(best_ratio_entry['filename']))
	print_stat(best_ratio_entry)

	print('Worst ratio: {}'.format(worst_ratio_entry['filename']))
	print_stat(worst_ratio_entry)
