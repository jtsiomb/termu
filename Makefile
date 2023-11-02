src = $(wildcard src/*.c)
obj = $(src:.c=.o) src/data.o
dep = $(src:.c=.d)
bin = termu

CFLAGS = -pedantic -Wall -g -DMINIGLUT_USE_LIBC -D_XOPEN_SOURCE=500 -MMD
LDFLAGS = -lGL -lX11 -lm

$(bin): $(obj)
	$(CC) -o $@ $(obj) $(LDFLAGS)

-include $(dep)

.PHONY: clean
clean:
	rm -f $(obj) $(bin)

.PHONY: cleandep
cleandep:
	rm -f $(dep)
