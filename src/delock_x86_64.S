/*
 * Copyright (C) Huawei Technologies Co., Ltd. 2024. All rights reserved.
 * SPDX-License-Identifier: MIT
 */

//#define DELOCK_YIELD

/*
 * callee-save: r12, r13, r14, r15, rbx, rsp, rbp
 * caller-save: rax, rcx, rdx, rsi, rdi, r8, r9, r10, r11
 * arguments: rdi, rsi, rdx, rcx, r8, r9
 * return values: rax, rdx
 * stack pointer: rsp
 */

.macro save_context reg
        /* push all callee-save registers */
	movq	%rbp,   (\reg)
	movq	%rbx,  8(\reg)
	movq	%r12, 16(\reg)
	movq	%r13, 24(\reg)
	movq	%r14, 32(\reg)
	movq	%r15, 40(\reg)
	movq	%rsp, 48(\reg)

	movq	%rax, 56(\reg)
	movq	%rcx, 64(\reg)
	movq	%rdx, 72(\reg)
	movq	%rsi, 80(\reg)
	movq	%rdi, 88(\reg)
	movq	%r8,  96(\reg)
	movq	%r9,  104(\reg)
	movq	%r10, 112(\reg)
	movq	%r11, 120(\reg)

 	.byte 0xf3, 0x48, 0x0f, 0xae, 0xc0 # rdfsbaseq %%rax
 	movq	%rax, 128(\reg)
        .byte 0xf3, 0x48, 0x0f, 0xae, 0xc8 # rdgsbaseq %%rax
 	movq	%rax, 136(\reg)
.endm
.macro load_context reg
	movq	\reg, %rax

	movq	%rax, %rdi
	movq    128(%rdi), %rax
        .byte 0xf3, 0x48, 0x0f, 0xae, 0xd0 # wrfsbaseq %%rax
	movq    136(%rdi), %rax
        .byte 0xf3, 0x48, 0x0f, 0xae, 0xd8 # wrgsbaseq %%rax
	movq	%rdi, %rax

	movq	  (%rax), %rbp
	movq	 8(%rax), %rbx
	movq	16(%rax), %r12
	movq	24(%rax), %r13
	movq	32(%rax), %r14
	movq	40(%rax), %r15
	movq	48(%rax), %rsp

	movq	64(%rax), %rcx
	movq	72(%rax), %rdx
	movq	80(%rax), %rsi
	movq	88(%rax), %rdi
	movq	96(%rax), %r8
	movq	104(%rax), %r9
	movq	112(%rax), %r10
	movq	120(%rax), %r11
	movq	56(%rax), %rax
.endm

/* *****************************************************************************
 * int _delock_delegate(state_t *state, void *spin, vatomic32_t *status)
 * ****************************************************************************/
	.text
	.align	4
	.global	_delock_delegate
	.type	_delock_delegate, %function

_delock_delegate: /* args (rdi, rsi, rdx) */
	save_context %rdi

	/* so that we can do function calls and not lose the arguments */
	movq	%rdi, %r12  // context pointer
	movq	%rsi, %r13  // spin pointer
	movq	%rdx, %r14  // status pointer

	/* set DELEGATE(2) to spin to inform a server thread we are ready. */
.Lprepare:
	movq	$2, %rcx   // new value DELEGATE
	movq	$1, %rax   // old value LOCKED
        lock	cmpxchgq %rcx, (%rsi)

	/* if cmpxchg failed, then restore and return to enter the CS */
	jne	.Labort

	/* it was successful, wait for status change */
.Lwait:

	movl	(%r14), %eax

	/* check if ABORTED(0) or DONE(2) */
        cmpl    $0, %eax
        je 	.Labort
        cmpl 	$2, %eax
        je 	.Ldone

#ifdef DELOCK_YIELD
	/* sched yield */
	movq	$124, %rax 	// SYS_sched_yield
	syscall
#endif
	jmp	.Lwait

.Ldone:
	//movq	%r12, %rdi
	//movq	%r13, %rsi
	//movq	%r14, %rdx
	load_context %r12

	/* thread returns from _delock_swap() */
	ret

.Labort:
	load_context %r12
	//movq	%r12, %rdi
	//movq	%r13, %rsi
	//movq	%r14, %rdx
	//movq 	16(%rdi), %r12
	//movq 	24(%rdi), %r13
	//movq 	32(%rdi), %r14

	/* return from _delock_delegate() with 0 */
	movl	$0, %eax
	ret

	.size	_delock_delegate, .- _delock_delegate

/* *****************************************************************************
 * void _delock_swap(state_t *state, *next, vatomic32_t *status, uint32_t val)
 * ****************************************************************************/
	.align	4
	.global	_delock_swap
	.hidden	_delock_swap
	.type	_delock_swap, %function

_delock_swap: /* (rdi, rsi, rdx, rcx) */
	save_context %rdi

	/* notify other thread */
	cmpl 	$0, %ecx
	je 	.Lswap
	movl 	%ecx, (%rdx)

.Lswap:
	load_context %rsi

	/* thread return from _delock_delegate() */
 	movl	$1, %eax
	ret
	.size	_delock_swap, .- _delock_swap
