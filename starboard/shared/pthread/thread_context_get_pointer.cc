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

#include <ucontext.h>

#include "starboard/common/log.h"
#include "starboard/shared/pthread/thread_context_internal.h"

bool SbThreadContextGetPointer(SbThreadContext context,
                               SbThreadContextProperty property,
                               void** out_value) {
  if (context == kSbThreadContextInvalid || context->ip_ == nullptr) {
    return false;
  }

  switch (property) {
    case kSbThreadContextInstructionPointer:
      *out_value = context->ip_;
      return true;

    case kSbThreadContextStackPointer:
      *out_value = context->sp_;
      return true;

    case kSbThreadContextFramePointer:
      *out_value = context->fp_;
      return true;

#if SB_API_VERSION >= 12
    case kSbThreadContextLinkRegister:
      *out_value = context->lr_;
      return true;
#endif  // SB_API_VERSION >= 12

    default:
      SB_NOTIMPLEMENTED() << "SbThreadContextGetPointer not implemented for "
                          << property;
      return false;
  }
}
