
CC := gcc
CFLAGS := -O2
LDFLAGS := -Wl,-O1 -Wl,--hash-style=gnu

TARGETS := libinterpose_memcpy.so \
           libinterpose_syscall.so

all: $(TARGETS)

libinterpose_%.so: %.c
	$(CC) $(CFLAGS) $(LDFLAGS) -fPIC -Wall -Werror -std=c99 -shared -o $@ $^ -ldl

clean:
	rm -f $(TARGETS)
