// Copyright 2016 Google Inc. All Rights Reserved.
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

#include <unicode/putil.h>
#include <unicode/udata.h>

#include <string>

#include "starboard/configuration.h"
#include "starboard/log.h"
#include "starboard/once.h"
#include "starboard/system.h"

namespace {

// Once control for initializing ICU.
SbOnceControl g_initialization_once = SB_ONCE_INITIALIZER;

// Initializes ICU and TimeZones so the rest of the functions will work. Should
// only be called once.
void Initialize() {
  // Minimal Initialization of ICU.
  char base_path[SB_FILE_MAX_PATH];
  bool result = SbSystemGetPath(kSbSystemPathContentDirectory, base_path,
                                SB_ARRAY_SIZE_INT(base_path));
  SB_DCHECK(result);
  std::string data_path = base_path;
  data_path += SB_FILE_SEP_STRING;
  data_path += "icu";
  data_path += SB_FILE_SEP_STRING;
#if U_IS_BIG_ENDIAN
  data_path += "icudt46b";
#else
  data_path += "icudt46l";
#endif
  // set this as the data directory.
  u_setDataDirectory(data_path.c_str());

  UErrorCode err = U_ZERO_ERROR;
  udata_setFileAccess(UDATA_FILES_FIRST, &err);
  SB_DCHECK(err <= U_ZERO_ERROR);
}

}  // namespace

void SbIcuInit() {
  SbOnce(&g_initialization_once, &Initialize);
}
