	.file	"wasm_add_rep.c"
	.text
	.globl	jump_table
	.bss
	.align 32
	.type	jump_table, @object
	.size	jump_table, 2048
jump_table:
	.zero	2048
	.globl	memory_value
	.align 64
	.type	memory_value, @object
	.size	memory_value, 32768
memory_value:
	.zero	32768
	.text
	.globl	perform_operations
	.type	perform_operations, @function
perform_operations:
.LFB0:
	.cfi_startproc
	endbr64
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset 6, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register 6
	pushq	%r12
	pushq	%rbx
	.cfi_offset 12, -24
	.cfi_offset 3, -32
#APP
# 12 "wasm_add_rep.c" 1
	xor %rax, %rax
	xor %rbx, %rbx
	xor %rcx, %rcx
	xor %rdx, %rdx
	xor %rsi, %rsi
	xor %rdi, %rdi
	xor %r11, %r11
	xor %r12, %r12
	lea memory_value(%rip), %rbx
	lea jump_table(%rip), %r10
	1:
	endbr64
	sub $0x4, %rbx
	mov (%rbx), %eax
	add $0x1, %r12
	add %eax, -0x4(%rbx)
	movzbl -0x1(%r12), %eax
	mov (%r10,%rax,8), %rax
	/* fake jmp */
	add $1, %r11
	cmp $0xF4240, %r11
	jne 1b
	
# 0 "" 2
#NO_APP
	movl	$0, %eax
	call	stop_measurement_breakpoint@PLT
	nop
	popq	%rbx
	popq	%r12
	popq	%rbp
	.cfi_def_cfa 7, 8
	ret
	.cfi_endproc
.LFE0:
	.size	perform_operations, .-perform_operations
	.section	.rodata
.LC1:
	.string	"Execution time %ld us.\n"
	.text
	.globl	main
	.type	main, @function
main:
.LFB1:
	.cfi_startproc
	endbr64
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset 6, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register 6
	subq	$32, %rsp
	movl	$0, %eax
	call	start_measurement_breakpoint@PLT
	call	clock@PLT
	movq	%rax, -16(%rbp)
	movl	$0, %eax
	call	perform_operations
	call	clock@PLT
	movq	%rax, -8(%rbp)
	movq	-8(%rbp), %rax
	subq	-16(%rbp), %rax
	pxor	%xmm0, %xmm0
	cvtsi2ssq	%rax, %xmm0
	movss	.LC0(%rip), %xmm1
	divss	%xmm1, %xmm0
	movss	%xmm0, -20(%rbp)
	movq	-8(%rbp), %rax
	subq	-16(%rbp), %rax
	movq	%rax, %rsi
	leaq	.LC1(%rip), %rax
	movq	%rax, %rdi
	movl	$0, %eax
	call	printf@PLT
	movl	$0, %eax
	leave
	.cfi_def_cfa 7, 8
	ret
	.cfi_endproc
.LFE1:
	.size	main, .-main
	.section	.rodata
	.align 4
.LC0:
	.long	1232348160
	.ident	"GCC: (Ubuntu 11.4.0-1ubuntu1~22.04.2) 11.4.0"
	.section	.note.GNU-stack,"",@progbits
	.section	.note.gnu.property,"a"
	.align 8
	.long	1f - 0f
	.long	4f - 1f
	.long	5
0:
	.string	"GNU"
1:
	.align 8
	.long	0xc0000002
	.long	3f - 2f
2:
	.long	0x3
3:
	.align 8
4:
