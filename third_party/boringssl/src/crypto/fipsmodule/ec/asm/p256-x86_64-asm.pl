#! /usr/bin/env perl
# Copyright 2014-2016 The OpenSSL Project Authors. All Rights Reserved.
# Copyright (c) 2014, Intel Corporation. All Rights Reserved.
# Copyright (c) 2015 CloudFlare, Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# Originally written by Shay Gueron (1, 2), and Vlad Krasnov (1, 3)
# (1) Intel Corporation, Israel Development Center, Haifa, Israel
# (2) University of Haifa, Israel
# (3) CloudFlare, Inc.
#
# Reference:
# S.Gueron and V.Krasnov, "Fast Prime Field Elliptic Curve Cryptography with
#                          256 Bit Primes"

# Further optimization by <appro@openssl.org>:

$flavour = shift;
$output  = shift;
if ($flavour =~ /\./) { $output = $flavour; undef $flavour; }

$win64=0; $win64=1 if ($flavour =~ /[nm]asm|mingw64/ || $output =~ /\.asm$/);

$0 =~ m/(.*[\/\\])[^\/\\]+$/; $dir=$1;
( $xlate="${dir}x86_64-xlate.pl" and -f $xlate ) or
( $xlate="${dir}../../../perlasm/x86_64-xlate.pl" and -f $xlate) or
die "can't locate x86_64-xlate.pl";

open OUT, "|-", $^X, $xlate, $flavour, $output;
*STDOUT=*OUT;

$avx = 2;
$addx = 1;

$code.=<<___;
.text

# Constants for computations modulo ord(p256)
.Lord:
.quad 0xf3b9cac2fc632551, 0xbce6faada7179e84, 0xffffffffffffffff, 0xffffffff00000000
.LordK:
.quad 0xccd1c8aaee00bc4f
.text
___


{
my ($r_ptr,$a_ptr,$b_org,$b_ptr)=("%rdi","%rsi","%rdx","%rbx");
my ($acc0,$acc1,$acc2,$acc3,$acc4,$acc5,$acc6,$acc7)=map("%r$_",(8..15));
my ($t0,$t1,$t2,$t3,$t4)=("%rcx","%rbp","%rbx","%rdx","%rax");
my ($poly1,$poly3)=($acc6,$acc7);

$code.=<<___;
################################################################################
# void ecp_nistz256_ord_mul_mont(
#   uint64_t res[4],
#   uint64_t a[4],
#   uint64_t b[4]);

.globl	ecp_nistz256_ord_mul_mont_nohw
.type	ecp_nistz256_ord_mul_mont_nohw,\@function,3
.align	32
ecp_nistz256_ord_mul_mont_nohw:
.cfi_startproc
	_CET_ENDBR
___
$code.=<<___;
	push	%rbp
.cfi_push	%rbp
	push	%rbx
.cfi_push	%rbx
	push	%r12
.cfi_push	%r12
	push	%r13
.cfi_push	%r13
	push	%r14
.cfi_push	%r14
	push	%r15
.cfi_push	%r15
.Lord_mul_body:

	mov	8*0($b_org), %rax
	mov	$b_org, $b_ptr
	lea	.Lord(%rip), %r14
	mov	.LordK(%rip), %r15

	################################# * b[0]
	mov	%rax, $t0
	mulq	8*0($a_ptr)
	mov	%rax, $acc0
	mov	$t0, %rax
	mov	%rdx, $acc1

	mulq	8*1($a_ptr)
	add	%rax, $acc1
	mov	$t0, %rax
	adc	\$0, %rdx
	mov	%rdx, $acc2

	mulq	8*2($a_ptr)
	add	%rax, $acc2
	mov	$t0, %rax
	adc	\$0, %rdx

	 mov	$acc0, $acc5
	 imulq	%r15,$acc0

	mov	%rdx, $acc3
	mulq	8*3($a_ptr)
	add	%rax, $acc3
	 mov	$acc0, %rax
	adc	\$0, %rdx
	mov	%rdx, $acc4

	################################# First reduction step
	mulq	8*0(%r14)
	mov	$acc0, $t1
	add	%rax, $acc5		# guaranteed to be zero
	mov	$acc0, %rax
	adc	\$0, %rdx
	mov	%rdx, $t0

	sub	$acc0, $acc2
	sbb	\$0, $acc0		# can't borrow

	mulq	8*1(%r14)
	add	$t0, $acc1
	adc	\$0, %rdx
	add	%rax, $acc1
	mov	$t1, %rax
	adc	%rdx, $acc2
	mov	$t1, %rdx
	adc	\$0, $acc0		# can't overflow

	shl	\$32, %rax
	shr	\$32, %rdx
	sub	%rax, $acc3
	 mov	8*1($b_ptr), %rax
	sbb	%rdx, $t1		# can't borrow

	add	$acc0, $acc3
	adc	$t1, $acc4
	adc	\$0, $acc5

	################################# * b[1]
	mov	%rax, $t0
	mulq	8*0($a_ptr)
	add	%rax, $acc1
	mov	$t0, %rax
	adc	\$0, %rdx
	mov	%rdx, $t1

	mulq	8*1($a_ptr)
	add	$t1, $acc2
	adc	\$0, %rdx
	add	%rax, $acc2
	mov	$t0, %rax
	adc	\$0, %rdx
	mov	%rdx, $t1

	mulq	8*2($a_ptr)
	add	$t1, $acc3
	adc	\$0, %rdx
	add	%rax, $acc3
	mov	$t0, %rax
	adc	\$0, %rdx

	 mov	$acc1, $t0
	 imulq	%r15, $acc1

	mov	%rdx, $t1
	mulq	8*3($a_ptr)
	add	$t1, $acc4
	adc	\$0, %rdx
	xor	$acc0, $acc0
	add	%rax, $acc4
	 mov	$acc1, %rax
	adc	%rdx, $acc5
	adc	\$0, $acc0

	################################# Second reduction step
	mulq	8*0(%r14)
	mov	$acc1, $t1
	add	%rax, $t0		# guaranteed to be zero
	mov	$acc1, %rax
	adc	%rdx, $t0

	sub	$acc1, $acc3
	sbb	\$0, $acc1		# can't borrow

	mulq	8*1(%r14)
	add	$t0, $acc2
	adc	\$0, %rdx
	add	%rax, $acc2
	mov	$t1, %rax
	adc	%rdx, $acc3
	mov	$t1, %rdx
	adc	\$0, $acc1		# can't overflow

	shl	\$32, %rax
	shr	\$32, %rdx
	sub	%rax, $acc4
	 mov	8*2($b_ptr), %rax
	sbb	%rdx, $t1		# can't borrow

	add	$acc1, $acc4
	adc	$t1, $acc5
	adc	\$0, $acc0

	################################## * b[2]
	mov	%rax, $t0
	mulq	8*0($a_ptr)
	add	%rax, $acc2
	mov	$t0, %rax
	adc	\$0, %rdx
	mov	%rdx, $t1

	mulq	8*1($a_ptr)
	add	$t1, $acc3
	adc	\$0, %rdx
	add	%rax, $acc3
	mov	$t0, %rax
	adc	\$0, %rdx
	mov	%rdx, $t1

	mulq	8*2($a_ptr)
	add	$t1, $acc4
	adc	\$0, %rdx
	add	%rax, $acc4
	mov	$t0, %rax
	adc	\$0, %rdx

	 mov	$acc2, $t0
	 imulq	%r15, $acc2

	mov	%rdx, $t1
	mulq	8*3($a_ptr)
	add	$t1, $acc5
	adc	\$0, %rdx
	xor	$acc1, $acc1
	add	%rax, $acc5
	 mov	$acc2, %rax
	adc	%rdx, $acc0
	adc	\$0, $acc1

	################################# Third reduction step
	mulq	8*0(%r14)
	mov	$acc2, $t1
	add	%rax, $t0		# guaranteed to be zero
	mov	$acc2, %rax
	adc	%rdx, $t0

	sub	$acc2, $acc4
	sbb	\$0, $acc2		# can't borrow

	mulq	8*1(%r14)
	add	$t0, $acc3
	adc	\$0, %rdx
	add	%rax, $acc3
	mov	$t1, %rax
	adc	%rdx, $acc4
	mov	$t1, %rdx
	adc	\$0, $acc2		# can't overflow

	shl	\$32, %rax
	shr	\$32, %rdx
	sub	%rax, $acc5
	 mov	8*3($b_ptr), %rax
	sbb	%rdx, $t1		# can't borrow

	add	$acc2, $acc5
	adc	$t1, $acc0
	adc	\$0, $acc1

	################################# * b[3]
	mov	%rax, $t0
	mulq	8*0($a_ptr)
	add	%rax, $acc3
	mov	$t0, %rax
	adc	\$0, %rdx
	mov	%rdx, $t1

	mulq	8*1($a_ptr)
	add	$t1, $acc4
	adc	\$0, %rdx
	add	%rax, $acc4
	mov	$t0, %rax
	adc	\$0, %rdx
	mov	%rdx, $t1

	mulq	8*2($a_ptr)
	add	$t1, $acc5
	adc	\$0, %rdx
	add	%rax, $acc5
	mov	$t0, %rax
	adc	\$0, %rdx

	 mov	$acc3, $t0
	 imulq	%r15, $acc3

	mov	%rdx, $t1
	mulq	8*3($a_ptr)
	add	$t1, $acc0
	adc	\$0, %rdx
	xor	$acc2, $acc2
	add	%rax, $acc0
	 mov	$acc3, %rax
	adc	%rdx, $acc1
	adc	\$0, $acc2

	################################# Last reduction step
	mulq	8*0(%r14)
	mov	$acc3, $t1
	add	%rax, $t0		# guaranteed to be zero
	mov	$acc3, %rax
	adc	%rdx, $t0

	sub	$acc3, $acc5
	sbb	\$0, $acc3		# can't borrow

	mulq	8*1(%r14)
	add	$t0, $acc4
	adc	\$0, %rdx
	add	%rax, $acc4
	mov	$t1, %rax
	adc	%rdx, $acc5
	mov	$t1, %rdx
	adc	\$0, $acc3		# can't overflow

	shl	\$32, %rax
	shr	\$32, %rdx
	sub	%rax, $acc0
	sbb	%rdx, $t1		# can't borrow

	add	$acc3, $acc0
	adc	$t1, $acc1
	adc	\$0, $acc2

	################################# Subtract ord
	 mov	$acc4, $a_ptr
	sub	8*0(%r14), $acc4
	 mov	$acc5, $acc3
	sbb	8*1(%r14), $acc5
	 mov	$acc0, $t0
	sbb	8*2(%r14), $acc0
	 mov	$acc1, $t1
	sbb	8*3(%r14), $acc1
	sbb	\$0, $acc2

	cmovc	$a_ptr, $acc4
	cmovc	$acc3, $acc5
	cmovc	$t0, $acc0
	cmovc	$t1, $acc1

	mov	$acc4, 8*0($r_ptr)
	mov	$acc5, 8*1($r_ptr)
	mov	$acc0, 8*2($r_ptr)
	mov	$acc1, 8*3($r_ptr)

	mov	0(%rsp),%r15
.cfi_restore	%r15
	mov	8(%rsp),%r14
.cfi_restore	%r14
	mov	16(%rsp),%r13
.cfi_restore	%r13
	mov	24(%rsp),%r12
.cfi_restore	%r12
	mov	32(%rsp),%rbx
.cfi_restore	%rbx
	mov	40(%rsp),%rbp
.cfi_restore	%rbp
	lea	48(%rsp),%rsp
.cfi_adjust_cfa_offset	-48
.Lord_mul_epilogue:
	ret
.cfi_endproc
.size	ecp_nistz256_ord_mul_mont_nohw,.-ecp_nistz256_ord_mul_mont_nohw

################################################################################
# void ecp_nistz256_ord_sqr_mont(
#   uint64_t res[4],
#   uint64_t a[4],
#   uint64_t rep);

.globl	ecp_nistz256_ord_sqr_mont_nohw
.type	ecp_nistz256_ord_sqr_mont_nohw,\@function,3
.align	32
ecp_nistz256_ord_sqr_mont_nohw:
.cfi_startproc
	_CET_ENDBR
___
$code.=<<___;
	push	%rbp
.cfi_push	%rbp
	push	%rbx
.cfi_push	%rbx
	push	%r12
.cfi_push	%r12
	push	%r13
.cfi_push	%r13
	push	%r14
.cfi_push	%r14
	push	%r15
.cfi_push	%r15
.Lord_sqr_body:

	mov	8*0($a_ptr), $acc0
	mov	8*1($a_ptr), %rax
	mov	8*2($a_ptr), $acc6
	mov	8*3($a_ptr), $acc7
	lea	.Lord(%rip), $a_ptr	# pointer to modulus
	mov	$b_org, $b_ptr
	jmp	.Loop_ord_sqr

.align	32
.Loop_ord_sqr:
	################################# a[1:] * a[0]
	mov	%rax, $t1		# put aside a[1]
	mul	$acc0			# a[1] * a[0]
	mov	%rax, $acc1
	movq	$t1, %xmm1		# offload a[1]
	mov	$acc6, %rax
	mov	%rdx, $acc2

	mul	$acc0			# a[2] * a[0]
	add	%rax, $acc2
	mov	$acc7, %rax
	movq	$acc6, %xmm2		# offload a[2]
	adc	\$0, %rdx
	mov	%rdx, $acc3

	mul	$acc0			# a[3] * a[0]
	add	%rax, $acc3
	mov	$acc7, %rax
	movq	$acc7, %xmm3		# offload a[3]
	adc	\$0, %rdx
	mov	%rdx, $acc4

	################################# a[3] * a[2]
	mul	$acc6			# a[3] * a[2]
	mov	%rax, $acc5
	mov	$acc6, %rax
	mov	%rdx, $acc6

	################################# a[2:] * a[1]
	mul	$t1			# a[2] * a[1]
	add	%rax, $acc3
	mov	$acc7, %rax
	adc	\$0, %rdx
	mov	%rdx, $acc7

	mul	$t1			# a[3] * a[1]
	add	%rax, $acc4
	adc	\$0, %rdx

	add	$acc7, $acc4
	adc	%rdx, $acc5
	adc	\$0, $acc6		# can't overflow

	################################# *2
	xor	$acc7, $acc7
	mov	$acc0, %rax
	add	$acc1, $acc1
	adc	$acc2, $acc2
	adc	$acc3, $acc3
	adc	$acc4, $acc4
	adc	$acc5, $acc5
	adc	$acc6, $acc6
	adc	\$0, $acc7

	################################# Missing products
	mul	%rax			# a[0] * a[0]
	mov	%rax, $acc0
	movq	%xmm1, %rax
	mov	%rdx, $t1

	mul	%rax			# a[1] * a[1]
	add	$t1, $acc1
	adc	%rax, $acc2
	movq	%xmm2, %rax
	adc	\$0, %rdx
	mov	%rdx, $t1

	mul	%rax			# a[2] * a[2]
	add	$t1, $acc3
	adc	%rax, $acc4
	movq	%xmm3, %rax
	adc	\$0, %rdx
	mov	%rdx, $t1

	 mov	$acc0, $t0
	 imulq	8*4($a_ptr), $acc0	# *= .LordK

	mul	%rax			# a[3] * a[3]
	add	$t1, $acc5
	adc	%rax, $acc6
	 mov	8*0($a_ptr), %rax	# modulus[0]
	adc	%rdx, $acc7		# can't overflow

	################################# First reduction step
	mul	$acc0
	mov	$acc0, $t1
	add	%rax, $t0		# guaranteed to be zero
	mov	8*1($a_ptr), %rax	# modulus[1]
	adc	%rdx, $t0

	sub	$acc0, $acc2
	sbb	\$0, $t1		# can't borrow

	mul	$acc0
	add	$t0, $acc1
	adc	\$0, %rdx
	add	%rax, $acc1
	mov	$acc0, %rax
	adc	%rdx, $acc2
	mov	$acc0, %rdx
	adc	\$0, $t1		# can't overflow

	 mov	$acc1, $t0
	 imulq	8*4($a_ptr), $acc1	# *= .LordK

	shl	\$32, %rax
	shr	\$32, %rdx
	sub	%rax, $acc3
	 mov	8*0($a_ptr), %rax
	sbb	%rdx, $acc0		# can't borrow

	add	$t1, $acc3
	adc	\$0, $acc0		# can't overflow

	################################# Second reduction step
	mul	$acc1
	mov	$acc1, $t1
	add	%rax, $t0		# guaranteed to be zero
	mov	8*1($a_ptr), %rax
	adc	%rdx, $t0

	sub	$acc1, $acc3
	sbb	\$0, $t1		# can't borrow

	mul	$acc1
	add	$t0, $acc2
	adc	\$0, %rdx
	add	%rax, $acc2
	mov	$acc1, %rax
	adc	%rdx, $acc3
	mov	$acc1, %rdx
	adc	\$0, $t1		# can't overflow

	 mov	$acc2, $t0
	 imulq	8*4($a_ptr), $acc2	# *= .LordK

	shl	\$32, %rax
	shr	\$32, %rdx
	sub	%rax, $acc0
	 mov	8*0($a_ptr), %rax
	sbb	%rdx, $acc1		# can't borrow

	add	$t1, $acc0
	adc	\$0, $acc1		# can't overflow

	################################# Third reduction step
	mul	$acc2
	mov	$acc2, $t1
	add	%rax, $t0		# guaranteed to be zero
	mov	8*1($a_ptr), %rax
	adc	%rdx, $t0

	sub	$acc2, $acc0
	sbb	\$0, $t1		# can't borrow

	mul	$acc2
	add	$t0, $acc3
	adc	\$0, %rdx
	add	%rax, $acc3
	mov	$acc2, %rax
	adc	%rdx, $acc0
	mov	$acc2, %rdx
	adc	\$0, $t1		# can't overflow

	 mov	$acc3, $t0
	 imulq	8*4($a_ptr), $acc3	# *= .LordK

	shl	\$32, %rax
	shr	\$32, %rdx
	sub	%rax, $acc1
	 mov	8*0($a_ptr), %rax
	sbb	%rdx, $acc2		# can't borrow

	add	$t1, $acc1
	adc	\$0, $acc2		# can't overflow

	################################# Last reduction step
	mul	$acc3
	mov	$acc3, $t1
	add	%rax, $t0		# guaranteed to be zero
	mov	8*1($a_ptr), %rax
	adc	%rdx, $t0

	sub	$acc3, $acc1
	sbb	\$0, $t1		# can't borrow

	mul	$acc3
	add	$t0, $acc0
	adc	\$0, %rdx
	add	%rax, $acc0
	mov	$acc3, %rax
	adc	%rdx, $acc1
	mov	$acc3, %rdx
	adc	\$0, $t1		# can't overflow

	shl	\$32, %rax
	shr	\$32, %rdx
	sub	%rax, $acc2
	sbb	%rdx, $acc3		# can't borrow

	add	$t1, $acc2
	adc	\$0, $acc3		# can't overflow

	################################# Add bits [511:256] of the sqr result
	xor	%rdx, %rdx
	add	$acc4, $acc0
	adc	$acc5, $acc1
	 mov	$acc0, $acc4
	adc	$acc6, $acc2
	adc	$acc7, $acc3
	 mov	$acc1, %rax
	adc	\$0, %rdx

	################################# Compare to modulus
	sub	8*0($a_ptr), $acc0
	 mov	$acc2, $acc6
	sbb	8*1($a_ptr), $acc1
	sbb	8*2($a_ptr), $acc2
	 mov	$acc3, $acc7
	sbb	8*3($a_ptr), $acc3
	sbb	\$0, %rdx

	cmovc	$acc4, $acc0
	cmovnc	$acc1, %rax
	cmovnc	$acc2, $acc6
	cmovnc	$acc3, $acc7

	dec	$b_ptr
	jnz	.Loop_ord_sqr

	mov	$acc0, 8*0($r_ptr)
	mov	%rax,  8*1($r_ptr)
	pxor	%xmm1, %xmm1
	mov	$acc6, 8*2($r_ptr)
	pxor	%xmm2, %xmm2
	mov	$acc7, 8*3($r_ptr)
	pxor	%xmm3, %xmm3

	mov	0(%rsp),%r15
.cfi_restore	%r15
	mov	8(%rsp),%r14
.cfi_restore	%r14
	mov	16(%rsp),%r13
.cfi_restore	%r13
	mov	24(%rsp),%r12
.cfi_restore	%r12
	mov	32(%rsp),%rbx
.cfi_restore	%rbx
	mov	40(%rsp),%rbp
.cfi_restore	%rbp
	lea	48(%rsp),%rsp
.cfi_adjust_cfa_offset	-48
.Lord_sqr_epilogue:
	ret
.cfi_endproc
.size	ecp_nistz256_ord_sqr_mont_nohw,.-ecp_nistz256_ord_sqr_mont_nohw
___

$code.=<<___	if ($addx);
################################################################################
.globl	ecp_nistz256_ord_mul_mont_adx
.type	ecp_nistz256_ord_mul_mont_adx,\@function,3
.align	32
ecp_nistz256_ord_mul_mont_adx:
.cfi_startproc
.Lecp_nistz256_ord_mul_mont_adx:
	_CET_ENDBR
	push	%rbp
.cfi_push	%rbp
	push	%rbx
.cfi_push	%rbx
	push	%r12
.cfi_push	%r12
	push	%r13
.cfi_push	%r13
	push	%r14
.cfi_push	%r14
	push	%r15
.cfi_push	%r15
.Lord_mulx_body:

	mov	$b_org, $b_ptr
	mov	8*0($b_org), %rdx
	mov	8*0($a_ptr), $acc1
	mov	8*1($a_ptr), $acc2
	mov	8*2($a_ptr), $acc3
	mov	8*3($a_ptr), $acc4
	lea	-128($a_ptr), $a_ptr	# control u-op density
	lea	.Lord-128(%rip), %r14
	mov	.LordK(%rip), %r15

	################################# Multiply by b[0]
	mulx	$acc1, $acc0, $acc1
	mulx	$acc2, $t0, $acc2
	mulx	$acc3, $t1, $acc3
	add	$t0, $acc1
	mulx	$acc4, $t0, $acc4
	 mov	$acc0, %rdx
	 mulx	%r15, %rdx, %rax
	adc	$t1, $acc2
	adc	$t0, $acc3
	adc	\$0, $acc4

	################################# reduction
	xor	$acc5, $acc5		# $acc5=0, cf=0, of=0
	mulx	8*0+128(%r14), $t0, $t1
	adcx	$t0, $acc0		# guaranteed to be zero
	adox	$t1, $acc1

	mulx	8*1+128(%r14), $t0, $t1
	adcx	$t0, $acc1
	adox	$t1, $acc2

	mulx	8*2+128(%r14), $t0, $t1
	adcx	$t0, $acc2
	adox	$t1, $acc3

	mulx	8*3+128(%r14), $t0, $t1
	 mov	8*1($b_ptr), %rdx
	adcx	$t0, $acc3
	adox	$t1, $acc4
	adcx	$acc0, $acc4
	adox	$acc0, $acc5
	adc	\$0, $acc5		# cf=0, of=0

	################################# Multiply by b[1]
	mulx	8*0+128($a_ptr), $t0, $t1
	adcx	$t0, $acc1
	adox	$t1, $acc2

	mulx	8*1+128($a_ptr), $t0, $t1
	adcx	$t0, $acc2
	adox	$t1, $acc3

	mulx	8*2+128($a_ptr), $t0, $t1
	adcx	$t0, $acc3
	adox	$t1, $acc4

	mulx	8*3+128($a_ptr), $t0, $t1
	 mov	$acc1, %rdx
	 mulx	%r15, %rdx, %rax
	adcx	$t0, $acc4
	adox	$t1, $acc5

	adcx	$acc0, $acc5
	adox	$acc0, $acc0
	adc	\$0, $acc0		# cf=0, of=0

	################################# reduction
	mulx	8*0+128(%r14), $t0, $t1
	adcx	$t0, $acc1		# guaranteed to be zero
	adox	$t1, $acc2

	mulx	8*1+128(%r14), $t0, $t1
	adcx	$t0, $acc2
	adox	$t1, $acc3

	mulx	8*2+128(%r14), $t0, $t1
	adcx	$t0, $acc3
	adox	$t1, $acc4

	mulx	8*3+128(%r14), $t0, $t1
	 mov	8*2($b_ptr), %rdx
	adcx	$t0, $acc4
	adox	$t1, $acc5
	adcx	$acc1, $acc5
	adox	$acc1, $acc0
	adc	\$0, $acc0		# cf=0, of=0

	################################# Multiply by b[2]
	mulx	8*0+128($a_ptr), $t0, $t1
	adcx	$t0, $acc2
	adox	$t1, $acc3

	mulx	8*1+128($a_ptr), $t0, $t1
	adcx	$t0, $acc3
	adox	$t1, $acc4

	mulx	8*2+128($a_ptr), $t0, $t1
	adcx	$t0, $acc4
	adox	$t1, $acc5

	mulx	8*3+128($a_ptr), $t0, $t1
	 mov	$acc2, %rdx
	 mulx	%r15, %rdx, %rax
	adcx	$t0, $acc5
	adox	$t1, $acc0

	adcx	$acc1, $acc0
	adox	$acc1, $acc1
	adc	\$0, $acc1		# cf=0, of=0

	################################# reduction
	mulx	8*0+128(%r14), $t0, $t1
	adcx	$t0, $acc2		# guaranteed to be zero
	adox	$t1, $acc3

	mulx	8*1+128(%r14), $t0, $t1
	adcx	$t0, $acc3
	adox	$t1, $acc4

	mulx	8*2+128(%r14), $t0, $t1
	adcx	$t0, $acc4
	adox	$t1, $acc5

	mulx	8*3+128(%r14), $t0, $t1
	 mov	8*3($b_ptr), %rdx
	adcx	$t0, $acc5
	adox	$t1, $acc0
	adcx	$acc2, $acc0
	adox	$acc2, $acc1
	adc	\$0, $acc1		# cf=0, of=0

	################################# Multiply by b[3]
	mulx	8*0+128($a_ptr), $t0, $t1
	adcx	$t0, $acc3
	adox	$t1, $acc4

	mulx	8*1+128($a_ptr), $t0, $t1
	adcx	$t0, $acc4
	adox	$t1, $acc5

	mulx	8*2+128($a_ptr), $t0, $t1
	adcx	$t0, $acc5
	adox	$t1, $acc0

	mulx	8*3+128($a_ptr), $t0, $t1
	 mov	$acc3, %rdx
	 mulx	%r15, %rdx, %rax
	adcx	$t0, $acc0
	adox	$t1, $acc1

	adcx	$acc2, $acc1
	adox	$acc2, $acc2
	adc	\$0, $acc2		# cf=0, of=0

	################################# reduction
	mulx	8*0+128(%r14), $t0, $t1
	adcx	$t0, $acc3		# guaranteed to be zero
	adox	$t1, $acc4

	mulx	8*1+128(%r14), $t0, $t1
	adcx	$t0, $acc4
	adox	$t1, $acc5

	mulx	8*2+128(%r14), $t0, $t1
	adcx	$t0, $acc5
	adox	$t1, $acc0

	mulx	8*3+128(%r14), $t0, $t1
	lea	128(%r14),%r14
	 mov	$acc4, $t2
	adcx	$t0, $acc0
	adox	$t1, $acc1
	 mov	$acc5, $t3
	adcx	$acc3, $acc1
	adox	$acc3, $acc2
	adc	\$0, $acc2

	#################################
	# Branch-less conditional subtraction of P
	 mov	$acc0, $t0
	sub	8*0(%r14), $acc4
	sbb	8*1(%r14), $acc5
	sbb	8*2(%r14), $acc0
	 mov	$acc1, $t1
	sbb	8*3(%r14), $acc1
	sbb	\$0, $acc2

	cmovc	$t2, $acc4
	cmovc	$t3, $acc5
	cmovc	$t0, $acc0
	cmovc	$t1, $acc1

	mov	$acc4, 8*0($r_ptr)
	mov	$acc5, 8*1($r_ptr)
	mov	$acc0, 8*2($r_ptr)
	mov	$acc1, 8*3($r_ptr)

	mov	0(%rsp),%r15
.cfi_restore	%r15
	mov	8(%rsp),%r14
.cfi_restore	%r14
	mov	16(%rsp),%r13
.cfi_restore	%r13
	mov	24(%rsp),%r12
.cfi_restore	%r12
	mov	32(%rsp),%rbx
.cfi_restore	%rbx
	mov	40(%rsp),%rbp
.cfi_restore	%rbp
	lea	48(%rsp),%rsp
.cfi_adjust_cfa_offset	-48
.Lord_mulx_epilogue:
	ret
.cfi_endproc
.size	ecp_nistz256_ord_mul_mont_adx,.-ecp_nistz256_ord_mul_mont_adx

.globl	ecp_nistz256_ord_sqr_mont_adx
.type	ecp_nistz256_ord_sqr_mont_adx,\@function,3
.align	32
ecp_nistz256_ord_sqr_mont_adx:
.cfi_startproc
	_CET_ENDBR
.Lecp_nistz256_ord_sqr_mont_adx:
	push	%rbp
.cfi_push	%rbp
	push	%rbx
.cfi_push	%rbx
	push	%r12
.cfi_push	%r12
	push	%r13
.cfi_push	%r13
	push	%r14
.cfi_push	%r14
	push	%r15
.cfi_push	%r15
.Lord_sqrx_body:

	mov	$b_org, $b_ptr
	mov	8*0($a_ptr), %rdx
	mov	8*1($a_ptr), $acc6
	mov	8*2($a_ptr), $acc7
	mov	8*3($a_ptr), $acc0
	lea	.Lord(%rip), $a_ptr
	jmp	.Loop_ord_sqrx

.align	32
.Loop_ord_sqrx:
	mulx	$acc6, $acc1, $acc2	# a[0]*a[1]
	mulx	$acc7, $t0, $acc3	# a[0]*a[2]
	 mov	%rdx, %rax		# offload a[0]
	 movq	$acc6, %xmm1		# offload a[1]
	mulx	$acc0, $t1, $acc4	# a[0]*a[3]
	 mov	$acc6, %rdx
	add	$t0, $acc2
	 movq	$acc7, %xmm2		# offload a[2]
	adc	$t1, $acc3
	adc	\$0, $acc4
	xor	$acc5, $acc5		# $acc5=0,cf=0,of=0
	#################################
	mulx	$acc7, $t0, $t1		# a[1]*a[2]
	adcx	$t0, $acc3
	adox	$t1, $acc4

	mulx	$acc0, $t0, $t1		# a[1]*a[3]
	 mov	$acc7, %rdx
	adcx	$t0, $acc4
	adox	$t1, $acc5
	adc	\$0, $acc5
	#################################
	mulx	$acc0, $t0, $acc6	# a[2]*a[3]
	mov	%rax, %rdx
	 movq	$acc0, %xmm3		# offload a[3]
	xor	$acc7, $acc7		# $acc7=0,cf=0,of=0
	 adcx	$acc1, $acc1		# acc1:6<<1
	adox	$t0, $acc5
	 adcx	$acc2, $acc2
	adox	$acc7, $acc6		# of=0

	################################# a[i]*a[i]
	mulx	%rdx, $acc0, $t1
	movq	%xmm1, %rdx
	 adcx	$acc3, $acc3
	adox	$t1, $acc1
	 adcx	$acc4, $acc4
	mulx	%rdx, $t0, $t4
	movq	%xmm2, %rdx
	 adcx	$acc5, $acc5
	adox	$t0, $acc2
	 adcx	$acc6, $acc6
	mulx	%rdx, $t0, $t1
	.byte	0x67
	movq	%xmm3, %rdx
	adox	$t4, $acc3
	 adcx	$acc7, $acc7
	adox	$t0, $acc4
	adox	$t1, $acc5
	mulx	%rdx, $t0, $t4
	adox	$t0, $acc6
	adox	$t4, $acc7

	################################# reduction
	mov	$acc0, %rdx
	mulx	8*4($a_ptr), %rdx, $t0

	xor	%rax, %rax		# cf=0, of=0
	mulx	8*0($a_ptr), $t0, $t1
	adcx	$t0, $acc0		# guaranteed to be zero
	adox	$t1, $acc1
	mulx	8*1($a_ptr), $t0, $t1
	adcx	$t0, $acc1
	adox	$t1, $acc2
	mulx	8*2($a_ptr), $t0, $t1
	adcx	$t0, $acc2
	adox	$t1, $acc3
	mulx	8*3($a_ptr), $t0, $t1
	adcx	$t0, $acc3
	adox	$t1, $acc0		# of=0
	adcx	%rax, $acc0		# cf=0

	#################################
	mov	$acc1, %rdx
	mulx	8*4($a_ptr), %rdx, $t0

	mulx	8*0($a_ptr), $t0, $t1
	adox	$t0, $acc1		# guaranteed to be zero
	adcx	$t1, $acc2
	mulx	8*1($a_ptr), $t0, $t1
	adox	$t0, $acc2
	adcx	$t1, $acc3
	mulx	8*2($a_ptr), $t0, $t1
	adox	$t0, $acc3
	adcx	$t1, $acc0
	mulx	8*3($a_ptr), $t0, $t1
	adox	$t0, $acc0
	adcx	$t1, $acc1		# cf=0
	adox	%rax, $acc1		# of=0

	#################################
	mov	$acc2, %rdx
	mulx	8*4($a_ptr), %rdx, $t0

	mulx	8*0($a_ptr), $t0, $t1
	adcx	$t0, $acc2		# guaranteed to be zero
	adox	$t1, $acc3
	mulx	8*1($a_ptr), $t0, $t1
	adcx	$t0, $acc3
	adox	$t1, $acc0
	mulx	8*2($a_ptr), $t0, $t1
	adcx	$t0, $acc0
	adox	$t1, $acc1
	mulx	8*3($a_ptr), $t0, $t1
	adcx	$t0, $acc1
	adox	$t1, $acc2		# of=0
	adcx	%rax, $acc2		# cf=0

	#################################
	mov	$acc3, %rdx
	mulx	8*4($a_ptr), %rdx, $t0

	mulx	8*0($a_ptr), $t0, $t1
	adox	$t0, $acc3		# guaranteed to be zero
	adcx	$t1, $acc0
	mulx	8*1($a_ptr), $t0, $t1
	adox	$t0, $acc0
	adcx	$t1, $acc1
	mulx	8*2($a_ptr), $t0, $t1
	adox	$t0, $acc1
	adcx	$t1, $acc2
	mulx	8*3($a_ptr), $t0, $t1
	adox	$t0, $acc2
	adcx	$t1, $acc3
	adox	%rax, $acc3

	################################# accumulate upper half
	add	$acc0, $acc4		# add	$acc4, $acc0
	adc	$acc5, $acc1
	 mov	$acc4, %rdx
	adc	$acc6, $acc2
	adc	$acc7, $acc3
	 mov	$acc1, $acc6
	adc	\$0, %rax

	################################# compare to modulus
	sub	8*0($a_ptr), $acc4
	 mov	$acc2, $acc7
	sbb	8*1($a_ptr), $acc1
	sbb	8*2($a_ptr), $acc2
	 mov	$acc3, $acc0
	sbb	8*3($a_ptr), $acc3
	sbb	\$0, %rax

	cmovnc	$acc4, %rdx
	cmovnc	$acc1, $acc6
	cmovnc	$acc2, $acc7
	cmovnc	$acc3, $acc0

	dec	$b_ptr
	jnz	.Loop_ord_sqrx

	mov	%rdx, 8*0($r_ptr)
	mov	$acc6, 8*1($r_ptr)
	pxor	%xmm1, %xmm1
	mov	$acc7, 8*2($r_ptr)
	pxor	%xmm2, %xmm2
	mov	$acc0, 8*3($r_ptr)
	pxor	%xmm3, %xmm3

	mov	0(%rsp),%r15
.cfi_restore	%r15
	mov	8(%rsp),%r14
.cfi_restore	%r14
	mov	16(%rsp),%r13
.cfi_restore	%r13
	mov	24(%rsp),%r12
.cfi_restore	%r12
	mov	32(%rsp),%rbx
.cfi_restore	%rbx
	mov	40(%rsp),%rbp
.cfi_restore	%rbp
	lea	48(%rsp),%rsp
.cfi_adjust_cfa_offset	-48
.Lord_sqrx_epilogue:
	ret
.cfi_endproc
.size	ecp_nistz256_ord_sqr_mont_adx,.-ecp_nistz256_ord_sqr_mont_adx
___

}

# EXCEPTION_DISPOSITION handler (EXCEPTION_RECORD *rec,ULONG64 frame,
#		CONTEXT *context,DISPATCHER_CONTEXT *disp)
if ($win64) {
$rec="%rcx";
$frame="%rdx";
$context="%r8";
$disp="%r9";

$code.=<<___;
.extern	__imp_RtlVirtualUnwind

.type	short_handler,\@abi-omnipotent
.align	16
short_handler:
	push	%rsi
	push	%rdi
	push	%rbx
	push	%rbp
	push	%r12
	push	%r13
	push	%r14
	push	%r15
	pushfq
	sub	\$64,%rsp

	mov	120($context),%rax	# pull context->Rax
	mov	248($context),%rbx	# pull context->Rip

	mov	8($disp),%rsi		# disp->ImageBase
	mov	56($disp),%r11		# disp->HandlerData

	mov	0(%r11),%r10d		# HandlerData[0]
	lea	(%rsi,%r10),%r10	# end of prologue label
	cmp	%r10,%rbx		# context->Rip<end of prologue label
	jb	.Lcommon_seh_tail

	mov	152($context),%rax	# pull context->Rsp

	mov	4(%r11),%r10d		# HandlerData[1]
	lea	(%rsi,%r10),%r10	# epilogue label
	cmp	%r10,%rbx		# context->Rip>=epilogue label
	jae	.Lcommon_seh_tail

	lea	16(%rax),%rax

	mov	-8(%rax),%r12
	mov	-16(%rax),%r13
	mov	%r12,216($context)	# restore context->R12
	mov	%r13,224($context)	# restore context->R13

	jmp	.Lcommon_seh_tail
.size	short_handler,.-short_handler

.type	full_handler,\@abi-omnipotent
.align	16
full_handler:
	push	%rsi
	push	%rdi
	push	%rbx
	push	%rbp
	push	%r12
	push	%r13
	push	%r14
	push	%r15
	pushfq
	sub	\$64,%rsp

	mov	120($context),%rax	# pull context->Rax
	mov	248($context),%rbx	# pull context->Rip

	mov	8($disp),%rsi		# disp->ImageBase
	mov	56($disp),%r11		# disp->HandlerData

	mov	0(%r11),%r10d		# HandlerData[0]
	lea	(%rsi,%r10),%r10	# end of prologue label
	cmp	%r10,%rbx		# context->Rip<end of prologue label
	jb	.Lcommon_seh_tail

	mov	152($context),%rax	# pull context->Rsp

	mov	4(%r11),%r10d		# HandlerData[1]
	lea	(%rsi,%r10),%r10	# epilogue label
	cmp	%r10,%rbx		# context->Rip>=epilogue label
	jae	.Lcommon_seh_tail

	mov	8(%r11),%r10d		# HandlerData[2]
	lea	(%rax,%r10),%rax

	mov	-8(%rax),%rbp
	mov	-16(%rax),%rbx
	mov	-24(%rax),%r12
	mov	-32(%rax),%r13
	mov	-40(%rax),%r14
	mov	-48(%rax),%r15
	mov	%rbx,144($context)	# restore context->Rbx
	mov	%rbp,160($context)	# restore context->Rbp
	mov	%r12,216($context)	# restore context->R12
	mov	%r13,224($context)	# restore context->R13
	mov	%r14,232($context)	# restore context->R14
	mov	%r15,240($context)	# restore context->R15

.Lcommon_seh_tail:
	mov	8(%rax),%rdi
	mov	16(%rax),%rsi
	mov	%rax,152($context)	# restore context->Rsp
	mov	%rsi,168($context)	# restore context->Rsi
	mov	%rdi,176($context)	# restore context->Rdi

	mov	40($disp),%rdi		# disp->ContextRecord
	mov	$context,%rsi		# context
	mov	\$154,%ecx		# sizeof(CONTEXT)
	.long	0xa548f3fc		# cld; rep movsq

	mov	$disp,%rsi
	xor	%rcx,%rcx		# arg1, UNW_FLAG_NHANDLER
	mov	8(%rsi),%rdx		# arg2, disp->ImageBase
	mov	0(%rsi),%r8		# arg3, disp->ControlPc
	mov	16(%rsi),%r9		# arg4, disp->FunctionEntry
	mov	40(%rsi),%r10		# disp->ContextRecord
	lea	56(%rsi),%r11		# &disp->HandlerData
	lea	24(%rsi),%r12		# &disp->EstablisherFrame
	mov	%r10,32(%rsp)		# arg5
	mov	%r11,40(%rsp)		# arg6
	mov	%r12,48(%rsp)		# arg7
	mov	%rcx,56(%rsp)		# arg8, (NULL)
	call	*__imp_RtlVirtualUnwind(%rip)

	mov	\$1,%eax		# ExceptionContinueSearch
	add	\$64,%rsp
	popfq
	pop	%r15
	pop	%r14
	pop	%r13
	pop	%r12
	pop	%rbp
	pop	%rbx
	pop	%rdi
	pop	%rsi
	ret
.size	full_handler,.-full_handler

.section	.pdata
.align	4
	.rva	.LSEH_begin_ecp_nistz256_ord_mul_mont_nohw
	.rva	.LSEH_end_ecp_nistz256_ord_mul_mont_nohw
	.rva	.LSEH_info_ecp_nistz256_ord_mul_mont_nohw

	.rva	.LSEH_begin_ecp_nistz256_ord_sqr_mont_nohw
	.rva	.LSEH_end_ecp_nistz256_ord_sqr_mont_nohw
	.rva	.LSEH_info_ecp_nistz256_ord_sqr_mont_nohw
___
$code.=<<___	if ($addx);
	.rva	.LSEH_begin_ecp_nistz256_ord_mul_mont_adx
	.rva	.LSEH_end_ecp_nistz256_ord_mul_mont_adx
	.rva	.LSEH_info_ecp_nistz256_ord_mul_mont_adx

	.rva	.LSEH_begin_ecp_nistz256_ord_sqr_mont_adx
	.rva	.LSEH_end_ecp_nistz256_ord_sqr_mont_adx
	.rva	.LSEH_info_ecp_nistz256_ord_sqr_mont_adx
___
$code.=<<___;

.section	.xdata
.align	8
.LSEH_info_ecp_nistz256_ord_mul_mont_nohw:
	.byte	9,0,0,0
	.rva	full_handler
	.rva	.Lord_mul_body,.Lord_mul_epilogue	# HandlerData[]
	.long	48,0
.LSEH_info_ecp_nistz256_ord_sqr_mont_nohw:
	.byte	9,0,0,0
	.rva	full_handler
	.rva	.Lord_sqr_body,.Lord_sqr_epilogue	# HandlerData[]
	.long	48,0
___
$code.=<<___ if ($addx);
.LSEH_info_ecp_nistz256_ord_mul_mont_adx:
	.byte	9,0,0,0
	.rva	full_handler
	.rva	.Lord_mulx_body,.Lord_mulx_epilogue	# HandlerData[]
	.long	48,0
.LSEH_info_ecp_nistz256_ord_sqr_mont_adx:
	.byte	9,0,0,0
	.rva	full_handler
	.rva	.Lord_sqrx_body,.Lord_sqrx_epilogue	# HandlerData[]
	.long	48,0
___

}

$code =~ s/\`([^\`]*)\`/eval $1/gem;
print $code;
close STDOUT or die "error closing STDOUT: $!";
