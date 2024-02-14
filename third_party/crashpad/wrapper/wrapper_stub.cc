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

#include "third_party/crashpad/wrapper/wrapper.h"

namespace third_party {
namespace crashpad {
namespace wrapper {

const char kCrashpadVersionKey[]  = "";
const char kCrashpadProductKey[]  = "";
const char kCrashpadUserAgentStringKey[]  = "";
const char kCrashpadCertScopeKey[] = "";

void InstallCrashpadHandler(const std::string& ca_certificates_path) {}

bool AddEvergreenInfoToCrashpad(EvergreenInfo evergreen_info) {
  return false;
}

bool InsertCrashpadAnnotation(const char* key, const char* value) {
  return false;
}

}  // namespace wrapper
}  // namespace crashpad
}  // namespace third_party