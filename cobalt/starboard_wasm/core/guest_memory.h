// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_STARBOARD_WASM_CORE_GUEST_MEMORY_H_
#define COBALT_STARBOARD_WASM_CORE_GUEST_MEMORY_H_

#include <stddef.h>
#include <stdint.h>

#include "base/memory/stack_allocated.h"
#include "v8/include/v8-forward.h"
#include "v8/include/v8-persistent-handle.h"

namespace cobalt {
namespace starboard_wasm {

// Resolves guest pointers, which are byte offsets into the WebAssembly.Memory
// bound with bindMemory(), for the duration of one import call. Memory can
// grow between calls, so the backing store is looked up once per call, on
// first use.
class GuestMemory {
  STACK_ALLOCATED();

 public:
  // |memory| may be empty, in which case every lookup fails.
  GuestMemory(v8::Isolate* isolate,
              const v8::Global<v8::WasmMemoryObject>& memory);
  GuestMemory(const GuestMemory&) = delete;
  GuestMemory& operator=(const GuestMemory&) = delete;

  // Returns a host pointer for [ptr, ptr + len), or nullptr if the range is
  // empty or outside guest memory.
  uint8_t* Bytes(uint32_t ptr, uint32_t len);

  // Like Bytes() for |count| elements of |elem_size| bytes each.
  uint8_t* Array(uint32_t ptr, uint32_t count, size_t elem_size);

 private:
  uint8_t* Resolve(uint32_t ptr, uint64_t len);
  bool Load();

  v8::Isolate* const isolate_;
  const v8::Global<v8::WasmMemoryObject>& memory_;
  bool loaded_ = false;
  uint8_t* base_ = nullptr;
  size_t size_ = 0;
};

}  // namespace starboard_wasm
}  // namespace cobalt

#endif  // COBALT_STARBOARD_WASM_CORE_GUEST_MEMORY_H_
