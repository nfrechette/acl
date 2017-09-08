import numpy
import time

def format_elapsed_time(elapsed_time):
	hours, rem = divmod(elapsed_time, 3600)
	minutes, seconds = divmod(rem, 60)
	return '{:0>2}h {:0>2}m {:05.2f}s'.format(int(hours), int(minutes), seconds)

input_data_type_def = { 'names': ('clip_names', 'errors'), 'formats': ('S128', 'f4') }
columns_to_extract = (0, 3)

output_csv_file_path = 'D:\\acl-dev\\tools\\graph_generation\\full_errors.csv'
output_csv_file_path_top10 = 'D:\\acl-dev\\tools\\graph_generation\\full_errors_top10.csv'

input_csv_files =  []
input_csv_files.append(('ACL 0.4 @ 0.01cm', 'D:\\test_animations\\carnegie-mellon-acl-stats-0.4.0-error\\stats_error.csv'))
input_csv_files.append(('UE 4.15 @ 0.01cm', 'D:\\test_animations\\carnegie-mellon-ue4-stats-0.4.0-0.01\\stats_error.csv'))
input_csv_files.append(('UE 4.15 @ 0.1cm', 'D:\\test_animations\\carnegie-mellon-ue4-stats-0.4.0-0.1\\stats_error.csv'))
#input_csv_files.append(('UE 4.15 @ 1.0cm', 'D:\\test_animations\\carnegie-mellon-ue4-stats-0.4.0-1.0\\stats_error.csv'))

desired_percentiles = [x * 0.1 for x in range(0, 1001)]
desired_percentiles_top10 = [90.0 + (x * 0.01) for x in range(0, 1001)]

output_csv_data = []
output_csv_data_top10 = []
output_csv_headers = []

for (header, input_csv_file_path) in input_csv_files:
	print('Parsing {} ...'.format(header))
	parsing_start_time = time.clock();
	csv_data = numpy.loadtxt(input_csv_file_path, delimiter=',', dtype=input_data_type_def, skiprows=1, usecols=columns_to_extract)
	parsing_end_time = time.clock();
	print('Parsed {} ({}) rows in {}'.format(header, len(csv_data['errors']), format_elapsed_time(parsing_end_time - parsing_start_time)))

	percentiles = numpy.percentile(csv_data['errors'], desired_percentiles)
	percentiles_top10 = numpy.percentile(csv_data['errors'], desired_percentiles_top10)
	output_csv_data.append(percentiles)
	output_csv_data_top10.append(percentiles_top10)
	output_csv_headers.append(header)

output_csv_data = numpy.column_stack(output_csv_data)
output_csv_data_top10 = numpy.column_stack(output_csv_data_top10)

with open(output_csv_file_path, 'wb') as f:
	header = bytes('{}\n'.format(','.join(output_csv_headers)), 'utf-8')
	f.write(header)
	numpy.savetxt(f, output_csv_data, delimiter=',', fmt=('%f'))

with open(output_csv_file_path_top10, 'wb') as f:
	header = bytes('{}\n'.format(','.join(output_csv_headers)), 'utf-8')
	f.write(header)
	numpy.savetxt(f, output_csv_data_top10, delimiter=',', fmt=('%f'))
