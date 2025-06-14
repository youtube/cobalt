// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#include <pthread.h>

#include <string>
#include <vector>

#include "starboard/common/log.h"
#include "starboard/configuration_constants.h"
#include "starboard/file.h"
#include "starboard/system.h"
#include "unicode/putil.h"
#include "unicode/udata.h"

namespace cobalt {
namespace common {
namespace icu {

namespace {

pthread_once_t g_initialization_once = PTHREAD_ONCE_INIT;

// Initializes ICU and TimeZones so the rest of the functions will work. Should
// only be called once.
void Initialize() {
  // Minimal Initialization of ICU.
  std::vector<char> base_path(kSbFileMaxPath);
  bool result = SbSystemGetPath(kSbSystemPathContentDirectory, base_path.data(),
                                base_path.size());
  SB_DCHECK(result);

  std::string data_path(base_path.data());
  u_setDataDirectory(data_path.c_str());
  SB_LOG(INFO) << __FUNCTION__ << " ICU data directory " << data_path;

  UErrorCode err = U_ZERO_ERROR;
  udata_setFileAccess(UDATA_PACKAGES_FIRST, &err);
  SB_DCHECK(err <= U_ZERO_ERROR);
}

bool IcuInit() {
  pthread_once(&g_initialization_once, &Initialize);
  return true;
}
}  // namespace

static bool g_icu_is_initialized = IcuInit();

}  // namespace icu
}  // namespace common
}  // namespace cobalt
