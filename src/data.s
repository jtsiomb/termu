	.text

	.globl rom_upper
rom_upper:
	.incbin "data/adm3u.rom"

	.globl rom_lower
rom_lower:
	.incbin "data/adm3l.rom"

	.globl bezel_imgdata
bezel_imgdata:
	.incbin "data/adm3a.jpg"
	.globl bezel_imgdata_end
bezel_imgdata_end:
