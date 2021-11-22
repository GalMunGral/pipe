CC = cc

debug: CFLAGS = -g -Wall -Wextra -Werror
debug: all

all: CFLAGS = -Wall -Wextra -Werror
all: local remote 

local: dist/local.o dist/lib.o
	${CC} -o dist/local dist/local.o dist/lib.o -pthread

remote: dist/remote.o dist/lib.o
	${CC} -o dist/remote dist/remote.o dist/lib.o -pthread

dist/%.o: %.c lib.h
	${CC} ${CFLAGS} -c -o $@ $<

clean:
	rm -rf dist/*

