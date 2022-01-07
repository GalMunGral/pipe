CC = cc

all: local remote http

debug: CFLAGS = -g -O0 -Wall -Wextra -Werror
debug: all

http: dist/http.o dist/lib.o
	${CC} -o dist/http dist/http.o dist/lib.o -pthread

local: dist/local.o dist/lib.o
	${CC} -o dist/local dist/local.o dist/lib.o -pthread

remote: dist/remote.o dist/lib.o
	${CC} -o dist/remote dist/remote.o dist/lib.o -pthread

dist/%.o: %.c lib.h
	${CC} ${CFLAGS} -c -o $@ $<

clean:
	rm -rf dist/*

