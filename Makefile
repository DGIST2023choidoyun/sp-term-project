CC      := gcc
CFLAGS  := 
LDFLAGS := -ljansson

DEBUG_FLAGS   := -DDEBUG

ASAN_FLAGS    := -g -O0 -fsanitize=address

.PHONY: all release debug test run clean board run_board

all: release

release:
	$(CC) $(CFLAGS) client.c board.c -o client $(LDFLAGS)

debug:
	$(CC) $(CFLAGS) client.c board.c -o client $(LDFLAGS) $(DEBUG_FLAGS)

test:
	$(CC) $(CFLAGS) $(ASAN_FLAGS) client.c board.c -o client $(LDFLAGS) -lpthread $(DEBUG_FLAGS)

run:
	@./client -ip 10.8.128.233 -port 8080 -username sujaehado

clean:
	rm -f client

board:
	g++ -DSTANDALONE -I. -I./include -L./lib board.c -lpthread -lrgbmatrix -lrt -o board
run_board:
	sudo ./board
