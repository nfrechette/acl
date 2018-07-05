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
	if len(sys.argv) != 2:
		print('Usage: python gen_decomp_delta_stats.py <path/to/input_file.sjson>')
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

	decomp_delta_cold_csv_file = open('decomp_delta_cold_forward_stats.csv', 'w')

	for entry in input_sjson_data['inputs']:
		print('Processing {} ...'.format(entry['stats_dir']))

		if len(clip_names) == 0:
			clip_names = get_clip_names(entry['stats_dir'])
			print('Variants,Config,Version,{}'.format(','.join(clip_names)), file = decomp_delta_cold_csv_file)

		pose_cold_medians = []
		pose_warm_medians = []
		bone_cold_medians = []

		stat_files = get_clip_stat_files(entry['stats_dir'])
		for (clip_name, stat_filename) in stat_files:
			print('  Processing {} ...'.format(stat_filename))

			with open(stat_filename, 'r') as file:
				clip_sjson_data = sjson.loads(file.read())

			run_data = clip_sjson_data['runs'][0]['decompression_time_per_sample']

			# We also convert the elapsed time from milliseconds into microseconds
			forward_data_pose_cold = run_data['forward_pose_cold']['data']
			pose_cold_medians.append(str(numpy.median(forward_data_pose_cold) * 1000.0))

			forward_data_bone_cold = run_data['forward_bone_cold']['data']
			bone_cold_medians.append(str(numpy.median(forward_data_bone_cold) * 1000.0))

		print('decompress_pose,{},{},{}'.format(entry['name'], entry['version'], ','.join(pose_cold_medians)), file = decomp_delta_cold_csv_file)
		print('decompress_bone,{},{},{}'.format(entry['name'], entry['version'], ','.join(bone_cold_medians)), file = decomp_delta_cold_csv_file)

	decomp_delta_cold_csv_file.close()
