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

#include <stdlib.h>

extern "C" {

// sdallocx is a sized |free| function. By passing the size (which we happen to
// always know in BoringSSL), the malloc implementation can save work. We cannot
// depend on |sdallocx| being available, however, so it's a weak symbol.
//
// This is defined here to satisfy the weak symbol in BoringSSL and prevent it
// from being exported as an undefined weak symbol.
void sdallocx(void* ptr, size_t /*size*/, int /*flags*/) {
  free(ptr);
}

}  // extern "C"
