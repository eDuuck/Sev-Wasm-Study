
SRC_FOLDER=src
CHESS_FOLDER=tscp_stripped
C_LD=emcc
C_LDFLAGS= -s STANDALONE_WASM=1 --no-entry -s EXPORTED_FUNCTIONS='["_func"]'
CHESS_LDFLAGS = -s STANDALONE_WASM=1 --no-entry -s EXPORTED_FUNCTIONS='["_one_move"]'

WAT_LD=wat2wasm
OUTPUT_FOLDER =WASM_files


all: wat c_files


wat: ref_fingerprints primes montgom
c_files: ref_calls rec_c aes chess


ref_fingerprints:
	@mkdir -p $(OUTPUT_FOLDER)
	$(WAT_LD) $(SRC_FOLDER)/fingerprints.wat -o $(OUTPUT_FOLDER)/ref_fingerprints.wasm

primes:
	@mkdir -p $(OUTPUT_FOLDER)
	$(WAT_LD) $(SRC_FOLDER)/primes.wat -o $(OUTPUT_FOLDER)/primes.wasm

montgom:
	@mkdir -p $(OUTPUT_FOLDER)
	$(WAT_LD) $(SRC_FOLDER)/montgom.wat -o $(OUTPUT_FOLDER)/montgom.wasm

ref_calls:
	@mkdir -p $(OUTPUT_FOLDER)
	$(C_LD) -O0 $(C_LDFLAGS) $(SRC_FOLDER)/calls.c -o $(OUTPUT_FOLDER)/ref_calls.wasm

rec_c:
	@mkdir -p $(OUTPUT_FOLDER)
	$(C_LD) -O0 $(C_LDFLAGS) $(SRC_FOLDER)/c_ref.c -o $(OUTPUT_FOLDER)/ref_c.wasm

aes: tiny-AES-c
	@mkdir -p $(OUTPUT_FOLDER)
	$(C_LD) -O0 $(C_LDFLAGS) $(SRC_FOLDER)/aes.c tiny-AES-c/aes.c -o $(OUTPUT_FOLDER)/aes.wasm
	$(C_LD) -O1 $(C_LDFLAGS) $(SRC_FOLDER)/aes.c tiny-AES-c/aes.c -o $(OUTPUT_FOLDER)/aes_O1.wasm
	$(C_LD) -O2 $(C_LDFLAGS) $(SRC_FOLDER)/aes.c tiny-AES-c/aes.c -o $(OUTPUT_FOLDER)/aes_O2.wasm

chess:
	@mkdir -p $(OUTPUT_FOLDER)
	$(C_LD) -O0 $(CHESS_LDFLAGS) $(CHESS_FOLDER)/*.c -o $(OUTPUT_FOLDER)/chess.wasm
	$(C_LD) -O1 $(CHESS_LDFLAGS) $(CHESS_FOLDER)/*.c -o $(OUTPUT_FOLDER)/chess_O1.wasm
	$(C_LD) -O2 $(CHESS_LDFLAGS) $(CHESS_FOLDER)/*.c -o $(OUTPUT_FOLDER)/chess_O2.wasm

tiny-AES-c:
	@echo ">>> Checking out https://github.com/kokke/tiny-AES-c.git..."
	git clone https://github.com/kokke/tiny-AES-c.git tiny-AES-c


clean:
	rm -rf WASM_files
	rm -rf tiny-AES-c
	rm -rf logs
