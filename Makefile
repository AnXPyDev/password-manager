CC=gcc
CCARGS=

main: main.o arg.o
	$(CC) build/main.o build/arg.o -o build/pwman

main.o: src/main.c
	$(CC) $(CCARGS) -c src/main.c -o build/main.o

arg.o: src/arg.c
	$(CC) $(CCARGS) -c src/arg.c -o build/arg.o

init:
	mkdir -p build

all: main

clean:
	rm -r build
