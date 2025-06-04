DEBUG_FLAGS   := -DDEBUG

ASAN_FLAGS    := -g -O0 -fsanitize=address

.PHONY: all release debug test run clean board run_board

all: release

release:
	sudo g++ -I. -I./include -L./lib board.c client.c -o client -ljansson -lpthread -lrgbmatrix -lrt
debug:
	sudo g++ -I. -I./include  -L./lib board.c client.c -o client -ljansson -lpthread -lrgbmatrix -lrt $(DEBUG_FLAGS)
test:
	sudo g++ -I. -I./include  -L./lib board.c client.c -o client -ljansson -lpthread -lrgbmatrix -lrt $(DEBUG_FLAGS) $(ASAN_FLAGS)
run:
	@./client -ip 10.8.128.233 -port 8080 -username sujaehado
clean:
	rm -f client
board:
	g++ -DSTANDALONE -I. -I./include -L./lib board.c -lpthread -lrgbmatrix -lrt -o board
run_board:
	sudo ./board
