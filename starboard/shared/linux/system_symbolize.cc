// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

// TODO: Figure out how to gracefully use symbolize given where it has
// been placed in the source tree. What follows is an ungraceful use.

#include "starboard/system.h"

#include "base/third_party/symbolize/symbolize.h"

bool SbSystemSymbolize(const void* address, char* out_buffer, int buffer_size) {
  // I believe this casting-away const in the implementation is better than the
  // alternative of removing const-ness from the address parameter.
  return google::Symbolize(const_cast<void*>(address), out_buffer, buffer_size);
}
