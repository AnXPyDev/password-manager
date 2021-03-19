CC=gcc
CCARGS=

main: main.o
	$(CC) build/main.o -o build/password-manager

main.o: src/main.c
	$(CC) $(CCARGS) -c src/main.c -o build/main.o

init:
	mkdir -p build

all: main

clean:
	rm -r build
