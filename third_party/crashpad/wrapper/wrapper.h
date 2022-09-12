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

#ifndef THIRD_PARTY_CRASHPAD_WRAPPER_WRAPPER_H_
#define THIRD_PARTY_CRASHPAD_WRAPPER_WRAPPER_H_

#include "starboard/elf_loader/evergreen_info.h" // nogncheck

namespace third_party {
namespace crashpad {
namespace wrapper {

// The key name used in Crashpad for the version annotation.
extern const char kCrashpadVersionKey[];

// The key name used in Crashpad for the version annotation.
extern const char kCrashpadProductKey[];

// The key name used in Crashpad for the user_agent_string annotation.
extern const char kCrashpadUserAgentStringKey[];

void InstallCrashpadHandler(bool start_at_crash);

bool AddEvergreenInfoToCrashpad(EvergreenInfo evergreen_info);

// Associates the given value with the given key in Crashpad's map of
// annotations, updating the existing value if the map already contains the
// key. Returns true on success and false on failure.
bool InsertCrashpadAnnotation(const char* key, const char* value);

}  // namespace wrapper
}  // namespace crashpad
}  // namespace third_party

#endif  // THIRD_PARTY_CRASHPAD_WRAPPER_WRAPPER_H_
