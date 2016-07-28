; Copyright (c) 2011 The Chromium Authors. All rights reserved.
; Use of this source code is governed by a BSD-style license that can be
; found in the LICENSE file.

  global    mangle(SYMBOL) PRIVATE
  align     function_align

mangle(SYMBOL):
  %assign   stack_offset 0

  extern    mangle(kCoefficientsRgbY)

; Parameters are in the following order:
; 1. Y plane
; 2. U plane
; 3. V plane
; 4. ARGB frame
; 5. Width
; 6. Source dx

PROLOGUE  6, 7, 3, Y, U, V, ARGB, R1, R2, TEMP

%ifdef ARCH_X86_64
%define     WORD_SIZE   QWORD
%else
%define     WORD_SIZE   DWORD
%endif

%ifdef PIC
  PUSH      R1q  ; Width
%endif
  PUSH      R2q  ; Source dx

%define     SOURCE_DX   WORD_SIZE [rsp]

; PIC code.
%ifdef PIC
  LOAD_SYM  R1q, mangle(kCoefficientsRgbY)
%define     WIDTH       WORD_SIZE [rsp + gprsize]
%define     TABLE       R1q  
%define     Xq           R2q

; Non-PIC code.
%else
%define     WIDTH       R1q
%define     TABLE       mangle(kCoefficientsRgbY)
%define     Xq           R2q
%endif

  ; Set Xq index to 0.
  xor       Xq, Xq
  jmp       .scaleend

.scaleloop:
  ; TABLE can either be a register or a symbol depending on this is
  ; PIC or not.
  mov       TEMPq, Xq
  sar       TEMPq, 17
  movzx     TEMPd, BYTE [Uq + TEMPq]
  movq      mm0, [TABLE + 2048 + 8 * TEMPq]
  mov       TEMPq, Xq
  sar       TEMPq, 17
  movzx     TEMPd, BYTE [Vq + TEMPq]
  paddsw    mm0, [TABLE + 4096 + 8 * TEMPq]
  mov       TEMPq, Xq
  add       Xq, SOURCE_DX
  sar       TEMPq, 16
  movzx     TEMPd, BYTE [Yq + TEMPq]
  movq      mm1, [TABLE + 8 * TEMPq]
  mov       TEMPq, Xq
  add       Xq, SOURCE_DX
  sar       TEMPq, 16
  movzx     TEMPd, BYTE [Yq + TEMPq]
  movq      mm2, [TABLE + 8 * TEMPq]
  paddsw    mm1, mm0
  paddsw    mm2, mm0
  psraw     mm1, 6
  psraw     mm2, 6
  packuswb  mm1, mm2
  MOVQ      QWORD [ARGBq], mm1
  add       ARGBq, 8

.scaleend:
  ; WIDTH can either be a register or memory depending on this is
  ; PIC or not.
  sub       WIDTH, 2
  jns       .scaleloop

  and       WIDTH, 1             ; odd number of pixels?
  jz        .scaledone

  mov       TEMPq, Xq
  sar       TEMPq, 17
  movzx     TEMPd, BYTE [Uq + TEMPq]
  movq      mm0, [TABLE + 2048 + 8 * TEMPq]
  mov       TEMPq, Xq
  sar       TEMPq, 17
  movzx     TEMPd, BYTE [Vq + TEMPq]
  paddsw    mm0, [TABLE + 4096 + 8 * TEMPq]
  mov       TEMPq, Xq
  sar       TEMPq, 16
  movzx     TEMPd, BYTE [Yq + TEMPq]
  movq      mm1, [TABLE + 8 * TEMPq]
  paddsw    mm1, mm0
  psraw     mm1, 6
  packuswb  mm1, mm1
  movd      DWORD [ARGBq], mm1

.scaledone:
%ifdef PIC
  ADD       rsp, 2 * gprsize
%else
  ADD       rsp, gprsize
%endif
  RET
