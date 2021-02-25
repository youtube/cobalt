// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/system.h"

#if SB_IS(EVERGREEN_COMPATIBLE_LIBUNWIND)
#define UNW_LOCAL_ONLY
#include "starboard/common/log.h"
#include "starboard/memory.h"
#include "third_party/llvm-project/libunwind/include/libunwind.h"
#else
#include <execinfo.h>
#include <algorithm>
#endif

#if SB_IS(EVERGREEN_COMPATIBLE_LIBUNWIND)
int SbSystemGetStack(void** out_stack, int stack_size) {
  unw_cursor_t cursor;
  unw_context_t context;

  // Initialize cursor to current frame for local unwinding.
  int ret = unw_getcontext(&context);
  if (ret < 0) {
    return 0;
  }
  ret = unw_init_local(&cursor, &context);
  if (ret < 0) {
    return 0;
  }
  // Unwind frames one by one, going up the frame stack.
  int i = 0;
  for (; i < stack_size; i++) {
    ret = unw_step(&cursor);
    if (ret <= 0) {
      break;
    }
    unw_word_t pc = 0;
    ret = unw_get_reg(&cursor, UNW_REG_IP, &pc);
    if (pc == 0) {
      break;
    }
    out_stack[i] = reinterpret_cast<void*>(pc);
  }
  return i;
}
#else
int SbSystemGetStack(void** out_stack, int stack_size) {
  int count = std::max(backtrace(out_stack, stack_size), 0);

  if (count < 1) {
    // No stack, for some reason.
    return count;
  }

  // We have an extra stack frame (for this very function), so let's remove it.
  for (int i = 1; i < count; ++i) {
    out_stack[i - 1] = out_stack[i];
  }

  return count - 1;
}
#endif
