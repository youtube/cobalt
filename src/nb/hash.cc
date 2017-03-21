/*
 * Copyright 2016 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "nb/hash.h"

#include "starboard/types.h"

namespace nb {

namespace {
// Injects the super fast hash into the nb namespace.
#include "third_party/super_fast_hash/super_fast_hash.cc"
}  // namespace

uint32_t RuntimeHash32(const char* data, int length, uint32_t prev_hash) {
  return SuperFastHash(data, length, prev_hash);
}

}  // namespace nb
