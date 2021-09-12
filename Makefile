CC = cc

all: rosen

debug: CFLAGS = -g
debug: all

rosen: rosen.o session.o handler.o lib.h
	${CC} -o rosen rosen.o session.o handler.o

%.o: %.c
	${CC} ${CFLAGS} -c -o $@ $<

clean:
	rm -rf *.dSYM 
	rm -f *.o rosen

