# coding: utf-8

import sys

# 0_4_1_5_2_6_3_7
# We read: 0, 2, 4, 6, 1, 3, 5, 7
# In binary: b000, b010, b100, b110, b001, b011, b101, b111

# 0  -> 0
# 1  -> 2
# 2  -> 4
# 3  -> 6
# 4  -> 1
# 5  -> 3
# 6  -> 5
# 7  -> 7
# 8  -> 0
# 9  -> 2
# 10 -> 4
# ...

def rotate_right(value, offset):
	return ((value >> offset) | (value << (-offset & 31))) & 0xFFFFFFFF

if __name__ == "__main__":
	index = 0
	iter = 0
	lut = 0x75316420
	lut_2 = 0x75316420

	while (iter < 35):
		next_index = index + 1

		# 7 instructions
		# and + shl
		read_index_0 = (index & 0x07) << 1
		# shr
		overflow_bit = read_index_0 >> 3
		# xor + and
		read_index_1 = (read_index_0 ^ overflow_bit) & 0x07
		# and + add
		read_index = (index & ~0x07) + read_index_1

		print('{} = {} ({}, {}, {})'.format(index, read_index, read_index_0, overflow_bit, read_index_1))

		# 6 instructions (requires LUT to remain in register)
		# and + shl
		read_index_0 = (index & 0x07) << 2
		# shr + and
		read_index_1 = (lut >> read_index_0) & 0x0F
		# and + add
		read_index = (index & ~0x07) + read_index_1

		print('{} = {} ({}, {})'.format(index, read_index, read_index_0, read_index_1))

		# 4 instructions (requires LUT to remain in register)
		# Viable candidate, both AND instructions can execute in parallel
		# followed by ROR and ADD which can execute in parallel
		# Total overhead should be 2 cycles per consumed rotation
		# Could be folded in with % 32 that exists and is otherwise needed
		# We can retain bits 4 and 5 while masking out the other bits instead
		# of using ~0x07, we use 0x18 (%32 is & 0x1F, ~0x07 is 0xF8, if we AND both we get 0x18)
		# and
		read_index_0 = lut_2 & 0x0F
		# ror
		lut_2 = rotate_right(lut_2, 4)
		# and, add
		read_index = (index & ~0x07) + read_index_0

		print('{} = {} ({}, 0x{:08X})'.format(index, read_index, read_index_0, lut_2))

		index = next_index
		iter += 1
