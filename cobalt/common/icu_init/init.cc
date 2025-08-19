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

#include "cobalt/common/icu_init/init.h"

#include <fcntl.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unicode/utypes.h>
#include <unistd.h>

#include <string>
#include <vector>

// #include "base/i18n/icu_util.h" // nogncheck
#include "starboard/common/log.h"
#include "starboard/configuration_constants.h"
#include "starboard/file.h"
#include "starboard/system.h"
#include "unicode/putil.h"
#include "unicode/udata.h"

namespace cobalt {
namespace common {
namespace icu_init {

namespace {

pthread_once_t g_initialization_once = PTHREAD_ONCE_INIT;
bool g_initialization_result = false;

// Initialize the ICU using the file named kIcuDataFileName in the
// kSbSystemPathContentDirectory folder. Note that this gives the ICU its
// database, but it does not actually set a default timezone or locale.
void InitializeIcuDatabase() {
  // test not initializing icu
  // base::i18n::InitializeICU();
  g_initialization_result = true;
}

bool IcuInit() {
  pthread_once(&g_initialization_once, &InitializeIcuDatabase);
  return g_initialization_result;
}
}  // namespace

// Initialize ICU with a static initializer to ensure it is initialized
// before program execution begins. While this is very early, it is not
// guaranteed to be early enough for ICU to be used by other global
// initializers.
static bool g_icu_is_initialized = IcuInit();

void EnsureInitialized() {
  // Even though IcuInit() is called before main() is called, when the static
  // initializers are called, the order of execution of static initializers is
  // undefined. This function allows functions that may use ICU from static
  // initializers themselves to guarantee that ICU is initialized first.
  // Note: This is thread-safe because spurious calls to IcuInit() are
  // thread-safe.
  if (!g_icu_is_initialized) {
    g_icu_is_initialized = IcuInit();
  }
}

}  // namespace icu_init
}  // namespace common
}  // namespace cobalt
