CC = cc

all: local remote http 

debug: CFLAGS = -g -O0 -Wall -WExtra -Werror
debug: all

pre-build:
	mkdir -p dist

dist/%.o: %.c lib.h
	$(CC) $(CFLAGS) -c -o $@ $<

http: pre-build
	make dist/http

dist/http: dist/http.o dist/lib.o
	$(CC) -o $@ $^ -pthread

remote: pre-build 
	make dist/remote

dist/remote: remote.c
	$(CC) $(CFLAGS) -o $@ $^ -luv

local: pre-build 
	make dist/local

dist/local: local.c
	$(CC) $(CFLAGS) -o $@ $^ -luv

clean:
	rm -rf dist
