/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_BaselineRegisters_h
#define jit_BaselineRegisters_h

#ifdef JS_ION

#if defined(JS_CPU_X86)
# include "x86/BaselineRegisters-x86.h"
#elif defined(JS_CPU_X64)
# include "x64/BaselineRegisters-x64.h"
#elif defined(JS_CPU_ARM)
# include "arm/BaselineRegisters-arm.h"
#elif defined(JS_CPU_MIPS)
# include "mips/BaselineRegisters-mips.h"
#else
#error "Unknown CPU architecture."
#endif

namespace js {
namespace jit {

} // namespace jit
} // namespace js

#endif // JS_ION

#endif /* jit_BaselineRegisters_h */
