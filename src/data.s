	.text

	.globl rom_upper
rom_upper:
	.incbin "data/adm3u.rom"

	.globl rom_lower
rom_lower:
	.incbin "data/adm3l.rom"
