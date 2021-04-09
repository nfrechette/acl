import numpy
import os
import sys

# This script depends on a SJSON parsing package:
# https://pypi.python.org/pypi/SJSON/1.1.0
# https://shelter13.net/projects/SJSON/
# https://bitbucket.org/Anteru/sjson/src
import sjson

if __name__ == "__main__":
	if sys.version_info < (3, 4):
		print('Python 3.4 or higher needed to run this script')
		sys.exit(1)

	if len(sys.argv) != 2:
		print('Usage: python gen_summary_stats.py <path/to/input_file.sjson>')
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

	input_data_type_def = {
		'names': ('algorithm_names', 'raw_sizes', 'compression_ratios', 'durations', 'max_errors'),
		'formats': ('S128', 'f4', 'f4', 'f4', 'f4')
	}

	columns_to_extract_acl_pre_v06 = (0, 1, 3, 5, 7)
	columns_to_extract_acl_post_v06 = (1, 2, 4, 6, 8)
	columns_to_extract_ue4 = (0, 1, 3, 4, 5)

	output_csv_dir = os.getcwd()
	output_csv_file_path_ratios = os.path.join(output_csv_dir, 'compression_ratios.csv')
	output_csv_file_path_ratios_by_raw_size = os.path.join(output_csv_dir, 'compression_ratios_by_raw_size.csv')
	output_csv_file_path_max_errors = os.path.join(output_csv_dir, 'max_errors.csv')
	output_csv_file_path_max_errors_by_raw_size = os.path.join(output_csv_dir, 'max_errors_by_raw_size.csv')
	output_csv_file_path_ratio_vs_max_error = os.path.join(output_csv_dir, 'ratio_vs_max_error.csv')
	output_csv_file_path_durations = os.path.join(output_csv_dir, 'durations.csv')

	output_csv_data_ratios = []
	output_csv_data_ratios_by_raw_size = []
	output_csv_data_max_errors = []
	output_csv_data_max_errors_by_raw_size = []
	output_csv_data_ratio_vs_max_error = []
	output_csv_data_durations = []
	output_csv_headers = []

	for entry in input_sjson_data['inputs']:
		if entry['version'] >= 0.6:
			if entry['source'] == 'acl':
				columns_to_extract = columns_to_extract_acl_post_v06
			else:
				columns_to_extract = columns_to_extract_ue4
		else:
			if entry['source'] == 'acl':
				columns_to_extract = columns_to_extract_acl_pre_v06
			else:
				columns_to_extract = columns_to_extract_ue4

		print('Parsing {} ...'.format(entry['header']))
		csv_data = numpy.loadtxt(entry['file'], delimiter=',', dtype=input_data_type_def, skiprows=1, usecols=columns_to_extract)

		filter = entry.get('filter', None)
		if filter != None:
			best_variable_data_mask = csv_data['algorithm_names'] == bytes(entry['filter'], encoding = 'utf-8')
			csv_data = csv_data[best_variable_data_mask]

		csv_data_ratios = numpy.sort(csv_data, order='compression_ratios')['compression_ratios']
		csv_data_ratios_by_raw_size = numpy.sort(csv_data, order='raw_sizes')['compression_ratios']
		csv_data_max_errors = numpy.sort(csv_data, order='max_errors')['max_errors']
		csv_data_max_errors_by_raw_size = numpy.sort(csv_data, order='raw_sizes')['max_errors']
		csv_data_ratio_by_max_error = numpy.sort(csv_data, order='max_errors')['compression_ratios']
		csv_data_durations = numpy.sort(csv_data, order='durations')['durations']

		output_csv_data_ratios.append(csv_data_ratios)
		output_csv_data_ratios_by_raw_size.append(csv_data_ratios_by_raw_size)
		output_csv_data_max_errors.append(csv_data_max_errors)
		output_csv_data_max_errors_by_raw_size.append(csv_data_max_errors_by_raw_size)
		output_csv_data_ratio_vs_max_error.append(csv_data_max_errors)
		output_csv_data_ratio_vs_max_error.append(csv_data_ratio_by_max_error)
		output_csv_data_durations.append(csv_data_durations)

		output_csv_headers.append(entry['header'])

	output_csv_data_ratios = numpy.column_stack(output_csv_data_ratios)
	output_csv_data_ratios_by_raw_size = numpy.column_stack(output_csv_data_ratios_by_raw_size)
	output_csv_data_max_errors = numpy.column_stack(output_csv_data_max_errors)
	output_csv_data_max_errors_by_raw_size = numpy.column_stack(output_csv_data_max_errors_by_raw_size)
	output_csv_data_ratio_vs_max_error = numpy.column_stack(output_csv_data_ratio_vs_max_error)
	output_csv_data_durations = numpy.column_stack(output_csv_data_durations)

	with open(output_csv_file_path_ratios, 'wb') as f:
		header = bytes('{}\n'.format(','.join(output_csv_headers)), 'utf-8')
		f.write(header)
		numpy.savetxt(f, output_csv_data_ratios, delimiter=',', fmt=('%f'))

	with open(output_csv_file_path_ratios_by_raw_size, 'wb') as f:
		header = bytes('{}\n'.format(','.join(output_csv_headers)), 'utf-8')
		f.write(header)
		numpy.savetxt(f, output_csv_data_ratios_by_raw_size, delimiter=',', fmt=('%f'))

	with open(output_csv_file_path_max_errors, 'wb') as f:
		header = bytes('{}\n'.format(','.join(output_csv_headers)), 'utf-8')
		f.write(header)
		numpy.savetxt(f, output_csv_data_max_errors, delimiter=',', fmt=('%f'))

	with open(output_csv_file_path_max_errors_by_raw_size, 'wb') as f:
		header = bytes('{}\n'.format(','.join(output_csv_headers)), 'utf-8')
		f.write(header)
		numpy.savetxt(f, output_csv_data_max_errors_by_raw_size, delimiter=',', fmt=('%f'))

	with open(output_csv_file_path_ratio_vs_max_error, 'wb') as f:
		header = bytes('{}\n'.format(','.join(output_csv_headers)), 'utf-8')
		f.write(header)
		numpy.savetxt(f, output_csv_data_ratio_vs_max_error, delimiter=',', fmt=('%f'))

	with open(output_csv_file_path_durations, 'wb') as f:
		header = bytes('{}\n'.format(','.join(output_csv_headers)), 'utf-8')
		f.write(header)
		numpy.savetxt(f, output_csv_data_durations, delimiter=',', fmt=('%f'))
