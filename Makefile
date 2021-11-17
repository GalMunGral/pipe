CC = cc

all: CFLAGS = -Wall -Wextra -Werror
all: local remote socks5

debug: CFLAGS = -g -Wall -Wextra -Werror
debug: all

local: local.o 
	${CC} -o local local.o

remote: remote.o 
	${CC} -o remote remote.o

socks5: socks5.o
	${CC} -o socks5 socks5.o

%.o: %.c
	${CC} ${CFLAGS} -c -o $@ $<

clean:
	rm -rf *.dSYM 
	rm -f *.o remote local socks5

