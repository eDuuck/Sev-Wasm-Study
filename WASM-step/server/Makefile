AS=nasm
ASFLAGS=-f elf64
LD=gcc
LDFLAGS=-O0
EXECUTABLE=server

all: server assembly pingpong_test

server: ##Put this in the server folder instead. Make output folder in there that .gitignore can ignore.
	$(AS) $(ASFLAGS) assembly_loops.asm -o assembly_loops.o
	$(LD) $(LDFLAGS) assembly_loops.o vm_server.c -o $(EXECUTABLE)

assembly:
	objdump -d $(EXECUTABLE) > $(EXECUTABLE).asm

pingpong_test:
	$(LD) $(LDFLAGS) pingpong_test.c -o pingpong_test

clean:
	rm -f assembly_loops.o $(EXECUTABLE) $(EXECUTABLE).asm pingpong_test