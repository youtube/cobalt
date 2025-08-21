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

namespace cobalt {
namespace common {
namespace icu_init {

// Initialize ICU with a static initializer to ensure it is initialized
// before program execution begins. While this is very early, it is not
// guaranteed to be early enough for ICU to be used by other global
// initializers.
static bool g_icu_is_initialized = false;

void EnsureInitialized() {
  // No special ICU initialization is needed if the icu data is linked
  // statically in the binary. To see more details refer to
  // ICU_UTIL_DATA_STATIC in base/i18n/icu_util.cc

  g_icu_is_initialized = true;
}

}  // namespace icu_init
}  // namespace common
}  // namespace cobalt
