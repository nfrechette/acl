
#const uint32_t bit_shift = 32 - num_bits;
#const uint32_t x32 = (vector_u32 >> (bit_shift - (bit_offset % 8)));
#bit_offset += num_bits;
#const uint32_t y32 = (vector_u32 >> (bit_shift - (bit_offset % 8)));
#bit_offset += num_bits;
#const uint32_t z32 = (vector_u32 >> (bit_shift - (bit_offset % 8)));

print('static constexpr uint8_t shift_values[8][20][3] =')
print('{')
for offset in range(0, 8):
	triplets = []
	for num_bits in range(0, 20):
		values = []
		bit_offset = offset
		bit_shift = 32 - num_bits
		shift = bit_shift - (bit_offset % 8)
		values.append(str(shift))

		bit_offset += num_bits
		shift = bit_shift - (bit_offset % 8)
		values.append(str(shift))

		bit_offset += num_bits
		shift = bit_shift - (bit_offset % 8)
		values.append(str(shift))

		triplets.append('{{ {} }}'.format(', '.join(values)))

	print('\t{{ {} }},'.format(', '.join(triplets)))

print('};')
