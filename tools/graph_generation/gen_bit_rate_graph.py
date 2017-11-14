import numpy

input_data_type_def = {
	'names': ('algorithm_names', '0', '3', '4', '5', '6', '7', '8', '9', '10', '11', '12', '13', '14', '15', '16', '17', '18', '19', '32'),
	'formats': ('S128', 'f4', 'f4', 'f4', 'f4', 'f4', 'f4', 'f4', 'f4', 'f4', 'f4', 'f4', 'f4', 'f4', 'f4', 'f4', 'f4', 'f4', 'f4', 'f4')
}
columns_to_extract = (0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19)

output_csv_file_path = 'D:\\acl-dev\\tools\\graph_generation\\bit_rates.csv'

input_csv_files =  []
input_csv_files.append(('ACL v0.5 @ 0.01cm', columns_to_extract, 'D:\\test_animations\\carnegie-mellon-acl-stats-0.5.0-detailed\\stats_bit_rate.csv'))

acl_best_algorithm_name = b'R:QuatNoWVar|T:Vec3Var|S:Vec3Var Clip RR:Rot|Trans|Scale Segment RR:Rot|Trans|Scale'

output_csv_data = []
output_csv_headers = ['Bit Rate']

output_csv_data.append(['0', '3', '4', '5', '6', '7', '8', '9', '10', '11', '12', '13', '14', '15', '16', '17', '18', '19', '32'])

for (header, columns_to_extract, input_csv_file_path) in input_csv_files:
	csv_data = numpy.loadtxt(input_csv_file_path, delimiter=',', dtype=input_data_type_def, skiprows=1, usecols=columns_to_extract)
	best_variable_data_mask = csv_data['algorithm_names'] == acl_best_algorithm_name
	csv_data = csv_data[best_variable_data_mask]

	# Strip algorithm name
	output_csv_data.append(csv_data[0].tolist()[1:])

	output_csv_headers.append(header)

output_csv_data = numpy.column_stack(output_csv_data)

with open(output_csv_file_path, 'wb') as f:
	header = bytes('{}\n'.format(','.join(output_csv_headers)), 'utf-8')
	f.write(header)
	numpy.savetxt(f, output_csv_data, delimiter=',', fmt=('%s'))
