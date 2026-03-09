
SRC_FOLDER=src
CHESS_FOLDER=tscp_stripped
C_LD=emcc
C_LDFLAGS= -s STANDALONE_WASM=1 --no-entry -s EXPORTED_FUNCTIONS='["_func"]'
CHESS_LDFLAGS = -s STANDALONE_WASM=1 --no-entry -s EXPORTED_FUNCTIONS='["_one_move"]'

WAT_LD=wat2wasm
OUTPUT_FOLDER =WASM_files


all: wat c_files


wat: ref_fingerprints primes montgom memops
c_files: ref_calls rec_c aes chess wasmmark


ref_fingerprints: $(OUTPUT_FOLDER)/ref_fingerprints.wasm
$(OUTPUT_FOLDER)/ref_fingerprints.wasm:
	@mkdir -p $(OUTPUT_FOLDER)
	$(WAT_LD) $(SRC_FOLDER)/fingerprints.wat -o $(OUTPUT_FOLDER)/ref_fingerprints.wasm

primes: $(OUTPUT_FOLDER)/primes.wasm
$(OUTPUT_FOLDER)/primes.wasm:
	@mkdir -p $(OUTPUT_FOLDER)
	$(WAT_LD) $(SRC_FOLDER)/primes.wat -o $(OUTPUT_FOLDER)/primes.wasm

montgom:$(OUTPUT_FOLDER)/montgom.wasm
$(OUTPUT_FOLDER)/montgom.wasm:
	@mkdir -p $(OUTPUT_FOLDER)
	$(WAT_LD) $(SRC_FOLDER)/montgom.wat -o $(OUTPUT_FOLDER)/montgom.wasm

memops: $(OUTPUT_FOLDER)/memops.wasm
$(OUTPUT_FOLDER)/memops.wasm:
	@mkdir -p $(OUTPUT_FOLDER)
	$(WAT_LD) $(SRC_FOLDER)/memops.wat -o $(OUTPUT_FOLDER)/memops.wasm

ref_calls: $(OUTPUT_FOLDER)/ref_calls.wasm
$(OUTPUT_FOLDER)/ref_calls.wasm:
	@mkdir -p $(OUTPUT_FOLDER)
	$(C_LD) -O1 $(C_LDFLAGS) $(SRC_FOLDER)/calls.c -o $(OUTPUT_FOLDER)/ref_calls.wasm

rec_c: $(OUTPUT_FOLDER)/ref_c.wasm
$(OUTPUT_FOLDER)/ref_c.wasm:
	@mkdir -p $(OUTPUT_FOLDER)
	$(C_LD) -O1 $(C_LDFLAGS) $(SRC_FOLDER)/c_ref.c -o $(OUTPUT_FOLDER)/ref_c.wasm

aes: $(OUTPUT_FOLDER)/aes.wasm
$(OUTPUT_FOLDER)/aes.wasm: tiny-AES-c
	@mkdir -p $(OUTPUT_FOLDER)
	$(C_LD) -O0 $(C_LDFLAGS) $(SRC_FOLDER)/aes.c tiny-AES-c/aes.c -o $(OUTPUT_FOLDER)/aes.wasm
	$(C_LD) -O1 $(C_LDFLAGS) $(SRC_FOLDER)/aes.c tiny-AES-c/aes.c -o $(OUTPUT_FOLDER)/aes_O1.wasm
	$(C_LD) -O2 $(C_LDFLAGS) $(SRC_FOLDER)/aes.c tiny-AES-c/aes.c -o $(OUTPUT_FOLDER)/aes_O2.wasm
	$(C_LD) -O3 $(C_LDFLAGS) $(SRC_FOLDER)/aes.c tiny-AES-c/aes.c -o $(OUTPUT_FOLDER)/aes_O3.wasm

chess: $(OUTPUT_FOLDER)/chess.wasm
$(OUTPUT_FOLDER)/chess.wasm:
	@mkdir -p $(OUTPUT_FOLDER)
	$(C_LD) -O0 $(CHESS_LDFLAGS) $(CHESS_FOLDER)/*.c -o $(OUTPUT_FOLDER)/chess.wasm
	$(C_LD) -O1 $(CHESS_LDFLAGS) $(CHESS_FOLDER)/*.c -o $(OUTPUT_FOLDER)/chess_O1.wasm
	$(C_LD) -O2 $(CHESS_LDFLAGS) $(CHESS_FOLDER)/*.c -o $(OUTPUT_FOLDER)/chess_O2.wasm
	$(C_LD) -O3 $(CHESS_LDFLAGS) $(CHESS_FOLDER)/*.c -o $(OUTPUT_FOLDER)/chess_O3.wasm

wasmmark: $(OUTPUT_FOLDER)/coremark.wasm
$(OUTPUT_FOLDER)/coremark.wasm: coremark
	@mkdir -p $(OUTPUT_FOLDER)
	$(C_LD) -O2 -I coremark/linux -I coremark/posix -I coremark \
	-DFLAGS_STR=\""-O2 -DPERFORMANCE_RUN=1  -lrt"\" -DITERATIONS=0 -DPERFORMANCE_RUN=1 \
	coremark/core_list_join.c coremark/core_matrix.c coremark/core_util.c coremark/posix/core_portme.c coremark/core_state.c \
	src/coremark.c -s STANDALONE_WASM=1 --no-entry -s EXPORTED_FUNCTIONS='["_func"]' -o $(OUTPUT_FOLDER)/coremark.wasm -lrt


tiny-AES-c:
	@echo ">>> Checking out https://github.com/kokke/tiny-AES-c.git..."
	git clone https://github.com/kokke/tiny-AES-c.git tiny-AES-c

coremark:
	@echo ">>> Checking out https://github.com/eembc/coremark..."
	git clone https://github.com/eembc/coremark coremark


clean:
	rm -rf WASM_files
	rm -rf tiny-AES-c
	rm -rf coremark
	rm -rf logs
