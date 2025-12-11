	.file	"rep_comms.c"
	.text
	.globl	memory_value
	.data
	.align 8
	.type	memory_value, @object
	.size	memory_value, 8
memory_value:
	.quad	3405691582
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
	pushq	%rbx
	subq	$8, %rsp
	.cfi_offset 3, -24
#APP
# 9 "rep_comms.c" 1
	mov $0, %rax
	mov $0, %rbx
	mov $0, %rcx
	mov $0, %rdx
	mov $0, %rsi
	mov $0, %rdi
	mov $0, %r11
	lea memory_value(%rip), %r8
	1:
	rdrand %rax
	mov %rax, (%r8)
	add %rax, %rax
	mul %rax
	add $1, %r11
	cmp $0xF4240, %r11
	jne 1b
	
# 0 "" 2
#NO_APP
	movl	$0, %eax
	call	stop_measurement_breakpoint@PLT
	nop
	movq	-8(%rbp), %rbx
	leave
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
