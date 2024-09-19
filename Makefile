# Compiler for C++ and C
CPP = g++
CC = gcc

# Compiler flags
CPP_FLAGS = -std=c++11 -Wall -g -pthread
CC_FLAGS = -Wall -g

# Targets
all: test client server

# Compiling C code for ncurses-based test
main_curses.o: main_curses.c
	$(CC) $(CC_FLAGS) -I. -c main_curses.c

# Compiling C++ client code
client.o: client.cpp
	$(CPP) $(CPP_FLAGS) -c client.cpp

# Compiling C++ server code
server.o: server.cpp
	$(CPP) $(CPP_FLAGS) -c server.cpp

# Linking the test executable
test: main_curses.o
	$(CC) $(CC_FLAGS) -I./ main_curses.o -lncurses -o test

# Linking the C++ client executable
client: client.o
	$(CPP) $(CPP_FLAGS) -o cchat client.o

# Linking the C++ server executable
server: server.o
	$(CPP) $(CPP_FLAGS) -o cserverd server.o

# Cleaning up object files and executables
clean:
	rm -f *.o test cserverd cchat
