# coding: utf-8

import sys

# ACL_BIT_RATE_EXPANSION: Added 1, 2, 20, 21, 22, and 23.
k_bit_rate_num_bits = [ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 32 ]

k_highest_bit_rate = len(k_bit_rate_num_bits) - 1
k_lowest_bit_rate = 1
k_num_bit_rates = len(k_bit_rate_num_bits)
k_invalid_bit_rate = 255

# This code assumes that rotations, translations, and scales are packed on 3 components (e.g. quat drop w)

if __name__ == "__main__":
	if sys.version_info < (3, 4):
		print('Python 3.4 or higher needed to run this script')
		sys.exit(1)

	permutation_dof_1 = []
	permutation_dof_2 = []
	permutation_dof_3 = []

	for dof_1 in range(k_num_bit_rates):
		dof_1_size = k_bit_rate_num_bits[dof_1] * 3;
		permutation_dof_1.append((dof_1_size, dof_1))

		for dof_2 in range(k_num_bit_rates):
			dof_2_size = dof_1_size + k_bit_rate_num_bits[dof_2] * 3
			permutation_dof_2.append((dof_2_size, dof_1, dof_2))

			for dof_3 in range(k_num_bit_rates):
				dof_3_size = dof_2_size + k_bit_rate_num_bits[dof_3] * 3
				permutation_dof_3.append((dof_3_size, dof_1, dof_2, dof_3))

	# Sort by transform size, then by each bit rate
	permutation_dof_1.sort()
	permutation_dof_2.sort()
	permutation_dof_3.sort()

	print('// Buffer size in bytes: {}'.format(len(permutation_dof_1) * 1));
	print('constexpr uint8_t k_local_bit_rate_permutations_1_dof[{}][1] ='.format(len(permutation_dof_1)))
	print('{')
	for transform_size, dof_1 in permutation_dof_1:
		print('\t{{ {} }},\t\t// {} bits per transform'.format(dof_1, transform_size))
	print('};')
	print()
	print('// Buffer size in bytes: {}'.format(len(permutation_dof_2) * 2));
	print('constexpr uint8_t k_local_bit_rate_permutations_2_dof[{}][2] ='.format(len(permutation_dof_2)))
	print('{')
	for transform_size, dof_1, dof_2 in permutation_dof_2:
		print('\t{{ {}, {} }},\t\t// {} bits per transform'.format(dof_1, dof_2, transform_size))
	print('};')
	print()
	print('// Buffer size in bytes: {}'.format(len(permutation_dof_3) * 3));
	print('constexpr uint8_t k_local_bit_rate_permutations_3_dof[{}][3] ='.format(len(permutation_dof_3)))
	print('{')
	for transform_size, dof_1, dof_2, dof_3 in permutation_dof_3:
		print('\t{{ {}, {}, {} }},\t\t// {} bits per transform'.format(dof_1, dof_2, dof_3, transform_size))
	print('};')
	print()
