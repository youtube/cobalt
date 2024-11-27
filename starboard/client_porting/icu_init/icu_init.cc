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

#include <pthread.h>

#include <string>
#include <vector>

#include "starboard/common/log.h"
#include "starboard/common/once.h"
#include "starboard/configuration.h"
#include "starboard/configuration_constants.h"
#include "starboard/system.h"
#include "third_party/icu/source/common/unicode/putil.h"
#include "third_party/icu/source/common/unicode/udata.h"

namespace {

// Once control for initializing ICU.
pthread_once_t g_initialization_once = PTHREAD_ONCE_INIT;

// Should only be called once.
void Initialize() {
  UErrorCode err = U_ZERO_ERROR;

  // ICU data is linked into the binary.
  udata_setFileAccess(UDATA_NO_FILES, &err);
  SB_DCHECK(err <= U_ZERO_ERROR);
}

}  // namespace

void IcuInit() {
  pthread_once(&g_initialization_once, &Initialize);
}
