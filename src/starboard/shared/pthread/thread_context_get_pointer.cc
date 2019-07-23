// Copyright 2019 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/thread.h"

#if SB_API_VERSION >= 11

#include <ucontext.h>

#include "starboard/common/log.h"
#include "starboard/shared/pthread/thread_context_internal.h"

// TODO: Implement for e.g. Mac OSX, BSD flavors, QNX - maybe in another file.
#if !defined(__gnu_linux__)  // Note: __linux__ is undef'd for Starboard builds.
#error "SbThreadContext is only implemented for Linux"
#endif

// UCLIBC pretends to be GLIBC.
#if (defined(__GLIBC__) || defined(__GNU_LIBRARY__)) && !defined(__UCLIBC__)
#define SB_IS_GLIBC 1
#else
#define SB_IS_GLIBC 0
#endif

// clang-format off
#if SB_IS_ARCH_X86
# if SB_IS_32_BIT
#  error("TODO: Validate UCONTEXT macros on 32-bit X86")
#  define UCONTEXT_IP(ucontext) ((ucontext)->uc_mcontext.gregs[REG_EIP])
#  define UCONTEXT_SP(ucontext) ((ucontext)->uc_mcontext.gregs[REG_ESP])
#  define UCONTEXT_FP(ucontext) ((ucontext)->uc_mcontext.gregs[REG_EBP])
# elif SB_IS_64_BIT
#  define UCONTEXT_IP(ucontext) ((ucontext)->uc_mcontext.gregs[REG_RIP])
#  define UCONTEXT_SP(ucontext) ((ucontext)->uc_mcontext.gregs[REG_RSP])
#  define UCONTEXT_FP(ucontext) ((ucontext)->uc_mcontext.gregs[REG_RBP])
# endif
#elif SB_IS_ARCH_ARM
# if SB_IS_32_BIT
#  if SB_IS_GLIBC && ((__GLIBC__ * 100 + __GLIBC_MINOR__) < 204)
#   error("TODO: Validate UCONTEXT macros on 32-bit ARM w/ old GLIBC")
#   define UCONTEXT_IP(ucontext) ((ucontext)->uc_mcontext.gregs[R15])
#   define UCONTEXT_SP(ucontext) ((ucontext)->uc_mcontext.gregs[R13])
#   define UCONTEXT_FP(ucontext) ((ucontext)->uc_mcontext.gregs[R11])
#  else  // SB_IS_GLIBC < 2.04
#   define UCONTEXT_IP(ucontext) ((ucontext)->uc_mcontext.arm_pc)
#   define UCONTEXT_SP(ucontext) ((ucontext)->uc_mcontext.arm_sp)
#   define UCONTEXT_FP(ucontext) ((ucontext)->uc_mcontext.arm_fp)
#  endif  // SB_IS_GLIBC < 2.04
# elif SB_IS_64_BIT
#  error("TODO: Validate UCONTEXT macros on 64-bit ARM")
#  define UCONTEXT_IP(ucontext) ((ucontext)->uc_mcontext.pc)
#  define UCONTEXT_SP(ucontext) ((ucontext)->uc_mcontext.sp)
#  define UCONTEXT_FP(ucontext) ((ucontext)->uc_mcontext.regs[29])
# endif
#elif SB_IS_ARCH_MIPS
#  error("TODO: Validate UCONTEXT macros on MIPS")
#  define UCONTEXT_IP(ucontext) ((ucontext)->uc_mcontext.pc)
#  define UCONTEXT_SP(ucontext) ((ucontext)->uc_mcontext.gregs[29])
#  define UCONTEXT_FP(ucontext) ((ucontext)->uc_mcontext.gregs[30])
#elif SB_IS_ARCH_PPC
# if SB_IS_GLIBC
#  error("TODO: Validate UCONTEXT macros on PPC w/ GLIBC")
#  define UCONTEXT_IP(ucontext) ((ucontext)->uc_mcontext.regs->nip)
#  define UCONTEXT_SP(ucontext) ((ucontext)->uc_mcontext.regs->gpr[PT_R1])
#  define UCONTEXT_FP(ucontext) ((ucontext)->uc_mcontext.regs->gpr[PT_R31])
# else  // SB_IS_GLIBC
#  error("TODO: Validate UCONTEXT macros on PPC")
#  define UCONTEXT_IP(ucontext) ((ucontext)->uc_mcontext.gp_regs[32])
#  define UCONTEXT_SP(ucontext) ((ucontext)->uc_mcontext.gp_regs[1])
#  define UCONTEXT_FP(ucontext) ((ucontext)->uc_mcontext.gp_regs[31])
# endif  // SB_IS_GLIBC
#else  // SB_IS_ARCH_XXX
# error "SbThreadContext isn't implemented for this CPU architecture"
#endif  // SB_IS_ARCH_XXX
// clang-format on

bool SbThreadContextGetPointer(SbThreadContext context,
                               SbThreadContextProperty property,
                               void** out_value) {
  if (context == kSbThreadContextInvalid || context->ucontext == nullptr) {
    return false;
  }

  switch (property) {
    case kSbThreadContextInstructionPointer:
      *out_value = reinterpret_cast<void*>(UCONTEXT_IP(context->ucontext));
      return true;

    case kSbThreadContextStackPointer:
      *out_value = reinterpret_cast<void*>(UCONTEXT_SP(context->ucontext));
      return true;

    case kSbThreadContextFramePointer:
      *out_value = reinterpret_cast<void*>(UCONTEXT_FP(context->ucontext));
      return true;

    default:
      SB_NOTIMPLEMENTED() << "SbThreadContextGetPointer not implemented for "
                          << property;
      return false;
  }
}

#endif  // SB_API_VERSION >= 11
