import json
import numpy
import os
import re
import sys

# This script depends on a SJSON parsing package:
# https://pypi.python.org/pypi/SJSON/1.1.0
# https://shelter13.net/projects/SJSON/
# https://bitbucket.org/Anteru/sjson/src
import sjson

def get_clip_names(benchmarks):
	clip_names = []
	for bench in benchmarks:
		run_name = bench['name']

		matches = re.search('^([\w\_]+).acl/', run_name)
		if matches == None:
			print('Failed to find the clip name from benchmark run: {}', run_name)
		else:
			clip_name = matches.group(1)
			clip_names.append(clip_name)
			bench['clip_name'] = clip_name

	return sorted(list(set(clip_names)))

def get_median_runs(clip_name, benchmarks):
	pose = None
	bone = None
	for bench in benchmarks:
		if bench['clip_name'] != clip_name:
			continue	# Not our clip

		if 'Dir:0' not in bench['name']:
			continue	# Wrong direction

		if bench['run_type'] != 'aggregate':
			continue	# Not an aggregate value

		if bench['aggregate_name'] != 'median':
			continue	# Not our median

		if 'Func:0' in bench['name']:
			# Decompress pose
			pose = bench
		elif 'Func:1' in bench['name']:
			# Decompress bone
			bone = bench

	return (pose, bone)

def ns_to_us(time_ns):
	return time_ns / 1000.0

def bytessec_to_mbsec(bytes_per_sec):
	return bytes_per_sec / (1024.0 * 1024.0)

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

	decomp_delta_us_csv_file = open('decomp_delta_forward_stats_us.csv', 'w')
	decomp_delta_mbsec_csv_file = open('decomp_delta_forward_stats_mbsec.csv', 'w')

	pose_size_per_clip = {}
	per_entry_data = []

	for entry in input_sjson_data['inputs']:
		print('Processing {} ...'.format(entry['stats_dir']))

		benchmark_json_file = os.path.join(entry['stats_dir'], 'benchmark_results.json')
		with open(benchmark_json_file, 'r') as file:
			json_data = json.loads(file.read())

		benchmarks = json_data['benchmarks']

		if len(clip_names) == 0:
			clip_names = get_clip_names(benchmarks)
			print('Variants,Config,Version,{}'.format(','.join(clip_names)), file = decomp_delta_us_csv_file)
			print('Variants,Config,Version,{}'.format(','.join(clip_names)), file = decomp_delta_mbsec_csv_file)
		else:
			get_clip_names(benchmarks)

		pose_medians_us = {}
		bone_medians_us = {}
		pose_medians_mbsec = {}
		bone_medians_mbsec = {}

		for clip_name in clip_names:
			print('  Processing {} ...'.format(clip_name))

			(pose_median_run, bone_median_run) = get_median_runs(clip_name, benchmarks)

			# Convert from nanoseconds into milliseconds
			pose_median = ns_to_us(pose_median_run['real_time'])
			bone_median = ns_to_us(bone_median_run['real_time'])

			pose_medians_us[clip_name] = pose_median
			bone_medians_us[clip_name] = bone_median

			# Convert from bytes/sec to megabytes/sec
			pose_speed = bytessec_to_mbsec(pose_median_run['Speed'])
			bone_speed = bytessec_to_mbsec(bone_median_run['Speed'])

			pose_medians_mbsec[clip_name] = pose_speed
			bone_medians_mbsec[clip_name] = bone_speed

		data = {}
		data['name'] = entry['name']
		data['version'] = entry['version']
		data['pose_medians_us'] = pose_medians_us
		data['bone_medians_us'] = bone_medians_us
		data['pose_medians_mbsec'] = pose_medians_mbsec
		data['bone_medians_mbsec'] = bone_medians_mbsec
		data['clip_names'] = clip_names
		per_entry_data.append(data)

	for data in per_entry_data:
		pose_medians_us = data['pose_medians_us']
		bone_medians_us = data['bone_medians_us']
		pose_medians_mbsec = data['pose_medians_mbsec']
		bone_medians_mbsec = data['bone_medians_mbsec']
		clip_names = data['clip_names']

		pose_medians_us_csv = []
		bone_medians_us_csv = []
		pose_medians_mbsec_csv = []
		bone_medians_mbsec_csv = []

		for clip_name in clip_names:
			pose_size = pose_size_per_clip[clip_name]
			pose_cold_median_us = pose_medians_us[clip_name]
			bone_cold_median_us = bone_medians_us[clip_name]

			pose_medians_us_csv.append(str(pose_cold_median_us))
			bone_medians_us_csv.append(str(bone_cold_median_us))

			pose_cold_speed_mbsec = pose_medians_mbsec[clip_name]
			bone_cold_speed_mbsec = bone_medians_mbsec[clip_name]

			# Convert the speed into MB/sec
			pose_medians_mbsec_csv.append(str(pose_cold_speed_mbsec))
			bone_medians_mbsec_csv.append(str(bone_cold_speed_mbsec))

		print('decompress_pose,{},{},{}'.format(data['name'], data['version'], ','.join(pose_medians_us_csv)), file = decomp_delta_us_csv_file)
		print('decompress_bone,{},{},{}'.format(data['name'], data['version'], ','.join(bone_medians_us_csv)), file = decomp_delta_us_csv_file)

		print('decompress_pose,{},{},{}'.format(data['name'], data['version'], ','.join(pose_medians_mbsec_csv)), file = decomp_delta_mbsec_csv_file)
		print('decompress_bone,{},{},{}'.format(data['name'], data['version'], ','.join(bone_medians_mbsec_csv)), file = decomp_delta_mbsec_csv_file)

	decomp_delta_us_csv_file.close()
	decomp_delta_mbsec_csv_file.close()
