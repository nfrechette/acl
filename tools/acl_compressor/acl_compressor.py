import multiprocessing
import numpy
import os
import platform
import queue
import threading
import time
import signal
import sys

from itertools import chain

# This script depends on a SJSON parsing package:
# https://pypi.python.org/pypi/SJSON/1.1.0
# https://shelter13.net/projects/SJSON/
# https://bitbucket.org/Anteru/sjson/src
import sjson


def parse_argv():
	options = {}
	options['acl'] = ""
	options['stats'] = ""
	options['out'] = ""
	options['csv_summary'] = False
	options['csv_bit_rate'] = False
	options['csv_animated_size'] = False
	options['csv_error'] = False
	options['refresh'] = False
	options['num_threads'] = 1
	options['has_progress_bar'] = True
	options['stat_detailed'] = False
	options['stat_exhaustive'] = False
	options['level'] = 'Medium'
	options['print_help'] = False

	for i in range(1, len(sys.argv)):
		value = sys.argv[i]

		# TODO: Strip trailing '/' or '\'
		if value.startswith('-acl='):
			options['acl'] = value[len('-acl='):].replace('"', '')
			options['acl'] = os.path.expanduser(options['acl'])

		if value.startswith('-stats='):
			options['stats'] = value[len('-stats='):].replace('"', '')
			options['stats'] = os.path.expanduser(options['stats'])

		if value.startswith('-out='):
			options['out'] = value[len('-out='):].replace('"', '')
			options['out'] = os.path.expanduser(options['out'])

		if value == '-csv_summary':
			options['csv_summary'] = True

		if value == '-csv_bit_rate':
			options['csv_bit_rate'] = True

		if value == '-csv_animated_size':
			options['csv_animated_size'] = True

		if value == '-csv_error':
			options['csv_error'] = True

		if value == '-refresh':
			options['refresh'] = True

		if value == '-no_progress_bar':
			options['has_progress_bar'] = False

		if value == '-stat_detailed':
			options['stat_detailed'] = True

		if value == '-stat_exhaustive':
			options['stat_exhaustive'] = True

		if value.startswith('-parallel='):
			options['num_threads'] = int(value[len('-parallel='):].replace('"', ''))

		if value.startswith('-level='):
			options['level'] = value[len('-level='):].replace('"', '').capitalize()

		if value == '-help':
			options['print_help'] = True

	if options['print_help']:
		print_help()
		sys.exit(1)

	if len(options['acl']) == 0:
		print('ACL input directory not found')
		print_usage()
		sys.exit(1)

	if len(options['stats']) == 0:
		print('Stat output directory not found')
		print_usage()
		sys.exit(1)

	if options['num_threads'] <= 0:
		print('-parallel switch argument must be greater than 0')
		print_usage()
		sys.exit(1)

	if not os.path.exists(options['acl']) or not os.path.isdir(options['acl']):
		print('ACL input directory not found: {}'.format(options['acl']))
		print_usage()
		sys.exit(1)

	if not os.path.exists(options['stats']):
		os.makedirs(options['stats'])

	if not os.path.isdir(options['stats']):
		print('The output stat argument must be a directory')
		print_usage()
		sys.exit(1)

	return options

def print_usage():
	print('Usage: python acl_compressor.py -acl=<path to directory containing ACL files> -stats=<path to output directory for stats> [-csv_summary] [-csv_bit_rate] [-csv_animated_size] [-csv_error] [-refresh] [-parallel={Num Threads}] [-help]')

def print_help():
	print('Usage: python acl_compressor.py [arguments]')
	print()
	print('Arguments:')
	print('  At least one argument must be provided.')
	print('  -acl=<path>: Input directory tree containing clips to compress.')
	print('  -stats=<path>: Output directory tree for the stats to output.')
	print('  -out=<path>: Output directory tree for the compressed binaries to output.')
	print('  -csv_summary: Generates a basic summary CSV file with various clip information and statistics.')
	print('  -csv_bit_rate: Generates a CSV with the bit rate usage frequency by the variable quantization algorithm. The executable must be compiled with detailed statistics enabled.')
	print('  -csv_animated_size: Generates a CSV with statistics about the animated size of key frames. The executable must be compiled with detailed statistics enabled.')
	print('  -csv_error: Generates a CSV with the error for every bone at every key frame. The executable must be compiled with exhaustive statistics enabled.')
	print('  -refresh: If an output stat file already exists for a particular clip, it is recompressed anyway instead of being skipped.')
	print('  -parallel=<Num Threads>: Allows multiple clips to be compressed and processed in parallel.')
	print('  -no_progress_bar: Suppresses the progress bar output')
	print('  -stat_detailed: Enables detailed stat logging')
	print('  -stat_exhaustive: Enables exhaustive stat logging')
	print('  -help: Prints this help message.')

def print_stat(stat):
	print('Algorithm: {}, Format: [{}], Ratio: {:.2f}, Error: {:.4f}'.format(stat['algorithm_name'], stat['desc'], stat['compression_ratio'], stat['max_error']))
	print('')

def bytes_to_mb(size_in_bytes):
	return size_in_bytes / (1024.0 * 1024.0)

def bytes_to_kb(size_in_bytes):
	return size_in_bytes / 1024.0

def format_elapsed_time(elapsed_time):
	hours, rem = divmod(elapsed_time, 3600)
	minutes, seconds = divmod(rem, 60)
	return '{:0>2}h {:0>2}m {:05.2f}s'.format(int(hours), int(minutes), seconds)

def sanitize_csv_entry(entry):
	return entry.replace(', ', ' ').replace(',', '_')

def create_csv(options):
	csv_data = {}
	stat_dir = options['stats']
	if options['csv_summary']:
		stats_summary_csv_filename = os.path.join(stat_dir, 'stats_summary.csv')
		stats_summary_csv_file = open(stats_summary_csv_filename, 'w')
		csv_data['stats_summary_csv_file'] = stats_summary_csv_file

		print('Generating CSV file {} ...'.format(stats_summary_csv_filename))
		print('Clip Name, Algorithm Name, Raw Size, Compressed Size, Compression Ratio, Compression Time, Clip Duration, Num Animated Tracks, Max Error, Num Transforms, Num Samples Per Track, Quantization Memory Usage', file = stats_summary_csv_file)

	if options['csv_bit_rate']:
		stats_bit_rate_csv_filename = os.path.join(stat_dir, 'stats_bit_rate.csv')
		stats_bit_rate_csv_file = open(stats_bit_rate_csv_filename, 'w')
		csv_data['stats_bit_rate_csv_file'] = stats_bit_rate_csv_file

		print('Generating CSV file {} ...'.format(stats_bit_rate_csv_filename))
		print('Algorithm Name, 0, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 32', file = stats_bit_rate_csv_file)

	if options['csv_animated_size']:
		stats_animated_size_csv_filename = os.path.join(stat_dir, 'stats_animated_size.csv')
		stats_animated_size_csv_file = open(stats_animated_size_csv_filename, 'w')
		csv_data['stats_animated_size_csv_file'] = stats_animated_size_csv_file

		print('Generating CSV file {} ...'.format(stats_animated_size_csv_filename))
		print('Algorithm Name, Segment Index, Animated Size, Num Animated Tracks', file = stats_animated_size_csv_file)

	if options['csv_error']:
		stats_error_csv_filename = os.path.join(stat_dir, 'stats_error.csv')
		stats_error_csv_file = open(stats_error_csv_filename, 'w')
		csv_data['stats_error_csv_file'] = stats_error_csv_file

		print('Generating CSV file {} ...'.format(stats_error_csv_filename))
		print('Clip Name, Key Frame, Bone Index, Error', file = stats_error_csv_file)

	return csv_data

def close_csv(csv_data):
	if len(csv_data) == 0:
		return

	if 'stats_summary_csv_file' in csv_data:
		csv_data['stats_summary_csv_file'].close()

	if 'stats_bit_rate_csv_file' in csv_data:
		csv_data['stats_bit_rate_csv_file'].close()

	if 'stats_animated_size_csv_file' in csv_data:
		csv_data['stats_animated_size_csv_file'].close()

	if 'stats_error_csv_file' in csv_data:
		csv_data['stats_error_csv_file'].close()

def append_csv(csv_data, job_data):
	if 'stats_summary_csv_file' in csv_data:
		data = job_data['stats_summary_data']
		for (clip_name, algo_name, raw_size, compressed_size, compression_ratio, compression_time, duration, num_animated_tracks, max_error, num_transforms, num_samples_per_track, quantization_memory_usage) in data:
			print('{}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}'.format(clip_name, algo_name, raw_size, compressed_size, compression_ratio, compression_time, duration, num_animated_tracks, max_error, num_transforms, num_samples_per_track, quantization_memory_usage), file = csv_data['stats_summary_csv_file'])

	if 'stats_animated_size_csv_file' in csv_data:
		size_data = job_data['stats_animated_size']
		for (name, segment_index, animated_size, num_animated) in size_data:
			print('{}, {}, {}, {}'.format(name, segment_index, animated_size, num_animated), file = csv_data['stats_animated_size_csv_file'])

	if 'stats_error_csv_file' in csv_data:
		error_data = job_data['stats_error_data']
		for (name, segment_index, data) in error_data:
			key_frame = 0
			for frame_errors in data:
				bone_index = 0
				for bone_error in frame_errors:
					print('{}, {}, {}, {}'.format(name, key_frame, bone_index, bone_error), file = csv_data['stats_error_csv_file'])
					bone_index += 1

				key_frame += 1

def write_csv(csv_data, agg_data):
	if 'stats_bit_rate_csv_file' in csv_data:
		for algorithm_uid, algo_data in agg_data.items():
			total_count = float(sum(algo_data['bit_rates']))
			if total_count <= 0.0:
				inv_total_count = 0.0	# Clamp to zero if a bit rate isn't used
			else:
				inv_total_count = 1.0 / total_count
			print('{}, {}'.format(algo_data['csv_name'], ', '.join([str((float(x) * inv_total_count) * 100.0) for x in algo_data['bit_rates']])), file = csv_data['stats_bit_rate_csv_file'])

def print_progress(iteration, total, prefix='', suffix='', decimals = 1, bar_length = 40):
	# Taken from https://stackoverflow.com/questions/3173320/text-progress-bar-in-the-console
	# With minor tweaks
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

	# We need to clear any previous line we might have to ensure we have no visual artifacts
	# Note that if this function is called too quickly, the text might flicker
	terminal_width = 80
	sys.stdout.write('{}\r'.format(' ' * terminal_width))
	sys.stdout.flush()

	sys.stdout.write('%s |%s| %s%s %s\r' % (prefix, bar, percents, '%', suffix)),
	sys.stdout.flush()

	if iteration == total:
		sys.stdout.write('\n')

def run_acl_compressor(cmd_queue, result_queue):
	while True:
		entry = cmd_queue.get()
		if entry is None:
			return

		(acl_filename, cmd) = entry

		result = os.system(cmd)
		if result != 0:
			print('Failed to execute cmd: {}'.format(cmd))
		result_queue.put(acl_filename)

def compress_clips(options):
	acl_dir = options['acl']
	stat_dir = options['stats']
	if platform.system() == 'Windows':
		stat_dir = '\\\\?\\{}'.format(stat_dir)
	refresh = options['refresh']

	if platform.system() == 'Windows':
		compressor_exe_path = '../../build/bin/acl_compressor.exe'
	else:
		compressor_exe_path = '../../build/bin/acl_compressor'

	compressor_exe_path = os.path.abspath(compressor_exe_path)
	if not os.path.exists(compressor_exe_path):
		print('Compressor exe not found: {}'.format(compressor_exe_path))
		sys.exit(1)

	stat_files = []
	cmd_queue = queue.Queue()

	out_dir = None
	if len(options['out']) != 0:
		if not os.path.exists(options['out']):
			os.makedirs(options['out'])

		if os.path.exists(options['out']) and os.path.isdir(options['out']):
			out_dir = options['out']

	for (dirpath, dirnames, filenames) in os.walk(acl_dir):
		stat_dirname = dirpath.replace(acl_dir, stat_dir)

		for filename in filenames:
			if not (filename.endswith('.acl.sjson') or filename.endswith('.acl')):
				continue

			acl_filename = os.path.join(dirpath, filename)
			if filename.endswith('.acl.sjson'):
				stat_filename = os.path.join(stat_dirname, filename.replace('.acl.sjson', '_stats.sjson'))
			else:
				stat_filename = os.path.join(stat_dirname, filename.replace('.acl', '_stats.sjson'))

			stat_files.append(stat_filename)

			if os.path.exists(stat_filename) and os.path.isfile(stat_filename) and not refresh:
				continue

			if not os.path.exists(stat_dirname):
				os.makedirs(stat_dirname)

			stat_filename = stat_filename.replace('\\\\?\\', '')

			cmd = '{} -acl="{}" -stats="{}" -level={}'.format(compressor_exe_path, acl_filename, stat_filename, options['level'])

			if out_dir:
				if filename.endswith('.acl.sjson'):
					out_filename = os.path.join(options['out'], filename.replace('.acl.sjson', '.acl'))
				else:
					out_filename = os.path.join(options['out'], filename)
				cmd = '{} -out="{}"'.format(cmd, out_filename)

			if options['stat_detailed']:
				cmd = '{} -stat_detailed'.format(cmd)

			if options['stat_exhaustive']:
				cmd = '{} -stat_exhaustive'.format(cmd)

			if platform.system() == 'Windows':
				cmd = cmd.replace('/', '\\')

			cmd_queue.put((acl_filename, cmd))

	if len(stat_files) == 0:
		print("No ACL clips found to compress")
		sys.exit(0)

	if not cmd_queue.empty():
		# Add a marker to terminate the threads
		for i in range(options['num_threads']):
			cmd_queue.put(None)

		result_queue = queue.Queue()
		compression_start_time = time.perf_counter()

		threads = [ threading.Thread(target = run_acl_compressor, args = (cmd_queue, result_queue)) for _i in range(options['num_threads']) ]
		for thread in threads:
			thread.daemon = True
			thread.start()

		if options['has_progress_bar']:
			print_progress(0, len(stat_files), 'Compressing clips:', '{} / {}'.format(0, len(stat_files)))
		try:
			while True:
				for thread in threads:
					thread.join(1.0)

				num_processed = result_queue.qsize()
				if options['has_progress_bar']:
					print_progress(num_processed, len(stat_files), 'Compressing clips:', '{} / {}'.format(num_processed, len(stat_files)))

				all_threads_done = True
				for thread in threads:
					if thread.is_alive():
						all_threads_done = False

				if all_threads_done:
					break
		except KeyboardInterrupt:
			sys.exit(1)

		compression_end_time = time.perf_counter()
		print()
		print('Compressed {} clips in {}'.format(len(stat_files), format_elapsed_time(compression_end_time - compression_start_time)))

	return stat_files

def shorten_rotation_format(format):
	if format == 'quatf_full':
		return 'R:Quat'
	elif format == 'quatf_drop_w_full':
		return 'R:QuatNoW96'
	elif format == 'QuatDropW_48':
		return 'R:QuatNoW48'
	elif format == 'QuatDropW_32':
		return 'R:QuatNoW32'
	elif format == 'quatf_drop_w_variable':
		return 'R:QuatNoWVar'
	else:
		return 'R:???'

def shorten_translation_format(format):
	if format == 'vector3f_full':
		return 'T:Vec3_96'
	elif format == 'Vector3_48':
		return 'T:Vec3_48'
	elif format == 'Vector3_32':
		return 'T:Vec3_32'
	elif format == 'vector3f_variable':
		return 'T:Vec3Var'
	else:
		return 'T:???'

def shorten_scale_format(format):
	if format == 'vector3f_full':
		return 'S:Vec3_96'
	elif format == 'Vector3_48':
		return 'S:Vec3_48'
	elif format == 'Vector3_32':
		return 'S:Vec3_32'
	elif format == 'vector3f_variable':
		return 'S:Vec3Var'
	else:
		return 'S:???'

def aggregate_stats(agg_run_stats, run_stats):
	algorithm_uid = run_stats['algorithm_uid']
	if not algorithm_uid in agg_run_stats:
		agg_data = {}
		agg_data['name'] = run_stats['desc']
		agg_data['csv_name'] = run_stats['csv_desc']
		agg_data['total_raw_size'] = 0
		agg_data['total_compressed_size'] = 0
		agg_data['total_compression_time'] = 0.0
		agg_data['total_duration'] = 0.0
		agg_data['max_error'] = 0
		agg_data['num_runs'] = 0
		agg_data['bit_rates'] = [0] * 19
		agg_data['compressed_size'] = []

		# Detailed stats
		agg_data['num_segments'] = []
		agg_data['num_default_rotation_tracks'] = []
		agg_data['num_default_translation_tracks'] = []
		agg_data['num_default_scale_tracks'] = []
		agg_data['num_constant_rotation_tracks'] = []
		agg_data['num_constant_translation_tracks'] = []
		agg_data['num_constant_scale_tracks'] = []
		agg_data['num_animated_rotation_tracks'] = []
		agg_data['num_animated_translation_tracks'] = []
		agg_data['num_animated_scale_tracks'] = []
		agg_data['num_default_tracks'] = []
		agg_data['num_constant_tracks'] = []
		agg_data['num_animated_tracks'] = []
		agg_data['clip_header_size'] = []
		agg_data['clip_metadata_common_size'] = []
		agg_data['clip_metadata_rotation_constant_size'] = []
		agg_data['clip_metadata_translation_constant_size'] = []
		agg_data['clip_metadata_scale_constant_size'] = []
		agg_data['clip_metadata_rotation_animated_size'] = []
		agg_data['clip_metadata_translation_animated_size'] = []
		agg_data['clip_metadata_scale_animated_size'] = []
		agg_data['segment_metadata_common_size'] = []
		agg_data['segment_metadata_rotation_size'] = []
		agg_data['segment_metadata_translation_size'] = []
		agg_data['segment_metadata_scale_size'] = []
		agg_data['segment_animated_rotation_size'] = []
		agg_data['segment_animated_translation_size'] = []
		agg_data['segment_animated_scale_size'] = []
		agg_data['unknown_overhead_size'] = []

		agg_run_stats[algorithm_uid] = agg_data

	agg_data = agg_run_stats[algorithm_uid]
	agg_data['total_raw_size'] += run_stats['raw_size']
	agg_data['total_compressed_size'] += run_stats['compressed_size']
	agg_data['total_compression_time'] += run_stats['compression_time']
	agg_data['total_duration'] += run_stats['duration']
	agg_data['max_error'] = max(agg_data['max_error'], run_stats['max_error'])
	agg_data['num_runs'] += 1
	agg_data['compressed_size'].append(run_stats['compressed_size'])
	if 'segments' in run_stats and len(run_stats['segments']) > 0:
		for segment in run_stats['segments']:
			if 'bit_rate_counts' in segment:
				for i in range(19):
					agg_data['bit_rates'][i] += segment['bit_rate_counts'][i]

	# Detailed stats
	if 'num_default_rotation_tracks' in run_stats:
		agg_data['num_segments'].append(run_stats['segmenting']['num_segments'])
		agg_data['num_default_rotation_tracks'].append(run_stats['num_default_rotation_tracks'])
		agg_data['num_default_translation_tracks'].append(run_stats['num_default_translation_tracks'])
		agg_data['num_default_scale_tracks'].append(run_stats['num_default_scale_tracks'])
		agg_data['num_constant_rotation_tracks'].append(run_stats['num_constant_rotation_tracks'])
		agg_data['num_constant_translation_tracks'].append(run_stats['num_constant_translation_tracks'])
		agg_data['num_constant_scale_tracks'].append(run_stats['num_constant_scale_tracks'])
		agg_data['num_animated_rotation_tracks'].append(run_stats['num_animated_rotation_tracks'])
		agg_data['num_animated_translation_tracks'].append(run_stats['num_animated_translation_tracks'])
		agg_data['num_animated_scale_tracks'].append(run_stats['num_animated_scale_tracks'])
		agg_data['num_default_tracks'].append(run_stats['num_default_tracks'])
		agg_data['num_constant_tracks'].append(run_stats['num_constant_tracks'])
		agg_data['num_animated_tracks'].append(run_stats['num_animated_tracks'])
		agg_data['clip_header_size'].append(run_stats['clip_header_size'])
		agg_data['clip_metadata_common_size'].append(run_stats['clip_metadata_common_size'])
		agg_data['clip_metadata_rotation_constant_size'].append(run_stats['clip_metadata_rotation_constant_size'])
		agg_data['clip_metadata_translation_constant_size'].append(run_stats['clip_metadata_translation_constant_size'])
		agg_data['clip_metadata_scale_constant_size'].append(run_stats['clip_metadata_scale_constant_size'])
		agg_data['clip_metadata_rotation_animated_size'].append(run_stats['clip_metadata_rotation_animated_size'])
		agg_data['clip_metadata_translation_animated_size'].append(run_stats['clip_metadata_translation_animated_size'])
		agg_data['clip_metadata_scale_animated_size'].append(run_stats['clip_metadata_scale_animated_size'])
		agg_data['segment_metadata_common_size'].append(run_stats['segment_metadata_common_size'])
		agg_data['segment_metadata_rotation_size'].append(run_stats['segment_metadata_rotation_size'])
		agg_data['segment_metadata_translation_size'].append(run_stats['segment_metadata_translation_size'])
		agg_data['segment_metadata_scale_size'].append(run_stats['segment_metadata_scale_size'])
		agg_data['segment_animated_rotation_size'].append(run_stats['segment_animated_rotation_size'])
		agg_data['segment_animated_translation_size'].append(run_stats['segment_animated_translation_size'])
		agg_data['segment_animated_scale_size'].append(run_stats['segment_animated_scale_size'])
		agg_data['unknown_overhead_size'].append(run_stats['unknown_overhead_size'])

def track_best_runs(best_runs, run_stats):
	if run_stats['max_error'] < best_runs['best_error']:
		best_runs['best_error'] = run_stats['max_error']
		best_runs['best_error_entry'] = run_stats

	if run_stats['compression_ratio'] > best_runs['best_ratio']:
		best_runs['best_ratio'] = run_stats['compression_ratio']
		best_runs['best_ratio_entry'] = run_stats

def track_worst_runs(worst_runs, run_stats):
	if run_stats['max_error'] > worst_runs['worst_error']:
		worst_runs['worst_error'] = run_stats['max_error']
		worst_runs['worst_error_entry'] = run_stats

	if run_stats['compression_ratio'] < worst_runs['worst_ratio']:
		worst_runs['worst_ratio'] = run_stats['compression_ratio']
		worst_runs['worst_ratio_entry'] = run_stats

def run_stat_parsing(options, stat_queue, result_queue):
	#signal.signal(signal.SIGINT, signal.SIG_IGN)

	try:
		agg_run_stats = {}
		best_runs = {}
		best_runs['best_error'] = 100000000.0
		best_runs['best_error_entry'] = None
		best_runs['best_ratio'] = 0.0
		best_runs['best_ratio_entry'] = None
		worst_runs = {}
		worst_runs['worst_error'] = -100000000.0
		worst_runs['worst_error_entry'] = None
		worst_runs['worst_ratio'] = 100000000.0
		worst_runs['worst_ratio_entry'] = None
		num_runs = 0
		total_compression_time = 0.0
		stats_summary_data = []
		stats_error_data = []
		stats_animated_size = []
		bone_error_values = []
		compression_times = []

		while True:
			stat_filename = stat_queue.get()
			if stat_filename is None:
				break

			with open(stat_filename, 'r') as file:
				try:
					file_data = sjson.loads(file.read())
					runs = file_data['runs']

					for run_stats in runs:
						if len(run_stats) == 0:
							continue

						run_stats['filename'] = stat_filename.replace('\\\\?\\', '')
						run_stats['clip_name'] = os.path.splitext(os.path.basename(stat_filename))[0]
						run_stats['rotation_format'] = shorten_rotation_format(run_stats['rotation_format'])
						run_stats['translation_format'] = shorten_translation_format(run_stats['translation_format'])
						run_stats['scale_format'] = shorten_scale_format(run_stats['scale_format'])

						if isinstance(run_stats['duration'], str):
							run_stats['duration'] = 0.0

						if 'segmenting' in run_stats:
							run_stats['desc'] = '{}|{}|{}'.format(run_stats['rotation_format'], run_stats['translation_format'], run_stats['scale_format'])
							run_stats['csv_desc'] = '{}|{}|{}'.format(run_stats['rotation_format'], run_stats['translation_format'], run_stats['scale_format'])
						else:
							run_stats['desc'] = '{}|{}|{}'.format(run_stats['rotation_format'], run_stats['translation_format'], run_stats['scale_format'])
							run_stats['csv_desc'] = '{}|{}|{}'.format(run_stats['rotation_format'], run_stats['translation_format'], run_stats['scale_format'])

						aggregate_stats(agg_run_stats, run_stats)
						track_best_runs(best_runs, run_stats)
						track_worst_runs(worst_runs, run_stats)

						num_runs += 1
						total_compression_time += run_stats['compression_time']
						compression_times.append(run_stats['compression_time'])

						if options['csv_summary']:
							#(name, raw_size, compressed_size, compression_ratio, compression_time, duration, num_animated_tracks, max_error, num_transforms, num_samples_per_track, quantization_memory_usage)
							num_transforms = run_stats['num_bones']
							num_samples_per_track = run_stats['num_samples']
							num_animated_tracks = run_stats.get('num_animated_tracks', 0)
							quantization_memory_usage = run_stats.get('track_bit_rate_database_size', 0) + run_stats.get('transform_cache_size', 0)
							data = (run_stats['clip_name'], run_stats['csv_desc'], run_stats['raw_size'], run_stats['compressed_size'], run_stats['compression_ratio'], run_stats['compression_time'], run_stats['duration'], num_animated_tracks, run_stats['max_error'], num_transforms, num_samples_per_track, quantization_memory_usage)
							stats_summary_data.append(data)

						if 'segments' in run_stats and len(run_stats['segments']) > 0:
							segment_index = 0
							for segment in run_stats['segments']:
								if 'animated_frame_size' in segment and options['csv_animated_size']:
									stats_animated_size.append((run_stats['clip_name'], segment_index, segment['animated_frame_size'], run_stats['num_animated_tracks']))

								if 'error_per_frame_and_bone' in segment and len(segment['error_per_frame_and_bone']) > 0:
									# Convert to array, lower memory footprint and more efficient
									if options['csv_error']:
										#(name, segment_index, data)
										data = (run_stats['clip_name'], segment_index, segment['error_per_frame_and_bone'])
										stats_error_data.append(data)

									for frame_error_values in segment['error_per_frame_and_bone']:
										bone_error_values.extend([float(v) for v in frame_error_values])

									# Data isn't needed anymore, discard it
									segment['error_per_frame_and_bone'] = []

								segment_index += 1

					result_queue.put(('progress', stat_filename))
				except sjson.ParseException:
					print('Failed to parse SJSON file: {}'.format(stat_filename))

		# Done
		results = {}
		results['agg_run_stats'] = agg_run_stats
		results['best_runs'] = best_runs
		results['worst_runs'] = worst_runs
		results['num_runs'] = num_runs
		results['total_compression_time'] = total_compression_time
		results['stats_summary_data'] = stats_summary_data
		results['stats_error_data'] = stats_error_data
		results['stats_animated_size'] = stats_animated_size
		results['bone_error_values'] = bone_error_values
		results['compression_times'] = compression_times

		result_queue.put(('done', results))
	except KeyboardInterrupt:
		print('Interrupted')

def pretty_print(d, indent = 0):
	for key, value in d.items():
		if isinstance(value, dict):
			print('\t' * indent + str(key))
			pretty(value, indent + 1)
		else:
			print('\t' * indent + str(key) + ': ' + str(value))

def aggregate_job_stats(agg_job_results, job_results):
	if job_results['num_runs'] == 0:
		return

	if len(agg_job_results) == 0:
		# Convert array to numpy array
		job_results['bone_error_values'] = numpy.array(job_results['bone_error_values'])
		job_results['compression_times'] = numpy.array(job_results['compression_times'])

		agg_job_results.update(job_results)
	else:
		agg_job_results['num_runs'] += job_results['num_runs']
		agg_job_results['total_compression_time'] += job_results['total_compression_time']
		for key in job_results['agg_run_stats'].keys():
			if not key in agg_job_results['agg_run_stats']:
				agg_job_results['agg_run_stats'][key] = job_results['agg_run_stats'][key].copy()
			else:
				agg_job_results['agg_run_stats'][key]['total_raw_size'] += job_results['agg_run_stats'][key]['total_raw_size']
				agg_job_results['agg_run_stats'][key]['total_compressed_size'] += job_results['agg_run_stats'][key]['total_compressed_size']
				agg_job_results['agg_run_stats'][key]['total_compression_time'] += job_results['agg_run_stats'][key]['total_compression_time']
				agg_job_results['agg_run_stats'][key]['total_duration'] += job_results['agg_run_stats'][key]['total_duration']
				agg_job_results['agg_run_stats'][key]['max_error'] = max(agg_job_results['agg_run_stats'][key]['max_error'], job_results['agg_run_stats'][key]['max_error'])
				agg_job_results['agg_run_stats'][key]['num_runs'] += job_results['agg_run_stats'][key]['num_runs']
				agg_job_results['agg_run_stats'][key]['compressed_size'] += job_results['agg_run_stats'][key]['compressed_size']
				for i in range(19):
					agg_job_results['agg_run_stats'][key]['bit_rates'][i] += job_results['agg_run_stats'][key]['bit_rates'][i]

				# Detailed stats
				if 'num_default_rotation_tracks' in job_results['agg_run_stats'][key]:
					agg_job_results['agg_run_stats'][key]['num_segments']								+= job_results['agg_run_stats'][key]['num_segments']
					agg_job_results['agg_run_stats'][key]['num_default_rotation_tracks']				+= job_results['agg_run_stats'][key]['num_default_rotation_tracks']
					agg_job_results['agg_run_stats'][key]['num_default_translation_tracks']				+= job_results['agg_run_stats'][key]['num_default_translation_tracks']
					agg_job_results['agg_run_stats'][key]['num_default_scale_tracks']					+= job_results['agg_run_stats'][key]['num_default_scale_tracks']
					agg_job_results['agg_run_stats'][key]['num_constant_rotation_tracks']				+= job_results['agg_run_stats'][key]['num_constant_rotation_tracks']
					agg_job_results['agg_run_stats'][key]['num_constant_translation_tracks']			+= job_results['agg_run_stats'][key]['num_constant_translation_tracks']
					agg_job_results['agg_run_stats'][key]['num_constant_scale_tracks']					+= job_results['agg_run_stats'][key]['num_constant_scale_tracks']
					agg_job_results['agg_run_stats'][key]['num_animated_rotation_tracks']				+= job_results['agg_run_stats'][key]['num_animated_rotation_tracks']
					agg_job_results['agg_run_stats'][key]['num_animated_translation_tracks']			+= job_results['agg_run_stats'][key]['num_animated_translation_tracks']
					agg_job_results['agg_run_stats'][key]['num_animated_scale_tracks']					+= job_results['agg_run_stats'][key]['num_animated_scale_tracks']
					agg_job_results['agg_run_stats'][key]['num_default_tracks']							+= job_results['agg_run_stats'][key]['num_default_tracks']
					agg_job_results['agg_run_stats'][key]['num_constant_tracks']						+= job_results['agg_run_stats'][key]['num_constant_tracks']
					agg_job_results['agg_run_stats'][key]['num_animated_tracks']						+= job_results['agg_run_stats'][key]['num_animated_tracks']
					agg_job_results['agg_run_stats'][key]['clip_header_size']							+= job_results['agg_run_stats'][key]['clip_header_size']
					agg_job_results['agg_run_stats'][key]['clip_metadata_common_size']					+= job_results['agg_run_stats'][key]['clip_metadata_common_size']
					agg_job_results['agg_run_stats'][key]['clip_metadata_rotation_constant_size']		+= job_results['agg_run_stats'][key]['clip_metadata_rotation_constant_size']
					agg_job_results['agg_run_stats'][key]['clip_metadata_translation_constant_size']	+= job_results['agg_run_stats'][key]['clip_metadata_translation_constant_size']
					agg_job_results['agg_run_stats'][key]['clip_metadata_scale_constant_size']			+= job_results['agg_run_stats'][key]['clip_metadata_scale_constant_size']
					agg_job_results['agg_run_stats'][key]['clip_metadata_rotation_animated_size']		+= job_results['agg_run_stats'][key]['clip_metadata_rotation_animated_size']
					agg_job_results['agg_run_stats'][key]['clip_metadata_translation_animated_size']	+= job_results['agg_run_stats'][key]['clip_metadata_translation_animated_size']
					agg_job_results['agg_run_stats'][key]['clip_metadata_scale_animated_size']			+= job_results['agg_run_stats'][key]['clip_metadata_scale_animated_size']
					agg_job_results['agg_run_stats'][key]['segment_metadata_common_size']				+= job_results['agg_run_stats'][key]['segment_metadata_common_size']
					agg_job_results['agg_run_stats'][key]['segment_metadata_rotation_size']				+= job_results['agg_run_stats'][key]['segment_metadata_rotation_size']
					agg_job_results['agg_run_stats'][key]['segment_metadata_translation_size']			+= job_results['agg_run_stats'][key]['segment_metadata_translation_size']
					agg_job_results['agg_run_stats'][key]['segment_metadata_scale_size']				+= job_results['agg_run_stats'][key]['segment_metadata_scale_size']
					agg_job_results['agg_run_stats'][key]['segment_animated_rotation_size']				+= job_results['agg_run_stats'][key]['segment_animated_rotation_size']
					agg_job_results['agg_run_stats'][key]['segment_animated_translation_size']			+= job_results['agg_run_stats'][key]['segment_animated_translation_size']
					agg_job_results['agg_run_stats'][key]['segment_animated_scale_size']				+= job_results['agg_run_stats'][key]['segment_animated_scale_size']
					agg_job_results['agg_run_stats'][key]['unknown_overhead_size']						+= job_results['agg_run_stats'][key]['unknown_overhead_size']

		if job_results['best_runs']['best_error'] < agg_job_results['best_runs']['best_error']:
			agg_job_results['best_runs']['best_error'] = job_results['best_runs']['best_error']
			agg_job_results['best_runs']['best_error_entry'] = job_results['best_runs']['best_error_entry']

		if job_results['best_runs']['best_ratio'] > agg_job_results['best_runs']['best_ratio']:
			agg_job_results['best_runs']['best_ratio'] = job_results['best_runs']['best_ratio']
			agg_job_results['best_runs']['best_ratio_entry'] = job_results['best_runs']['best_ratio_entry']

		if job_results['worst_runs']['worst_error'] > agg_job_results['worst_runs']['worst_error']:
			agg_job_results['worst_runs']['worst_error'] = job_results['worst_runs']['worst_error']
			agg_job_results['worst_runs']['worst_error_entry'] = job_results['worst_runs']['worst_error_entry']

		if job_results['worst_runs']['worst_ratio'] < agg_job_results['worst_runs']['worst_ratio']:
			agg_job_results['worst_runs']['worst_ratio'] = job_results['worst_runs']['worst_ratio']
			agg_job_results['worst_runs']['worst_ratio_entry'] = job_results['worst_runs']['worst_ratio_entry']

		agg_job_results['bone_error_values'] = numpy.append(agg_job_results['bone_error_values'], job_results['bone_error_values'])
		agg_job_results['compression_times'] = numpy.append(agg_job_results['compression_times'], job_results['compression_times'])

def percentile_rank(values, value):
	return (values < value).mean() * 100.0

def aggregate_and_print_track_results(agg_run_stats, key):
	if key:
		label = '{} '.format(key)
		key = '{}_'.format(key)
	else:
		label = ''
		key = ''

	total_key = 'total_{}tracks'.format(key)
	default_ratios_key = 'default_{}tracks_ratios'.format(key)
	constant_ratios_key = 'constant_{}tracks_ratios'.format(key)
	animated_ratios_key = 'animated_{}tracks_ratios'.format(key)
	num_default_key = 'num_default_{}tracks'.format(key)
	num_constant_key = 'num_constant_{}tracks'.format(key)
	num_animated_key = 'num_animated_{}tracks'.format(key)

	for value in agg_run_stats.values():
		value[total_key] = [x + y + z for x, y, z in zip(value[num_default_key], value[num_constant_key], value[num_animated_key])]
		value[default_ratios_key] = [(x / y) * 100.0 for x, y in zip(value[num_default_key], value[total_key])]
		value[constant_ratios_key] = [(x / y) * 100.0 for x, y in zip(value[num_constant_key], value[total_key])]
		value[animated_ratios_key] = [(x / y) * 100.0 for x, y in zip(value[num_animated_key], value[total_key])]

		total_tracks = sum([sum(x[total_key]) for x in agg_run_stats.values()])
		total_default_tracks = sum([sum(x[num_default_key]) for x in agg_run_stats.values()])
		total_constant_tracks = sum([sum(x[num_constant_key]) for x in agg_run_stats.values()])
		total_animated_tracks = sum([sum(x[num_animated_key]) for x in agg_run_stats.values()])

		tmp = list(chain.from_iterable([x[default_ratios_key] for x in agg_run_stats.values()]))
		total_default_tracks_p50 = numpy.percentile(tmp, 50.0)
		total_default_tracks_p85 = numpy.percentile(tmp, 85.0)
		total_default_tracks_p99 = numpy.percentile(tmp, 99.0)
		tmp = list(chain.from_iterable([x[constant_ratios_key] for x in agg_run_stats.values()]))
		total_constant_tracks_p50 = numpy.percentile(tmp, 50.0)
		total_constant_tracks_p85 = numpy.percentile(tmp, 85.0)
		total_constant_tracks_p99 = numpy.percentile(tmp, 99.0)
		tmp = list(chain.from_iterable([x[animated_ratios_key] for x in agg_run_stats.values()]))
		total_animated_tracks_p50 = numpy.percentile(tmp, 50.0)
		total_animated_tracks_p85 = numpy.percentile(tmp, 85.0)
		total_animated_tracks_p99 = numpy.percentile(tmp, 99.0)

		print('Total {}tracks: {}'.format(label, total_tracks))
		print('Total default {}tracks: {} ({:.2f} %)'.format(label, total_default_tracks, (total_default_tracks / total_tracks) * 100.0))
		print('    50, 85, 99th percentile: {:.2f} %, {:.2f} %, {:.2f} %'.format(total_default_tracks_p50, total_default_tracks_p85, total_default_tracks_p99))
		print('Total constant {}tracks: {} ({:.2f} %)'.format(label, total_constant_tracks, (total_constant_tracks / total_tracks) * 100.0))
		print('    50, 85, 99th percentile: {:.2f} %, {:.2f} %, {:.2f} %'.format(total_constant_tracks_p50, total_constant_tracks_p85, total_constant_tracks_p99))
		print('Total animated {}tracks: {} ({:.2f} %)'.format(label, total_animated_tracks, (total_animated_tracks / total_tracks) * 100.0))
		print('    50, 85, 99th percentile: {:.2f} %, {:.2f} %, {:.2f} %'.format(total_animated_tracks_p50, total_animated_tracks_p85, total_animated_tracks_p99))

def aggregate_and_print_clip_metadata_results(agg_run_stats):
	for value in agg_run_stats.values():
		value['clip_metadata_total_constant_size'] = [x + y + z for x, y, z in zip(value['clip_metadata_rotation_constant_size'], value['clip_metadata_translation_constant_size'], value['clip_metadata_scale_constant_size'])]
		value['clip_metadata_total_animated_size'] = [x + y + z for x, y, z in zip(value['clip_metadata_rotation_animated_size'], value['clip_metadata_translation_animated_size'], value['clip_metadata_scale_animated_size'])]
		value['clip_metadata_total_size'] = [x + y + z + w for x, y, z, w in zip(value['clip_header_size'], value['clip_metadata_common_size'], value['clip_metadata_total_constant_size'], value['clip_metadata_total_animated_size'])]

		clip_metadata_total_size = sum([sum(x['clip_metadata_total_size']) for x in agg_run_stats.values()])
		total_compressed_size = sum([sum(x['compressed_size']) for x in agg_run_stats.values()])

		value['clip_metadata_ratios'] = [(x / y) * 100.0 for x, y in zip(value['clip_metadata_total_size'], value['compressed_size'])]

		tmp = list(chain.from_iterable([x['clip_metadata_ratios'] for x in agg_run_stats.values()]))
		clip_metadata_ratio_p50 = numpy.percentile(tmp, 50.0)
		clip_metadata_ratio_p85 = numpy.percentile(tmp, 85.0)
		clip_metadata_ratio_p99 = numpy.percentile(tmp, 99.0)

		print('Total clip metadata size: {:.2f} MB ({:.2f} %)'.format(bytes_to_mb(clip_metadata_total_size), (clip_metadata_total_size / total_compressed_size) * 100.0))
		print('    50, 85, 99th percentile: {:.2f} %, {:.2f} %, {:.2f} %'.format(clip_metadata_ratio_p50, clip_metadata_ratio_p85, clip_metadata_ratio_p99))

def aggregate_and_print_segment_metadata_results(agg_run_stats):
	for value in agg_run_stats.values():
		value['segment_metadata_total_size'] = [x + y + z + w for x, y, z, w in zip(value['segment_metadata_common_size'], value['segment_metadata_rotation_size'], value['segment_metadata_translation_size'], value['segment_metadata_scale_size'])]

		segment_metadata_total_size = sum([sum(x['segment_metadata_total_size']) for x in agg_run_stats.values()])
		total_compressed_size = sum([sum(x['compressed_size']) for x in agg_run_stats.values()])

		value['segment_metadata_ratios'] = [(x / y) * 100.0 for x, y in zip(value['segment_metadata_total_size'], value['compressed_size'])]

		tmp = list(chain.from_iterable([x['segment_metadata_ratios'] for x in agg_run_stats.values()]))
		segment_metadata_ratio_p50 = numpy.percentile(tmp, 50.0)
		segment_metadata_ratio_p85 = numpy.percentile(tmp, 85.0)
		segment_metadata_ratio_p99 = numpy.percentile(tmp, 99.0)

		print('Total segment metadata size: {:.2f} MB ({:.2f} %)'.format(bytes_to_mb(segment_metadata_total_size), (segment_metadata_total_size / total_compressed_size) * 100.0))
		print('    50, 85, 99th percentile: {:.2f} %, {:.2f} %, {:.2f} %'.format(segment_metadata_ratio_p50, segment_metadata_ratio_p85, segment_metadata_ratio_p99))

def aggregate_and_print_segment_animated_results(agg_run_stats):
	for value in agg_run_stats.values():
		value['segment_animated_total_size'] = [x + y + z for x, y, z in zip(value['segment_animated_rotation_size'], value['segment_animated_translation_size'], value['segment_animated_scale_size'])]

		segment_animated_total_size = sum([sum(x['segment_animated_total_size']) for x in agg_run_stats.values()])
		total_compressed_size = sum([sum(x['compressed_size']) for x in agg_run_stats.values()])

		value['segment_animated_ratios'] = [(x / y) * 100.0 for x, y in zip(value['segment_animated_total_size'], value['compressed_size'])]

		tmp = list(chain.from_iterable([x['segment_animated_ratios'] for x in agg_run_stats.values()]))
		segment_animated_ratio_p50 = numpy.percentile(tmp, 50.0)
		segment_animated_ratio_p85 = numpy.percentile(tmp, 85.0)
		segment_animated_ratio_p99 = numpy.percentile(tmp, 99.0)

		print('Total segment animated size: {:.2f} MB ({:.2f} %)'.format(bytes_to_mb(segment_animated_total_size), (segment_animated_total_size / total_compressed_size) * 100.0))
		print('    50, 85, 99th percentile: {:.2f} %, {:.2f} %, {:.2f} %'.format(segment_animated_ratio_p50, segment_animated_ratio_p85, segment_animated_ratio_p99))

def aggregate_and_print_num_segment_results(agg_run_stats):
	for value in agg_run_stats.values():
		value['total_num_segments'] = [x + y + z for x, y, z in zip(value['segment_animated_rotation_size'], value['segment_animated_translation_size'], value['segment_animated_scale_size'])]

		total_num_segments = sum([sum(x['num_segments']) for x in agg_run_stats.values()])

		value['segment_animated_ratios'] = [(x / y) * 100.0 for x, y in zip(value['segment_animated_total_size'], value['compressed_size'])]

		tmp = list(chain.from_iterable([x['num_segments'] for x in agg_run_stats.values()]))
		num_segments_p50 = numpy.percentile(tmp, 50.0)
		num_segments_p85 = numpy.percentile(tmp, 85.0)
		num_segments_p99 = numpy.percentile(tmp, 99.0)

		print('Total num segments: {}'.format(total_num_segments))
		print('    50, 85, 99th percentile: {:.2f}, {:.2f}, {:.2f}'.format(num_segments_p50, num_segments_p85, num_segments_p99))

if __name__ == "__main__":
	if sys.version_info < (3, 4):
		print('Python 3.4 or higher needed to run this script')
		sys.exit(1)

	options = parse_argv()

	stat_files = compress_clips(options)

	csv_data = create_csv(options)

	aggregating_start_time = time.perf_counter()

	stat_queue = multiprocessing.Queue()
	for stat_filename in stat_files:
		stat_queue.put(stat_filename)

	# Add a marker to terminate the jobs
	for i in range(options['num_threads']):
		stat_queue.put(None)

	result_queue = multiprocessing.Queue()

	jobs = [ multiprocessing.Process(target = run_stat_parsing, args = (options, stat_queue, result_queue)) for _i in range(options['num_threads']) ]
	for job in jobs:
		job.start()

	agg_job_results = {}
	num_stat_file_processed = 0
	if options['has_progress_bar']:
		print_progress(num_stat_file_processed, len(stat_files), 'Aggregating results:', '{} / {}'.format(num_stat_file_processed, len(stat_files)))
	try:
		while True:
			try:
				(msg, data) = result_queue.get(True, 1.0)
				if msg == 'progress':
					num_stat_file_processed += 1
					if options['has_progress_bar']:
						print_progress(num_stat_file_processed, len(stat_files), 'Aggregating results:', '{} / {}'.format(num_stat_file_processed, len(stat_files)))
				elif msg == 'done':
					aggregate_job_stats(agg_job_results, data)
					append_csv(csv_data, data)
			except queue.Empty:
				all_jobs_done = True
				for job in jobs:
					if job.is_alive():
						all_jobs_done = False

				if all_jobs_done:
					break
	except KeyboardInterrupt:
		sys.exit(1)

	agg_run_stats = agg_job_results['agg_run_stats']
	best_runs = agg_job_results['best_runs']
	worst_runs = agg_job_results['worst_runs']
	num_runs = agg_job_results['num_runs']

	write_csv(csv_data, agg_run_stats)

	aggregating_end_time = time.perf_counter()
	print()
	print('Found {} runs in {}'.format(num_runs, format_elapsed_time(aggregating_end_time - aggregating_start_time)))
	print()

	close_csv(csv_data)

	print('Stats per run type:')
	run_types_by_size = sorted(agg_run_stats.values(), key = lambda entry: entry['total_compressed_size'])
	for run_stats in run_types_by_size:
		ratio = float(run_stats['total_raw_size']) / float(run_stats['total_compressed_size'])
		print('Compressed {:.2f} MB, Elapsed {}, Ratio [{:.2f} : 1], Max error [{:.4f}] Run type: {}'.format(bytes_to_mb(run_stats['total_compressed_size']), format_elapsed_time(run_stats['total_compression_time']), ratio, run_stats['max_error'], run_stats['name']))
	print()
	print('Total:')
	total_raw_size = sum([x['total_raw_size'] for x in agg_run_stats.values()])
	total_compressed_size = sum([x['total_compressed_size'] for x in agg_run_stats.values()])
	total_compression_time = sum([x['total_compression_time'] for x in agg_run_stats.values()])
	total_max_error = max([x['max_error'] for x in agg_run_stats.values()])
	total_ratio = float(total_raw_size) / float(total_compressed_size)
	tmp = list(chain.from_iterable([x['compressed_size'] for x in agg_run_stats.values()]))
	compressed_size_p50 = bytes_to_kb(numpy.percentile(tmp, 50.0))
	compressed_size_p85 = bytes_to_kb(numpy.percentile(tmp, 85.0))
	compressed_size_p99 = bytes_to_kb(numpy.percentile(tmp, 99.0))
	print('Compressed {:.2f} MB, Elapsed {}, Ratio [{:.2f} : 1], Max error [{:.4f}]'.format(bytes_to_mb(total_compressed_size), format_elapsed_time(total_compression_time), total_ratio, total_max_error))
	print('    50, 85, 99th percentile: {:.2f} KB, {:.2f} KB, {:.2f} KB'.format(compressed_size_p50, compressed_size_p85, compressed_size_p99))
	print()

	total_duration = sum([x['total_duration'] for x in agg_run_stats.values()])

	print('Sum of clip durations: {}'.format(format_elapsed_time(total_duration)))
	print('Total compression time: {} ({:.3f} seconds)'.format(format_elapsed_time(total_compression_time), total_compression_time))
	print('Total raw size: {:.2f} MB'.format(bytes_to_mb(total_raw_size)))
	print('Compression speed: {:.2f} KB/sec'.format(bytes_to_kb(total_raw_size) / total_compression_time))
	print('Compression time 50, 85, 99th percentile: {:.3f}, {:.3f}, {:.3f} seconds'.format(numpy.percentile(agg_job_results['compression_times'], 50.0), numpy.percentile(agg_job_results['compression_times'], 85.0), numpy.percentile(agg_job_results['compression_times'], 99.0)))
	if len(agg_job_results['bone_error_values']) > 0:
		print('Bone error 99th percentile: {:.4f}'.format(numpy.percentile(agg_job_results['bone_error_values'], 99.0)))
		print('Error threshold percentile rank: {:.2f} (0.01)'.format(percentile_rank(agg_job_results['bone_error_values'], 0.01)))
	print()

	print('Most accurate: {}'.format(best_runs['best_error_entry']['filename']))
	print_stat(best_runs['best_error_entry'])

	print('Best ratio: {}'.format(best_runs['best_ratio_entry']['filename']))
	print_stat(best_runs['best_ratio_entry'])

	print('Least accurate: {}'.format(worst_runs['worst_error_entry']['filename']))
	print_stat(worst_runs['worst_error_entry'])

	print('Worst ratio: {}'.format(worst_runs['worst_ratio_entry']['filename']))
	print_stat(worst_runs['worst_ratio_entry'])

	if options['stat_detailed']:
		print('----- Detailed stats -----')
		print()

		aggregate_and_print_track_results(agg_run_stats, None)
		print()
		aggregate_and_print_track_results(agg_run_stats, 'rotation')
		print()
		aggregate_and_print_track_results(agg_run_stats, 'translation')
		print()
		aggregate_and_print_track_results(agg_run_stats, 'scale')
		print()

		aggregate_and_print_clip_metadata_results(agg_run_stats)
		print()

		aggregate_and_print_segment_metadata_results(agg_run_stats)
		print()

		aggregate_and_print_segment_animated_results(agg_run_stats)
		print()

		aggregate_and_print_num_segment_results(agg_run_stats)
		print()
