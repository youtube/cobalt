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

#ifndef COBALT_BROWSER_STACK_SIZE_CONSTANTS_H_
#define COBALT_BROWSER_STACK_SIZE_CONSTANTS_H_

#include "cobalt/base/address_sanitizer.h"

namespace cobalt {
namespace browser {
#if defined(COBALT_BUILD_TYPE_DEBUG)
// Non-optimized builds require a bigger stack size.
const size_t kBaseStackSize = 3 * 1024 * 1024;
#elif defined(COBALT_BUILD_TYPE_DEVEL)
// Devel builds require a slightly bigger stack size.
const size_t kBaseStackSize = 832 * 1024;
#else
const size_t kBaseStackSize = 768 * 1024;
#endif
const size_t kWebModuleStackSize =
    kBaseStackSize + base::kAsanAdditionalStackSize;
}  // namespace browser
}  // namespace cobalt

#endif  // COBALT_BROWSER_STACK_SIZE_CONSTANTS_H_
