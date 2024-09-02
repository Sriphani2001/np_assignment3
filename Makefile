CC = gcc
CC_FLAGS = -w -g

all: test client server

main_curses.o: main_curses.c
	$(CC) -Wall -I. -c main_curses.c

client.o: client.c
	$(CC) -Wall -c client.c

server.o: server.c
	$(CC) -Wall -c server.c

test: main_curses.o
	$(CC) -I./ -Wall main_curses.o -lncurses -o test 

client: client.o
	$(CC) -Wall -o cchat client.o

server: server.o
	$(CC) -Wall -o cserverd server.o

clean:
	rm -f *.o test cserverd cchat


