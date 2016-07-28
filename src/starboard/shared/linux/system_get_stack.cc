// Copyright 2015 Google Inc. All Rights Reserved.
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

#include <execinfo.h>

#include <algorithm>

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
