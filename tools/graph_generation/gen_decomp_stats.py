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
	decomp_cold_csv_file = open('decomp_cold_forward_stats.csv', 'w')
	decomp_cold_mbsec_csv_file = open('decomp_cold_forward_stats_mbsec.csv', 'w')

	for entry in input_sjson_data['inputs']:
		print('Processing {} ...'.format(entry['stats_dir']))

		benchmark_json_file = os.path.join(entry['stats_dir'], 'benchmark_results.json')
		with open(benchmark_json_file, 'r') as file:
			json_data = json.loads(file.read())

		benchmarks = json_data['benchmarks']

		if len(clip_names) == 0:
			clip_names = get_clip_names(benchmarks)
			print('Variants,{}'.format(','.join(clip_names)), file = pose_cold_csv_file)
			print('Variants,{}'.format(','.join(clip_names)), file = decomp_cold_csv_file)
			print('Variants,{}'.format(','.join(clip_names)), file = decomp_cold_mbsec_csv_file)
		else:
			get_clip_names(benchmarks)

		pose_cold_medians = []
		bone_cold_medians = []
		pose_cold_mbsec = []
		bone_cold_mbsec = []

		for clip_name in clip_names:
			print('  Processing {} ...'.format(clip_name))

			(pose_median_run, bone_median_run) = get_median_runs(clip_name, benchmarks)

			# Convert from nanoseconds into microseconds
			pose_median = pose_median_run['real_time'] / 1000.0
			bone_median = bone_median_run['real_time'] / 1000.0

			pose_cold_medians.append(str(pose_median))
			bone_cold_medians.append(str(bone_median))

			# Convert from bytes/sec to megabytes/sec
			pose_speed = pose_median_run['Speed'] / (1024.0 * 1024.0)
			bone_speed = bone_median_run['Speed'] / (1024.0 * 1024.0)

			pose_cold_mbsec.append(str(pose_speed))
			bone_cold_mbsec.append(str(bone_speed))

		print('{},{}'.format(entry['name'], ','.join(pose_cold_medians)), file = pose_cold_csv_file)
		print('{},{}'.format('decompress_pose {}'.format(entry['name']), ','.join(pose_cold_medians)), file = decomp_cold_csv_file)
		print('{},{}'.format('decompress_bone {}'.format(entry['name']), ','.join(bone_cold_medians)), file = decomp_cold_csv_file)
		print('{},{}'.format('decompress_pose {}'.format(entry['name']), ','.join(pose_cold_mbsec)), file = decomp_cold_mbsec_csv_file)
		print('{},{}'.format('decompress_bone {}'.format(entry['name']), ','.join(bone_cold_mbsec)), file = decomp_cold_mbsec_csv_file)

	pose_cold_csv_file.close()
	decomp_cold_csv_file.close()
	decomp_cold_mbsec_csv_file.close()
