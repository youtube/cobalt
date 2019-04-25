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

#include "starboard/shared/starboard/log_mutex.h"

#include "starboard/configuration.h"
#include "starboard/once.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace {
SB_ONCE_INITIALIZE_FUNCTION(RecursiveMutex, g_log_mutex);
}  // namespace

RecursiveMutex* GetLoggingMutex() {
  return g_log_mutex();
}

}  // namespace starboard
}  // namespace shared
}  // namespace starboard
