import multiprocessing
import os
import platform
import psutil
import queue
import threading
import time
import re
import signal
import subprocess
import sys

# This script depends on a SJSON parsing package:
# https://pypi.python.org/pypi/SJSON/1.1.0
# https://shelter13.net/projects/SJSON/
# https://bitbucket.org/Anteru/sjson/src
import sjson


def parse_argv():
	options = {}
	options['acl'] = os.path.join(os.getcwd(), '../../test_data/decomp_data_v3')
	options['stats'] = ""
	options['csv'] = False
	options['refresh'] = False
	options['num_threads'] = 1
	options['android'] = False
	options['ios'] = False
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

		if value == '-csv':
			options['csv'] = True

		if value == '-refresh':
			options['refresh'] = True

		if value == '-android':
			options['android'] = True

		if value == '-ios':
			options['ios'] = True

		#if value.startswith('-parallel='):
		#	options['num_threads'] = int(value[len('-parallel='):].replace('"', ''))

		if value == '-help':
			options['print_help'] = True

	if options['print_help']:
		print_help()
		sys.exit(1)

	if options['stats'] == None or len(options['stats']) == 0:
		print('-stats output directory is missing')
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
	print('Usage: python acl_decompressor.py -acl=<path to directory containing ACL files> -stats=<path to output directory for stats> [-csv] [-refresh] [-parallel={Num Threads}] [-help]')

def print_help():
	print('Usage: python acl_decompressor.py [arguments]')
	print()
	print('Arguments:')
	print('  At least one argument must be provided.')
	print('  -acl=<path>: Input directory tree containing clips to compress.')
	print('  -stats=<path>: Output directory tree for the stats to output.')
	print('  -csv: Generates a basic summary CSV file with various clip information and statistics.')
	print('  -refresh: If an output stat file already exists for a particular clip, it is recompressed anyway instead of being skipped.')
	print('  -android: Pulls the files from the android device')
	print('  -ios: Only computes the stats')
	#print('  -parallel=<Num Threads>: Allows multiple clips to be compressed and processed in parallel.')
	print('  -help: Prints this help message.')

def print_stat(stat):
	print('Algorithm: {}, Format: [{}], Ratio: {:.2f}, Error: {}'.format(stat['algorithm_name'], stat['desc'], stat['compression_ratio'], stat['max_error']))
	print('')

def format_elapsed_time(elapsed_time):
	hours, rem = divmod(elapsed_time, 3600)
	minutes, seconds = divmod(rem, 60)
	return '{:0>2}h {:0>2}m {:05.2f}s'.format(int(hours), int(minutes), seconds)

def get_decomp_categories():
	categories = []
	categories.append(('forward_pose_cold', 'forward', 'pose', 'cold'))
	categories.append(('backward_pose_cold', 'backward', 'pose', 'cold'))
	categories.append(('random_pose_cold', 'random', 'pose', 'cold'))
	categories.append(('forward_pose_warm', 'forward', 'pose', 'warm'))
	categories.append(('backward_pose_warm', 'backward', 'pose', 'warm'))
	categories.append(('random_pose_warm', 'random', 'pose', 'warm'))

	categories.append(('forward_bone_cold', 'forward', 'bone', 'cold'))
	categories.append(('backward_bone_cold', 'backward', 'bone', 'cold'))
	categories.append(('random_bone_cold', 'random', 'bone', 'cold'))
	categories.append(('forward_bone_warm', 'forward', 'bone', 'warm'))
	categories.append(('backward_bone_warm', 'backward', 'bone', 'warm'))
	categories.append(('random_bone_warm', 'random', 'bone', 'warm'))

	categories.append(('forward_ue4_cold', 'forward', 'ue4', 'cold'))
	categories.append(('backward_ue4_cold', 'backward', 'ue4', 'cold'))
	categories.append(('random_ue4_cold', 'random', 'ue4', 'cold'))
	categories.append(('forward_ue4_warm', 'forward', 'ue4', 'warm'))
	categories.append(('backward_ue4_warm', 'backward', 'ue4', 'warm'))
	categories.append(('random_ue4_warm', 'random', 'ue4', 'warm'))
	return categories

def create_csv(options):
	csv_data = {}
	stat_dir = options['stats']
	if options['csv']:
		decomp_categories = get_decomp_categories()
		for category in decomp_categories:
			stats_decompression_csv_filename = os.path.join(stat_dir, 'stats_decompression_{}_{}_{}.csv'.format(category[2], category[3], category[1]))
			stats_decompression_csv_file = open(stats_decompression_csv_filename, 'w')
			csv_data[category[0]] = stats_decompression_csv_file

			print('Generating CSV file {} ...'.format(stats_decompression_csv_filename))
			print('Clip Name, Min, Max, Avg', file = stats_decompression_csv_file)

	return csv_data

def close_csv(csv_data):
	if len(csv_data) == 0:
		return

	for csv_file in csv_data.values():
		csv_file.close()

def append_csv(csv_data, job_data):
	decomp_categories = get_decomp_categories()
	data = job_data['stats_summary_data']
	for (clip_name, perf_stats) in data:
		#perf_stats[key] = (category, decomp_min, decomp_max, decomp_avg)
		for key, stats in perf_stats.items():
			if key in csv_data:
				csv_file = csv_data[key]
				(category, decomp_min, decomp_max, decomp_avg) = stats
				print('{}, {}, {}, {}'.format(clip_name, decomp_min, decomp_max, decomp_avg), file = csv_file)

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

	if platform.system() == 'Darwin':
		# On OS X, \r doesn't appear to work properly in the terminal
		print('{}{} |{}| {}{} {}'.format('\b' * 100, prefix, bar, percents, '%', suffix), end='')
	else:
		sys.stdout.write('\r%s |%s| %s%s %s' % (prefix, bar, percents, '%', suffix)),

	if iteration == total:
		print('')

	sys.stdout.flush()

def run_acl_decompressor(cmd_queue, result_queue):
	while True:
		entry = cmd_queue.get()
		if entry is None:
			return

		(acl_filename, cmd) = entry

		os.system(cmd)
		result_queue.put(acl_filename)

def decompress_clips_android(options):
	acl_dir = options['acl']
	stat_dir = options['stats']

	stat_files = []

	for (dirpath, dirnames, filenames) in os.walk(acl_dir):
		stat_dirname = dirpath.replace(acl_dir, stat_dir)

		for filename in filenames:
			if not filename.endswith('.acl.sjson') and not filename.endswith('.acl.bin'):
				continue

			acl_filename = os.path.join(dirpath, filename)
			if filename.endswith('.acl.sjson'):
				stat_filename = os.path.join(stat_dirname, filename.replace('.acl.sjson', '_stats.sjson'))
			else:
				stat_filename = os.path.join(stat_dirname, filename.replace('.acl.bin', '_stats.sjson'))

			stat_files.append(stat_filename)

			if not os.path.exists(stat_dirname):
				os.makedirs(stat_dirname)

	if len(stat_files) == 0:
		print('No ACL clips found to decompress')
		sys.exit(0)

	output = str(subprocess.check_output('adb logcat -s acl -e "Stats will be written to:" -m 1 -d'))
	matches = re.search('Stats will be written to: ([/\.\w]+)', output)
	if matches == None:
		print('Failed to find Android source directory from ADB')
		print('/storage/emulated/0/Android/data/com.acl/files will be used instead')
		android_src_dir = '/storage/emulated/0/Android/data/com.acl/files'
	else:
		android_src_dir = matches.group(1)

	curr_dir = os.getcwd()
	os.chdir(stat_dir)

	for stat_filename in stat_files:
		dst_filename = os.path.basename(stat_filename)
		src_filename = os.path.join(android_src_dir, dst_filename).replace('\\', '/')
		cmd = 'adb pull "{}" "{}"'.format(src_filename, dst_filename)
		os.system(cmd)

	os.chdir(curr_dir)
	return stat_files

def decompress_clips(options):
	acl_dir = options['acl']
	stat_dir = options['stats']
	refresh = options['refresh']

	if platform.system() == 'Windows':
		decompressor_exe_path = '../../build/bin/acl_decompressor.exe'
	else:
		decompressor_exe_path = '../../build/bin/acl_decompressor'

	decompressor_exe_path = os.path.abspath(decompressor_exe_path)
	if not os.path.exists(decompressor_exe_path) and not options['ios']:
		print('Decompressor exe not found: {}'.format(decompressor_exe_path))
		sys.exit(1)

	stat_files = []
	cmd_queue = queue.Queue()

	for (dirpath, dirnames, filenames) in os.walk(acl_dir):
		stat_dirname = dirpath.replace(acl_dir, stat_dir)

		for filename in filenames:
			if not filename.endswith('.acl.sjson') and not filename.endswith('.acl.bin'):
				continue

			acl_filename = os.path.join(dirpath, filename)
			if filename.endswith('.acl.sjson'):
				stat_filename = os.path.join(stat_dirname, filename.replace('.acl.sjson', '_stats.sjson'))
			else:
				stat_filename = os.path.join(stat_dirname, filename.replace('.acl.bin', '_stats.sjson'))

			stat_files.append(stat_filename)

			if os.path.exists(stat_filename) and os.path.isfile(stat_filename) and not refresh:
				continue

			if options['ios']:
				continue

			if not os.path.exists(stat_dirname):
				os.makedirs(stat_dirname)

			cmd = '{} -acl="{}" -stats="{}" -decomp'.format(decompressor_exe_path, acl_filename, stat_filename)
			if platform.system() == 'Windows':
				cmd = cmd.replace('/', '\\')

			cmd_queue.put((acl_filename, cmd))

	if len(stat_files) == 0:
		print("No ACL clips found to decompress")
		sys.exit(0)

	if not cmd_queue.empty():
		# Add a marker to terminate the threads
		for i in range(options['num_threads']):
			cmd_queue.put(None)

		result_queue = queue.Queue()
		decompression_start_time = time.clock()

		threads = [ threading.Thread(target = run_acl_decompressor, args = (cmd_queue, result_queue)) for _i in range(options['num_threads']) ]
		for thread in threads:
			thread.daemon = True
			thread.start()

		print_progress(0, len(stat_files), 'Decompressing clips:', '{} / {}'.format(0, len(stat_files)))
		try:
			while True:
				for thread in threads:
					thread.join(1.0)

				num_processed = result_queue.qsize()
				print_progress(num_processed, len(stat_files), 'Decompressing clips:', '{} / {}'.format(num_processed, len(stat_files)))

				all_threads_done = True
				for thread in threads:
					if thread.isAlive():
						all_threads_done = False

				if all_threads_done:
					break
		except KeyboardInterrupt:
			sys.exit(1)

		decompression_end_time = time.clock()
		print()
		print('Compressed {} clips in {}'.format(len(stat_files), format_elapsed_time(decompression_end_time - decompression_start_time)))

	return stat_files

def run_stat_parsing(options, stat_queue, result_queue):
	#signal.signal(signal.SIGINT, signal.SIG_IGN)

	try:
		num_runs = 0
		stats_summary_data = []

		decomp_categories = get_decomp_categories()

		while True:
			stat_filename = stat_queue.get()
			if stat_filename is None:
				break

			with open(stat_filename, 'r') as file:
				try:
					file_data = sjson.loads(file.read())
					runs = file_data['runs']
					for run_stats in runs:
						run_stats['filename'] = stat_filename
						run_stats['clip_name'] = os.path.splitext(os.path.basename(stat_filename))[0]

						num_runs += 1

						perf_stats = {}
						if 'decompression_time_per_sample' in run_stats:
							for category in decomp_categories:
								key = category[0]
								if key in run_stats['decompression_time_per_sample']:
									decomp_data = run_stats['decompression_time_per_sample'][key]
									decomp_min = decomp_data['min_time_ms']
									decomp_max = decomp_data['max_time_ms']
									decomp_avg = decomp_data['avg_time_ms']
									perf_stats[key] = (category, decomp_min, decomp_max, decomp_avg)

						if options['csv']:
							#(name, perf_stats)
							data = (run_stats['clip_name'], perf_stats)
							stats_summary_data.append(data)

					result_queue.put(('progress', stat_filename))
				except sjson.ParseException:
					print('Failed to parse SJSON file: {}'.format(stat_filename))

		# Done
		results = {}
		results['num_runs'] = num_runs
		results['stats_summary_data'] = stats_summary_data

		result_queue.put(('done', results))
	except KeyboardInterrupt:
		print('Interrupted')

def aggregate_job_stats(agg_job_results, job_results):
	if job_results['num_runs'] == 0:
		return

	if len(agg_job_results) == 0:
		agg_job_results.update(job_results)
	else:
		agg_job_results['num_runs'] += job_results['num_runs']

def set_process_affinity(affinity):
	if platform.system() == 'Windows':
		p = psutil.Process()
		p.cpu_affinity([affinity])

if __name__ == "__main__":
	options = parse_argv()

	# Set the affinity to core 0, on platforms that support it, core 2 will be used to decompress
	set_process_affinity(0)

	if options['android']:
		stat_files = decompress_clips_android(options)
	else:
		stat_files = decompress_clips(options)

	csv_data = create_csv(options)

	aggregating_start_time = time.clock()

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
	print_progress(num_stat_file_processed, len(stat_files), 'Aggregating results:', '{} / {}'.format(num_stat_file_processed, len(stat_files)))
	try:
		while True:
			try:
				(msg, data) = result_queue.get(True, 1.0)
				if msg == 'progress':
					num_stat_file_processed += 1
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

	num_runs = agg_job_results['num_runs']

	aggregating_end_time = time.clock()
	print()
	print('Found {} runs in {}'.format(num_runs, format_elapsed_time(aggregating_end_time - aggregating_start_time)))
	print()

	close_csv(csv_data)
