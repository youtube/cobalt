// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_CPPGC_JS_UNIFIED_HEAP_MARKING_STATE_INL_H_
#define V8_HEAP_CPPGC_JS_UNIFIED_HEAP_MARKING_STATE_INL_H_

#include <atomic>

#include "include/v8-traced-handle.h"
#include "src/base/logging.h"
#include "src/handles/traced-handles.h"
#include "src/heap/cppgc-js/unified-heap-marking-state.h"
#include "src/heap/heap.h"
#include "src/heap/mark-compact.h"
#include "src/heap/marking-state-inl.h"
#include "src/heap/marking-worklist-inl.h"
#include "src/objects/objects-inl.h"

namespace v8 {
namespace internal {

class BasicTracedReferenceExtractor final {
 public:
  static Address* GetObjectSlotForMarking(const TracedReferenceBase& ref) {
    return const_cast<Address*>(
        reinterpret_cast<const Address*>(ref.GetSlotThreadSafe()));
  }
};

void UnifiedHeapMarkingState::MarkAndPush(
    const TracedReferenceBase& reference) {
  // The following code will crash with null pointer derefs when finding a
  // non-empty `TracedReferenceBase` when `CppHeap` is in detached mode.
  Address* traced_handle_location =
      BasicTracedReferenceExtractor::GetObjectSlotForMarking(reference);
  // We cannot assume that the reference is non-null as we may get here by
  // tracing an ephemeron which doesn't have early bailouts, see
  // `cppgc::Visitor::TraceEphemeron()` for non-Member values.
  if (!traced_handle_location) {
    return;
  }
  Object object = TracedHandles::Mark(traced_handle_location, mark_mode_);
  if (!object.IsHeapObject()) {
    // The embedder is not aware of whether numbers are materialized as heap
    // objects are just passed around as Smis.
    return;
  }
  HeapObject heap_object = HeapObject::cast(object);
  if (heap_object.InReadOnlySpace()) return;
  if (marking_state_->TryMark(heap_object)) {
    local_marking_worklist_->Push(heap_object);
  }
  if (V8_UNLIKELY(track_retaining_path_)) {
    heap_->AddRetainingRoot(Root::kWrapperTracing, heap_object);
  }
}

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_CPPGC_JS_UNIFIED_HEAP_MARKING_STATE_INL_H_
