# coding: utf-8

import sys

# Can we use Z-curve ordering to pack segment data?
metadata = ['segment0_metadata_group0', 'segment0_metadata_group1', 'segment0_metadata_group2', 'segment1_metadata_group0', 'segment1_metadata_group1', 'segment1_metadata_group2', 'segment2_metadata_group0', 'segment2_metadata_group1', 'segment2_metadata_group2']
range_data = ['segment0_range_group0', 'segment0_range_group1', 'segment0_range_group2', 'segment1_range_group0', 'segment1_range_group1', 'segment1_range_group2', 'segment2_range_group0', 'segment2_range_group1', 'segment2_range_group2']
animated_data_segment0 = ['segment0_animated0_group0', 'segment0_animated0_group1', 'segment0_animated0_group2', 'segment0_animated1_group0', 'segment0_animated1_group1', 'segment0_animated1_group2', 'segment0_animated2_group0', 'segment0_animated2_group1', 'segment0_animated2_group2']
animated_data_segment1 = ['segment1_animated0_group0', 'segment1_animated0_group1', 'segment1_animated0_group2', 'segment1_animated1_group0', 'segment1_animated1_group1', 'segment1_animated1_group2', 'segment1_animated2_group0', 'segment1_animated2_group1', 'segment1_animated2_group2']
animated_data_segment2 = ['segment2_animated0_group0', 'segment2_animated0_group1', 'segment2_animated0_group2', 'segment2_animated1_group0', 'segment2_animated1_group1', 'segment2_animated1_group2', 'segment2_animated2_group0', 'segment2_animated2_group1', 'segment2_animated2_group2']

coords_one_segment = [
	(metadata[0], range_data[0], animated_data_segment0[0], animated_data_segment0[3]),
	(metadata[1], range_data[1], animated_data_segment0[1], animated_data_segment0[4]),
	(metadata[2], range_data[2], animated_data_segment0[2], animated_data_segment0[5]),
]

coords_two_segment = [
	(metadata[0], metadata[3], range_data[0], range_data[3], animated_data_segment0[6], animated_data_segment1[0]),
	(metadata[1], metadata[4], range_data[1], range_data[4], animated_data_segment0[7], animated_data_segment1[1]),
	(metadata[2], metadata[5], range_data[2], range_data[5], animated_data_segment0[8], animated_data_segment1[2]),
]

# Linear ideal access order
print('Linear ideal access order with one segment:')
for entry in coords_one_segment:
	print('  {}'.format(entry))
print()

print('Linear ideal access order with two segments:')
for entry in coords_two_segment:
	print('  {}'.format(entry))
print()

# We have three axes: the metadata/range axis and the keyframe axis A and B that we sample
# As we unpack with one segment, the metadata axis group offset increments linearly in lockstep with the keyframe axis values
# [0, 0, 0], [1, 1, 1], ...

# Maybe work out how it would work with just 1/2D: contiguous metadata and keyframe A?
