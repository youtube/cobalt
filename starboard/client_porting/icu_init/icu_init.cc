// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/client_porting/icu_init/icu_init.h"

#include <string>
#include <vector>

#include "starboard/common/log.h"
#include "starboard/configuration.h"
#include "starboard/configuration_constants.h"
#include "starboard/file.h"
#include "starboard/once.h"
#include "starboard/system.h"
#include "third_party/icu/source/common/unicode/putil.h"
#include "third_party/icu/source/common/unicode/udata.h"

namespace {

// Once control for initializing ICU.
SbOnceControl g_initialization_once = SB_ONCE_INITIALIZER;

// Initializes ICU and TimeZones so the rest of the functions will work. Should
// only be called once.
void Initialize() {
  // Minimal Initialization of ICU.
  std::vector<char> base_path(kSbFileMaxPath);
  bool result = SbSystemGetPath(kSbSystemPathContentDirectory, base_path.data(),
                                base_path.size());
  SB_DCHECK(result);

  std::string data_path(base_path.data());
  data_path += kSbFileSepString;
  data_path += "icu";

  // set this as the data directory.
  u_setDataDirectory(data_path.c_str());

  UErrorCode err = U_ZERO_ERROR;
  udata_setFileAccess(UDATA_PACKAGES_FIRST, &err);
  SB_DCHECK(err <= U_ZERO_ERROR);
}

}  // namespace

void SbIcuInit() {
  SbOnce(&g_initialization_once, &Initialize);
}
