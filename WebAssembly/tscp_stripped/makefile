OBJECT_FILES = \
	board.o \
	data.o \
	eval.o \
	main.o \
	search.o

all: wasm

tscp: $(OBJECT_FILES)
	g++ -O3 -o tscp $(OBJECT_FILES)

%.o: %.c data.h defs.h protos.h
	g++ -O3 -x c -c $< -o $@

wasm:
	emcc *.c -O0 -s STANDALONE_WASM=1 --no-entry -s EXPORTED_FUNCTIONS='["_one_move"]' -o chess.wasm

clean:
	rm -f *.o
	rm -f tscp
	rm -f chess.wasm
