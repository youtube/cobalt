// This file is generated from a similarly-named Perl script in the BoringSSL
// source tree. Do not edit by hand.

#include <openssl/asm_base.h>

#if !defined(OPENSSL_NO_ASM) && defined(OPENSSL_X86_64) && defined(__APPLE__)
.text	


L$ord:
.quad	0xf3b9cac2fc632551, 0xbce6faada7179e84, 0xffffffffffffffff, 0xffffffff00000000
L$ordK:
.quad	0xccd1c8aaee00bc4f
.text	






.globl	_ecp_nistz256_ord_mul_mont_nohw
.private_extern _ecp_nistz256_ord_mul_mont_nohw

.p2align	5
_ecp_nistz256_ord_mul_mont_nohw:

_CET_ENDBR
	pushq	%rbp

	pushq	%rbx

	pushq	%r12

	pushq	%r13

	pushq	%r14

	pushq	%r15

L$ord_mul_body:

	movq	0(%rdx),%rax
	movq	%rdx,%rbx
	leaq	L$ord(%rip),%r14
	movq	L$ordK(%rip),%r15


	movq	%rax,%rcx
	mulq	0(%rsi)
	movq	%rax,%r8
	movq	%rcx,%rax
	movq	%rdx,%r9

	mulq	8(%rsi)
	addq	%rax,%r9
	movq	%rcx,%rax
	adcq	$0,%rdx
	movq	%rdx,%r10

	mulq	16(%rsi)
	addq	%rax,%r10
	movq	%rcx,%rax
	adcq	$0,%rdx

	movq	%r8,%r13
	imulq	%r15,%r8

	movq	%rdx,%r11
	mulq	24(%rsi)
	addq	%rax,%r11
	movq	%r8,%rax
	adcq	$0,%rdx
	movq	%rdx,%r12


	mulq	0(%r14)
	movq	%r8,%rbp
	addq	%rax,%r13
	movq	%r8,%rax
	adcq	$0,%rdx
	movq	%rdx,%rcx

	subq	%r8,%r10
	sbbq	$0,%r8

	mulq	8(%r14)
	addq	%rcx,%r9
	adcq	$0,%rdx
	addq	%rax,%r9
	movq	%rbp,%rax
	adcq	%rdx,%r10
	movq	%rbp,%rdx
	adcq	$0,%r8

	shlq	$32,%rax
	shrq	$32,%rdx
	subq	%rax,%r11
	movq	8(%rbx),%rax
	sbbq	%rdx,%rbp

	addq	%r8,%r11
	adcq	%rbp,%r12
	adcq	$0,%r13


	movq	%rax,%rcx
	mulq	0(%rsi)
	addq	%rax,%r9
	movq	%rcx,%rax
	adcq	$0,%rdx
	movq	%rdx,%rbp

	mulq	8(%rsi)
	addq	%rbp,%r10
	adcq	$0,%rdx
	addq	%rax,%r10
	movq	%rcx,%rax
	adcq	$0,%rdx
	movq	%rdx,%rbp

	mulq	16(%rsi)
	addq	%rbp,%r11
	adcq	$0,%rdx
	addq	%rax,%r11
	movq	%rcx,%rax
	adcq	$0,%rdx

	movq	%r9,%rcx
	imulq	%r15,%r9

	movq	%rdx,%rbp
	mulq	24(%rsi)
	addq	%rbp,%r12
	adcq	$0,%rdx
	xorq	%r8,%r8
	addq	%rax,%r12
	movq	%r9,%rax
	adcq	%rdx,%r13
	adcq	$0,%r8


	mulq	0(%r14)
	movq	%r9,%rbp
	addq	%rax,%rcx
	movq	%r9,%rax
	adcq	%rdx,%rcx

	subq	%r9,%r11
	sbbq	$0,%r9

	mulq	8(%r14)
	addq	%rcx,%r10
	adcq	$0,%rdx
	addq	%rax,%r10
	movq	%rbp,%rax
	adcq	%rdx,%r11
	movq	%rbp,%rdx
	adcq	$0,%r9

	shlq	$32,%rax
	shrq	$32,%rdx
	subq	%rax,%r12
	movq	16(%rbx),%rax
	sbbq	%rdx,%rbp

	addq	%r9,%r12
	adcq	%rbp,%r13
	adcq	$0,%r8


	movq	%rax,%rcx
	mulq	0(%rsi)
	addq	%rax,%r10
	movq	%rcx,%rax
	adcq	$0,%rdx
	movq	%rdx,%rbp

	mulq	8(%rsi)
	addq	%rbp,%r11
	adcq	$0,%rdx
	addq	%rax,%r11
	movq	%rcx,%rax
	adcq	$0,%rdx
	movq	%rdx,%rbp

	mulq	16(%rsi)
	addq	%rbp,%r12
	adcq	$0,%rdx
	addq	%rax,%r12
	movq	%rcx,%rax
	adcq	$0,%rdx

	movq	%r10,%rcx
	imulq	%r15,%r10

	movq	%rdx,%rbp
	mulq	24(%rsi)
	addq	%rbp,%r13
	adcq	$0,%rdx
	xorq	%r9,%r9
	addq	%rax,%r13
	movq	%r10,%rax
	adcq	%rdx,%r8
	adcq	$0,%r9


	mulq	0(%r14)
	movq	%r10,%rbp
	addq	%rax,%rcx
	movq	%r10,%rax
	adcq	%rdx,%rcx

	subq	%r10,%r12
	sbbq	$0,%r10

	mulq	8(%r14)
	addq	%rcx,%r11
	adcq	$0,%rdx
	addq	%rax,%r11
	movq	%rbp,%rax
	adcq	%rdx,%r12
	movq	%rbp,%rdx
	adcq	$0,%r10

	shlq	$32,%rax
	shrq	$32,%rdx
	subq	%rax,%r13
	movq	24(%rbx),%rax
	sbbq	%rdx,%rbp

	addq	%r10,%r13
	adcq	%rbp,%r8
	adcq	$0,%r9


	movq	%rax,%rcx
	mulq	0(%rsi)
	addq	%rax,%r11
	movq	%rcx,%rax
	adcq	$0,%rdx
	movq	%rdx,%rbp

	mulq	8(%rsi)
	addq	%rbp,%r12
	adcq	$0,%rdx
	addq	%rax,%r12
	movq	%rcx,%rax
	adcq	$0,%rdx
	movq	%rdx,%rbp

	mulq	16(%rsi)
	addq	%rbp,%r13
	adcq	$0,%rdx
	addq	%rax,%r13
	movq	%rcx,%rax
	adcq	$0,%rdx

	movq	%r11,%rcx
	imulq	%r15,%r11

	movq	%rdx,%rbp
	mulq	24(%rsi)
	addq	%rbp,%r8
	adcq	$0,%rdx
	xorq	%r10,%r10
	addq	%rax,%r8
	movq	%r11,%rax
	adcq	%rdx,%r9
	adcq	$0,%r10


	mulq	0(%r14)
	movq	%r11,%rbp
	addq	%rax,%rcx
	movq	%r11,%rax
	adcq	%rdx,%rcx

	subq	%r11,%r13
	sbbq	$0,%r11

	mulq	8(%r14)
	addq	%rcx,%r12
	adcq	$0,%rdx
	addq	%rax,%r12
	movq	%rbp,%rax
	adcq	%rdx,%r13
	movq	%rbp,%rdx
	adcq	$0,%r11

	shlq	$32,%rax
	shrq	$32,%rdx
	subq	%rax,%r8
	sbbq	%rdx,%rbp

	addq	%r11,%r8
	adcq	%rbp,%r9
	adcq	$0,%r10


	movq	%r12,%rsi
	subq	0(%r14),%r12
	movq	%r13,%r11
	sbbq	8(%r14),%r13
	movq	%r8,%rcx
	sbbq	16(%r14),%r8
	movq	%r9,%rbp
	sbbq	24(%r14),%r9
	sbbq	$0,%r10

	cmovcq	%rsi,%r12
	cmovcq	%r11,%r13
	cmovcq	%rcx,%r8
	cmovcq	%rbp,%r9

	movq	%r12,0(%rdi)
	movq	%r13,8(%rdi)
	movq	%r8,16(%rdi)
	movq	%r9,24(%rdi)

	movq	0(%rsp),%r15

	movq	8(%rsp),%r14

	movq	16(%rsp),%r13

	movq	24(%rsp),%r12

	movq	32(%rsp),%rbx

	movq	40(%rsp),%rbp

	leaq	48(%rsp),%rsp

L$ord_mul_epilogue:
	ret









.globl	_ecp_nistz256_ord_sqr_mont_nohw
.private_extern _ecp_nistz256_ord_sqr_mont_nohw

.p2align	5
_ecp_nistz256_ord_sqr_mont_nohw:

_CET_ENDBR
	pushq	%rbp

	pushq	%rbx

	pushq	%r12

	pushq	%r13

	pushq	%r14

	pushq	%r15

L$ord_sqr_body:

	movq	0(%rsi),%r8
	movq	8(%rsi),%rax
	movq	16(%rsi),%r14
	movq	24(%rsi),%r15
	leaq	L$ord(%rip),%rsi
	movq	%rdx,%rbx
	jmp	L$oop_ord_sqr

.p2align	5
L$oop_ord_sqr:

	movq	%rax,%rbp
	mulq	%r8
	movq	%rax,%r9
	movq	%rbp,%xmm1
	movq	%r14,%rax
	movq	%rdx,%r10

	mulq	%r8
	addq	%rax,%r10
	movq	%r15,%rax
	movq	%r14,%xmm2
	adcq	$0,%rdx
	movq	%rdx,%r11

	mulq	%r8
	addq	%rax,%r11
	movq	%r15,%rax
	movq	%r15,%xmm3
	adcq	$0,%rdx
	movq	%rdx,%r12


	mulq	%r14
	movq	%rax,%r13
	movq	%r14,%rax
	movq	%rdx,%r14


	mulq	%rbp
	addq	%rax,%r11
	movq	%r15,%rax
	adcq	$0,%rdx
	movq	%rdx,%r15

	mulq	%rbp
	addq	%rax,%r12
	adcq	$0,%rdx

	addq	%r15,%r12
	adcq	%rdx,%r13
	adcq	$0,%r14


	xorq	%r15,%r15
	movq	%r8,%rax
	addq	%r9,%r9
	adcq	%r10,%r10
	adcq	%r11,%r11
	adcq	%r12,%r12
	adcq	%r13,%r13
	adcq	%r14,%r14
	adcq	$0,%r15


	mulq	%rax
	movq	%rax,%r8
	movq	%xmm1,%rax
	movq	%rdx,%rbp

	mulq	%rax
	addq	%rbp,%r9
	adcq	%rax,%r10
	movq	%xmm2,%rax
	adcq	$0,%rdx
	movq	%rdx,%rbp

	mulq	%rax
	addq	%rbp,%r11
	adcq	%rax,%r12
	movq	%xmm3,%rax
	adcq	$0,%rdx
	movq	%rdx,%rbp

	movq	%r8,%rcx
	imulq	32(%rsi),%r8

	mulq	%rax
	addq	%rbp,%r13
	adcq	%rax,%r14
	movq	0(%rsi),%rax
	adcq	%rdx,%r15


	mulq	%r8
	movq	%r8,%rbp
	addq	%rax,%rcx
	movq	8(%rsi),%rax
	adcq	%rdx,%rcx

	subq	%r8,%r10
	sbbq	$0,%rbp

	mulq	%r8
	addq	%rcx,%r9
	adcq	$0,%rdx
	addq	%rax,%r9
	movq	%r8,%rax
	adcq	%rdx,%r10
	movq	%r8,%rdx
	adcq	$0,%rbp

	movq	%r9,%rcx
	imulq	32(%rsi),%r9

	shlq	$32,%rax
	shrq	$32,%rdx
	subq	%rax,%r11
	movq	0(%rsi),%rax
	sbbq	%rdx,%r8

	addq	%rbp,%r11
	adcq	$0,%r8


	mulq	%r9
	movq	%r9,%rbp
	addq	%rax,%rcx
	movq	8(%rsi),%rax
	adcq	%rdx,%rcx

	subq	%r9,%r11
	sbbq	$0,%rbp

	mulq	%r9
	addq	%rcx,%r10
	adcq	$0,%rdx
	addq	%rax,%r10
	movq	%r9,%rax
	adcq	%rdx,%r11
	movq	%r9,%rdx
	adcq	$0,%rbp

	movq	%r10,%rcx
	imulq	32(%rsi),%r10

	shlq	$32,%rax
	shrq	$32,%rdx
	subq	%rax,%r8
	movq	0(%rsi),%rax
	sbbq	%rdx,%r9

	addq	%rbp,%r8
	adcq	$0,%r9


	mulq	%r10
	movq	%r10,%rbp
	addq	%rax,%rcx
	movq	8(%rsi),%rax
	adcq	%rdx,%rcx

	subq	%r10,%r8
	sbbq	$0,%rbp

	mulq	%r10
	addq	%rcx,%r11
	adcq	$0,%rdx
	addq	%rax,%r11
	movq	%r10,%rax
	adcq	%rdx,%r8
	movq	%r10,%rdx
	adcq	$0,%rbp

	movq	%r11,%rcx
	imulq	32(%rsi),%r11

	shlq	$32,%rax
	shrq	$32,%rdx
	subq	%rax,%r9
	movq	0(%rsi),%rax
	sbbq	%rdx,%r10

	addq	%rbp,%r9
	adcq	$0,%r10


	mulq	%r11
	movq	%r11,%rbp
	addq	%rax,%rcx
	movq	8(%rsi),%rax
	adcq	%rdx,%rcx

	subq	%r11,%r9
	sbbq	$0,%rbp

	mulq	%r11
	addq	%rcx,%r8
	adcq	$0,%rdx
	addq	%rax,%r8
	movq	%r11,%rax
	adcq	%rdx,%r9
	movq	%r11,%rdx
	adcq	$0,%rbp

	shlq	$32,%rax
	shrq	$32,%rdx
	subq	%rax,%r10
	sbbq	%rdx,%r11

	addq	%rbp,%r10
	adcq	$0,%r11


	xorq	%rdx,%rdx
	addq	%r12,%r8
	adcq	%r13,%r9
	movq	%r8,%r12
	adcq	%r14,%r10
	adcq	%r15,%r11
	movq	%r9,%rax
	adcq	$0,%rdx


	subq	0(%rsi),%r8
	movq	%r10,%r14
	sbbq	8(%rsi),%r9
	sbbq	16(%rsi),%r10
	movq	%r11,%r15
	sbbq	24(%rsi),%r11
	sbbq	$0,%rdx

	cmovcq	%r12,%r8
	cmovncq	%r9,%rax
	cmovncq	%r10,%r14
	cmovncq	%r11,%r15

	decq	%rbx
	jnz	L$oop_ord_sqr

	movq	%r8,0(%rdi)
	movq	%rax,8(%rdi)
	pxor	%xmm1,%xmm1
	movq	%r14,16(%rdi)
	pxor	%xmm2,%xmm2
	movq	%r15,24(%rdi)
	pxor	%xmm3,%xmm3

	movq	0(%rsp),%r15

	movq	8(%rsp),%r14

	movq	16(%rsp),%r13

	movq	24(%rsp),%r12

	movq	32(%rsp),%rbx

	movq	40(%rsp),%rbp

	leaq	48(%rsp),%rsp

L$ord_sqr_epilogue:
	ret



.globl	_ecp_nistz256_ord_mul_mont_adx
.private_extern _ecp_nistz256_ord_mul_mont_adx

.p2align	5
_ecp_nistz256_ord_mul_mont_adx:

L$ecp_nistz256_ord_mul_mont_adx:
_CET_ENDBR
	pushq	%rbp

	pushq	%rbx

	pushq	%r12

	pushq	%r13

	pushq	%r14

	pushq	%r15

L$ord_mulx_body:

	movq	%rdx,%rbx
	movq	0(%rdx),%rdx
	movq	0(%rsi),%r9
	movq	8(%rsi),%r10
	movq	16(%rsi),%r11
	movq	24(%rsi),%r12
	leaq	-128(%rsi),%rsi
	leaq	L$ord-128(%rip),%r14
	movq	L$ordK(%rip),%r15


	mulxq	%r9,%r8,%r9
	mulxq	%r10,%rcx,%r10
	mulxq	%r11,%rbp,%r11
	addq	%rcx,%r9
	mulxq	%r12,%rcx,%r12
	movq	%r8,%rdx
	mulxq	%r15,%rdx,%rax
	adcq	%rbp,%r10
	adcq	%rcx,%r11
	adcq	$0,%r12


	xorq	%r13,%r13
	mulxq	0+128(%r14),%rcx,%rbp
	adcxq	%rcx,%r8
	adoxq	%rbp,%r9

	mulxq	8+128(%r14),%rcx,%rbp
	adcxq	%rcx,%r9
	adoxq	%rbp,%r10

	mulxq	16+128(%r14),%rcx,%rbp
	adcxq	%rcx,%r10
	adoxq	%rbp,%r11

	mulxq	24+128(%r14),%rcx,%rbp
	movq	8(%rbx),%rdx
	adcxq	%rcx,%r11
	adoxq	%rbp,%r12
	adcxq	%r8,%r12
	adoxq	%r8,%r13
	adcq	$0,%r13


	mulxq	0+128(%rsi),%rcx,%rbp
	adcxq	%rcx,%r9
	adoxq	%rbp,%r10

	mulxq	8+128(%rsi),%rcx,%rbp
	adcxq	%rcx,%r10
	adoxq	%rbp,%r11

	mulxq	16+128(%rsi),%rcx,%rbp
	adcxq	%rcx,%r11
	adoxq	%rbp,%r12

	mulxq	24+128(%rsi),%rcx,%rbp
	movq	%r9,%rdx
	mulxq	%r15,%rdx,%rax
	adcxq	%rcx,%r12
	adoxq	%rbp,%r13

	adcxq	%r8,%r13
	adoxq	%r8,%r8
	adcq	$0,%r8


	mulxq	0+128(%r14),%rcx,%rbp
	adcxq	%rcx,%r9
	adoxq	%rbp,%r10

	mulxq	8+128(%r14),%rcx,%rbp
	adcxq	%rcx,%r10
	adoxq	%rbp,%r11

	mulxq	16+128(%r14),%rcx,%rbp
	adcxq	%rcx,%r11
	adoxq	%rbp,%r12

	mulxq	24+128(%r14),%rcx,%rbp
	movq	16(%rbx),%rdx
	adcxq	%rcx,%r12
	adoxq	%rbp,%r13
	adcxq	%r9,%r13
	adoxq	%r9,%r8
	adcq	$0,%r8


	mulxq	0+128(%rsi),%rcx,%rbp
	adcxq	%rcx,%r10
	adoxq	%rbp,%r11

	mulxq	8+128(%rsi),%rcx,%rbp
	adcxq	%rcx,%r11
	adoxq	%rbp,%r12

	mulxq	16+128(%rsi),%rcx,%rbp
	adcxq	%rcx,%r12
	adoxq	%rbp,%r13

	mulxq	24+128(%rsi),%rcx,%rbp
	movq	%r10,%rdx
	mulxq	%r15,%rdx,%rax
	adcxq	%rcx,%r13
	adoxq	%rbp,%r8

	adcxq	%r9,%r8
	adoxq	%r9,%r9
	adcq	$0,%r9


	mulxq	0+128(%r14),%rcx,%rbp
	adcxq	%rcx,%r10
	adoxq	%rbp,%r11

	mulxq	8+128(%r14),%rcx,%rbp
	adcxq	%rcx,%r11
	adoxq	%rbp,%r12

	mulxq	16+128(%r14),%rcx,%rbp
	adcxq	%rcx,%r12
	adoxq	%rbp,%r13

	mulxq	24+128(%r14),%rcx,%rbp
	movq	24(%rbx),%rdx
	adcxq	%rcx,%r13
	adoxq	%rbp,%r8
	adcxq	%r10,%r8
	adoxq	%r10,%r9
	adcq	$0,%r9


	mulxq	0+128(%rsi),%rcx,%rbp
	adcxq	%rcx,%r11
	adoxq	%rbp,%r12

	mulxq	8+128(%rsi),%rcx,%rbp
	adcxq	%rcx,%r12
	adoxq	%rbp,%r13

	mulxq	16+128(%rsi),%rcx,%rbp
	adcxq	%rcx,%r13
	adoxq	%rbp,%r8

	mulxq	24+128(%rsi),%rcx,%rbp
	movq	%r11,%rdx
	mulxq	%r15,%rdx,%rax
	adcxq	%rcx,%r8
	adoxq	%rbp,%r9

	adcxq	%r10,%r9
	adoxq	%r10,%r10
	adcq	$0,%r10


	mulxq	0+128(%r14),%rcx,%rbp
	adcxq	%rcx,%r11
	adoxq	%rbp,%r12

	mulxq	8+128(%r14),%rcx,%rbp
	adcxq	%rcx,%r12
	adoxq	%rbp,%r13

	mulxq	16+128(%r14),%rcx,%rbp
	adcxq	%rcx,%r13
	adoxq	%rbp,%r8

	mulxq	24+128(%r14),%rcx,%rbp
	leaq	128(%r14),%r14
	movq	%r12,%rbx
	adcxq	%rcx,%r8
	adoxq	%rbp,%r9
	movq	%r13,%rdx
	adcxq	%r11,%r9
	adoxq	%r11,%r10
	adcq	$0,%r10



	movq	%r8,%rcx
	subq	0(%r14),%r12
	sbbq	8(%r14),%r13
	sbbq	16(%r14),%r8
	movq	%r9,%rbp
	sbbq	24(%r14),%r9
	sbbq	$0,%r10

	cmovcq	%rbx,%r12
	cmovcq	%rdx,%r13
	cmovcq	%rcx,%r8
	cmovcq	%rbp,%r9

	movq	%r12,0(%rdi)
	movq	%r13,8(%rdi)
	movq	%r8,16(%rdi)
	movq	%r9,24(%rdi)

	movq	0(%rsp),%r15

	movq	8(%rsp),%r14

	movq	16(%rsp),%r13

	movq	24(%rsp),%r12

	movq	32(%rsp),%rbx

	movq	40(%rsp),%rbp

	leaq	48(%rsp),%rsp

L$ord_mulx_epilogue:
	ret



.globl	_ecp_nistz256_ord_sqr_mont_adx
.private_extern _ecp_nistz256_ord_sqr_mont_adx

.p2align	5
_ecp_nistz256_ord_sqr_mont_adx:

_CET_ENDBR
L$ecp_nistz256_ord_sqr_mont_adx:
	pushq	%rbp

	pushq	%rbx

	pushq	%r12

	pushq	%r13

	pushq	%r14

	pushq	%r15

L$ord_sqrx_body:

	movq	%rdx,%rbx
	movq	0(%rsi),%rdx
	movq	8(%rsi),%r14
	movq	16(%rsi),%r15
	movq	24(%rsi),%r8
	leaq	L$ord(%rip),%rsi
	jmp	L$oop_ord_sqrx

.p2align	5
L$oop_ord_sqrx:
	mulxq	%r14,%r9,%r10
	mulxq	%r15,%rcx,%r11
	movq	%rdx,%rax
	movq	%r14,%xmm1
	mulxq	%r8,%rbp,%r12
	movq	%r14,%rdx
	addq	%rcx,%r10
	movq	%r15,%xmm2
	adcq	%rbp,%r11
	adcq	$0,%r12
	xorq	%r13,%r13

	mulxq	%r15,%rcx,%rbp
	adcxq	%rcx,%r11
	adoxq	%rbp,%r12

	mulxq	%r8,%rcx,%rbp
	movq	%r15,%rdx
	adcxq	%rcx,%r12
	adoxq	%rbp,%r13
	adcq	$0,%r13

	mulxq	%r8,%rcx,%r14
	movq	%rax,%rdx
	movq	%r8,%xmm3
	xorq	%r15,%r15
	adcxq	%r9,%r9
	adoxq	%rcx,%r13
	adcxq	%r10,%r10
	adoxq	%r15,%r14


	mulxq	%rdx,%r8,%rbp
	movq	%xmm1,%rdx
	adcxq	%r11,%r11
	adoxq	%rbp,%r9
	adcxq	%r12,%r12
	mulxq	%rdx,%rcx,%rax
	movq	%xmm2,%rdx
	adcxq	%r13,%r13
	adoxq	%rcx,%r10
	adcxq	%r14,%r14
	mulxq	%rdx,%rcx,%rbp
.byte	0x67
	movq	%xmm3,%rdx
	adoxq	%rax,%r11
	adcxq	%r15,%r15
	adoxq	%rcx,%r12
	adoxq	%rbp,%r13
	mulxq	%rdx,%rcx,%rax
	adoxq	%rcx,%r14
	adoxq	%rax,%r15


	movq	%r8,%rdx
	mulxq	32(%rsi),%rdx,%rcx

	xorq	%rax,%rax
	mulxq	0(%rsi),%rcx,%rbp
	adcxq	%rcx,%r8
	adoxq	%rbp,%r9
	mulxq	8(%rsi),%rcx,%rbp
	adcxq	%rcx,%r9
	adoxq	%rbp,%r10
	mulxq	16(%rsi),%rcx,%rbp
	adcxq	%rcx,%r10
	adoxq	%rbp,%r11
	mulxq	24(%rsi),%rcx,%rbp
	adcxq	%rcx,%r11
	adoxq	%rbp,%r8
	adcxq	%rax,%r8


	movq	%r9,%rdx
	mulxq	32(%rsi),%rdx,%rcx

	mulxq	0(%rsi),%rcx,%rbp
	adoxq	%rcx,%r9
	adcxq	%rbp,%r10
	mulxq	8(%rsi),%rcx,%rbp
	adoxq	%rcx,%r10
	adcxq	%rbp,%r11
	mulxq	16(%rsi),%rcx,%rbp
	adoxq	%rcx,%r11
	adcxq	%rbp,%r8
	mulxq	24(%rsi),%rcx,%rbp
	adoxq	%rcx,%r8
	adcxq	%rbp,%r9
	adoxq	%rax,%r9


	movq	%r10,%rdx
	mulxq	32(%rsi),%rdx,%rcx

	mulxq	0(%rsi),%rcx,%rbp
	adcxq	%rcx,%r10
	adoxq	%rbp,%r11
	mulxq	8(%rsi),%rcx,%rbp
	adcxq	%rcx,%r11
	adoxq	%rbp,%r8
	mulxq	16(%rsi),%rcx,%rbp
	adcxq	%rcx,%r8
	adoxq	%rbp,%r9
	mulxq	24(%rsi),%rcx,%rbp
	adcxq	%rcx,%r9
	adoxq	%rbp,%r10
	adcxq	%rax,%r10


	movq	%r11,%rdx
	mulxq	32(%rsi),%rdx,%rcx

	mulxq	0(%rsi),%rcx,%rbp
	adoxq	%rcx,%r11
	adcxq	%rbp,%r8
	mulxq	8(%rsi),%rcx,%rbp
	adoxq	%rcx,%r8
	adcxq	%rbp,%r9
	mulxq	16(%rsi),%rcx,%rbp
	adoxq	%rcx,%r9
	adcxq	%rbp,%r10
	mulxq	24(%rsi),%rcx,%rbp
	adoxq	%rcx,%r10
	adcxq	%rbp,%r11
	adoxq	%rax,%r11


	addq	%r8,%r12
	adcq	%r13,%r9
	movq	%r12,%rdx
	adcq	%r14,%r10
	adcq	%r15,%r11
	movq	%r9,%r14
	adcq	$0,%rax


	subq	0(%rsi),%r12
	movq	%r10,%r15
	sbbq	8(%rsi),%r9
	sbbq	16(%rsi),%r10
	movq	%r11,%r8
	sbbq	24(%rsi),%r11
	sbbq	$0,%rax

	cmovncq	%r12,%rdx
	cmovncq	%r9,%r14
	cmovncq	%r10,%r15
	cmovncq	%r11,%r8

	decq	%rbx
	jnz	L$oop_ord_sqrx

	movq	%rdx,0(%rdi)
	movq	%r14,8(%rdi)
	pxor	%xmm1,%xmm1
	movq	%r15,16(%rdi)
	pxor	%xmm2,%xmm2
	movq	%r8,24(%rdi)
	pxor	%xmm3,%xmm3

	movq	0(%rsp),%r15

	movq	8(%rsp),%r14

	movq	16(%rsp),%r13

	movq	24(%rsp),%r12

	movq	32(%rsp),%rbx

	movq	40(%rsp),%rbp

	leaq	48(%rsp),%rsp

L$ord_sqrx_epilogue:
	ret


#endif
