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

// TODO: I believe this code will have to be linked into all DLLs on
// Windows. I think Starboard can do this through GYP when the time comes.

#include <stdlib.h>
#include <new>

void* operator new(size_t size) {
  return malloc(size);
}

void operator delete(void* pointer) noexcept {
  free(pointer);
}

void* operator new(size_t size, const std::nothrow_t& nothrow_tag) noexcept {
  return malloc(size);
}

void operator delete(void* pointer, const std::nothrow_t& nothrow_tag) {
  free(pointer);
}

void* operator new[](size_t size) {
  return malloc(size);
}

void operator delete[](void* pointer) noexcept {
  free(pointer);
}

#if __cpp_aligned_new
void* operator new(size_t size, std::align_val_t align) {
  void* p = nullptr;
  posix_memalign(&p, static_cast<size_t>(align), size);
  return p;
}

void operator delete(void* pointer, std::align_val_t align) {
  free(pointer);
}
#endif
