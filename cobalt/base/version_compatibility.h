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

#ifndef COBALT_BASE_VERSION_COMPATIBILITY_H_
#define COBALT_BASE_VERSION_COMPATIBILITY_H_

#if defined(COBALT_ENABLE_VERSION_COMPATIBILITY_VALIDATIONS)

#include <string>

#include "base/atomicops.h"
#include "base/basictypes.h"
#include "base/memory/singleton.h"
#include "cobalt/base/c_val.h"

namespace base {

// This thread-safe singleton class provides basic support for Cobalt version
// compatibility validations.
// NOTE: This class exists for convenience and can be removed when we create a
// better mechanism for dependency injection in Cobalt.
class VersionCompatibility {
 public:
  // Method to get the singleton instance of this class.
  static VersionCompatibility* GetInstance();

  void SetMinimumVersion(int minimum_version);
  int GetMinimumVersion() const;

  // Called when a version compatibility violation is encountered.
  void ReportViolation(const std::string& violation_info);

 private:
  friend struct StaticMemorySingletonTraits<VersionCompatibility>;

  VersionCompatibility();
  ~VersionCompatibility() = default;

  // The number of violations that have been encountered.
  base::CVal<int, base::CValDebug> violation_count;

  // The minimum version of Cobalt to validate compatibility against.
  base::subtle::Atomic32 minimum_version_;

  DISALLOW_COPY_AND_ASSIGN(VersionCompatibility);
};

}  // namespace base

#endif  // defined(COBALT_ENABLE_VERSION_COMPATIBILITY_VALIDATIONS)

#endif  // COBALT_BASE_VERSION_COMPATIBILITY_H_
