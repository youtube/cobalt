   # With glibc headers, we cannot check __hwcap yet
   # Which is fine in this case, as the XMM registers
   # were introduced in CPUs starting in 1999.
.hidden __hwcap

.global feclearexcept
.type feclearexcept,@function
feclearexcept:	
	mov 4(%esp),%ecx
	and $0x3f,%ecx
	fnstsw %ax
	# consider sse fenv as well if the cpu has XMM capability
	call 1f
1:	addl $__hwcap-1b,(%esp)
	pop %edx
	testl $0x02000000,(%edx)
	jz 2f # Jump to x87 only path if no XMM
	# SSE path: maintain exceptions in mxcsr, clear x87 if needed
	test %eax,%ecx # Check if any relevant x87 flags are set that need clearing
	jz 1f         # If no relevant x87 flags, skip fnclex
	fnclex        # Clear x87 exceptions
1:	push %edx     # Save edx (original __hwcap value or garbage)
	stmxcsr (%esp)  # Store MXCSR to stack
	pop %edx      # Restore edx (now contains MXCSR value)
	and $0x3f,%eax  # Mask relevant x87 status bits
	or %eax,%edx    # Combine x87 status with MXCSR
	test %edx,%ecx  # Test if any of the exceptions to be cleared are set in combined status
	jz 1f         # If not, MXCSR is fine
	not %ecx      # Invert mask (bits to keep)
	and %ecx,%edx   # Clear relevant bits in MXCSR value
	push %edx     # Push modified MXCSR
	ldmxcsr (%esp)  # Load it
	pop %edx      # Clean stack
1:	xor %eax,%eax # feclearexcept returns 0
	ret

	# x87 only path (no XMM capability)
2:	test %eax,%ecx # Check if any relevant x87 flags are set
	jz 1f         # If not, nothing to do
	# x87 exception clearing logic (original block for when COBALT_MUSL_W_GLIBC_HEADERS was defined)
	# This is effectively the fallback if SSE is not available or no SSE flags needed clearing.
	# However, the primary path above now handles combined SSE & x87 clearing.
	# The original code had "not %ecx; and %ecx,%eax; test $0x3f,%eax; jz 1f; fnclex"
	# This seems to be for clearing specific flags in x87.
	# If we are here, it means __hwcap test failed (jz 2f)
	# So we only operate on x87 flags.
	not %ecx
	and %ecx,%eax # eax had original sw, inverted ecx is mask of flags to keep
	              # so this clears the flags specified in original ecx from eax
	test $0x3f,%eax # check if any of the *other* x87 flags are set
	jz 1f           # if no *other* flags are set, we might be done or just cleared them
	fnclex          # clear all x87 exceptions if some were set that we were asked to clear
	jmp 1f

.global feraiseexcept
.type feraiseexcept,@function
feraiseexcept:	
	mov 4(%esp),%eax
	and $0x3f,%eax
	sub $32,%esp
	fnstenv (%esp)
	or %al,4(%esp)
	fldenv (%esp)
	add $32,%esp
	xor %eax,%eax
	ret

.global __fesetround
.hidden __fesetround
.type __fesetround,@function
__fesetround:
	mov 4(%esp),%ecx
	push %eax
	xor %eax,%eax
	fnstcw (%esp)
	andb $0xf3,1(%esp)
	or %ch,1(%esp)
	fldcw (%esp)
	# consider sse fenv as well if the cpu has XMM capability
	call 1f
1:	addl $__hwcap-1b,(%esp)
	pop %edx
	testl $0x02000000,(%edx)
	jz 1f
	stmxcsr (%esp)
	shl $3,%ch
	andb $0x9f,1(%esp)
	or %ch,1(%esp)
	ldmxcsr (%esp)
1:	pop %ecx
	ret

.global fegetround
.type fegetround,@function
fegetround:
	push %eax
	fnstcw (%esp)
	pop %eax
	and $0xc00,%eax
	ret

.global fegetenv
.type fegetenv,@function
fegetenv:
	mov 4(%esp),%ecx
	xor %eax,%eax
	fnstenv (%ecx)
	# consider sse fenv as well if the cpu has XMM capability
	call 1f
1:	addl $__hwcap-1b,(%esp)
	pop %edx
	testl $0x02000000,(%edx)
	jz 1f
	push %eax
	stmxcsr (%esp)
	pop %edx
	and $0x3f,%edx
	or %edx,4(%ecx)
1:	ret

.global fesetenv
.type fesetenv,@function
fesetenv:
	mov 4(%esp),%ecx
	xor %eax,%eax
	inc %ecx
	jz 1f
	fldenv -1(%ecx)
	movl -1(%ecx),%ecx
	jmp 2f
1:	push %eax
	push %eax
	push %eax
	push %eax
	pushl $0xffff
	push %eax
	pushl $0x37f
	fldenv (%esp)
	add $28,%esp
	# consider sse fenv as well if the cpu has XMM capability
	call 1f
1:	addl $__hwcap-1b,(%esp)
	pop %edx
	testl $0x02000000,(%edx)
	jz 1f
	# mxcsr := same rounding mode, cleared exceptions, default mask
	and $0xc00,%ecx
	shl $3,%ecx
	or $0x1f80,%ecx
	mov %ecx,4(%esp)
	ldmxcsr 4(%esp)
1:	ret

.global fetestexcept
.type fetestexcept,@function
fetestexcept:
	mov 4(%esp),%ecx
	and $0x3f,%ecx
	fnstsw %ax
	# consider sse fenv as well if the cpu has XMM capability
	call 1f
1:	addl $__hwcap-1b,(%esp)
	pop %edx
	testl $0x02000000,(%edx)
	jz 1f
	stmxcsr 4(%esp)
	or 4(%esp),%eax
1:	and %ecx,%eax
	ret
