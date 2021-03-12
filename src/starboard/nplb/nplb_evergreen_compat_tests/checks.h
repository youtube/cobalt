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

#ifndef STARBOARD_NPLB_NPLB_EVERGREEN_COMPAT_TESTS_CHECKS_H_
#define STARBOARD_NPLB_NPLB_EVERGREEN_COMPAT_TESTS_CHECKS_H_

#include "starboard/configuration.h"

#if SB_IS(EVERGREEN_COMPATIBLE)

#if SB_API_VERSION < 12
#error "Evergreen requires starboard version 12 or higher!"
#endif

#if SB_API_VERSION < 12 && !SB_IS(EVERGREEN_COMPATIBLE_LITE)
#error \
    "Evergreen requires support for the kSbSystemPathStorageDirectory" \
    " system property!"
#endif

#if SB_API_VERSION < 12
#error "Evergreen requires memory mapping!"
#endif

#if !SB_CAN(MAP_EXECUTABLE_MEMORY)
#error "Evergreen requires executable memory support!"
#endif

#endif  // SB_IS(EVERGREEN_COMPATIBLE)
#endif  // STARBOARD_NPLB_NPLB_EVERGREEN_COMPAT_TESTS_CHECKS_H_
