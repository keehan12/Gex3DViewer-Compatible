from PIL import Image
from sys import argv

if len(argv) <= 1:
	print("File missing...")
	exit(1)

def ToU32(pixel: tuple[int, int, int, int]):
	return (pixel[0] << 24) | (pixel[1] << 16) | (pixel[2] << 8) | (pixel[3])

# Basic RLE
def TryCompress(datain: bytearray):
	dataout = bytearray()
	b = datain[0]
	count = 0
	written = True
	for i in range(len(datain)):
		written = False
		if b == datain[i]:
			count += 1
			
		if b != datain[i] or count == 255:
			if count > 0:
				dataout.extend(bytes([count, b]))
			count = 0 if b == datain[i] else 1
			b = datain[i]
			written = True
	
	if not written:
		if count > 0:
			dataout.extend(bytes([count, b]))
	return dataout

with Image.open(argv[1]) as im:
	pixels = im.getdata()
	if pixels is not None:
		palette = []
		ba = bytearray()
		for pixel in pixels:
			p = ToU32(pixel)
			if p not in palette:
				palette.append(p)
			ba.extend(int.to_bytes(palette.index(p), 1))
		
		with open(argv[1].replace(".png", ".bin"), "wb") as f:
			f.write(int.to_bytes(im.width, length=4, byteorder='little', signed=False))
			f.write(int.to_bytes(im.height, length=4, byteorder='little', signed=False))

			f.write(int.to_bytes(len(palette), 2, 'little')) # 65535 colors should be sufficient
			for p in palette:
				f.write(int.to_bytes(p, 4, 'little'))

			cp = TryCompress(ba)
			if (4 + len(cp)) < len(ba):
				f.write(int.to_bytes(1, 1))
				f.write(int.to_bytes(len(cp), 4, 'little'))
				f.write(cp)
			else:
				f.write(int.to_bytes(0, 1))
				f.write(ba)