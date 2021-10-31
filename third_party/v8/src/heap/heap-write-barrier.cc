// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/heap-write-barrier.h"

#include "src/heap/heap-write-barrier-inl.h"
#include "src/heap/marking-barrier.h"
#include "src/objects/descriptor-array.h"
#include "src/objects/maybe-object.h"
#include "src/objects/slots-inl.h"
#include "src/objects/slots.h"

#if !defined(V8_OS_STARBOARD)
namespace v8 {
namespace internal {

namespace {
thread_local MarkingBarrier* current_marking_barrier = nullptr;
}  // namespace

void WriteBarrier::SetForThread(MarkingBarrier* marking_barrier) {
  DCHECK_NULL(current_marking_barrier);
  current_marking_barrier = marking_barrier;
}

void WriteBarrier::ClearForThread(MarkingBarrier* marking_barrier) {
  DCHECK_EQ(current_marking_barrier, marking_barrier);
  current_marking_barrier = nullptr;
}

MarkingBarrier* GetMarkingBarrier() { return current_marking_barrier; }
#else
#include "starboard/common/log.h"
#include "starboard/once.h"
#include "starboard/thread.h"

namespace v8 {
namespace internal {

namespace {
SbOnceControl s_once_flag = SB_ONCE_INITIALIZER;
SbThreadLocalKey s_thread_local_key = kSbThreadLocalKeyInvalid;

void InitThreadLocalKey() {
  s_thread_local_key = SbThreadCreateLocalKey(NULL);
  SB_DCHECK(SbThreadIsValidLocalKey(s_thread_local_key));
}

void EnsureThreadLocalKeyInited() {
  SbOnce(&s_once_flag, InitThreadLocalKey);
  SB_DCHECK(SbThreadIsValidLocalKey(s_thread_local_key));
}

MarkingBarrier* GetMarkingBarrier() {
  return static_cast<MarkingBarrier*>(
      SbThreadGetLocalValue(s_thread_local_key));
}
}  // namespace

void WriteBarrier::SetForThread(MarkingBarrier* marking_barrier) {
  EnsureThreadLocalKeyInited();
  SbThreadSetLocalValue(s_thread_local_key, marking_barrier);
}

void WriteBarrier::ClearForThread(MarkingBarrier* marking_barrier) {
  SbThreadSetLocalValue(s_thread_local_key, NULL);
}

#endif

void WriteBarrier::MarkingSlow(Heap* heap, HeapObject host, HeapObjectSlot slot,
                               HeapObject value) {
  MarkingBarrier* current_marking_barrier = GetMarkingBarrier();
  MarkingBarrier* marking_barrier = current_marking_barrier
                                        ? current_marking_barrier
                                        : heap->marking_barrier();
  marking_barrier->Write(host, slot, value);
}

void WriteBarrier::MarkingSlow(Heap* heap, Code host, RelocInfo* reloc_info,
                               HeapObject value) {
  MarkingBarrier* current_marking_barrier = GetMarkingBarrier();
  MarkingBarrier* marking_barrier = current_marking_barrier
                                        ? current_marking_barrier
                                        : heap->marking_barrier();
  marking_barrier->Write(host, reloc_info, value);
}

void WriteBarrier::MarkingSlow(Heap* heap, JSArrayBuffer host,
                               ArrayBufferExtension* extension) {
  MarkingBarrier* current_marking_barrier = GetMarkingBarrier();
  MarkingBarrier* marking_barrier = current_marking_barrier
                                        ? current_marking_barrier
                                        : heap->marking_barrier();
  marking_barrier->Write(host, extension);
}

void WriteBarrier::MarkingSlow(Heap* heap, DescriptorArray descriptor_array,
                               int number_of_own_descriptors) {
  MarkingBarrier* current_marking_barrier = GetMarkingBarrier();
  MarkingBarrier* marking_barrier = current_marking_barrier
                                        ? current_marking_barrier
                                        : heap->marking_barrier();
  marking_barrier->Write(descriptor_array, number_of_own_descriptors);
}

int WriteBarrier::MarkingFromCode(Address raw_host, Address raw_slot) {
  HeapObject host = HeapObject::cast(Object(raw_host));
  MaybeObjectSlot slot(raw_slot);
  WriteBarrier::Marking(host, slot, *slot);
  // Called by RecordWriteCodeStubAssembler, which doesnt accept void type
  return 0;
}

}  // namespace internal
}  // namespace v8
