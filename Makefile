CC=gcc
CFLAGS=-Wall -Wextra -O2
LDFLAGS=-lncursesw

OBJS=main.o editor.o generator.o

testgen: $(OBJS)
	$(CC) -o testgen $(OBJS) $(LDFLAGS)

main.o: main.c testgen.h
	$(CC) $(CFLAGS) -c main.c

editor.o: editor.c testgen.h
	$(CC) $(CFLAGS) -c editor.c

generator.o: generator.c testgen.h
	$(CC) $(CFLAGS) -c generator.c

clean:
	rm -f $(OBJS) testgen test_generated.sh
