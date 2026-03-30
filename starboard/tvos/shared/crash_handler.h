// Copyright 2022 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_TVOS_SHARED_CRASH_HANDLER_H_
#define STARBOARD_TVOS_SHARED_CRASH_HANDLER_H_

#ifdef __cplusplus
extern "C" {
#endif
// When making changes to BreakpadSetAnnotationsCallback, definition in
// starboard/darwin/tvos/main.mm and
// googlemac/iPhone/YouTubeTV/v2/main.m should also be updated.
typedef void (*BreakpadSetAnnotationsCallback)(const char* key,
                                               const char* value);
#ifdef __cplusplus
}
#endif

namespace starboard {
namespace shared {
namespace uikit {

const void* GetCrashHandlerApi();

}  // namespace uikit
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_TVOS_SHARED_CRASH_HANDLER_H_
