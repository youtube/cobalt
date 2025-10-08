/*
 * Copyright (C) 2020 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "perfetto/ext/base/version.h"

#include "perfetto/base/build_config.h"

#include <stdio.h>

#if PERFETTO_BUILDFLAG(PERFETTO_VERSION_GEN)
#include "perfetto_version.gen.h"
#else
<<<<<<< HEAD
#define PERFETTO_VERSION_STRING() nullptr
=======
#define PERFETTO_VERSION_STRING() "v0.0"
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
#define PERFETTO_VERSION_SCM_REVISION() "unknown"
#endif

namespace perfetto {
namespace base {

<<<<<<< HEAD
const char* GetVersionCode() {
  return PERFETTO_VERSION_STRING();
}

const char* GetVersionString() {
  static const char* version_str = [] {
    static constexpr size_t kMaxLen = 256;
    const char* version_code = PERFETTO_VERSION_STRING();
    if (version_code == nullptr) {
      version_code = "v0.0";
    }
    char* version = new char[kMaxLen + 1];
    snprintf(version, kMaxLen, "Perfetto %s (%s)", version_code,
=======
const char* GetVersionString() {
  static const char* version_str = [] {
    static constexpr size_t kMaxLen = 256;
    char* version = new char[kMaxLen + 1];
    snprintf(version, kMaxLen, "Perfetto %s (%s)", PERFETTO_VERSION_STRING(),
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
             PERFETTO_VERSION_SCM_REVISION());
    return version;
  }();
  return version_str;
}

}  // namespace base
}  // namespace perfetto
