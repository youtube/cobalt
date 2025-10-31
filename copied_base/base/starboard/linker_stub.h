// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#ifndef COPIED_BASE_BASE_STARBOARD_LINKER_STUB_H_
#define COPIED_BASE_BASE_STARBOARD_LINKER_STUB_H_

#include "starboard/common/log.h"

// TODO: b/413130400 - Cobalt: Remove this file and implement linker stubs

#if BUILDFLAG(COBALT_IS_RELEASE_BUILD)
#define COBALT_LINKER_STUB()
#else
#define COBALT_LINKER_STUB_MSG                                    \
  "This is a stub needed for linking cobalt. You need to remove " \
  "COBALT_LINKER_STUB and implement "                             \
      << SB_FUNCTION

#define COBALT_LINKER_STUB()                   \
  do {                                         \
    static int count = 0;                      \
    if (0 == count++) {                        \
      SB_LOG(ERROR) << COBALT_LINKER_STUB_MSG; \
    }                                          \
  } while (0);                                 \
  SbSystemBreakIntoDebugger();
#endif  // BUILDFLAG(COBALT_IS_RELEASE_BUILD)

#endif  // COPIED_BASE_BASE_STARBOARD_LINKER_STUB_H_
