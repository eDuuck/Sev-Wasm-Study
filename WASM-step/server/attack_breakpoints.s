	.file	"attack_breakpoints.c"
	.text
	.globl	attack_page1
	.bss
	.align 8
	.type	attack_page1, @object
	.size	attack_page1, 8
attack_page1:
	.zero	8
	.globl	attack_page2
	.align 8
	.type	attack_page2, @object
	.size	attack_page2, 8
attack_page2:
	.zero	8
	.globl	phys1
	.align 8
	.type	phys1, @object
	.size	phys1, 8
phys1:
	.zero	8
	.globl	phys2
	.align 8
	.type	phys2, @object
	.size	phys2, 8
phys2:
	.zero	8
	.section	.rodata
.LC0:
	.string	"/proc/self/pagemap"
.LC1:
	.string	"Failed to open pagemap file"
	.align 8
.LC2:
	.string	"Failed to seek in pagemap file"
	.align 8
.LC3:
	.string	"Failed to read from pagemap file"
.LC4:
	.string	"Page not present in memory"
	.text
	.type	virt_to_phys, @function
virt_to_phys:
.LFB6:
	.cfi_startproc
	endbr64
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset 6, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register 6
	subq	$64, %rsp
	movq	%rdi, -56(%rbp)
	movq	%fs:40, %rax
	movq	%rax, -8(%rbp)
	xorl	%eax, %eax
	movq	-56(%rbp), %rax
	shrq	$12, %rax
	movq	%rax, -32(%rbp)
	movq	-32(%rbp), %rax
	salq	$3, %rax
	movq	%rax, -24(%rbp)
	movq	$0, -40(%rbp)
	movl	$0, %esi
	leaq	.LC0(%rip), %rax
	movq	%rax, %rdi
	movl	$0, %eax
	call	open@PLT
	movl	%eax, -44(%rbp)
	cmpl	$0, -44(%rbp)
	jns	.L2
	leaq	.LC1(%rip), %rax
	movq	%rax, %rdi
	call	perror@PLT
	movl	$0, %eax
	jmp	.L7
.L2:
	movq	-24(%rbp), %rcx
	movl	-44(%rbp), %eax
	movl	$0, %edx
	movq	%rcx, %rsi
	movl	%eax, %edi
	call	lseek@PLT
	testq	%rax, %rax
	jns	.L4
	leaq	.LC2(%rip), %rax
	movq	%rax, %rdi
	call	perror@PLT
	movl	-44(%rbp), %eax
	movl	%eax, %edi
	call	close@PLT
	movl	$0, %eax
	jmp	.L7
.L4:
	leaq	-40(%rbp), %rcx
	movl	-44(%rbp), %eax
	movl	$8, %edx
	movq	%rcx, %rsi
	movl	%eax, %edi
	call	read@PLT
	cmpq	$8, %rax
	je	.L5
	leaq	.LC3(%rip), %rax
	movq	%rax, %rdi
	call	perror@PLT
	movl	-44(%rbp), %eax
	movl	%eax, %edi
	call	close@PLT
	movl	$0, %eax
	jmp	.L7
.L5:
	movl	-44(%rbp), %eax
	movl	%eax, %edi
	call	close@PLT
	movq	-40(%rbp), %rax
	testq	%rax, %rax
	js	.L6
	leaq	.LC4(%rip), %rax
	movq	%rax, %rdi
	call	perror@PLT
	movl	$0, %eax
	jmp	.L7
.L6:
	movq	-40(%rbp), %rax
	movabsq	$36028797018963967, %rdx
	andq	%rdx, %rax
	movq	%rax, -16(%rbp)
	movq	-16(%rbp), %rax
	salq	$12, %rax
	movq	%rax, %rdx
	movq	-56(%rbp), %rax
	andl	$4095, %eax
	addq	%rdx, %rax
.L7:
	movq	-8(%rbp), %rdx
	subq	%fs:40, %rdx
	je	.L8
	call	__stack_chk_fail@PLT
.L8:
	leave
	.cfi_def_cfa 7, 8
	ret
	.cfi_endproc
.LFE6:
	.size	virt_to_phys, .-virt_to_phys
	.section	.rodata
.LC5:
	.string	"mmap failed"
	.text
	.globl	allocate_pages
	.type	allocate_pages, @function
allocate_pages:
.LFB7:
	.cfi_startproc
	endbr64
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset 6, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register 6
	subq	$32, %rsp
	movq	%rdi, -24(%rbp)
	movl	%esi, -28(%rbp)
	movl	-28(%rbp), %eax
	movslq	%eax, %rdx
	movq	-24(%rbp), %rax
	movq	%rdx, %r9
	movl	$-1, %r8d
	movl	$34, %ecx
	movl	$3, %edx
	movq	%rax, %rsi
	movl	$0, %edi
	call	mmap@PLT
	movq	%rax, -8(%rbp)
	movq	-8(%rbp), %rax
	movl	$4096, %edx
	movl	$0, %esi
	movq	%rax, %rdi
	call	memset@PLT
	cmpq	$-1, -8(%rbp)
	jne	.L10
	leaq	.LC5(%rip), %rax
	movq	%rax, %rdi
	call	perror@PLT
	movl	$1, %edi
	call	exit@PLT
.L10:
	movq	-8(%rbp), %rax
	leave
	.cfi_def_cfa 7, 8
	ret
	.cfi_endproc
.LFE7:
	.size	allocate_pages, .-allocate_pages
	.section	.rodata
	.align 8
.LC6:
	.string	"Failed to allocate attack page"
.LC7:
	.string	"Maybe you didn't run as sudo?"
	.align 8
.LC8:
	.string	"Virtual_1: %p \342\206\222 Physical_1: 0x%lx\n"
	.align 8
.LC9:
	.string	"Virtual_2: %p \342\206\222 Physical_2: 0x%lx\n"
.LC10:
	.string	"0x%lx-0x%lx\n"
	.align 8
.LC11:
	.string	"Waiting for user input to start measurement..."
	.text
	.globl	start_measurement_breakpoint
	.type	start_measurement_breakpoint, @function
start_measurement_breakpoint:
.LFB8:
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
	movl	$0, %esi
	movl	$4096, %edi
	call	allocate_pages
	movq	%rax, attack_page1(%rip)
	movl	$0, %esi
	movl	$4096, %edi
	call	allocate_pages
	movq	%rax, attack_page2(%rip)
	movq	attack_page1(%rip), %rax
	movq	%rax, %rdi
	call	virt_to_phys
	movq	%rax, phys1(%rip)
	movq	attack_page2(%rip), %rax
	movq	%rax, %rdi
	call	virt_to_phys
	movq	%rax, phys2(%rip)
	movq	phys1(%rip), %rax
	testq	%rax, %rax
	je	.L13
	movq	phys2(%rip), %rax
	testq	%rax, %rax
	jne	.L14
.L13:
	leaq	.LC6(%rip), %rax
	movq	%rax, %rdi
	call	puts@PLT
	leaq	.LC7(%rip), %rax
	movq	%rax, %rdi
	call	puts@PLT
	movl	$1, %edi
	call	exit@PLT
.L14:
	movq	phys1(%rip), %rdx
	movq	attack_page1(%rip), %rax
	movq	%rax, %rsi
	leaq	.LC8(%rip), %rax
	movq	%rax, %rdi
	movl	$0, %eax
	call	printf@PLT
	movq	phys2(%rip), %rdx
	movq	attack_page2(%rip), %rax
	movq	%rax, %rsi
	leaq	.LC9(%rip), %rax
	movq	%rax, %rdi
	movl	$0, %eax
	call	printf@PLT
	movq	phys2(%rip), %rax
	shrq	$12, %rax
	movq	%rax, %rdx
	movq	phys1(%rip), %rax
	shrq	$12, %rax
	movq	%rax, %rsi
	leaq	.LC10(%rip), %rax
	movq	%rax, %rdi
	movl	$0, %eax
	call	printf@PLT
	leaq	.LC11(%rip), %rax
	movq	%rax, %rdi
	call	puts@PLT
	movq	attack_page1(%rip), %rbx
	call	getchar@PLT
	movl	%eax, (%rbx)
	movq	attack_page1(%rip), %rax
	movl	(%rax), %edx
	movq	attack_page2(%rip), %rax
	addl	%edx, %edx
	movl	%edx, (%rax)
	nop
	movq	-8(%rbp), %rbx
	leave
	.cfi_def_cfa 7, 8
	ret
	.cfi_endproc
.LFE8:
	.size	start_measurement_breakpoint, .-start_measurement_breakpoint
	.section	.rodata
.LC12:
	.string	"Measurement stopped."
	.text
	.globl	stop_measurement_breakpoint
	.type	stop_measurement_breakpoint, @function
stop_measurement_breakpoint:
.LFB9:
	.cfi_startproc
	endbr64
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset 6, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register 6
	movq	attack_page1(%rip), %rax
	movl	$-559038737, (%rax)
	movq	attack_page1(%rip), %rax
	movl	(%rax), %edx
	movq	attack_page2(%rip), %rax
	addl	%edx, %edx
	movl	%edx, (%rax)
	leaq	.LC12(%rip), %rax
	movq	%rax, %rdi
	call	puts@PLT
	nop
	popq	%rbp
	.cfi_def_cfa 7, 8
	ret
	.cfi_endproc
.LFE9:
	.size	stop_measurement_breakpoint, .-stop_measurement_breakpoint
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
