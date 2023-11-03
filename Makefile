src = $(wildcard src/*.c)
obj = $(src:.c=.o) src/data.o
dep = $(src:.c=.d)
bin = termu

CFLAGS = -pedantic -Wall -g -DMINIGLUT_USE_LIBC -D_XOPEN_SOURCE=500 -MMD
LDFLAGS = -lGL -lGLU -lX11 -limago -lm

$(bin): $(obj)
	$(CC) -o $@ $(obj) $(LDFLAGS)

-include $(dep)

src/data.o: src/data.s data/adm3l.rom data/adm3u.rom data/adm3a.jpg

.PHONY: clean
clean:
	rm -f $(obj) $(bin)

.PHONY: cleandep
cleandep:
	rm -f $(dep)
