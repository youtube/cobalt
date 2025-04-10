// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_EVACUATION_VERIFIER_INL_H_
#define V8_HEAP_EVACUATION_VERIFIER_INL_H_

#include "src/heap/evacuation-verifier.h"
#include "src/heap/heap-inl.h"
#include "src/heap/mark-compact.h"

namespace v8 {
namespace internal {

#ifdef VERIFY_HEAP

void FullEvacuationVerifier::VerifyHeapObjectImpl(HeapObject heap_object) {
  if (!ShouldVerifyObject(heap_object)) return;
  CHECK_IMPLIES(Heap::InYoungGeneration(heap_object),
                Heap::InToPage(heap_object));
  CHECK(!MarkCompactCollector::IsOnEvacuationCandidate(heap_object));
}

bool FullEvacuationVerifier::ShouldVerifyObject(HeapObject heap_object) {
  const bool in_shared_heap = heap_object.InWritableSharedSpace();
  return heap_->isolate()->is_shared_space_isolate() ? in_shared_heap
                                                     : !in_shared_heap;
}

template <typename TSlot>
void FullEvacuationVerifier::VerifyPointersImpl(TSlot start, TSlot end) {
  for (TSlot current = start; current < end; ++current) {
    typename TSlot::TObject object = current.load(cage_base());
    HeapObject heap_object;
    if (object.GetHeapObjectIfStrong(&heap_object)) {
      VerifyHeapObjectImpl(heap_object);
    }
  }
}

void YoungGenerationEvacuationVerifier::VerifyHeapObjectImpl(
    HeapObject heap_object) {
  CHECK_IMPLIES(Heap::InYoungGeneration(heap_object),
                Heap::InToPage(heap_object));
}

template <typename TSlot>
void YoungGenerationEvacuationVerifier::VerifyPointersImpl(TSlot start,
                                                           TSlot end) {
  for (TSlot current = start; current < end; ++current) {
    typename TSlot::TObject object = current.load(cage_base());
    HeapObject heap_object;
    if (object.GetHeapObject(&heap_object)) {
      VerifyHeapObjectImpl(heap_object);
    }
  }
}

#endif  // VERIFY_HEAP

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_EVACUATION_VERIFIER_INL_H_
