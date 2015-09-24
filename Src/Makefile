CFLAGS = -std=c99 -D_GNU_SOURCE -Wall -g -O -m32
CC = gcc-4.8

#
#	make file for new leda interpreter
#

lc: y.tab.o lc.o gen.o interp.o types.o memory.o
	$(CC) $(CFLAGS) -o lc -p y.tab.o lc.o gen.o interp.o types.o memory.o

y.tab.o: memory.h lc.h interp.h y.tab.c lex.yy.c
lc.o: lc.h interp.h lc.c
interp.o: lc.h interp.h memory.h interp.c
memory.o: memory.h memory.c
gen.o: lc.h interp.h gen.c
types.o: lc.h types.c

y.tab.c: gram.y
	yacc gram.y

lex.yy.c: lexer.l
	flex lexer.l

clean:
	rm -f lex.yy.c y.tab.c *.o lc
