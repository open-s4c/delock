/*
 * Copyright (C) Huawei Technologies Co., Ltd. 2022. All rights reserved.
 * SPDX-License-Identifier: MIT
 */

//#define DELOCK_YIELD

/********************************************************************
 * _delock_delegate
 *******************************************************************/
	.text
	.align	4
	.global	_delock_delegate
	.type	_delock_delegate, %function
_delock_delegate:
	/* save context */
	stp	x19, x20, [x0], #16
	stp	x21, x22, [x0], #16
	stp	x23, x24, [x0], #16
	stp	x25, x26, [x0], #16
	stp	x27, x28, [x0], #16
	stp	x29, x30, [x0], #16
	stp	d8,  d9,  [x0], #16
	stp	d10, d11, [x0], #16
	stp	d12, d13, [x0], #16
	stp	d14, d15, [x0], #16
	mov	x10, sp
	mrs	x11, TPIDR_EL0
	stp	x10, x11, [x0], #16

	/* save x0. We use x19 (and x20) for saving pointers
	 * without using the stack */
	mov	x19, x0
	mov	x20, x1

#if defined(DELOCK_NICE)
	mov	x8, #34		// __NR_nice
	mov	w0, #10
	svc	#0
#endif

	/* set DELEGATE(2) */
	mov	w6, #2
.Lprepare:
	ldaxr	w4, [x20]
	cbz	w4, .Lrenice
	stlxr	w7, w6, [x20]
	cbnz	w7, .Lprepare

	/* check if DONE(2) or ABORTED(0) */
	//mov	w3, #100
.Lwait:
	mov	w6, #2
	ldar	w4, [x19]
	cbz	w4, .Lrenice
	cmp	w4, w6
	b.eq	.Lrestore
	//sub	w3, w3, #1
	//cbnz	w3, .Lwait

#ifdef DELOCK_YIELD
	/* sched yield */
	mov	x8, #124	// SYS_sched_yield
	svc 	#0
#endif
	b	.Lwait

.Lrestore:
	/* work done, recover state */
	ldp	x10, x11, [x19, #-16]!
	mov	sp, x10
	msr	TPIDR_EL0, x11
	ldp	d14, d15, [x19, #-16]!
	ldp	d12, d13, [x19, #-16]!
	ldp	d10, d11, [x19, #-16]!
	ldp	d8,  d9,  [x19, #-16]!
	ldp	x29, x30, [x19, #-16]!
	ldp	x27, x28, [x19, #-16]!
	ldp	x25, x26, [x19, #-16]!
	ldp	x23, x24, [x19, #-16]!
	ldp	x21, x22, [x19, #-16]!
	ldp	x19, x20, [x19, #-16]

#if defined(DELOCK_NICE)
	mov	x8, #34		// __NR_nice
	mov	w0, #-10
	svc	#0
#endif
	ret	// return at _delock_swap

.Lrenice:
#if defined(DELOCK_NICE)
	mov	x8, #34		// __NR_nice
	mov	w0, #-10
	svc	#0
#endif

.Labort:
	/* recover x19, x20, x29 and x30 */
	mov	x0, x19
	ldp	x29, x30, [x0, #-6*16]
	ldp	x19, x20, [x0, #-11*16]
	mov	x0, #0
	ret	// return at _delock_delegate
	.size	_delock_delegate, .- _delock_delegate

/********************************************************************
 * _delock_swap
 *******************************************************************/
	.align	4
	.global	_delock_swap
	.hidden	_delock_swap
	.type	_delock_swap, %function
_delock_swap:
	/* save context */
	stp	x19, x20, [x0], #16
	stp	x21, x22, [x0], #16
	stp	x23, x24, [x0], #16
	stp	x25, x26, [x0], #16
	stp	x27, x28, [x0], #16
	stp	x29, x30, [x0], #16
	stp	d8,  d9,  [x0], #16
	stp	d10, d11, [x0], #16
	stp	d12, d13, [x0], #16
	stp	d14, d15, [x0], #16
        mov     x10, sp
	mrs	x11, TPIDR_EL0
        stp     x10, x11, [x0], #16

	/* notify other thread */
	cbz     w1, .Lswap
	stlr	w1, [x0]

.Lswap:
	/* swap context */
	ldp	x19, x20, [x2], #16
	ldp	x21, x22, [x2], #16
	ldp	x23, x24, [x2], #16
	ldp	x25, x26, [x2], #16
	ldp	x27, x28, [x2], #16
	ldp	x29, x30, [x2], #16
	ldp	d8,  d9,  [x2], #16
	ldp	d10, d11, [x2], #16
	ldp	d12, d13, [x2], #16
	ldp	d14, d15, [x2], #16
        ldp     x10, x11, [x2], #16
	mov	sp, x10
	msr     TPIDR_EL0, x11

	mov	x0, #1 // return at _delock_delegate
	ret
	.size	_delock_swap, .- _delock_swap
