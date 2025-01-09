from PIL import Image
from sys import argv

if len(argv) <= 1:
	print("File missing...")
	exit(1)

with Image.open(argv[1]) as im:
	pixels = im.getdata()
	if pixels is not None:
		with open(argv[1].replace(".png", ".bin"), "wb") as f:
			f.write(int.to_bytes(im.width, length=4, byteorder='little', signed=False))
			f.write(int.to_bytes(im.height, length=4, byteorder='little', signed=False))
			for pixel in pixels:
				for i in range(4):
					f.write(int.to_bytes(pixel[i], length=1))