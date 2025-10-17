all: libsevstep.a single-step-measure end2end-tests-vm-server end2end-tests-hv-client kaslr-attack  sev-step-tests paper-experiments nemesis-eval
.PHONY: clean clean-apps dependencies end2end-tests-vm-server end2end-tests-hv-client libsevstep.a kaslr-attack  sev-step-tests paper-experiments nemesis-eval

include environment.env

ifndef KERNEL_HEADER_PATH
$(error Missing KERNEL_HEADER_PATH env var. Did you source environment.env ? Also check the comments in the file on how to set this variable)
endif

INCLUDES =   -I./external-dependencies/json-c-install/include -I./external-dependencies/curl/include  -I${KERNEL_HEADER_PATH}
LIBDIRS = -L./build/libs -L./external-dependencies/curl-build/lib -L./external-dependencies/json-c-install/lib
CFLAGS = -fsanitize=address -Wall -Wextra -fstack-protector -Wshadow -fno-omit-frame-pointer -g #-Wno-unused-variable -Werror


SUBDIR = sev-step-lib end2end-tests/host-client example-apps
OBJ_FILE_DIR = ./build/obj
OUTPUT_BINARY_DIR = ./build/binaries
OUTPUT_LIB_DIR = ./build/libs
OUTPUT_LOG_DIR = ./build/logs

INCS = $(wildcard *.h $(foreach fd, $(SUBDIR), $(fd)/*.h))
SRCS = $(wildcard *.c $(foreach fd, $(SUBDIR), $(fd)/*.c))
NODIR_SRC = $(notdir $(SRCS))
OBJS = $(addprefix $(OBJ_FILE_DIR)/, $(SRCS:c=o)) # obj/xxx.o obj/folder/xxx .o



#Intended to be run once at the start
dependencies:
	git submodule update --init .
	mkdir external-dependencies/curl-build
	cd external-dependencies/curl-build && cmake ../curl && make
	mkdir external-dependencies/json-c-build
	mkdir external-dependencies/json-c-install
	cd external-dependencies/json-c-build && cmake -DCMAKE_INSTALL_PREFIX=../json-c-install ../json-c && make && make install

#build all object files
$(OBJ_FILE_DIR)/%.o: %.c $(INCS)
	mkdir -p $(@D)
	clang $(INCLUDES) $(CFLAGS) -o $@ -c $<  $(INC_DIRS)

kaslr-attack: $(OBJ_FILE_DIR)/example-apps/kaslr-attack/main.o $(OUTPUT_LIB_DIR)/libsevstep.a
	mkdir -p $(OUTPUT_BINARY_DIR)
	clang  $(INCLUDES) $(LIBDIRS)  $(CFLAGS) -o $(OUTPUT_BINARY_DIR)/kaslr-attack $^ -lsevstep

end2end-tests-vm-server:
	mkdir -p ./build/rust/target/
	cargo build --target-dir ./build/rust/target/  -p vm-server --release || (echo "failed to build end2end-tests-vm-server; exit1")

end2end-tests-hv-client: $(OBJ_FILE_DIR)/end2end-tests/host-client/main.o $(OBJ_FILE_DIR)/end2end-tests/host-client/vm-server-client.o $(OUTPUT_LIB_DIR)/libsevstep.a
	mkdir -p $(OUTPUT_BINARY_DIR)
	mkdir -p ${OUTPUT_LOG_DIR}
	clang  $(INCLUDES) $(LIBDIRS)  $(CFLAGS)  -o $(OUTPUT_BINARY_DIR)/end2end-tests-hv-client $^ -lsevstep -lcurl -ljson-c -pthread -lm

paper-experiments: $(OBJ_FILE_DIR)/example-apps/paper_experiments/paper_experiments.o $(OBJ_FILE_DIR)/end2end-tests/host-client/vm-server-client.o $(OUTPUT_LIB_DIR)/libsevstep.a
	mkdir -p $(OUTPUT_BINARY_DIR)
	mkdir -p ${OUTPUT_LOG_DIR}
	clang  $(INCLUDES) $(LIBDIRS)  $(CFLAGS)  -o $(OUTPUT_BINARY_DIR)/paper-experiments $^ -lsevstep -lcurl -ljson-c -pthread -lm

nemesis-eval: $(OBJ_FILE_DIR)/example-apps/nemesis_main.o $(OBJ_FILE_DIR)/end2end-tests/host-client/vm-server-client.o $(OUTPUT_LIB_DIR)/libsevstep.a
	mkdir -p $(OUTPUT_BINARY_DIR)
	mkdir -p ${OUTPUT_LOG_DIR}
	clang  $(INCLUDES) $(LIBDIRS)  $(CFLAGS)  -o $(OUTPUT_BINARY_DIR)/nemesis-eval $^ -lsevstep -lcurl -ljson-c -pthread -lm


$(OBJ_FILE_DIR)/sev-step-lib/raw_spinlock.o:  sev-step-lib/raw_spinlock.asm
	mkdir -p $(@D)
	nasm -f elf64 -o $(OBJ_FILE_DIR)/sev-step-lib/raw_spinlock.o $^

libsevstep.a: $(OBJ_FILE_DIR)/sev-step-lib/sev_step_api.o $(OBJ_FILE_DIR)/sev-step-lib/raw_spinlock.o $(OBJ_FILE_DIR)/sev-step-lib/sev_step_routines.o $(OBJ_FILE_DIR)/sev-step-lib/sev_step_eviction_set_builders.o $(OBJ_FILE_DIR)/sev-step-lib/sev_step_pagemap_parser.o $(OBJ_FILE_DIR)/sev-step-lib/sev_step_cache_attack_log.o $(OBJ_FILE_DIR)/sev-step-lib/sev_step_http_client.o
	mkdir -p $(OUTPUT_LIB_DIR)
	ar rcs $(OUTPUT_LIB_DIR)/libsevstep.a $^

sev-step-tests: $(OBJ_FILE_DIR)/sev-step-lib/sev_step_tests.o
	mkdir -p $(OUTPUT_BINARY_DIR)
	clang  $(INCLUDES) $(LIBDIRS)  $(CFLAGS)  -o $(OUTPUT_BINARY_DIR)/sev-step-tests $^ -lsevstep -pthread -ljson-c -lm -lcurl

#single-step-measure: $(OBJ_FILE_DIR)/single-step-measure/single-step-measure.o $(OUTPUT_LIB_DIR)/libsevstep.a
#	mkdir -p $(OUTPUT_BINARY_DIR)
#	clang $(INCLUDES) $(LIBDIRS) $(CFLAGS) -o $(OUTPUT_BINARY_DIR)/single-step-measure $^ -lsevstep -pthread -lm -lcurl -ljson-c

testing: $(OBJ_FILE_DIR)/single-step-measure/main.o $(OBJ_FILE_DIR)/single-step-measure/SEVStudy_thread.o  $(OUTPUT_LIB_DIR)/libsevstep.a
	mkdir -p $(OUTPUT_BINARY_DIR)
	clang $(INCLUDES) $(LIBDIRS) $(CFLAGS) -o $(OUTPUT_BINARY_DIR)/testing $^ -lsevstep -pthread -lm -lcurl -ljson-c

idle-page-tracker: $(OBJ_FILE_DIR)/WASM-step/idle-page-tracker.o $(OUTPUT_LIB_DIR)/libsevstep.a
	mkdir -p $(OUTPUT_BINARY_DIR)
	clang $(INCLUDES) $(LIBDIRS) $(CFLAGS) -o $(OUTPUT_BINARY_DIR)/idle-page-tracker $^ -lsevstep -pthread -lm -lcurl -ljson-c

track-all-page-modes: $(OBJ_FILE_DIR)/WASM-step/track-all-page-modes.o $(OUTPUT_LIB_DIR)/libsevstep.a
	mkdir -p $(OUTPUT_BINARY_DIR)
	clang $(INCLUDES) $(LIBDIRS) $(CFLAGS) -o $(OUTPUT_BINARY_DIR)/track-all-page-modes $^ -lsevstep -pthread -lm -lcurl -ljson-c

target-measurement: $(OBJ_FILE_DIR)/WASM-step/target-measurement.o $(OUTPUT_LIB_DIR)/libsevstep.a
	mkdir -p $(OUTPUT_BINARY_DIR)
	mkdir -p output
	clang $(INCLUDES) $(LIBDIRS) $(CFLAGS) -o $(OUTPUT_BINARY_DIR)/target-measurement $^ -lsevstep -pthread -lm -lcurl -ljson-c

track-debugging: $(OBJ_FILE_DIR)/WASM-step/track-debugging.o $(OUTPUT_LIB_DIR)/libsevstep.a
	mkdir -p $(OUTPUT_BINARY_DIR)
	clang $(INCLUDES) $(LIBDIRS) $(CFLAGS) -o $(OUTPUT_BINARY_DIR)/track-debugging $^ -lsevstep -pthread -lm -lcurl -ljson-c

send_to_server: $(OBJ_FILE_DIR)/single-step-measure/send_to_server.o
	mkdir -p $(OUTPUT_BINARY_DIR)
	clang $(INCLUDES) $(LIBDIRS) $(CFLAGS) -o $(OUTPUT_BINARY_DIR)/send_to_server $^ -lsevstep -pthread -lm -lcurl -ljson-c

clean-apps:
	rm -f libsevstep.a
	rm -rf build

clean: clean-apps
	rm -rf external-dependencies/json-c-build
	rm -rf external-dependencies/json-c-install
	rm -rf external-dependencies/curl-build
	rm -rf output
	cargo clean
