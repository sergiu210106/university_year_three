CC = gcc
CFLAGS = -Wall -Wextra -O2 -fPIC

all: libst.so

libst.so: st.c st.h
	$(CC) $(CFLAGS) -shared -o $@ st.c

clean:
	rm -f libst.so PIF.out ST.out

.PHONY: all clean
