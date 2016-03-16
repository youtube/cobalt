// Copyright 2016 Google Inc. All Rights Reserved.
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

#include "starboard/log.h"

#include "starboard/system.h"

void SbLogRawDumpStack(int frames_to_skip) {
  if (frames_to_skip < 0) {
    frames_to_skip = 0;
  }

  void* stack[256];
  int count = SbSystemGetStack(stack, SB_ARRAY_SIZE_INT(stack));

  // Skip over SbLogRawDumpStack's stack frame.
  for (int i = 1 + frames_to_skip; i < count; ++i) {
    char symbol[512];
    bool result =
        SbSystemSymbolize(stack[i], symbol, SB_ARRAY_SIZE_INT(symbol));
    SbLogRawFormatF("\t%s [%p]\n", (result ? symbol : "<unknown>"), stack[i]);
    SbLogFlush();
  }
}
