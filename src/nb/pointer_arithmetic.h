/*
 * Copyright 2014 Google Inc. All Rights Reserved.
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

#ifndef NB_POINTER_ARITHMETIC_H_
#define NB_POINTER_ARITHMETIC_H_

#include "starboard/types.h"

namespace nb {

// Helper method to cast the passed in void* pointer to a integer type.
inline uintptr_t AsInteger(const void* memory_value) {
  return reinterpret_cast<uintptr_t>(memory_value);
}

// Helper method to cast the passed in integer to a uint8_t* pointer type.
inline uint8_t* AsPointer(uintptr_t integer_value) {
  return reinterpret_cast<uint8_t*>(integer_value);
}

// Helper method to align addresses up to a specified value.
// Returns the the smallest value that is greater than or equal to value, but
// aligned to alignment.
template <typename T>
T AlignUp(T value, T alignment) {
  T decremented_value = value - 1;
  return decremented_value + alignment - (decremented_value % alignment);
}

template <typename T>
T* AlignUp(T* value, uintptr_t alignment) {
  return reinterpret_cast<T*>(AlignUp(AsInteger(value), alignment));
}

// Helper method to align addresses down to a specified value.
// Returns the the largest value that is less than or equal to value, but
// aligned to alignment.
template <typename T>
T AlignDown(T value, T alignment) {
  return value / alignment * alignment;
}

template <typename T>
T* AlignDown(T* value, uintptr_t alignment) {
  return reinterpret_cast<T*>(AlignDown(AsInteger(value), alignment));
}

// Helper method for subclasses to determine if a given address or value
// is aligned or not.
template <typename T>
static bool IsAligned(T value, size_t alignment) {
  return AsInteger(value) % alignment == 0;
}

}  // namespace nb

#endif  // NB_POINTER_ARITHMETIC_H_
