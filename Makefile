CC = clang
CFLAGS = -g -I/opt/homebrew/include -Wall -Werror -Wno-unused-function -Wno-unused-parameter -Wno-unused-variable
LFLAGS = -L/opt/homebrew/lib -lglfw

build: bin/playground

bin/playground: src/main.c
	$(CC) $(CFLAGS) $(LFLAGS) $< -o $@

run: build
	bin/playground

clean:
	rm -rf bin
	mkdir bin

.PHONY: build run clean
