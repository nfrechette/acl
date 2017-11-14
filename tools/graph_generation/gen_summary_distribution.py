import numpy

input_data_type_def = { 'names': ('algorithm_names', 'raw_sizes', 'compression_ratios', 'durations', 'max_errors'), 'formats': ('S128', 'f4', 'f4', 'f4', 'f4') }
columns_to_extract_acl = (0, 1, 3, 5, 7)
columns_to_extract_ue4 = (0, 1, 3, 4, 5)

output_csv_file_path_ratios = 'D:\\acl-dev\\tools\\graph_generation\\compression_ratios.csv'
output_csv_file_path_ratios_by_raw_size = 'D:\\acl-dev\\tools\\graph_generation\\compression_ratios_by_raw_size.csv'
output_csv_file_path_max_errors = 'D:\\acl-dev\\tools\\graph_generation\\max_errors.csv'
output_csv_file_path_max_errors_by_raw_size = 'D:\\acl-dev\\tools\\graph_generation\\max_errors_by_raw_size.csv'
output_csv_file_path_ratio_vs_max_error = 'D:\\acl-dev\\tools\\graph_generation\\ratio_vs_max_error.csv'
output_csv_file_path_durations = 'D:\\acl-dev\\tools\\graph_generation\\durations.csv'

input_csv_files =  []
input_csv_files.append(('ACL v0.4 @ 0.01cm', columns_to_extract_acl, 'D:\\test_animations\\carnegie-mellon-acl-stats-0.4.0-summary-fixed-ratio\\stats_summary.csv'))
input_csv_files.append(('ACL v0.5 @ 0.01cm', columns_to_extract_acl, 'D:\\test_animations\\carnegie-mellon-acl-stats-0.5.0-summary\\stats_summary.csv'))
input_csv_files.append(('UE 4.15 @ 0.01cm', columns_to_extract_ue4, 'D:\\test_animations\\carnegie-mellon-ue4-4.15-stats-0.4.0-0.01-fixed-ratio\\stats_summary.csv'))
input_csv_files.append(('UE 4.15 @ 0.1cm', columns_to_extract_ue4, 'D:\\test_animations\\carnegie-mellon-ue4-4.15-stats-0.4.0-0.1-fixed-ratio\\stats_summary.csv'))
input_csv_files.append(('UE 4.15 @ 1.0cm', columns_to_extract_ue4, 'D:\\test_animations\\carnegie-mellon-ue4-4.15-stats-0.4.0-1.0-fixed-ratio\\stats_summary.csv'))

acl_best_algorithm_name_v04 = b'Quat Drop W Variable Vector3 Variable Clip RR:Rot|Trans Segment RR:Rot|Trans'
acl_best_algorithm_name = b'R:QuatNoWVar|T:Vec3Var|S:Vec3Var Clip RR:Rot|Trans|Scale Segment RR:Rot|Trans|Scale'

output_csv_data_ratios = []
output_csv_data_ratios_by_raw_size = []
output_csv_data_max_errors = []
output_csv_data_max_errors_by_raw_size = []
output_csv_data_ratio_vs_max_error = []
output_csv_data_durations = []
output_csv_headers = []

for (header, columns_to_extract, input_csv_file_path) in input_csv_files:
	csv_data = numpy.loadtxt(input_csv_file_path, delimiter=',', dtype=input_data_type_def, skiprows=1, usecols=columns_to_extract)
	if header.startswith('ACL v0.4'):
		acl_best_variable_data_mask = csv_data['algorithm_names'] == acl_best_algorithm_name_v04
		csv_data = csv_data[acl_best_variable_data_mask]
	elif header.startswith('ACL'):
		acl_best_variable_data_mask = csv_data['algorithm_names'] == acl_best_algorithm_name
		csv_data = csv_data[acl_best_variable_data_mask]

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

	output_csv_headers.append(header)

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
