; This file is generated from a similarly-named Perl script in the BoringSSL
; source tree. Do not edit by hand.

%ifidn __OUTPUT_FORMAT__, win64
default	rel
%define XMMWORD
%define YMMWORD
%define ZMMWORD
%define _CET_ENDBR

%ifdef BORINGSSL_PREFIX
%include "boringssl_prefix_symbols_internal_x86_64_win_asm.inc"
%endif
section	.text code align=64



p256_constants:
$L$ord:
	DQ	0xf3b9cac2fc632551,0xbce6faada7179e84,0xffffffffffffffff,0xffffffff00000000
$L$ordK:
	DQ	0xccd1c8aaee00bc4f
section	.text







global	ecp_nistz256_ord_mul_mont_nohw

ALIGN	32
ecp_nistz256_ord_mul_mont_nohw:
	mov	QWORD[8+rsp],rdi	;WIN64 prologue
	mov	QWORD[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_ecp_nistz256_ord_mul_mont_nohw:
	mov	rdi,rcx
	mov	rsi,rdx
	mov	rdx,r8



_CET_ENDBR
	push	rbp

	push	rbx

	push	r12

	push	r13

	push	r14

	push	r15

$L$ord_mul_body:

	mov	rax,QWORD[rdx]
	mov	rbx,rdx
	lea	r14,[$L$ord]
	mov	r15,QWORD[$L$ordK]


	mov	rcx,rax
	mul	QWORD[rsi]
	mov	r8,rax
	mov	rax,rcx
	mov	r9,rdx

	mul	QWORD[8+rsi]
	add	r9,rax
	mov	rax,rcx
	adc	rdx,0
	mov	r10,rdx

	mul	QWORD[16+rsi]
	add	r10,rax
	mov	rax,rcx
	adc	rdx,0

	mov	r13,r8
	imul	r8,r15

	mov	r11,rdx
	mul	QWORD[24+rsi]
	add	r11,rax
	mov	rax,r8
	adc	rdx,0
	mov	r12,rdx


	mul	QWORD[r14]
	mov	rbp,r8
	add	r13,rax
	mov	rax,r8
	adc	rdx,0
	mov	rcx,rdx

	sub	r10,r8
	sbb	r8,0

	mul	QWORD[8+r14]
	add	r9,rcx
	adc	rdx,0
	add	r9,rax
	mov	rax,rbp
	adc	r10,rdx
	mov	rdx,rbp
	adc	r8,0

	shl	rax,32
	shr	rdx,32
	sub	r11,rax
	mov	rax,QWORD[8+rbx]
	sbb	rbp,rdx

	add	r11,r8
	adc	r12,rbp
	adc	r13,0


	mov	rcx,rax
	mul	QWORD[rsi]
	add	r9,rax
	mov	rax,rcx
	adc	rdx,0
	mov	rbp,rdx

	mul	QWORD[8+rsi]
	add	r10,rbp
	adc	rdx,0
	add	r10,rax
	mov	rax,rcx
	adc	rdx,0
	mov	rbp,rdx

	mul	QWORD[16+rsi]
	add	r11,rbp
	adc	rdx,0
	add	r11,rax
	mov	rax,rcx
	adc	rdx,0

	mov	rcx,r9
	imul	r9,r15

	mov	rbp,rdx
	mul	QWORD[24+rsi]
	add	r12,rbp
	adc	rdx,0
	xor	r8,r8
	add	r12,rax
	mov	rax,r9
	adc	r13,rdx
	adc	r8,0


	mul	QWORD[r14]
	mov	rbp,r9
	add	rcx,rax
	mov	rax,r9
	adc	rcx,rdx

	sub	r11,r9
	sbb	r9,0

	mul	QWORD[8+r14]
	add	r10,rcx
	adc	rdx,0
	add	r10,rax
	mov	rax,rbp
	adc	r11,rdx
	mov	rdx,rbp
	adc	r9,0

	shl	rax,32
	shr	rdx,32
	sub	r12,rax
	mov	rax,QWORD[16+rbx]
	sbb	rbp,rdx

	add	r12,r9
	adc	r13,rbp
	adc	r8,0


	mov	rcx,rax
	mul	QWORD[rsi]
	add	r10,rax
	mov	rax,rcx
	adc	rdx,0
	mov	rbp,rdx

	mul	QWORD[8+rsi]
	add	r11,rbp
	adc	rdx,0
	add	r11,rax
	mov	rax,rcx
	adc	rdx,0
	mov	rbp,rdx

	mul	QWORD[16+rsi]
	add	r12,rbp
	adc	rdx,0
	add	r12,rax
	mov	rax,rcx
	adc	rdx,0

	mov	rcx,r10
	imul	r10,r15

	mov	rbp,rdx
	mul	QWORD[24+rsi]
	add	r13,rbp
	adc	rdx,0
	xor	r9,r9
	add	r13,rax
	mov	rax,r10
	adc	r8,rdx
	adc	r9,0


	mul	QWORD[r14]
	mov	rbp,r10
	add	rcx,rax
	mov	rax,r10
	adc	rcx,rdx

	sub	r12,r10
	sbb	r10,0

	mul	QWORD[8+r14]
	add	r11,rcx
	adc	rdx,0
	add	r11,rax
	mov	rax,rbp
	adc	r12,rdx
	mov	rdx,rbp
	adc	r10,0

	shl	rax,32
	shr	rdx,32
	sub	r13,rax
	mov	rax,QWORD[24+rbx]
	sbb	rbp,rdx

	add	r13,r10
	adc	r8,rbp
	adc	r9,0


	mov	rcx,rax
	mul	QWORD[rsi]
	add	r11,rax
	mov	rax,rcx
	adc	rdx,0
	mov	rbp,rdx

	mul	QWORD[8+rsi]
	add	r12,rbp
	adc	rdx,0
	add	r12,rax
	mov	rax,rcx
	adc	rdx,0
	mov	rbp,rdx

	mul	QWORD[16+rsi]
	add	r13,rbp
	adc	rdx,0
	add	r13,rax
	mov	rax,rcx
	adc	rdx,0

	mov	rcx,r11
	imul	r11,r15

	mov	rbp,rdx
	mul	QWORD[24+rsi]
	add	r8,rbp
	adc	rdx,0
	xor	r10,r10
	add	r8,rax
	mov	rax,r11
	adc	r9,rdx
	adc	r10,0


	mul	QWORD[r14]
	mov	rbp,r11
	add	rcx,rax
	mov	rax,r11
	adc	rcx,rdx

	sub	r13,r11
	sbb	r11,0

	mul	QWORD[8+r14]
	add	r12,rcx
	adc	rdx,0
	add	r12,rax
	mov	rax,rbp
	adc	r13,rdx
	mov	rdx,rbp
	adc	r11,0

	shl	rax,32
	shr	rdx,32
	sub	r8,rax
	sbb	rbp,rdx

	add	r8,r11
	adc	r9,rbp
	adc	r10,0


	mov	rsi,r12
	sub	r12,QWORD[r14]
	mov	r11,r13
	sbb	r13,QWORD[8+r14]
	mov	rcx,r8
	sbb	r8,QWORD[16+r14]
	mov	rbp,r9
	sbb	r9,QWORD[24+r14]
	sbb	r10,0

	cmovc	r12,rsi
	cmovc	r13,r11
	cmovc	r8,rcx
	cmovc	r9,rbp

	mov	QWORD[rdi],r12
	mov	QWORD[8+rdi],r13
	mov	QWORD[16+rdi],r8
	mov	QWORD[24+rdi],r9

	mov	r15,QWORD[rsp]

	mov	r14,QWORD[8+rsp]

	mov	r13,QWORD[16+rsp]

	mov	r12,QWORD[24+rsp]

	mov	rbx,QWORD[32+rsp]

	mov	rbp,QWORD[40+rsp]

	lea	rsp,[48+rsp]

$L$ord_mul_epilogue:
	mov	rdi,QWORD[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD[16+rsp]
	ret

$L$SEH_end_ecp_nistz256_ord_mul_mont_nohw:







global	ecp_nistz256_ord_sqr_mont_nohw

ALIGN	32
ecp_nistz256_ord_sqr_mont_nohw:
	mov	QWORD[8+rsp],rdi	;WIN64 prologue
	mov	QWORD[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_ecp_nistz256_ord_sqr_mont_nohw:
	mov	rdi,rcx
	mov	rsi,rdx
	mov	rdx,r8



_CET_ENDBR
	push	rbp

	push	rbx

	push	r12

	push	r13

	push	r14

	push	r15

$L$ord_sqr_body:

	mov	r8,QWORD[rsi]
	mov	rax,QWORD[8+rsi]
	mov	r14,QWORD[16+rsi]
	mov	r15,QWORD[24+rsi]
	lea	rsi,[$L$ord]
	mov	rbx,rdx
	jmp	NEAR $L$oop_ord_sqr

ALIGN	32
$L$oop_ord_sqr:

	mov	rbp,rax
	mul	r8
	mov	r9,rax
	movq	xmm1,rbp
	mov	rax,r14
	mov	r10,rdx

	mul	r8
	add	r10,rax
	mov	rax,r15
	movq	xmm2,r14
	adc	rdx,0
	mov	r11,rdx

	mul	r8
	add	r11,rax
	mov	rax,r15
	movq	xmm3,r15
	adc	rdx,0
	mov	r12,rdx


	mul	r14
	mov	r13,rax
	mov	rax,r14
	mov	r14,rdx


	mul	rbp
	add	r11,rax
	mov	rax,r15
	adc	rdx,0
	mov	r15,rdx

	mul	rbp
	add	r12,rax
	adc	rdx,0

	add	r12,r15
	adc	r13,rdx
	adc	r14,0


	xor	r15,r15
	mov	rax,r8
	add	r9,r9
	adc	r10,r10
	adc	r11,r11
	adc	r12,r12
	adc	r13,r13
	adc	r14,r14
	adc	r15,0


	mul	rax
	mov	r8,rax
	movq	rax,xmm1
	mov	rbp,rdx

	mul	rax
	add	r9,rbp
	adc	r10,rax
	movq	rax,xmm2
	adc	rdx,0
	mov	rbp,rdx

	mul	rax
	add	r11,rbp
	adc	r12,rax
	movq	rax,xmm3
	adc	rdx,0
	mov	rbp,rdx

	mov	rcx,r8
	imul	r8,QWORD[32+rsi]

	mul	rax
	add	r13,rbp
	adc	r14,rax
	mov	rax,QWORD[rsi]
	adc	r15,rdx


	mul	r8
	mov	rbp,r8
	add	rcx,rax
	mov	rax,QWORD[8+rsi]
	adc	rcx,rdx

	sub	r10,r8
	sbb	rbp,0

	mul	r8
	add	r9,rcx
	adc	rdx,0
	add	r9,rax
	mov	rax,r8
	adc	r10,rdx
	mov	rdx,r8
	adc	rbp,0

	mov	rcx,r9
	imul	r9,QWORD[32+rsi]

	shl	rax,32
	shr	rdx,32
	sub	r11,rax
	mov	rax,QWORD[rsi]
	sbb	r8,rdx

	add	r11,rbp
	adc	r8,0


	mul	r9
	mov	rbp,r9
	add	rcx,rax
	mov	rax,QWORD[8+rsi]
	adc	rcx,rdx

	sub	r11,r9
	sbb	rbp,0

	mul	r9
	add	r10,rcx
	adc	rdx,0
	add	r10,rax
	mov	rax,r9
	adc	r11,rdx
	mov	rdx,r9
	adc	rbp,0

	mov	rcx,r10
	imul	r10,QWORD[32+rsi]

	shl	rax,32
	shr	rdx,32
	sub	r8,rax
	mov	rax,QWORD[rsi]
	sbb	r9,rdx

	add	r8,rbp
	adc	r9,0


	mul	r10
	mov	rbp,r10
	add	rcx,rax
	mov	rax,QWORD[8+rsi]
	adc	rcx,rdx

	sub	r8,r10
	sbb	rbp,0

	mul	r10
	add	r11,rcx
	adc	rdx,0
	add	r11,rax
	mov	rax,r10
	adc	r8,rdx
	mov	rdx,r10
	adc	rbp,0

	mov	rcx,r11
	imul	r11,QWORD[32+rsi]

	shl	rax,32
	shr	rdx,32
	sub	r9,rax
	mov	rax,QWORD[rsi]
	sbb	r10,rdx

	add	r9,rbp
	adc	r10,0


	mul	r11
	mov	rbp,r11
	add	rcx,rax
	mov	rax,QWORD[8+rsi]
	adc	rcx,rdx

	sub	r9,r11
	sbb	rbp,0

	mul	r11
	add	r8,rcx
	adc	rdx,0
	add	r8,rax
	mov	rax,r11
	adc	r9,rdx
	mov	rdx,r11
	adc	rbp,0

	shl	rax,32
	shr	rdx,32
	sub	r10,rax
	sbb	r11,rdx

	add	r10,rbp
	adc	r11,0


	xor	rdx,rdx
	add	r8,r12
	adc	r9,r13
	mov	r12,r8
	adc	r10,r14
	adc	r11,r15
	mov	rax,r9
	adc	rdx,0


	sub	r8,QWORD[rsi]
	mov	r14,r10
	sbb	r9,QWORD[8+rsi]
	sbb	r10,QWORD[16+rsi]
	mov	r15,r11
	sbb	r11,QWORD[24+rsi]
	sbb	rdx,0

	cmovc	r8,r12
	cmovnc	rax,r9
	cmovnc	r14,r10
	cmovnc	r15,r11

	dec	rbx
	jnz	NEAR $L$oop_ord_sqr

	mov	QWORD[rdi],r8
	mov	QWORD[8+rdi],rax
	pxor	xmm1,xmm1
	mov	QWORD[16+rdi],r14
	pxor	xmm2,xmm2
	mov	QWORD[24+rdi],r15
	pxor	xmm3,xmm3

	mov	r15,QWORD[rsp]

	mov	r14,QWORD[8+rsp]

	mov	r13,QWORD[16+rsp]

	mov	r12,QWORD[24+rsp]

	mov	rbx,QWORD[32+rsp]

	mov	rbp,QWORD[40+rsp]

	lea	rsp,[48+rsp]

$L$ord_sqr_epilogue:
	mov	rdi,QWORD[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD[16+rsp]
	ret

$L$SEH_end_ecp_nistz256_ord_sqr_mont_nohw:

global	ecp_nistz256_ord_mul_mont_adx

ALIGN	32
ecp_nistz256_ord_mul_mont_adx:
	mov	QWORD[8+rsp],rdi	;WIN64 prologue
	mov	QWORD[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_ecp_nistz256_ord_mul_mont_adx:
	mov	rdi,rcx
	mov	rsi,rdx
	mov	rdx,r8



$L$ecp_nistz256_ord_mul_mont_adx:
_CET_ENDBR
	push	rbp

	push	rbx

	push	r12

	push	r13

	push	r14

	push	r15

$L$ord_mulx_body:

	mov	rbx,rdx
	mov	rdx,QWORD[rdx]
	mov	r9,QWORD[rsi]
	mov	r10,QWORD[8+rsi]
	mov	r11,QWORD[16+rsi]
	mov	r12,QWORD[24+rsi]
	lea	rsi,[((-128))+rsi]
	lea	r14,[(($L$ord-128))]
	mov	r15,QWORD[$L$ordK]


	mulx	r9,r8,r9
	mulx	r10,rcx,r10
	mulx	r11,rbp,r11
	add	r9,rcx
	mulx	r12,rcx,r12
	mov	rdx,r8
	mulx	rax,rdx,r15
	adc	r10,rbp
	adc	r11,rcx
	adc	r12,0


	xor	r13,r13
	mulx	rbp,rcx,QWORD[((0+128))+r14]
	adcx	r8,rcx
	adox	r9,rbp

	mulx	rbp,rcx,QWORD[((8+128))+r14]
	adcx	r9,rcx
	adox	r10,rbp

	mulx	rbp,rcx,QWORD[((16+128))+r14]
	adcx	r10,rcx
	adox	r11,rbp

	mulx	rbp,rcx,QWORD[((24+128))+r14]
	mov	rdx,QWORD[8+rbx]
	adcx	r11,rcx
	adox	r12,rbp
	adcx	r12,r8
	adox	r13,r8
	adc	r13,0


	mulx	rbp,rcx,QWORD[((0+128))+rsi]
	adcx	r9,rcx
	adox	r10,rbp

	mulx	rbp,rcx,QWORD[((8+128))+rsi]
	adcx	r10,rcx
	adox	r11,rbp

	mulx	rbp,rcx,QWORD[((16+128))+rsi]
	adcx	r11,rcx
	adox	r12,rbp

	mulx	rbp,rcx,QWORD[((24+128))+rsi]
	mov	rdx,r9
	mulx	rax,rdx,r15
	adcx	r12,rcx
	adox	r13,rbp

	adcx	r13,r8
	adox	r8,r8
	adc	r8,0


	mulx	rbp,rcx,QWORD[((0+128))+r14]
	adcx	r9,rcx
	adox	r10,rbp

	mulx	rbp,rcx,QWORD[((8+128))+r14]
	adcx	r10,rcx
	adox	r11,rbp

	mulx	rbp,rcx,QWORD[((16+128))+r14]
	adcx	r11,rcx
	adox	r12,rbp

	mulx	rbp,rcx,QWORD[((24+128))+r14]
	mov	rdx,QWORD[16+rbx]
	adcx	r12,rcx
	adox	r13,rbp
	adcx	r13,r9
	adox	r8,r9
	adc	r8,0


	mulx	rbp,rcx,QWORD[((0+128))+rsi]
	adcx	r10,rcx
	adox	r11,rbp

	mulx	rbp,rcx,QWORD[((8+128))+rsi]
	adcx	r11,rcx
	adox	r12,rbp

	mulx	rbp,rcx,QWORD[((16+128))+rsi]
	adcx	r12,rcx
	adox	r13,rbp

	mulx	rbp,rcx,QWORD[((24+128))+rsi]
	mov	rdx,r10
	mulx	rax,rdx,r15
	adcx	r13,rcx
	adox	r8,rbp

	adcx	r8,r9
	adox	r9,r9
	adc	r9,0


	mulx	rbp,rcx,QWORD[((0+128))+r14]
	adcx	r10,rcx
	adox	r11,rbp

	mulx	rbp,rcx,QWORD[((8+128))+r14]
	adcx	r11,rcx
	adox	r12,rbp

	mulx	rbp,rcx,QWORD[((16+128))+r14]
	adcx	r12,rcx
	adox	r13,rbp

	mulx	rbp,rcx,QWORD[((24+128))+r14]
	mov	rdx,QWORD[24+rbx]
	adcx	r13,rcx
	adox	r8,rbp
	adcx	r8,r10
	adox	r9,r10
	adc	r9,0


	mulx	rbp,rcx,QWORD[((0+128))+rsi]
	adcx	r11,rcx
	adox	r12,rbp

	mulx	rbp,rcx,QWORD[((8+128))+rsi]
	adcx	r12,rcx
	adox	r13,rbp

	mulx	rbp,rcx,QWORD[((16+128))+rsi]
	adcx	r13,rcx
	adox	r8,rbp

	mulx	rbp,rcx,QWORD[((24+128))+rsi]
	mov	rdx,r11
	mulx	rax,rdx,r15
	adcx	r8,rcx
	adox	r9,rbp

	adcx	r9,r10
	adox	r10,r10
	adc	r10,0


	mulx	rbp,rcx,QWORD[((0+128))+r14]
	adcx	r11,rcx
	adox	r12,rbp

	mulx	rbp,rcx,QWORD[((8+128))+r14]
	adcx	r12,rcx
	adox	r13,rbp

	mulx	rbp,rcx,QWORD[((16+128))+r14]
	adcx	r13,rcx
	adox	r8,rbp

	mulx	rbp,rcx,QWORD[((24+128))+r14]
	lea	r14,[128+r14]
	mov	rbx,r12
	adcx	r8,rcx
	adox	r9,rbp
	mov	rdx,r13
	adcx	r9,r11
	adox	r10,r11
	adc	r10,0



	mov	rcx,r8
	sub	r12,QWORD[r14]
	sbb	r13,QWORD[8+r14]
	sbb	r8,QWORD[16+r14]
	mov	rbp,r9
	sbb	r9,QWORD[24+r14]
	sbb	r10,0

	cmovc	r12,rbx
	cmovc	r13,rdx
	cmovc	r8,rcx
	cmovc	r9,rbp

	mov	QWORD[rdi],r12
	mov	QWORD[8+rdi],r13
	mov	QWORD[16+rdi],r8
	mov	QWORD[24+rdi],r9

	mov	r15,QWORD[rsp]

	mov	r14,QWORD[8+rsp]

	mov	r13,QWORD[16+rsp]

	mov	r12,QWORD[24+rsp]

	mov	rbx,QWORD[32+rsp]

	mov	rbp,QWORD[40+rsp]

	lea	rsp,[48+rsp]

$L$ord_mulx_epilogue:
	mov	rdi,QWORD[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD[16+rsp]
	ret

$L$SEH_end_ecp_nistz256_ord_mul_mont_adx:

global	ecp_nistz256_ord_sqr_mont_adx

ALIGN	32
ecp_nistz256_ord_sqr_mont_adx:
	mov	QWORD[8+rsp],rdi	;WIN64 prologue
	mov	QWORD[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_ecp_nistz256_ord_sqr_mont_adx:
	mov	rdi,rcx
	mov	rsi,rdx
	mov	rdx,r8



_CET_ENDBR
$L$ecp_nistz256_ord_sqr_mont_adx:
	push	rbp

	push	rbx

	push	r12

	push	r13

	push	r14

	push	r15

$L$ord_sqrx_body:

	mov	rbx,rdx
	mov	rdx,QWORD[rsi]
	mov	r14,QWORD[8+rsi]
	mov	r15,QWORD[16+rsi]
	mov	r8,QWORD[24+rsi]
	lea	rsi,[$L$ord]
	jmp	NEAR $L$oop_ord_sqrx

ALIGN	32
$L$oop_ord_sqrx:
	mulx	r10,r9,r14
	mulx	r11,rcx,r15
	mov	rax,rdx
	movq	xmm1,r14
	mulx	r12,rbp,r8
	mov	rdx,r14
	add	r10,rcx
	movq	xmm2,r15
	adc	r11,rbp
	adc	r12,0
	xor	r13,r13

	mulx	rbp,rcx,r15
	adcx	r11,rcx
	adox	r12,rbp

	mulx	rbp,rcx,r8
	mov	rdx,r15
	adcx	r12,rcx
	adox	r13,rbp
	adc	r13,0

	mulx	r14,rcx,r8
	mov	rdx,rax
	movq	xmm3,r8
	xor	r15,r15
	adcx	r9,r9
	adox	r13,rcx
	adcx	r10,r10
	adox	r14,r15


	mulx	rbp,r8,rdx
	movq	rdx,xmm1
	adcx	r11,r11
	adox	r9,rbp
	adcx	r12,r12
	mulx	rax,rcx,rdx
	movq	rdx,xmm2
	adcx	r13,r13
	adox	r10,rcx
	adcx	r14,r14
	mulx	rbp,rcx,rdx
	DB	0x67
	movq	rdx,xmm3
	adox	r11,rax
	adcx	r15,r15
	adox	r12,rcx
	adox	r13,rbp
	mulx	rax,rcx,rdx
	adox	r14,rcx
	adox	r15,rax


	mov	rdx,r8
	mulx	rcx,rdx,QWORD[32+rsi]

	xor	rax,rax
	mulx	rbp,rcx,QWORD[rsi]
	adcx	r8,rcx
	adox	r9,rbp
	mulx	rbp,rcx,QWORD[8+rsi]
	adcx	r9,rcx
	adox	r10,rbp
	mulx	rbp,rcx,QWORD[16+rsi]
	adcx	r10,rcx
	adox	r11,rbp
	mulx	rbp,rcx,QWORD[24+rsi]
	adcx	r11,rcx
	adox	r8,rbp
	adcx	r8,rax


	mov	rdx,r9
	mulx	rcx,rdx,QWORD[32+rsi]

	mulx	rbp,rcx,QWORD[rsi]
	adox	r9,rcx
	adcx	r10,rbp
	mulx	rbp,rcx,QWORD[8+rsi]
	adox	r10,rcx
	adcx	r11,rbp
	mulx	rbp,rcx,QWORD[16+rsi]
	adox	r11,rcx
	adcx	r8,rbp
	mulx	rbp,rcx,QWORD[24+rsi]
	adox	r8,rcx
	adcx	r9,rbp
	adox	r9,rax


	mov	rdx,r10
	mulx	rcx,rdx,QWORD[32+rsi]

	mulx	rbp,rcx,QWORD[rsi]
	adcx	r10,rcx
	adox	r11,rbp
	mulx	rbp,rcx,QWORD[8+rsi]
	adcx	r11,rcx
	adox	r8,rbp
	mulx	rbp,rcx,QWORD[16+rsi]
	adcx	r8,rcx
	adox	r9,rbp
	mulx	rbp,rcx,QWORD[24+rsi]
	adcx	r9,rcx
	adox	r10,rbp
	adcx	r10,rax


	mov	rdx,r11
	mulx	rcx,rdx,QWORD[32+rsi]

	mulx	rbp,rcx,QWORD[rsi]
	adox	r11,rcx
	adcx	r8,rbp
	mulx	rbp,rcx,QWORD[8+rsi]
	adox	r8,rcx
	adcx	r9,rbp
	mulx	rbp,rcx,QWORD[16+rsi]
	adox	r9,rcx
	adcx	r10,rbp
	mulx	rbp,rcx,QWORD[24+rsi]
	adox	r10,rcx
	adcx	r11,rbp
	adox	r11,rax


	add	r12,r8
	adc	r9,r13
	mov	rdx,r12
	adc	r10,r14
	adc	r11,r15
	mov	r14,r9
	adc	rax,0


	sub	r12,QWORD[rsi]
	mov	r15,r10
	sbb	r9,QWORD[8+rsi]
	sbb	r10,QWORD[16+rsi]
	mov	r8,r11
	sbb	r11,QWORD[24+rsi]
	sbb	rax,0

	cmovnc	rdx,r12
	cmovnc	r14,r9
	cmovnc	r15,r10
	cmovnc	r8,r11

	dec	rbx
	jnz	NEAR $L$oop_ord_sqrx

	mov	QWORD[rdi],rdx
	mov	QWORD[8+rdi],r14
	pxor	xmm1,xmm1
	mov	QWORD[16+rdi],r15
	pxor	xmm2,xmm2
	mov	QWORD[24+rdi],r8
	pxor	xmm3,xmm3

	mov	r15,QWORD[rsp]

	mov	r14,QWORD[8+rsp]

	mov	r13,QWORD[16+rsp]

	mov	r12,QWORD[24+rsp]

	mov	rbx,QWORD[32+rsp]

	mov	rbp,QWORD[40+rsp]

	lea	rsp,[48+rsp]

$L$ord_sqrx_epilogue:
	mov	rdi,QWORD[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD[16+rsp]
	ret

$L$SEH_end_ecp_nistz256_ord_sqr_mont_adx:
EXTERN	__imp_RtlVirtualUnwind


ALIGN	16
short_handler:
	push	rsi
	push	rdi
	push	rbx
	push	rbp
	push	r12
	push	r13
	push	r14
	push	r15
	pushfq
	sub	rsp,64

	mov	rax,QWORD[120+r8]
	mov	rbx,QWORD[248+r8]

	mov	rsi,QWORD[8+r9]
	mov	r11,QWORD[56+r9]

	mov	r10d,DWORD[r11]
	lea	r10,[r10*1+rsi]
	cmp	rbx,r10
	jb	NEAR $L$common_seh_tail

	mov	rax,QWORD[152+r8]

	mov	r10d,DWORD[4+r11]
	lea	r10,[r10*1+rsi]
	cmp	rbx,r10
	jae	NEAR $L$common_seh_tail

	lea	rax,[16+rax]

	mov	r12,QWORD[((-8))+rax]
	mov	r13,QWORD[((-16))+rax]
	mov	QWORD[216+r8],r12
	mov	QWORD[224+r8],r13

	jmp	NEAR $L$common_seh_tail



ALIGN	16
full_handler:
	push	rsi
	push	rdi
	push	rbx
	push	rbp
	push	r12
	push	r13
	push	r14
	push	r15
	pushfq
	sub	rsp,64

	mov	rax,QWORD[120+r8]
	mov	rbx,QWORD[248+r8]

	mov	rsi,QWORD[8+r9]
	mov	r11,QWORD[56+r9]

	mov	r10d,DWORD[r11]
	lea	r10,[r10*1+rsi]
	cmp	rbx,r10
	jb	NEAR $L$common_seh_tail

	mov	rax,QWORD[152+r8]

	mov	r10d,DWORD[4+r11]
	lea	r10,[r10*1+rsi]
	cmp	rbx,r10
	jae	NEAR $L$common_seh_tail

	mov	r10d,DWORD[8+r11]
	lea	rax,[r10*1+rax]

	mov	rbp,QWORD[((-8))+rax]
	mov	rbx,QWORD[((-16))+rax]
	mov	r12,QWORD[((-24))+rax]
	mov	r13,QWORD[((-32))+rax]
	mov	r14,QWORD[((-40))+rax]
	mov	r15,QWORD[((-48))+rax]
	mov	QWORD[144+r8],rbx
	mov	QWORD[160+r8],rbp
	mov	QWORD[216+r8],r12
	mov	QWORD[224+r8],r13
	mov	QWORD[232+r8],r14
	mov	QWORD[240+r8],r15

$L$common_seh_tail:
	mov	rdi,QWORD[8+rax]
	mov	rsi,QWORD[16+rax]
	mov	QWORD[152+r8],rax
	mov	QWORD[168+r8],rsi
	mov	QWORD[176+r8],rdi

	mov	rdi,QWORD[40+r9]
	mov	rsi,r8
	mov	ecx,154
	DD	0xa548f3fc

	mov	rsi,r9
	xor	rcx,rcx
	mov	rdx,QWORD[8+rsi]
	mov	r8,QWORD[rsi]
	mov	r9,QWORD[16+rsi]
	mov	r10,QWORD[40+rsi]
	lea	r11,[56+rsi]
	lea	r12,[24+rsi]
	mov	QWORD[32+rsp],r10
	mov	QWORD[40+rsp],r11
	mov	QWORD[48+rsp],r12
	mov	QWORD[56+rsp],rcx
	call	QWORD[__imp_RtlVirtualUnwind]

	mov	eax,1
	add	rsp,64
	popfq
	pop	r15
	pop	r14
	pop	r13
	pop	r12
	pop	rbp
	pop	rbx
	pop	rdi
	pop	rsi
	ret


section	.pdata rdata align=4
ALIGN	4
	DD	$L$SEH_begin_ecp_nistz256_ord_mul_mont_nohw wrt ..imagebase
	DD	$L$SEH_end_ecp_nistz256_ord_mul_mont_nohw wrt ..imagebase
	DD	$L$SEH_info_ecp_nistz256_ord_mul_mont_nohw wrt ..imagebase

	DD	$L$SEH_begin_ecp_nistz256_ord_sqr_mont_nohw wrt ..imagebase
	DD	$L$SEH_end_ecp_nistz256_ord_sqr_mont_nohw wrt ..imagebase
	DD	$L$SEH_info_ecp_nistz256_ord_sqr_mont_nohw wrt ..imagebase
	DD	$L$SEH_begin_ecp_nistz256_ord_mul_mont_adx wrt ..imagebase
	DD	$L$SEH_end_ecp_nistz256_ord_mul_mont_adx wrt ..imagebase
	DD	$L$SEH_info_ecp_nistz256_ord_mul_mont_adx wrt ..imagebase

	DD	$L$SEH_begin_ecp_nistz256_ord_sqr_mont_adx wrt ..imagebase
	DD	$L$SEH_end_ecp_nistz256_ord_sqr_mont_adx wrt ..imagebase
	DD	$L$SEH_info_ecp_nistz256_ord_sqr_mont_adx wrt ..imagebase

section	.xdata rdata align=8
ALIGN	8
$L$SEH_info_ecp_nistz256_ord_mul_mont_nohw:
	DB	9,0,0,0
	DD	full_handler wrt ..imagebase
	DD	$L$ord_mul_body wrt ..imagebase,$L$ord_mul_epilogue wrt ..imagebase
	DD	48,0
$L$SEH_info_ecp_nistz256_ord_sqr_mont_nohw:
	DB	9,0,0,0
	DD	full_handler wrt ..imagebase
	DD	$L$ord_sqr_body wrt ..imagebase,$L$ord_sqr_epilogue wrt ..imagebase
	DD	48,0
$L$SEH_info_ecp_nistz256_ord_mul_mont_adx:
	DB	9,0,0,0
	DD	full_handler wrt ..imagebase
	DD	$L$ord_mulx_body wrt ..imagebase,$L$ord_mulx_epilogue wrt ..imagebase
	DD	48,0
$L$SEH_info_ecp_nistz256_ord_sqr_mont_adx:
	DB	9,0,0,0
	DD	full_handler wrt ..imagebase
	DD	$L$ord_sqrx_body wrt ..imagebase,$L$ord_sqrx_epilogue wrt ..imagebase
	DD	48,0
%else
; Work around https://bugzilla.nasm.us/show_bug.cgi?id=3392738
ret
%endif
