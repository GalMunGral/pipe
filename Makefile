CC = cc

all: CFLAGS = -Wall -Wextra -Werror
all: local remote 

debug: CFLAGS = -g -Wall -Wextra -Werror
debug: all

local: local.o lib.o
	${CC} -o local local.o lib.o -pthread

remote: remote.o lib.o
	${CC} -o remote remote.o lib.o -pthread

%.o: %.c lib.h
	${CC} ${CFLAGS} -c -o $@ $<

clean:
	rm -rf *.dSYM 
	rm -f *.o remote local 

