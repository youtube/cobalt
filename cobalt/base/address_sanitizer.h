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

#ifndef COBALT_BASE_ADDRESS_SANITIZER_H_
#define COBALT_BASE_ADDRESS_SANITIZER_H_

namespace base {

// ASAN requires much larger stack sizes.  The value |kAsanAdditionalStackSize|
// can be added to thread stack sizes if stack overflow issues are encountered
// due to ASAN being active.
#if defined(ADDRESS_SANITIZER)
const size_t kAsanAdditionalStackSize = 4 * 1024 * 1024;
#else
const size_t kAsanAdditionalStackSize = 0;
#endif  // defined(ADDRESS_SANITIZER)

}  // namespace base

#endif  // COBALT_BASE_ADDRESS_SANITIZER_H_
