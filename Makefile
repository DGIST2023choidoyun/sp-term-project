LDFLAGS := -ljansson

DEBUG_FLAGS   := -DDEBUG

ASAN_FLAGS    := -g -O0 -fsanitize=address

.PHONY: all release debug test run clean board run_board

all: release

release:
	sudo g++ -I. -I./include client.c board.c -L./lib -lpthread -lrgbmatrix -lrt -o client $(LDFLAGS)

debug:
	sudo g++ -I. -I./include client.c board.c -L./lib -lpthread -lrgbmatrix -lrt -o client $(LDFLAGS) $(DEBUG_FLAGS)

test:
	sudo g++ ${ASAN_FLAGS} -I. -I./include client.c board.c -L./lib -lpthread -lrgbmatrix -lrt -o client $(LDFLAGS) -lpthread $(DEBUG_FLAGS)

run:
	@./client -ip 10.8.128.233 -port 8080 -username sujaehado

clean:
	rm -f client

board:
	g++ -DSTANDALONE -I. -I./include -L./lib board.c -lpthread -lrgbmatrix -lrt -o board
run_board:
	sudo ./board
