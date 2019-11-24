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

def ms_to_us(time_ms):
	return time_ms * 1000.0

def ms_to_s(time_ms):
	return time_ms / 1000.0

def bytes_to_mb(num_bytes):
	return num_bytes / (1024 * 1024)

if __name__ == "__main__":
	if sys.version_info < (3, 4):
		print('Python 3.4 or higher needed to run this script')
		sys.exit(1)

	if len(sys.argv) != 2 and len(sys.argv) != 3:
		print('Usage: python gen_decomp_delta_stats.py <path/to/input_file.sjson> [-warm]')
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
	if len(sys.argv) == 3 and sys.argv[2] == '-warm':
		label = 'warm'
	else:
		label = 'cold'

	decomp_delta_us_csv_file = open('decomp_delta_{}_forward_stats_us.csv'.format(label), 'w')
	decomp_delta_mbsec_csv_file = open('decomp_delta_{}_forward_stats_mbsec.csv'.format(label), 'w')

	pose_size_per_clip = {}
	per_entry_data = []

	for entry in input_sjson_data['inputs']:
		print('Processing {} ...'.format(entry['stats_dir']))

		if len(clip_names) == 0:
			clip_names = get_clip_names(entry['stats_dir'])
			print('Variants,Config,Version,{}'.format(','.join(clip_names)), file = decomp_delta_us_csv_file)
			print('Variants,Config,Version,{}'.format(','.join(clip_names)), file = decomp_delta_mbsec_csv_file)

		pose_medians_ms = {}
		bone_medians_ms = {}
		clip_names = []

		stat_files = get_clip_stat_files(entry['stats_dir'])
		for (clip_name, stat_filename) in stat_files:
			print('  Processing {} ...'.format(stat_filename))

			with open(stat_filename, 'r') as file:
				clip_sjson_data = sjson.loads(file.read())

			clip_names.append(clip_name)

			run_data = clip_sjson_data['runs'][0]['decompression_time_per_sample']

			forward_data_pose = run_data['forward_pose_{}'.format(label)]['data']
			forward_data_bone = run_data['forward_bone_{}'.format(label)]['data']

			pose_medians_ms[clip_name] = numpy.median(forward_data_pose)
			bone_medians_ms[clip_name] = numpy.median(forward_data_bone)

			if 'pose_size' in clip_sjson_data['runs'][0]:
				pose_size = clip_sjson_data['runs'][0]['pose_size']
				pose_size_per_clip[clip_name] = pose_size

		data = {}
		data['name'] = entry['name']
		data['version'] = entry['version']
		data['pose_medians_ms'] = pose_medians_ms
		data['bone_medians_ms'] = bone_medians_ms
		data['clip_names'] = clip_names
		per_entry_data.append(data)

	for data in per_entry_data:
		pose_medians_ms = data['pose_medians_ms']
		bone_medians_ms = data['bone_medians_ms']
		clip_names = data['clip_names']

		pose_medians_us = []
		bone_medians_us = []
		pose_medians_mbsec = []
		bone_medians_mbsec = []

		for clip_name in clip_names:
			pose_size = pose_size_per_clip[clip_name]
			pose_cold_median_ms = pose_medians_ms[clip_name]
			bone_cold_median_ms = bone_medians_ms[clip_name]

			# Convert the elapsed time from milliseconds into microseconds
			pose_medians_us.append(str(ms_to_us(pose_cold_median_ms)))
			bone_medians_us.append(str(ms_to_us(bone_cold_median_ms)))

			# Convert the speed into MB/sec
			pose_medians_mbsec.append(str(bytes_to_mb(pose_size) / ms_to_s(pose_cold_median_ms)))
			bone_medians_mbsec.append(str(bytes_to_mb(pose_size) / ms_to_s(bone_cold_median_ms)))

		print('decompress_pose,{},{},{}'.format(data['name'], data['version'], ','.join(pose_medians_us)), file = decomp_delta_us_csv_file)
		print('decompress_bone,{},{},{}'.format(data['name'], data['version'], ','.join(bone_medians_us)), file = decomp_delta_us_csv_file)

		print('decompress_pose,{},{},{}'.format(data['name'], data['version'], ','.join(pose_medians_mbsec)), file = decomp_delta_mbsec_csv_file)
		print('decompress_bone,{},{},{}'.format(data['name'], data['version'], ','.join(bone_medians_mbsec)), file = decomp_delta_mbsec_csv_file)

	decomp_delta_us_csv_file.close()
	decomp_delta_mbsec_csv_file.close()
