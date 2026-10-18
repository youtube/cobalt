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

#include "cobalt/starboard_wasm/core/guest_memory.h"

#include <limits>

#include "v8/include/v8-array-buffer.h"
#include "v8/include/v8-local-handle.h"
#include "v8/include/v8-wasm.h"

namespace cobalt {
namespace starboard_wasm {

GuestMemory::GuestMemory(v8::Isolate* isolate,
                         const v8::Global<v8::WasmMemoryObject>& memory)
    : isolate_(isolate), memory_(memory) {}

uint8_t* GuestMemory::Bytes(uint32_t ptr, uint32_t len) {
  return Resolve(ptr, len);
}

uint8_t* GuestMemory::Array(uint32_t ptr, uint32_t count, size_t elem_size) {
  const uint64_t bytes = static_cast<uint64_t>(count) * elem_size;
  if (bytes > std::numeric_limits<uint32_t>::max()) {
    return nullptr;
  }
  return Resolve(ptr, bytes);
}

uint8_t* GuestMemory::Resolve(uint32_t ptr, uint64_t len) {
  if (len == 0 || !Load()) {
    return nullptr;
  }
  if (ptr > size_ || len > size_ - ptr) {
    return nullptr;
  }
  return base_ + ptr;
}

bool GuestMemory::Load() {
  if (loaded_) {
    return base_ != nullptr;
  }
  loaded_ = true;
  if (memory_.IsEmpty() || !isolate_) {
    return false;
  }
  v8::HandleScope handle_scope(isolate_);
  v8::Local<v8::ArrayBuffer> buffer = memory_.Get(isolate_)->Buffer();
  base_ = static_cast<uint8_t*>(buffer->Data());
  size_ = buffer->ByteLength();
  return base_ != nullptr;
}

}  // namespace starboard_wasm
}  // namespace cobalt
