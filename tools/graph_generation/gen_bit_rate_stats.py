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
		print('Usage: python gen_bit_rate_stats.py <path/to/input_file.sjson>')
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
		'names': ('algorithm_names', '0', '3', '4', '5', '6', '7', '8', '9', '10', '11', '12', '13', '14', '15', '16', '17', '18', '19', '32'),
		'formats': ('S128', 'f4', 'f4', 'f4', 'f4', 'f4', 'f4', 'f4', 'f4', 'f4', 'f4', 'f4', 'f4', 'f4', 'f4', 'f4', 'f4', 'f4', 'f4', 'f4')
	}
	columns_to_extract = (0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19)

	output_csv_file_path = 'D:\\acl-dev\\tools\\graph_generation\\bit_rates.csv'

	output_csv_data = []
	output_csv_headers = ['Bit Rate']

	output_csv_data.append(['0', '3', '4', '5', '6', '7', '8', '9', '10', '11', '12', '13', '14', '15', '16', '17', '18', '19', '32'])

	for entry in input_sjson_data['inputs']:
		print('Parsing {} ...'.format(entry['header']))
		csv_data = numpy.loadtxt(entry['file'], delimiter=',', dtype=input_data_type_def, skiprows=1, usecols=columns_to_extract)

		filter = entry.get('filter', None)
		if filter != None:
			best_variable_data_mask = csv_data['algorithm_names'] == bytes(entry['filter'], encoding = 'utf-8')
			csv_data = csv_data[best_variable_data_mask]

		# Strip algorithm name
		output_csv_data.append(csv_data[0].tolist()[1:])

		output_csv_headers.append(entry['header'])

	output_csv_data = numpy.column_stack(output_csv_data)

	with open(output_csv_file_path, 'wb') as f:
		header = bytes('{}\n'.format(','.join(output_csv_headers)), 'utf-8')
		f.write(header)
		numpy.savetxt(f, output_csv_data, delimiter=',', fmt=('%s'))
