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

#ifndef NB_HASH_H_
#define NB_HASH_H_

#include "nb/bit_cast.h"
#include "starboard/types.h"

namespace nb {

enum { kDefaultSeed = 0x12345789 };

// RuntimeHash32 is a 32 bit hash for data. The only guarantee is that this
// hash is persistent for the lifetime of the program. This hash function
// is not guaranteed to be consistent across platforms. The hash value should
// never be saved to disk.
// It's sufficient however, to use as an in-memory hashtable.
uint32_t RuntimeHash32(const char* data,
                       int length,
                       uint32_t prev_hash = kDefaultSeed);

// Convenience functor for hashing plain old data types like int/float etc and
// POD structs.
template <typename T>
struct PODHasher {
  uint32_t operator()(const T& value) const {
    return RuntimeHash32(reinterpret_cast<const char*>(&value), sizeof(value));
  }
};

// Specialization so that all pointers are PODs.
template <typename T>
struct PODHasher<T*> {
  uint32_t operator()(const void* ptr) const {
    uintptr_t ptr_value = bit_cast<uintptr_t>(ptr);
    PODHasher<uintptr_t> pod_hasher;
    return pod_hasher(ptr_value);
  }
};

}  // namespace nb

#endif  // NB_HASH_H_
