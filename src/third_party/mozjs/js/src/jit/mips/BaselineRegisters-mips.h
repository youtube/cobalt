/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_mips_BaselineRegisters_mips_h
#define jit_mips_BaselineRegisters_mips_h

#ifdef JS_ION

#include "jit/IonMacroAssembler.h"

namespace js {
namespace jit {

static MOZ_CONSTEXPR_VAR Register BaselineFrameReg = s5;
static MOZ_CONSTEXPR_VAR Register BaselineStackReg = sp;

static MOZ_CONSTEXPR_VAR ValueOperand R0(v1, v0);
static MOZ_CONSTEXPR_VAR ValueOperand R1(s7, s6);
static MOZ_CONSTEXPR_VAR ValueOperand R2(t7, t6);

// BaselineTailCallReg and BaselineStubReg
// These use registers that are not preserved across calls.
static MOZ_CONSTEXPR_VAR Register BaselineTailCallReg = ra;
static MOZ_CONSTEXPR_VAR Register BaselineStubReg = t5;

static MOZ_CONSTEXPR_VAR Register ExtractTemp0 = InvalidReg;
static MOZ_CONSTEXPR_VAR Register ExtractTemp1 = InvalidReg;

// Register used internally by MacroAssemblerMIPS.
static MOZ_CONSTEXPR_VAR Register BaselineSecondScratchReg = SecondScratchReg;

// Note that BaselineTailCallReg is actually just the link register.
// In MIPS code emission, we do not clobber BaselineTailCallReg since we keep
// the return address for calls there.

// FloatReg0 must be equal to ReturnFloatReg.
static MOZ_CONSTEXPR_VAR FloatRegister FloatReg0 = f0;
static MOZ_CONSTEXPR_VAR FloatRegister FloatReg1 = f2;

} // namespace jit
} // namespace js

#endif // JS_ION

#endif /* jit_mips_BaselineRegisters_mips_h */

