// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_TVOS_SHARED_UIKIT_H_
#define STARBOARD_TVOS_SHARED_UIKIT_H_

#include "starboard/export.h"

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/**
 *  @brief The main function that clients should call in order to initialize the
 *      @c main run loop.
 */
SB_EXPORT_PLATFORM int SBDUIKitApplicationMain(int argc, char* argv[]);

/**
 *  @brief The function clients should call when the OS sends a search/play
 *      AppIntent. If the intent is for search, |isSearch| should be non-zero.
 */
SB_EXPORT_PLATFORM void SBProcessAppIntent(const char* query, int isSearch);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif  // STARBOARD_TVOS_SHARED_UIKIT_H_
