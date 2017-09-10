import glob
import os
import sys

# Note: This script depends on Pillow
from PIL import Image

if __name__ == "__main__":
	if len(sys.argv) != 2:
		print('Usage: python pack_png.py <path to png image>')
		sys.exit(1)

	img_path = sys.argv[1]
	if not img_path.endswith('.png'):
		print('Expected a PNG image as input: {}'.format(img_path))
		sys.exit(1)

	for filename in glob.glob(img_path):
		with Image.open(filename, 'r') as img:
			file_path_no_ext, file_ext = os.path.splitext(filename)
			img.save(filename, 'PNG', optimize=True)
