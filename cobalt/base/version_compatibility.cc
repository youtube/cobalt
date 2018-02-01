// Copyright 2018 Google Inc. All Rights Reserved.
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

#if defined(COBALT_ENABLE_VERSION_COMPATIBILITY_VALIDATIONS)

#include "cobalt/base/version_compatibility.h"

#include "base/logging.h"

namespace base {

VersionCompatibility* VersionCompatibility::GetInstance() {
  return Singleton<VersionCompatibility,
                   StaticMemorySingletonTraits<VersionCompatibility> >::get();
}

VersionCompatibility::VersionCompatibility()
    : violation_count(
          "Count.VersionCompatibilityViolation", 0,
          "The number of Cobalt version compatibility violations encountered."),
      minimum_version_(0) {}

void VersionCompatibility::SetMinimumVersion(int minimum_version) {
  base::subtle::NoBarrier_Store(&minimum_version_, minimum_version);
}

int VersionCompatibility::GetMinimumVersion() const {
  return base::subtle::NoBarrier_Load(&minimum_version_);
}

void VersionCompatibility::ReportViolation(
    const std::string& violation_message) {
  const std::string kVersionCompatibilityViolationTag =
      "[VERSION COMPATIBILITY VIOLATION]";
  LOG(ERROR) << kVersionCompatibilityViolationTag << " " << violation_message;
  ++violation_count;
}

}  // namespace base

#endif  // defined(COBALT_ENABLE_VERSION_COMPATIBILITY_VALIDATIONS)
