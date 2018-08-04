
#uint32_t byte_offset = bit_offset / 8;
#bit_offset += num_bits;
#byte_offset = bit_offset / 8;
#bit_offset += num_bits;
#byte_offset = bit_offset / 8;

print('static constexpr uint8_t byte_offset_values[8][19][2] =')
print('{')
for bit_offset in range(0, 8):
	triplets = []
	for num_bits in range(0, 19):
		values = []
		byte_offset = 0
		#values.append(str(byte_offset))
		bit_offset += num_bits
		byte_offset = bit_offset // 8
		values.append(str(byte_offset))
		bit_offset += num_bits
		byte_offset = bit_offset // 8
		values.append(str(byte_offset))

		triplets.append('{{ {} }}'.format(', '.join(values)))

	print('\t{{ {} }},'.format(', '.join(triplets)))

print('};')
