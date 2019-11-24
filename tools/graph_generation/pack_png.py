import glob
import sys

# Note: This script depends on Pillow
from PIL import Image

if __name__ == "__main__":
	if sys.version_info < (3, 4):
		print('Python 3.4 or higher needed to run this script')
		sys.exit(1)

	if len(sys.argv) != 2:
		print('Usage: python pack_png.py <path to png image>')
		sys.exit(1)

	img_path = sys.argv[1]
	if not img_path.endswith('.png'):
		print('Expected a PNG image as input: {}'.format(img_path))
		sys.exit(1)

	for filename in glob.glob(img_path):
		with Image.open(filename, 'r') as img:
			filename = filename.replace('.png', '_packed.png')
			img.save(filename, 'PNG', optimize=True)
