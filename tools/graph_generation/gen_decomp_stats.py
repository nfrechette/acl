import numpy
import os
import sys

# This script depends on a SJSON parsing package:
# https://pypi.python.org/pypi/SJSON/1.1.0
# https://shelter13.net/projects/SJSON/
# https://bitbucket.org/Anteru/sjson/src
import sjson

def get_clip_name(clip_filename):
	return clip_filename.replace('_stats.sjson', '')

def get_clip_names(stats_dir):
	clip_names = []
	for (dirpath, dirnames, filenames) in os.walk(stats_dir):
		for filename in filenames:
			if not filename.endswith('.sjson'):
				continue
			clip_names.append(get_clip_name(filename))
	return sorted(clip_names)

def get_clip_stat_files(stats_dir):
	stat_files = []
	for (dirpath, dirnames, filenames) in os.walk(stats_dir):
		for filename in filenames:
			if not filename.endswith('.sjson'):
				continue
			stat_files.append((get_clip_name(filename), os.path.join(dirpath, filename)))
	return sorted(stat_files, key = lambda x: x[0])

if __name__ == "__main__":
	if sys.version_info < (3, 4):
		print('Python 3.4 or higher needed to run this script')
		sys.exit(1)

	if len(sys.argv) != 2:
		print('Usage: python gen_decomp_stats.py <path/to/input_file.sjson>')
		sys.exit(1)

	input_sjson_file = sys.argv[1]
	if not input_sjson_file.endswith('.sjson'):
		print('Expected SJSON input file, found: {}'.format(input_sjson_file))
		sys.exit(1)

	if not os.path.exists(input_sjson_file):
		print('Input file not found: {}'.format(input_sjson_file))
		sys.exit(1)

	with open(input_sjson_file, 'r') as file:
		input_sjson_data = sjson.loads(file.read())

	clip_names = []

	pose_cold_csv_file = open('pose_cold_forward_stats.csv', 'w')
	pose_warm_csv_file = open('pose_warm_forward_stats.csv', 'w')
	decomp_cold_csv_file = open('decomp_cold_forward_stats.csv', 'w')

	for entry in input_sjson_data['inputs']:
		print('Processing {} ...'.format(entry['stats_dir']))

		if len(clip_names) == 0:
			clip_names = get_clip_names(entry['stats_dir'])
			print('Variants,{}'.format(','.join(clip_names)), file = pose_cold_csv_file)
			print('Variants,{}'.format(','.join(clip_names)), file = pose_warm_csv_file)
			print('Variants,{}'.format(','.join(clip_names)), file = decomp_cold_csv_file)

		platform_playback_csv_file = open('{}_playback_direction_stats.csv'.format(entry['name']), 'w')

		pose_cold_medians = []
		pose_warm_medians = []
		bone_cold_medians = []

		stat_files = get_clip_stat_files(entry['stats_dir'])
		for (clip_name, stat_filename) in stat_files:
			print('  Processing {} ...'.format(stat_filename))

			with open(stat_filename, 'r') as file:
				clip_sjson_data = sjson.loads(file.read())

			run_data = clip_sjson_data['runs'][0]['decompression_time_per_sample']
			forward_data = run_data['forward_pose_cold']['data']
			backward_data = run_data['backward_pose_cold']['data']
			random_data = run_data['random_pose_cold']['data']

			print(clip_name, file = platform_playback_csv_file)

			num_samples = len(forward_data)
			normalized_sample_times = [ str(i / float(max(num_samples - 1, 1))) for i in range(num_samples) ]
			print('Normalized Sample Time,{}'.format(','.join(normalized_sample_times)), file = platform_playback_csv_file)
			# We also convert the elapsed time from milliseconds into microseconds
			print('Forward,{}'.format(','.join(map(lambda x: str(x * 1000.0), forward_data))), file = platform_playback_csv_file)
			print('Backward,{}'.format(','.join(map(lambda x: str(x * 1000.0), backward_data))), file = platform_playback_csv_file)
			print('Random,{}'.format(','.join(map(lambda x: str(x * 1000.0), random_data))), file = platform_playback_csv_file)
			print(file = platform_playback_csv_file)

			pose_cold_medians.append(str(numpy.median(forward_data) * 1000.0))

			forward_data_warm = run_data['forward_pose_warm']['data']
			pose_warm_medians.append(str(numpy.median(forward_data_warm) * 1000.0))

			forward_data_bone_cold = run_data['forward_bone_cold']['data']
			bone_cold_medians.append(str(numpy.median(forward_data_bone_cold) * 1000.0))

		platform_playback_csv_file.close()

		print('{},{}'.format(entry['name'], ','.join(pose_cold_medians)), file = pose_cold_csv_file)
		print('{},{}'.format(entry['name'], ','.join(pose_warm_medians)), file = pose_warm_csv_file)
		print('{},{}'.format('decompress_pose {}'.format(entry['name']), ','.join(pose_cold_medians)), file = decomp_cold_csv_file)
		print('{},{}'.format('decompress_bone {}'.format(entry['name']), ','.join(bone_cold_medians)), file = decomp_cold_csv_file)

	pose_cold_csv_file.close()
	pose_warm_csv_file.close()
	decomp_cold_csv_file.close()
