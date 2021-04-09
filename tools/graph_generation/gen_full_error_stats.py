import numpy
import os
import sys
import time

# This script depends on a SJSON parsing package:
# https://pypi.python.org/pypi/SJSON/1.1.0
# https://shelter13.net/projects/SJSON/
# https://bitbucket.org/Anteru/sjson/src
import sjson

def format_elapsed_time(elapsed_time):
	hours, rem = divmod(elapsed_time, 3600)
	minutes, seconds = divmod(rem, 60)
	return '{:0>2}h {:0>2}m {:05.2f}s'.format(int(hours), int(minutes), seconds)

if __name__ == "__main__":
	if sys.version_info < (3, 4):
		print('Python 3.4 or higher needed to run this script')
		sys.exit(1)

	if len(sys.argv) != 2:
		print('Usage: python gen_full_error_stats.py <path/to/input_file.sjson>')
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

	input_data_type_def = { 'names': ('clip_names', 'errors'), 'formats': ('S128', 'f4') }
	columns_to_extract = (0, 3)

	output_csv_dir = os.getcwd()
	output_csv_file_path = os.path.join(output_csv_dir, 'full_errors.csv')
	output_csv_file_path_top10 = os.path.join(output_csv_dir, 'full_errors_top10.csv')

	desired_percentiles = [x * 0.1 for x in range(0, 1001)]
	desired_percentiles_top10 = [90.0 + (x * 0.01) for x in range(0, 1001)]

	output_csv_data = []
	output_csv_data_top10 = []
	output_csv_headers = []

	for entry in input_sjson_data['inputs']:
		print('Parsing {} ...'.format(entry['header']))
		parsing_start_time = time.perf_counter();
		csv_data = numpy.loadtxt(entry['file'], delimiter=',', dtype=input_data_type_def, skiprows=1, usecols=columns_to_extract)
		parsing_end_time = time.perf_counter();
		print('Parsed {} ({}) rows in {}'.format(entry['header'], len(csv_data['errors']), format_elapsed_time(parsing_end_time - parsing_start_time)))

		percentiles = numpy.percentile(csv_data['errors'], desired_percentiles)
		percentiles_top10 = numpy.percentile(csv_data['errors'], desired_percentiles_top10)

		output_csv_data.append(percentiles)
		output_csv_data_top10.append(percentiles_top10)

		output_csv_headers.append(entry['header'])

		# Clear data to release memory
		csv_data = None

	output_csv_headers.append('Percentile')

	output_csv_data.append(desired_percentiles)
	output_csv_data = numpy.column_stack(output_csv_data)

	output_csv_data_top10.append(desired_percentiles_top10)
	output_csv_data_top10 = numpy.column_stack(output_csv_data_top10)

	with open(output_csv_file_path, 'wb') as f:
		header = bytes('{}\n'.format(','.join(output_csv_headers)), 'utf-8')
		f.write(header)
		numpy.savetxt(f, output_csv_data, delimiter=',', fmt=('%f'))

	with open(output_csv_file_path_top10, 'wb') as f:
		header = bytes('{}\n'.format(','.join(output_csv_headers)), 'utf-8')
		f.write(header)
		numpy.savetxt(f, output_csv_data_top10, delimiter=',', fmt=('%f'))
