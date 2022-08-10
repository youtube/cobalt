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

#ifndef STARBOARD_SHARED_STARBOARD_STARBOARD_SWITCHES_H_
#define STARBOARD_SHARED_STARBOARD_STARBOARD_SWITCHES_H_

#include "starboard/configuration.h"

namespace starboard {
namespace shared {
namespace starboard {

// Start the crash handler only when a crash happens, instead of starting it
// before the app runs and keeping it running all the time. This option reduces
// memory consumption by the crash handler.
extern const char kStartHandlerAtCrash[];
// Use this flag to start the handler
// before the app launches. Without this flag, the crash handler starts only
// when a crash happens. This option increases the memory consumption.
extern const char kStartHandlerAtLaunch[];

}  // namespace starboard
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_STARBOARD_SWITCHES_H_
