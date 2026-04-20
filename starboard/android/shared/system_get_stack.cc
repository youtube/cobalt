// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

#include <unwind.h>

#include "starboard/common/log.h"
#include "starboard/system.h"

namespace {
class CallbackContext {
 public:
  void** out_stack;
  int stack_size;
  int count;
  CallbackContext(void** out_stack, int stack_size)
      : out_stack(out_stack), stack_size(stack_size), count(0) {}
};

_Unwind_Reason_Code UnwindCallback(struct _Unwind_Context* uwc,
                                   void* user_context) {
  CallbackContext* callback_context =
      static_cast<CallbackContext*>(user_context);
  _Unwind_Ptr ip = _Unwind_GetIP(uwc);

  if (ip == 0) {
    return _URC_END_OF_STACK;
  }

  if (callback_context->count == callback_context->stack_size) {
    return _URC_END_OF_STACK;
  }

  callback_context->out_stack[callback_context->count] =
      reinterpret_cast<void*>(ip);
  callback_context->count++;
  return _URC_NO_REASON;
}
}  // namespace

int SbSystemGetStack(void** out_stack, int stack_size) {
  CallbackContext callback_context(out_stack, stack_size);

  _Unwind_Backtrace(UnwindCallback, &callback_context);
  return callback_context.count;
}
