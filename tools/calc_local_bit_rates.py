# coding: utf-8

import sys

k_bit_rate_num_bits = [ 0, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 32 ]

k_highest_bit_rate = len(k_bit_rate_num_bits) - 1
k_lowest_bit_rate = 1
k_num_bit_rates = len(k_bit_rate_num_bits)
k_invalid_bit_rate = 255

# This code assumes that rotations, translations, and scales are packed on 3 components (e.g. quat drop w)

if __name__ == "__main__":
	if sys.version_info < (3, 4):
		print('Python 3.4 or higher needed to run this script')
		sys.exit(1)

	permutation_tries = []
	permutation_tries_no_scale = []

	for rotation_bit_rate in range(k_num_bit_rates):
		for translation_bit_rate in range(k_num_bit_rates):
			transform_size = k_bit_rate_num_bits[rotation_bit_rate] * 3 + k_bit_rate_num_bits[translation_bit_rate] * 3
			permutation_tries_no_scale.append((transform_size, rotation_bit_rate, translation_bit_rate))

			for scale_bit_rate in range(k_num_bit_rates):
				transform_size = k_bit_rate_num_bits[rotation_bit_rate] * 3 + k_bit_rate_num_bits[translation_bit_rate] * 3 + k_bit_rate_num_bits[scale_bit_rate] * 3
				permutation_tries.append((transform_size, rotation_bit_rate, translation_bit_rate, scale_bit_rate))

	# Sort by transform size, then by each bit rate
	permutation_tries.sort()
	permutation_tries_no_scale.sort()

	print('constexpr uint8_t k_local_bit_rate_permutations_no_scale[{}][2] ='.format(len(permutation_tries_no_scale)))
	print('{')
	for transform_size, rotation_bit_rate, translation_bit_rate in permutation_tries_no_scale:
		print('\t{{ {}, {} }},\t\t// {} bits per transform'.format(rotation_bit_rate, translation_bit_rate, transform_size))
	print('};')
	print()
	print('constexpr uint8_t k_local_bit_rate_permutations[{}][3] ='.format(len(permutation_tries)))
	print('{')
	for transform_size, rotation_bit_rate, translation_bit_rate, scale_bit_rate in permutation_tries:
		print('\t{{ {}, {}, {} }},\t\t// {} bits per transform'.format(rotation_bit_rate, translation_bit_rate, scale_bit_rate, transform_size))
	print('};')
