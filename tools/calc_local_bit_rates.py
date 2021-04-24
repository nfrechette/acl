# coding: utf-8

import sys

# ACL_BIT_RATE: Added 1, 2, 20, and 21.  TODO: 22, 23, and 24 fail regression tests.
k_bit_rate_num_bits = [ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 32 ]

k_highest_bit_rate = len(k_bit_rate_num_bits) - 1 # ACL_BIT_RATE: 18
k_lowest_bit_rate = 1
k_num_bit_rates = len(k_bit_rate_num_bits) # ACL_BIT_RATE: 19
k_invalid_bit_rate = 255

# This code assumes that rotations, translations, and scales are packed on 3 components (e.g. quat drop w)

if __name__ == "__main__":
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
