// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/common/assert-scope.h"

#include "src/base/bit-field.h"
#include "src/base/lazy-instance.h"
#include "src/base/platform/platform.h"
#include "src/execution/isolate.h"
#include "src/utils/utils.h"

namespace v8 {
namespace internal {

namespace {

template <PerThreadAssertType kType>
using PerThreadDataBit = base::BitField<bool, kType, 1>;
template <PerIsolateAssertType kType>
using PerIsolateDataBit = base::BitField<bool, kType, 1>;

// Thread-local storage for assert data. Default all asserts to "allow".
#if defined(V8_OS_STARBOARD)
static_assert(sizeof(uint32_t) <= sizeof(void*), "uint32_t won't fit in void*");
DEFINE_LAZY_LEAKY_OBJECT_GETTER(base::Thread::LocalStorageKey, GetAssertDataKey,
                                base::Thread::CreateThreadLocalKey())
#else
thread_local uint32_t current_per_thread_assert_data(~0);
#endif  // defined(V8_OS_STARBOARD)

}  // namespace

template <PerThreadAssertType kType, bool kAllow>
PerThreadAssertScope<kType, kAllow>::PerThreadAssertScope()
#if defined(V8_OS_STARBOARD)
    : old_data_(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(
          base::Thread::GetThreadLocal(*GetAssertDataKey())))) {
  uint32_t data = PerThreadDataBit<kType>::update(old_data_.value(), kAllow);
  base::Thread::SetThreadLocal(
      *GetAssertDataKey(),
      reinterpret_cast<void*>(static_cast<uintptr_t>(data)));
#else
    : old_data_(current_per_thread_assert_data) {
  current_per_thread_assert_data =
      PerThreadDataBit<kType>::update(old_data_.value(), kAllow);
#endif  // defined(V8_OS_STARBOARD)
}

template <PerThreadAssertType kType, bool kAllow>
PerThreadAssertScope<kType, kAllow>::~PerThreadAssertScope() {
  if (!old_data_.has_value()) return;
  Release();
}

template <PerThreadAssertType kType, bool kAllow>
void PerThreadAssertScope<kType, kAllow>::Release() {
#if defined(V8_OS_STARBOARD)
  base::Thread::SetThreadLocal(
      *GetAssertDataKey(),
      reinterpret_cast<void*>(static_cast<uintptr_t>(old_data_.value())));
#else
  current_per_thread_assert_data = old_data_.value();
#endif  // defined(V8_OS_STARBOARD)
  old_data_.reset();
}

// static
template <PerThreadAssertType kType, bool kAllow>
bool PerThreadAssertScope<kType, kAllow>::IsAllowed() {
#if defined(V8_OS_STARBOARD)
  uint32_t current_per_thread_assert_data =
      static_cast<uint32_t>(reinterpret_cast<uintptr_t>(
          base::Thread::GetThreadLocal(*GetAssertDataKey())));
#endif  // defined(V8_OS_STARBOARD)
  return PerThreadDataBit<kType>::decode(current_per_thread_assert_data);
}

template <PerIsolateAssertType kType, bool kAllow>
PerIsolateAssertScope<kType, kAllow>::PerIsolateAssertScope(Isolate* isolate)
    : isolate_(isolate), old_data_(isolate->per_isolate_assert_data()) {
  DCHECK_NOT_NULL(isolate);
  isolate_->set_per_isolate_assert_data(
      PerIsolateDataBit<kType>::update(old_data_, kAllow));
}

template <PerIsolateAssertType kType, bool kAllow>
PerIsolateAssertScope<kType, kAllow>::~PerIsolateAssertScope() {
  isolate_->set_per_isolate_assert_data(old_data_);
}

// static
template <PerIsolateAssertType kType, bool kAllow>
bool PerIsolateAssertScope<kType, kAllow>::IsAllowed(Isolate* isolate) {
  return PerIsolateDataBit<kType>::decode(isolate->per_isolate_assert_data());
}

// -----------------------------------------------------------------------------
// Instantiations.

template class PerThreadAssertScope<HEAP_ALLOCATION_ASSERT, false>;
template class PerThreadAssertScope<HEAP_ALLOCATION_ASSERT, true>;
template class PerThreadAssertScope<SAFEPOINTS_ASSERT, false>;
template class PerThreadAssertScope<SAFEPOINTS_ASSERT, true>;
template class PerThreadAssertScope<HANDLE_ALLOCATION_ASSERT, false>;
template class PerThreadAssertScope<HANDLE_ALLOCATION_ASSERT, true>;
template class PerThreadAssertScope<HANDLE_DEREFERENCE_ASSERT, false>;
template class PerThreadAssertScope<HANDLE_DEREFERENCE_ASSERT, true>;
template class PerThreadAssertScope<CODE_DEPENDENCY_CHANGE_ASSERT, false>;
template class PerThreadAssertScope<CODE_DEPENDENCY_CHANGE_ASSERT, true>;
template class PerThreadAssertScope<CODE_ALLOCATION_ASSERT, false>;
template class PerThreadAssertScope<CODE_ALLOCATION_ASSERT, true>;
template class PerThreadAssertScope<GC_MOLE, false>;

template class PerIsolateAssertScope<JAVASCRIPT_EXECUTION_ASSERT, false>;
template class PerIsolateAssertScope<JAVASCRIPT_EXECUTION_ASSERT, true>;
template class PerIsolateAssertScope<JAVASCRIPT_EXECUTION_THROWS, false>;
template class PerIsolateAssertScope<JAVASCRIPT_EXECUTION_THROWS, true>;
template class PerIsolateAssertScope<JAVASCRIPT_EXECUTION_DUMP, false>;
template class PerIsolateAssertScope<JAVASCRIPT_EXECUTION_DUMP, true>;
template class PerIsolateAssertScope<DEOPTIMIZATION_ASSERT, false>;
template class PerIsolateAssertScope<DEOPTIMIZATION_ASSERT, true>;
template class PerIsolateAssertScope<COMPILATION_ASSERT, false>;
template class PerIsolateAssertScope<COMPILATION_ASSERT, true>;
template class PerIsolateAssertScope<NO_EXCEPTION_ASSERT, false>;
template class PerIsolateAssertScope<NO_EXCEPTION_ASSERT, true>;

}  // namespace internal
}  // namespace v8
