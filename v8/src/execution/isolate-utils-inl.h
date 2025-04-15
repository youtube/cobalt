// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_EXECUTION_ISOLATE_UTILS_INL_H_
#define V8_EXECUTION_ISOLATE_UTILS_INL_H_

#include "src/common/ptr-compr-inl.h"
#include "src/execution/isolate-utils.h"
#include "src/execution/isolate.h"
#include "src/heap/heap-write-barrier-inl.h"

namespace v8 {
namespace internal {

V8_INLINE Heap* GetHeapFromWritableObject(HeapObject object) {
  // Avoid using the below GetIsolateFromWritableObject because we want to be
  // able to get the heap, but not the isolate, for off-thread objects.

#if defined V8_ENABLE_THIRD_PARTY_HEAP
  return Heap::GetIsolateFromWritableObject(object)->heap();
#else
  heap_internals::MemoryChunk* chunk =
      heap_internals::MemoryChunk::FromHeapObject(object);
  return chunk->GetHeap();
#endif  // V8_ENABLE_THIRD_PARTY_HEAP
}

V8_INLINE Isolate* GetIsolateFromWritableObject(HeapObject object) {
#ifdef V8_ENABLE_THIRD_PARTY_HEAP
  return Heap::GetIsolateFromWritableObject(object);
#else
  return Isolate::FromHeap(GetHeapFromWritableObject(object));
#endif  // V8_ENABLE_THIRD_PARTY_HEAP
}

V8_INLINE bool GetIsolateFromHeapObject(HeapObject object, Isolate** isolate) {
#ifdef V8_ENABLE_THIRD_PARTY_HEAP
  *isolate = Heap::GetIsolateFromWritableObject(object);
  return true;
#else
  heap_internals::MemoryChunk* chunk =
      heap_internals::MemoryChunk::FromHeapObject(object);
  if (chunk->InReadOnlySpace()) {
    *isolate = nullptr;
    return false;
  }
  *isolate = Isolate::FromHeap(chunk->GetHeap());
  return true;
#endif  // V8_ENABLE_THIRD_PARTY_HEAP
}

// Use this function instead of Internals::GetIsolateForSandbox for internal
// code, as this function is fully inlinable.
V8_INLINE static Isolate* GetIsolateForSandbox(HeapObject object) {
#ifdef V8_ENABLE_SANDBOX
  return GetIsolateFromWritableObject(object);
#else
  // Not used in non-sandbox mode.
  return nullptr;
#endif
}

}  // namespace internal
}  // namespace v8

#endif  // V8_EXECUTION_ISOLATE_UTILS_INL_H_
