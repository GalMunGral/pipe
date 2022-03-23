CC = cc

all: local remote http uv_remote uv_local

debug: CFLAGS = -g -O0 -Wall -Wextra -Werror
debug: all

http: dist/http.o dist/lib.o
	${CC} -o dist/http dist/http.o dist/lib.o -pthread

local: dist/local.o dist/lib.o
	${CC} -o dist/local dist/local.o dist/lib.o -pthread

remote: dist/remote.o dist/lib.o
	${CC} -o dist/remote dist/remote.o dist/lib.o -pthread

dist/%.o: %.c lib.h
	mkdir -p dist
	${CC} ${CFLAGS} -c -o $@ $<

uv_remote: CFLAGS = -g -O0
uv_remote: uv_remote.c
	$(CC) $(CFLAGS) -o dist/$@ $^ -luv

uv_local: CFLAGS = -g -O0
uv_local: uv_local.c
	$(CC) $(CFLAGS) -o dist/$@ $^ -luv

clean:
	rm -rf dist/*

