; Copyright (c) 2011 The Chromium Authors. All rights reserved.
; Use of this source code is governed by a BSD-style license that can be
; found in the LICENSE file.

  global    mangle(SYMBOL) PRIVATE
  align     function_align

; Non-PIC code is the fastest so use this if possible.
%ifndef PIC
mangle(SYMBOL):
  %assign   stack_offset 0
  PROLOGUE  5, 7, 3, Y, U, V, ARGB, WIDTH, TEMPU, TEMPV
  extern    mangle(kCoefficientsRgbY)
  jmp       .convertend

.convertloop:
  movzx     TEMPUd, BYTE [Uq]
  add       Uq, 1
  movzx     TEMPVd, BYTE [Vq]
  add       Vq, 1
  movq      mm0, [mangle(kCoefficientsRgbY) + 2048 + 8 * TEMPUq]
  movzx     TEMPUd, BYTE [Yq]
  paddsw    mm0, [mangle(kCoefficientsRgbY) + 4096 + 8 * TEMPVq]
  movzx     TEMPVd, BYTE [Yq + 1]
  movq      mm1, [mangle(kCoefficientsRgbY) + 8 * TEMPUq]
  add       Yq, 2
  movq      mm2, [mangle(kCoefficientsRgbY) + 8 * TEMPVq]
  paddsw    mm1, mm0
  paddsw    mm2, mm0
  psraw     mm1, 6
  psraw     mm2, 6
  packuswb  mm1, mm2
  MOVQ      [ARGBq], mm1
  add       ARGBq, 8

.convertend:
  sub       WIDTHq, 2
  jns       .convertloop

  ; If number of pixels is odd then compute it.
  and       WIDTHq, 1
  jz        .convertdone

  movzx     TEMPUd, BYTE [Uq]
  movq      mm0, [mangle(kCoefficientsRgbY) + 2048 + 8 * TEMPUq]
  movzx     TEMPVd, BYTE [Vq]
  paddsw    mm0, [mangle(kCoefficientsRgbY) + 4096 + 8 * TEMPVq]
  movzx     TEMPUd, BYTE [Yq]
  movq      mm1, [mangle(kCoefficientsRgbY) + 8 * TEMPUq]
  paddsw    mm1, mm0
  psraw     mm1, 6
  packuswb  mm1, mm1
  movd      [ARGBq], mm1

.convertdone:
  RET
%endif

; With PIC code we need to load the address of mangle(kCoefficientsRgbY).
; This code is slower than the above version.
%ifdef PIC
mangle(SYMBOL):
  %assign   stack_offset 0
  PROLOGUE  5, 7, 3, Y, U, V, ARGB, WIDTH, TEMP, TABLE

  extern    mangle(kCoefficientsRgbY)
  LOAD_SYM  TABLEq, mangle(kCoefficientsRgbY)

  jmp       .convertend

.convertloop:
  movzx     TEMPd, BYTE [Uq]
  movq      mm0, [TABLEq + 2048 + 8 * TEMPq]
  add       Uq, 1

  movzx     TEMPd, BYTE [Vq]
  paddsw    mm0, [TABLEq + 4096 + 8 * TEMPq]
  add       Vq, 1

  movzx     TEMPd, BYTE [Yq]
  movq      mm1, [TABLEq + 8 * TEMPq]

  movzx     TEMPd, BYTE [Yq + 1]
  movq      mm2, [TABLEq + 8 * TEMPq]
  add       Yq, 2

  ; Add UV components to Y component.
  paddsw    mm1, mm0
  paddsw    mm2, mm0

  ; Down shift and then pack.
  psraw     mm1, 6
  psraw     mm2, 6
  packuswb  mm1, mm2
  MOVQ      [ARGBq], mm1
  add       ARGBq, 8

.convertend:
  sub       WIDTHq, 2
  jns       .convertloop

  ; If number of pixels is odd then compute it.
  and       WIDTHq, 1
  jz        .convertdone

  movzx     TEMPd, BYTE [Uq]
  movq      mm0, [TABLEq + 2048 + 8 * TEMPq]
  movzx     TEMPd, BYTE [Vq]
  paddsw    mm0, [TABLEq + 4096 + 8 * TEMPq]
  movzx     TEMPd, BYTE [Yq]
  movq      mm1, [TABLEq + 8 * TEMPq]
  paddsw    mm1, mm0
  psraw     mm1, 6
  packuswb  mm1, mm1
  movd      [ARGBq], mm1

.convertdone:
  RET
%endif
