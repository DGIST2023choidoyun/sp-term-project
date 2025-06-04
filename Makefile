DEBUG_FLAGS   := -DDEBUG

ASAN_FLAGS    := -g -O0 -fsanitize=address

.PHONY: all release debug test run clean board run_board client

all: release

release:
	sudo apt install libjansson-dev
	sudo g++ -I. -I./include -L./lib board.c client.c -o client -ljansson -lpthread -lrgbmatrix -lrt
debug:
	sudo apt install libjansson-dev
	sudo g++ -I. -I./include  -L./lib board.c client.c -o client -ljansson -lpthread -lrgbmatrix -lrt $(DEBUG_FLAGS)
test:
	sudo apt install libjansson-dev
	sudo g++ -I. -I./include  -L./lib board.c client.c -o client -ljansson -lpthread -lrgbmatrix -lrt $(DEBUG_FLAGS) $(ASAN_FLAGS)
run:
	@./client -ip 10.8.128.233 -port 8080 -username sujaehado
clean:
	rm -f client
client:
	sudo apt install libjansson-dev
	sudo gcc client.c -o client -ljansson -lpthread -DCLIENT_ONLY
board:
	sudo g++ -DSTANDALONE -I. -I./include -L./lib board.c -lpthread -lrgbmatrix -lrt -o board
run_board:
	sudo ./board
