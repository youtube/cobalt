// Copyright 2020 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "starboard/shared/pthread/thread_context_internal.h"

#if !defined(__linux__)
#error "SbThreadContext is only implemented for Linux"
#endif

#if !(SB_IS_32_BIT || SB_IS_64_BIT)
#error "CPU architecture must be either 32-bit or 64-bit"
#endif

// UCLIBC pretends to be GLIBC.
#if (defined(__GLIBC__) || defined(__GNU_LIBRARY__)) && !defined(__UCLIBC__)
#define SB_IS_GLIBC 1
#else
#define SB_IS_GLIBC 0
#endif

SbThreadContextPrivate::SbThreadContextPrivate(ucontext_t* ucontext) {
  mcontext_t& mcontext = ucontext->uc_mcontext;

#if SB_IS_ARCH_X64
  // 64-bit X86 (aka X64)
  ip_ = reinterpret_cast<void*>(mcontext.gregs[REG_RIP]);
  sp_ = reinterpret_cast<void*>(mcontext.gregs[REG_RSP]);
  fp_ = reinterpret_cast<void*>(mcontext.gregs[REG_RBP]);
#elif SB_IS_ARCH_X86
  // 32-bit X86
  ip_ = reinterpret_cast<void*>(mcontext.gregs[REG_EIP]);
  sp_ = reinterpret_cast<void*>(mcontext.gregs[REG_ESP]);
  fp_ = reinterpret_cast<void*>(mcontext.gregs[REG_EBP]);
#elif SB_IS_ARCH_ARM64
  // 64-bit ARM
  ip_ = reinterpret_cast<void*>(mcontext.pc);
  sp_ = reinterpret_cast<void*>(mcontext.sp);
  fp_ = reinterpret_cast<void*>(mcontext.regs[29]);
  lr_ = reinterpret_cast<void*>(mcontext.regs[30]);
#elif SB_IS_ARCH_ARM
#if SB_IS_GLIBC && ((__GLIBC__ * 100 + __GLIBC_MINOR__) < 204)
  // 32-bit ARM w/ old GLIBC that used gregs[]
  ip_ = reinterpret_cast<void*>(mcontext.gregs[R15]);
  sp_ = reinterpret_cast<void*>(mcontext.gregs[R13]);
  fp_ = reinterpret_cast<void*>(mcontext.gregs[R11]);
  lr_ = reinterpret_cast<void*>(mcontext.gregs[R14]);
#else
  // 32-bit ARM
  ip_ = reinterpret_cast<void*>(mcontext.arm_pc);
  sp_ = reinterpret_cast<void*>(mcontext.arm_sp);
  fp_ = reinterpret_cast<void*>(mcontext.arm_fp);
  lr_ = reinterpret_cast<void*>(mcontext.arm_lr);
#endif
#else  // SB_IS_ARCH_XXX
#error "SbThreadContext isn't implemented for this CPU architecture"
#endif  // SB_IS_ARCH_XXX
}
