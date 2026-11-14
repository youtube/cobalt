#! /usr/bin/env perl
# Copyright 2015-2020 The OpenSSL Project Authors. All Rights Reserved.
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


# ====================================================================
# Originally written by Andy Polyakov <appro@openssl.org> for the OpenSSL
# project. Modified for BoringSSL.
# ====================================================================

# The first two arguments should always be the flavour and output file path.
if ($#ARGV < 1) { die "Not enough arguments provided.
  Two arguments are necessary: the flavour and the output file path."; }

$flavour = shift;
$output = shift;

$0 =~ m/(.*[\/\\])[^\/\\]+$/; $dir=$1;
( $xlate="${dir}arm-xlate.pl" and -f $xlate ) or
( $xlate="${dir}../../../perlasm/arm-xlate.pl" and -f $xlate) or
die "can't locate arm-xlate.pl";

open OUT, "|-", $^X, $xlate, $flavour, $output;
*STDOUT=*OUT;

{
my ($rp,$bp,$bi,$a0,$a1,$a2,$a3,$poly1,$t0,$t1,$t2,$t3,$poly3,
    $acc0,$acc1,$acc2,$acc3,$acc4,$acc5,$acc6) =
    map("x$_",(0..17,19,20));
my $acc7=$bi;	# used in ecp_nistz256_sqr_mont
my $poly1w = $poly1 =~ s/x/w/r;

$code.=<<___;
.globl	ecp_nistz256_mul_mont
.type	ecp_nistz256_mul_mont,%function
.align	4
ecp_nistz256_mul_mont:
	AARCH64_SIGN_LINK_REGISTER
	stp	x29,x30,[sp,#-32]!
	add	x29,sp,#0
	stp	x19,x20,[sp,#16]

	mul	$acc0,$a0,$bi		// a[0]*b[0]
	umulh	$t0,$a0,$bi

	mul	$acc1,$a1,$bi		// a[1]*b[0]
	umulh	$t1,$a1,$bi

	mul	$acc2,$a2,$bi		// a[2]*b[0]
	umulh	$t2,$a2,$bi

	mul	$acc3,$a3,$bi		// a[3]*b[0]
	umulh	$t3,$a3,$bi
	ldr	$bi,[$bp,#8]		// b[1]

	adds	$acc1,$acc1,$t0		// accumulate high parts of multiplication
	 lsl	$t0,$acc0,#32
	adcs	$acc2,$acc2,$t1
	 lsr	$t1,$acc0,#32
	adcs	$acc3,$acc3,$t2
	adc	$acc4,xzr,$t3
	mov	$acc5,xzr
___
for($i=1;$i<4;$i++) {
        # Reduction iteration is normally performed by accumulating
        # result of multiplication of modulus by "magic" digit [and
        # omitting least significant word, which is guaranteed to
        # be 0], but thanks to special form of modulus and "magic"
        # digit being equal to least significant word, it can be
        # performed with additions and subtractions alone. Indeed:
        #
        #            ffff0001.00000000.0000ffff.ffffffff
        # *                                     abcdefgh
        # + xxxxxxxx.xxxxxxxx.xxxxxxxx.xxxxxxxx.abcdefgh
        #
        # Now observing that ff..ff*x = (2^n-1)*x = 2^n*x-x, we
        # rewrite above as:
        #
        #   xxxxxxxx.xxxxxxxx.xxxxxxxx.xxxxxxxx.abcdefgh
        # + abcdefgh.abcdefgh.0000abcd.efgh0000.00000000
        # - 0000abcd.efgh0000.00000000.00000000.abcdefgh
        #
        # or marking redundant operations:
        #
        #   xxxxxxxx.xxxxxxxx.xxxxxxxx.xxxxxxxx.--------
        # + abcdefgh.abcdefgh.0000abcd.efgh0000.--------
        # - 0000abcd.efgh0000.--------.--------.--------

$code.=<<___;
	subs	$t2,$acc0,$t0		// "*0xffff0001"
	sbc	$t3,$acc0,$t1
	adds	$acc0,$acc1,$t0		// +=acc[0]<<96 and omit acc[0]
	 mul	$t0,$a0,$bi		// lo(a[0]*b[i])
	adcs	$acc1,$acc2,$t1
	 mul	$t1,$a1,$bi		// lo(a[1]*b[i])
	adcs	$acc2,$acc3,$t2		// +=acc[0]*0xffff0001
	 mul	$t2,$a2,$bi		// lo(a[2]*b[i])
	adcs	$acc3,$acc4,$t3
	 mul	$t3,$a3,$bi		// lo(a[3]*b[i])
	adc	$acc4,$acc5,xzr

	adds	$acc0,$acc0,$t0		// accumulate low parts of multiplication
	 umulh	$t0,$a0,$bi		// hi(a[0]*b[i])
	adcs	$acc1,$acc1,$t1
	 umulh	$t1,$a1,$bi		// hi(a[1]*b[i])
	adcs	$acc2,$acc2,$t2
	 umulh	$t2,$a2,$bi		// hi(a[2]*b[i])
	adcs	$acc3,$acc3,$t3
	 umulh	$t3,$a3,$bi		// hi(a[3]*b[i])
	adc	$acc4,$acc4,xzr
___
$code.=<<___	if ($i<3);
	ldr	$bi,[$bp,#8*($i+1)]	// b[$i+1]
___
$code.=<<___;
	adds	$acc1,$acc1,$t0		// accumulate high parts of multiplication
	 lsl	$t0,$acc0,#32
	adcs	$acc2,$acc2,$t1
	 lsr	$t1,$acc0,#32
	adcs	$acc3,$acc3,$t2
	adcs	$acc4,$acc4,$t3
	adc	$acc5,xzr,xzr
___
}
$code.=<<___;
	  mov $poly1w, #-1		// poly1 = 0x00000000ffffffff
	// last reduction
	subs	$t2,$acc0,$t0		// "*0xffff0001"
	sbc	$t3,$acc0,$t1
	adds	$acc0,$acc1,$t0		// +=acc[0]<<96 and omit acc[0]
	adcs	$acc1,$acc2,$t1
	adcs	$acc2,$acc3,$t2		// +=acc[0]*0xffff0001
	adcs	$acc3,$acc4,$t3
	adc	$acc4,$acc5,xzr

	neg     $poly3,$poly1		// poly3 = 0xffffffff00000001
	adds	$t0,$acc0,#1		// subs	$t0,$acc0,#-1 // tmp = ret-modulus
	sbcs	$t1,$acc1,$poly1
	sbcs	$t2,$acc2,xzr
	sbcs	$t3,$acc3,$poly3
	sbcs	xzr,$acc4,xzr		// did it borrow?

	csel	$acc0,$acc0,$t0,lo	// ret = borrow ? ret : ret-modulus
	csel	$acc1,$acc1,$t1,lo
	csel	$acc2,$acc2,$t2,lo
	stp	$acc0,$acc1,[$rp]
	csel	$acc3,$acc3,$t3,lo
	stp	$acc2,$acc3,[$rp,#16]

	ldp	x19,x20,[sp,#16]
	ldp	x29,x30,[sp],#32
	AARCH64_VALIDATE_LINK_REGISTER
	ret
.size	ecp_nistz256_mul_mont,.-ecp_nistz256_mul_mont

.globl	ecp_nistz256_sqr_mont
.type	ecp_nistz256_sqr_mont,%function
.align	4
ecp_nistz256_sqr_mont:
	AARCH64_SIGN_LINK_REGISTER
	stp	x29,x30,[sp,#-32]!
	add	x29,sp,#0
	stp	x19,x20,[sp,#16]

	//  |  |  |  |  |  |a1*a0|  |
	//  |  |  |  |  |a2*a0|  |  |
	//  |  |a3*a2|a3*a0|  |  |  |
	//  |  |  |  |a2*a1|  |  |  |
	//  |  |  |a3*a1|  |  |  |  |
	// *|  |  |  |  |  |  |  | 2|
	// +|a3*a3|a2*a2|a1*a1|a0*a0|
	//  |--+--+--+--+--+--+--+--|
	//  |A7|A6|A5|A4|A3|A2|A1|A0|, where Ax is $accx, i.e. follow $accx
	//
	//  "can't overflow" below mark carrying into high part of
	//  multiplication result, which can't overflow, because it
	//  can never be all ones.

	mul	$acc1,$a1,$a0		// a[1]*a[0]
	umulh	$t1,$a1,$a0
	mul	$acc2,$a2,$a0		// a[2]*a[0]
	umulh	$t2,$a2,$a0
	mul	$acc3,$a3,$a0		// a[3]*a[0]
	umulh	$acc4,$a3,$a0

	adds	$acc2,$acc2,$t1		// accumulate high parts of multiplication
	 mul	$t0,$a2,$a1		// a[2]*a[1]
	 umulh	$t1,$a2,$a1
	adcs	$acc3,$acc3,$t2
	 mul	$t2,$a3,$a1		// a[3]*a[1]
	 umulh	$t3,$a3,$a1
	adc	$acc4,$acc4,xzr		// can't overflow

	mul	$acc5,$a3,$a2		// a[3]*a[2]
	umulh	$acc6,$a3,$a2

	adds	$t1,$t1,$t2		// accumulate high parts of multiplication
	 mul	$acc0,$a0,$a0		// a[0]*a[0]
	adc	$t2,$t3,xzr		// can't overflow

	adds	$acc3,$acc3,$t0		// accumulate low parts of multiplication
	 umulh	$a0,$a0,$a0
	adcs	$acc4,$acc4,$t1
	 mul	$t1,$a1,$a1		// a[1]*a[1]
	adcs	$acc5,$acc5,$t2
	 umulh	$a1,$a1,$a1
	adc	$acc6,$acc6,xzr		// can't overflow

	adds	$acc1,$acc1,$acc1	// acc[1-6]*=2
	 mul	$t2,$a2,$a2		// a[2]*a[2]
	adcs	$acc2,$acc2,$acc2
	 umulh	$a2,$a2,$a2
	adcs	$acc3,$acc3,$acc3
	 mul	$t3,$a3,$a3		// a[3]*a[3]
	adcs	$acc4,$acc4,$acc4
	 umulh	$a3,$a3,$a3
	adcs	$acc5,$acc5,$acc5
	adcs	$acc6,$acc6,$acc6
	adc	$acc7,xzr,xzr

	adds	$acc1,$acc1,$a0		// +a[i]*a[i]
	adcs	$acc2,$acc2,$t1
	adcs	$acc3,$acc3,$a1
	adcs	$acc4,$acc4,$t2
	adcs	$acc5,$acc5,$a2
	 lsl	$t0,$acc0,#32
	adcs	$acc6,$acc6,$t3
	 lsr	$t1,$acc0,#32
	adc	$acc7,$acc7,$a3
___
for($i=0;$i<3;$i++) {			# reductions, see commentary in
					# multiplication for details
$code.=<<___;
	subs	$t2,$acc0,$t0		// "*0xffff0001"
	sbc	$t3,$acc0,$t1
	adds	$acc0,$acc1,$t0		// +=acc[0]<<96 and omit acc[0]
	adcs	$acc1,$acc2,$t1
	 lsl	$t0,$acc0,#32
	adcs	$acc2,$acc3,$t2		// +=acc[0]*0xffff0001
	 lsr	$t1,$acc0,#32
	adc	$acc3,$t3,xzr		// can't overflow
___
}
$code.=<<___;
	subs	$t2,$acc0,$t0		// "*0xffff0001"
	sbc	$t3,$acc0,$t1
	adds	$acc0,$acc1,$t0		// +=acc[0]<<96 and omit acc[0]
	adcs	$acc1,$acc2,$t1
	adcs	$acc2,$acc3,$t2		// +=acc[0]*0xffff0001
	adc	$acc3,$t3,xzr		// can't overflow

	  mov $poly1w, #-1		// poly1 = 0x00000000ffffffff
	adds	$acc0,$acc0,$acc4	// accumulate upper half
	adcs	$acc1,$acc1,$acc5
	adcs	$acc2,$acc2,$acc6
	adcs	$acc3,$acc3,$acc7
	adc	$acc4,xzr,xzr

	neg     $poly3,$poly1		// poly3 = 0xffffffff00000001
	adds	$t0,$acc0,#1		// subs	$t0,$acc0,#-1 // tmp = ret-modulus
	sbcs	$t1,$acc1,$poly1
	sbcs	$t2,$acc2,xzr
	sbcs	$t3,$acc3,$poly3
	sbcs	xzr,$acc4,xzr		// did it borrow?

	csel	$acc0,$acc0,$t0,lo	// ret = borrow ? ret : ret-modulus
	csel	$acc1,$acc1,$t1,lo
	csel	$acc2,$acc2,$t2,lo
	stp	$acc0,$acc1,[$rp]
	csel	$acc3,$acc3,$t3,lo
	stp	$acc2,$acc3,[$rp,#16]

	ldp	x19,x20,[sp,#16]
	ldp	x29,x30,[sp],#32
	AARCH64_VALIDATE_LINK_REGISTER
	ret
.size	ecp_nistz256_sqr_mont,.-ecp_nistz256_sqr_mont
___

if (1) {
my ($rp,$ap,$bp,$bi,$a0,$a1,$a2,$a3,$t0,$t1,$t2,$t3,$poly1,$poly3,
    $acc0,$acc1,$acc2,$acc3,$acc4,$acc5) =
    map("x$_",(0..17,19,20));

my $poly1w = $poly1 =~ s/x/w/r;
my ($ord0,$ord1) = ($poly1,$poly3);
my ($ord2,$ord3,$ordk,$t4) = map("x$_",(21..24));
my $acc6 = $ap;
my $acc7 = $bi;

$code.=<<___;
.section .rodata
.align	5
.Lord:
.quad  0xf3b9cac2fc632551,0xbce6faada7179e84,0xffffffffffffffff,0xffffffff00000000
.LordK:
.quad  0xccd1c8aaee00bc4f
 .asciz "ECP_NISTZ256 for ARMv8, CRYPTOGAMS by <appro\@openssl.org>"
 .text

// void ecp_nistz256_ord_mul_mont(uint64_t res[4], uint64_t a[4],
//                                uint64_t b[4]);
.globl	ecp_nistz256_ord_mul_mont
.type	ecp_nistz256_ord_mul_mont,%function
.align	4
ecp_nistz256_ord_mul_mont:
	AARCH64_VALID_CALL_TARGET
	// Armv8.3-A PAuth: even though x30 is pushed to stack it is not popped later.
	stp	x29,x30,[sp,#-64]!
	add	x29,sp,#0
	stp	x19,x20,[sp,#16]
	stp	x21,x22,[sp,#32]
	stp	x23,x24,[sp,#48]

	adrp	$ordk,:pg_hi21:.Lord
	add	$ordk,$ordk,:lo12:.Lord
	ldr	$bi,[$bp]		// bp[0]
	ldp	$a0,$a1,[$ap]
	ldp	$a2,$a3,[$ap,#16]

	ldp	$ord0,$ord1,[$ordk,#0]
	ldp	$ord2,$ord3,[$ordk,#16]
	ldr	$ordk,[$ordk,#32]	// LordK

	mul	$acc0,$a0,$bi		// a[0]*b[0]
	umulh	$t0,$a0,$bi

	mul	$acc1,$a1,$bi		// a[1]*b[0]
	umulh	$t1,$a1,$bi

	mul	$acc2,$a2,$bi		// a[2]*b[0]
	umulh	$t2,$a2,$bi

	mul	$acc3,$a3,$bi		// a[3]*b[0]
	umulh	$acc4,$a3,$bi

	mul	$t4,$acc0,$ordk

	adds	$acc1,$acc1,$t0		// accumulate high parts of multiplication
	adcs	$acc2,$acc2,$t1
	adcs	$acc3,$acc3,$t2
	adc	$acc4,$acc4,xzr
	mov	$acc5,xzr
___
for ($i=1;$i<4;$i++) {
	################################################################
	#            ffff0000.ffffffff.yyyyyyyy.zzzzzzzz
	# *                                     abcdefgh
	# + xxxxxxxx.xxxxxxxx.xxxxxxxx.xxxxxxxx.xxxxxxxx
	#
	# Now observing that ff..ff*x = (2^n-1)*x = 2^n*x-x, we
	# rewrite above as:
	#
	#   xxxxxxxx.xxxxxxxx.xxxxxxxx.xxxxxxxx.xxxxxxxx
	# - 0000abcd.efgh0000.abcdefgh.00000000.00000000
	# + abcdefgh.abcdefgh.yzayzbyz.cyzdyzey.zfyzgyzh
$code.=<<___;
	ldr	$bi,[$bp,#8*$i]		// b[i]

	lsl	$t0,$t4,#32
	subs	$acc2,$acc2,$t4
	lsr	$t1,$t4,#32
	sbcs	$acc3,$acc3,$t0
	sbcs	$acc4,$acc4,$t1
	sbc	$acc5,$acc5,xzr

	subs	xzr,$acc0,#1
	umulh	$t1,$ord0,$t4
	mul	$t2,$ord1,$t4
	umulh	$t3,$ord1,$t4

	adcs	$t2,$t2,$t1
	 mul	$t0,$a0,$bi
	adc	$t3,$t3,xzr
	 mul	$t1,$a1,$bi

	adds	$acc0,$acc1,$t2
	 mul	$t2,$a2,$bi
	adcs	$acc1,$acc2,$t3
	 mul	$t3,$a3,$bi
	adcs	$acc2,$acc3,$t4
	adcs	$acc3,$acc4,$t4
	adc	$acc4,$acc5,xzr

	adds	$acc0,$acc0,$t0		// accumulate low parts
	umulh	$t0,$a0,$bi
	adcs	$acc1,$acc1,$t1
	umulh	$t1,$a1,$bi
	adcs	$acc2,$acc2,$t2
	umulh	$t2,$a2,$bi
	adcs	$acc3,$acc3,$t3
	umulh	$t3,$a3,$bi
	adc	$acc4,$acc4,xzr
	mul	$t4,$acc0,$ordk
	adds	$acc1,$acc1,$t0		// accumulate high parts
	adcs	$acc2,$acc2,$t1
	adcs	$acc3,$acc3,$t2
	adcs	$acc4,$acc4,$t3
	adc	$acc5,xzr,xzr
___
}
$code.=<<___;
	lsl	$t0,$t4,#32		// last reduction
	subs	$acc2,$acc2,$t4
	lsr	$t1,$t4,#32
	sbcs	$acc3,$acc3,$t0
	sbcs	$acc4,$acc4,$t1
	sbc	$acc5,$acc5,xzr

	subs	xzr,$acc0,#1
	umulh	$t1,$ord0,$t4
	mul	$t2,$ord1,$t4
	umulh	$t3,$ord1,$t4

	adcs	$t2,$t2,$t1
	adc	$t3,$t3,xzr

	adds	$acc0,$acc1,$t2
	adcs	$acc1,$acc2,$t3
	adcs	$acc2,$acc3,$t4
	adcs	$acc3,$acc4,$t4
	adc	$acc4,$acc5,xzr

	subs	$t0,$acc0,$ord0		// ret -= modulus
	sbcs	$t1,$acc1,$ord1
	sbcs	$t2,$acc2,$ord2
	sbcs	$t3,$acc3,$ord3
	sbcs	xzr,$acc4,xzr

	csel	$acc0,$acc0,$t0,lo	// ret = borrow ? ret : ret-modulus
	csel	$acc1,$acc1,$t1,lo
	csel	$acc2,$acc2,$t2,lo
	stp	$acc0,$acc1,[$rp]
	csel	$acc3,$acc3,$t3,lo
	stp	$acc2,$acc3,[$rp,#16]

	ldp	x19,x20,[sp,#16]
	ldp	x21,x22,[sp,#32]
	ldp	x23,x24,[sp,#48]
	ldr	x29,[sp],#64
	ret
.size	ecp_nistz256_ord_mul_mont,.-ecp_nistz256_ord_mul_mont

////////////////////////////////////////////////////////////////////////
// void ecp_nistz256_ord_sqr_mont(uint64_t res[4], uint64_t a[4],
//                                uint64_t rep);
.globl	ecp_nistz256_ord_sqr_mont
.type	ecp_nistz256_ord_sqr_mont,%function
.align	4
ecp_nistz256_ord_sqr_mont:
	AARCH64_VALID_CALL_TARGET
	// Armv8.3-A PAuth: even though x30 is pushed to stack it is not popped later.
	stp	x29,x30,[sp,#-64]!
	add	x29,sp,#0
	stp	x19,x20,[sp,#16]
	stp	x21,x22,[sp,#32]
	stp	x23,x24,[sp,#48]

	adrp	$ordk,:pg_hi21:.Lord
	add	$ordk,$ordk,:lo12:.Lord
	ldp	$a0,$a1,[$ap]
	ldp	$a2,$a3,[$ap,#16]

	ldp	$ord0,$ord1,[$ordk,#0]
	ldp	$ord2,$ord3,[$ordk,#16]
	ldr	$ordk,[$ordk,#32]
	b	.Loop_ord_sqr

.align	4
.Loop_ord_sqr:
	sub	$bp,$bp,#1
	////////////////////////////////////////////////////////////////
	//  |  |  |  |  |  |a1*a0|  |
	//  |  |  |  |  |a2*a0|  |  |
	//  |  |a3*a2|a3*a0|  |  |  |
	//  |  |  |  |a2*a1|  |  |  |
	//  |  |  |a3*a1|  |  |  |  |
	// *|  |  |  |  |  |  |  | 2|
	// +|a3*a3|a2*a2|a1*a1|a0*a0|
	//  |--+--+--+--+--+--+--+--|
	//  |A7|A6|A5|A4|A3|A2|A1|A0|, where Ax is $accx, i.e. follow $accx
	//
	//  "can't overflow" below mark carrying into high part of
	//  multiplication result, which can't overflow, because it
	//  can never be all ones.

	mul	$acc1,$a1,$a0		// a[1]*a[0]
	umulh	$t1,$a1,$a0
	mul	$acc2,$a2,$a0		// a[2]*a[0]
	umulh	$t2,$a2,$a0
	mul	$acc3,$a3,$a0		// a[3]*a[0]
	umulh	$acc4,$a3,$a0

	adds	$acc2,$acc2,$t1		// accumulate high parts of multiplication
	 mul	$t0,$a2,$a1		// a[2]*a[1]
	 umulh	$t1,$a2,$a1
	adcs	$acc3,$acc3,$t2
	 mul	$t2,$a3,$a1		// a[3]*a[1]
	 umulh	$t3,$a3,$a1
	adc	$acc4,$acc4,xzr		// can't overflow

	mul	$acc5,$a3,$a2		// a[3]*a[2]
	umulh	$acc6,$a3,$a2

	adds	$t1,$t1,$t2		// accumulate high parts of multiplication
	 mul	$acc0,$a0,$a0		// a[0]*a[0]
	adc	$t2,$t3,xzr		// can't overflow

	adds	$acc3,$acc3,$t0		// accumulate low parts of multiplication
	 umulh	$a0,$a0,$a0
	adcs	$acc4,$acc4,$t1
	 mul	$t1,$a1,$a1		// a[1]*a[1]
	adcs	$acc5,$acc5,$t2
	 umulh	$a1,$a1,$a1
	adc	$acc6,$acc6,xzr		// can't overflow

	adds	$acc1,$acc1,$acc1	// acc[1-6]*=2
	 mul	$t2,$a2,$a2		// a[2]*a[2]
	adcs	$acc2,$acc2,$acc2
	 umulh	$a2,$a2,$a2
	adcs	$acc3,$acc3,$acc3
	 mul	$t3,$a3,$a3		// a[3]*a[3]
	adcs	$acc4,$acc4,$acc4
	 umulh	$a3,$a3,$a3
	adcs	$acc5,$acc5,$acc5
	adcs	$acc6,$acc6,$acc6
	adc	$acc7,xzr,xzr

	adds	$acc1,$acc1,$a0		// +a[i]*a[i]
	 mul	$t4,$acc0,$ordk
	adcs	$acc2,$acc2,$t1
	adcs	$acc3,$acc3,$a1
	adcs	$acc4,$acc4,$t2
	adcs	$acc5,$acc5,$a2
	adcs	$acc6,$acc6,$t3
	adc	$acc7,$acc7,$a3
___
for($i=0; $i<4; $i++) {			# reductions
$code.=<<___;
	subs	xzr,$acc0,#1
	umulh	$t1,$ord0,$t4
	mul	$t2,$ord1,$t4
	umulh	$t3,$ord1,$t4

	adcs	$t2,$t2,$t1
	adc	$t3,$t3,xzr

	adds	$acc0,$acc1,$t2
	adcs	$acc1,$acc2,$t3
	adcs	$acc2,$acc3,$t4
	adc	$acc3,xzr,$t4		// can't overflow
___
$code.=<<___	if ($i<3);
	mul	$t3,$acc0,$ordk
___
$code.=<<___;
	lsl	$t0,$t4,#32
	subs	$acc1,$acc1,$t4
	lsr	$t1,$t4,#32
	sbcs	$acc2,$acc2,$t0
	sbc	$acc3,$acc3,$t1		// can't borrow
___
	($t3,$t4) = ($t4,$t3);
}
$code.=<<___;
	adds	$acc0,$acc0,$acc4	// accumulate upper half
	adcs	$acc1,$acc1,$acc5
	adcs	$acc2,$acc2,$acc6
	adcs	$acc3,$acc3,$acc7
	adc	$acc4,xzr,xzr

	subs	$t0,$acc0,$ord0		// ret -= modulus
	sbcs	$t1,$acc1,$ord1
	sbcs	$t2,$acc2,$ord2
	sbcs	$t3,$acc3,$ord3
	sbcs	xzr,$acc4,xzr

	csel	$a0,$acc0,$t0,lo	// ret = borrow ? ret : ret-modulus
	csel	$a1,$acc1,$t1,lo
	csel	$a2,$acc2,$t2,lo
	csel	$a3,$acc3,$t3,lo

	cbnz	$bp,.Loop_ord_sqr

	stp	$a0,$a1,[$rp]
	stp	$a2,$a3,[$rp,#16]

	ldp	x19,x20,[sp,#16]
	ldp	x21,x22,[sp,#32]
	ldp	x23,x24,[sp,#48]
	ldr	x29,[sp],#64
	ret
.size	ecp_nistz256_ord_sqr_mont,.-ecp_nistz256_ord_sqr_mont
___
}	}

foreach (split("\n",$code)) {
	s/\`([^\`]*)\`/eval $1/ge;

	print $_,"\n";
}
close STDOUT or die "error closing STDOUT: $!";	# enforce flush
