/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/typed_arrays/array_buffer/array_buffer_contents.h"

#include <cstring>

#include "base/allocator/partition_allocator/partition_alloc.h"
#include "base/bits.h"
#include "base/system/sys_info.h"
#include "gin/array_buffer.h"
#include "third_party/blink/renderer/platform/instrumentation/instance_counters.h"
#include "third_party/blink/renderer/platform/wtf/allocator/partitions.h"

namespace blink {

ArrayBufferContents::ArrayBufferContents(
    const base::subtle::PlatformSharedMemoryRegion& region,
    uint64_t offset,
    size_t length) {
  DCHECK(region.IsValid());

  // The offset must be a multiples of |SysInfo::VMAllocationGranularity()|.
  size_t offset_rounding = offset % base::SysInfo::VMAllocationGranularity();
  uint64_t real_offset = offset - offset_rounding;
  size_t real_length = length + offset_rounding;

  absl::optional<base::span<uint8_t>> result = region.MapAt(
      real_offset, real_length, gin::GetSharedMemoryMapperForArrayBuffers());
  if (!result.has_value()) {
    return;
  }

  auto deleter = [](void* buffer, size_t length, void* data) {
    size_t offset = reinterpret_cast<uintptr_t>(buffer) %
                    base::SysInfo::VMAllocationGranularity();
    uint8_t* base = static_cast<uint8_t*>(buffer) - offset;
    base::span<uint8_t> mapping = base::make_span(base, length + offset);
    gin::GetSharedMemoryMapperForArrayBuffers()->Unmap(mapping);
  };
  void* base = result.value().data() + offset_rounding;
  backing_store_ =
      v8::ArrayBuffer::NewBackingStore(base, length, deleter, nullptr);
}

ArrayBufferContents::ArrayBufferContents(
    size_t num_elements,
    absl::optional<size_t> max_num_elements,
    size_t element_byte_size,
    SharingType is_shared,
    ArrayBufferContents::InitializationPolicy policy) {
  auto checked_length =
      base::CheckedNumeric<size_t>(num_elements) * element_byte_size;
  if (!checked_length.IsValid()) {
    // The requested size is too big, we cannot allocate the memory and
    // therefore just return.
    return;
  }
  size_t length = checked_length.ValueOrDie();

  if (!max_num_elements) {
    // Create a fixed-length ArrayBuffer.
    void* data = AllocateMemoryOrNull(length, policy);
    if (!data) {
      return;
    }
    auto deleter = [](void* data, size_t, void*) { FreeMemory(data); };
    if (is_shared == kNotShared) {
      backing_store_ =
          v8::ArrayBuffer::NewBackingStore(data, length, deleter, nullptr);
    } else {
      backing_store_ = v8::SharedArrayBuffer::NewBackingStore(data, length,
                                                              deleter, nullptr);
    }
  } else {
    // The resizable form of the constructor is currently only used for IPC
    // transfers of ArrayBuffers, and SharedArrayBuffers cannot be transferred
    // across agent clusters.
    DCHECK_EQ(kNotShared, is_shared);
    // Currently V8 does not support embedder-allocated resizable backing
    // stores. It does not zero resizable allocations, which use a
    // reserve-and-partially-commit pattern. Check that the caller is not
    // expecting zeroed memory.
    CHECK_EQ(kDontInitialize, policy);
    auto max_checked_length =
        base::CheckedNumeric<size_t>(*max_num_elements) * element_byte_size;
    size_t max_length = max_checked_length.ValueOrDie();
    backing_store_ =
        v8::ArrayBuffer::NewResizableBackingStore(length, max_length);
  }
}

ArrayBufferContents::~ArrayBufferContents() = default;

void ArrayBufferContents::Detach() {
  backing_store_.reset();
}

void ArrayBufferContents::Reset() {
  backing_store_.reset();
}

void ArrayBufferContents::Transfer(ArrayBufferContents& other) {
  DCHECK(!IsShared());
  DCHECK(!other.Data());
  other.backing_store_ = std::move(backing_store_);
}

void ArrayBufferContents::ShareWith(ArrayBufferContents& other) {
  DCHECK(IsShared());
  DCHECK(!other.Data());
  other.backing_store_ = backing_store_;
}

void ArrayBufferContents::ShareNonSharedForInternalUse(
    ArrayBufferContents& other) {
  DCHECK(!IsShared());
  DCHECK(!other.Data());
  DCHECK(Data());
  other.backing_store_ = backing_store_;
}

void ArrayBufferContents::CopyTo(ArrayBufferContents& other) {
  other = ArrayBufferContents(
      DataLength(), 1, IsShared() ? kShared : kNotShared, kDontInitialize);
  if (!IsValid() || !other.IsValid())
    return;
  std::memcpy(other.Data(), Data(), DataLength());
}

void* ArrayBufferContents::AllocateMemoryWithFlags(size_t size,
                                                   InitializationPolicy policy,
                                                   unsigned int flags) {
  // The array buffer contents are sometimes expected to be 16-byte aligned in
  // order to get the best optimization of SSE, especially in case of audio and
  // video buffers.  Hence, align the given size up to 16-byte boundary.
  // Technically speaking, 16-byte aligned size doesn't mean 16-byte aligned
  // address, but this heuristics works with the current implementation of
  // PartitionAlloc (and PartitionAlloc doesn't support a better way for now).
  //
  // `partition_alloc::internal::kAlignment` is a compile-time constant.
  if (partition_alloc::internal::kAlignment < 16) {
    size_t aligned_size = base::bits::AlignUp(size, size_t{16});
    if (size == 0) {
      aligned_size = 16;
    }
    if (aligned_size >= size) {  // Only when no overflow
      size = aligned_size;
    }
  }

#ifdef V8_ENABLE_SANDBOX
  // The V8 sandbox requires all ArrayBuffer backing stores to be allocated
  // inside the sandbox address space. This isn't guaranteed if allocation
  // override hooks (which are e.g. used by GWP-ASan) are enabled or if a
  // memory tool (e.g. ASan) overrides malloc. However, allocation observer
  // hooks (which are e.g. used by the heap profiler) should still be invoked.
  // Using the kNoOverrideHooks and kNoMemoryToolOverride flags with
  // accomplishes this.
  flags |= partition_alloc::AllocFlags::kNoOverrideHooks;
  flags |= partition_alloc::AllocFlags::kNoMemoryToolOverride;
#endif
  if (policy == kZeroInitialize) {
    flags |= partition_alloc::AllocFlags::kZeroFill;
  }
  void* data = WTF::Partitions::ArrayBufferPartition()->AllocWithFlags(
      flags, size, WTF_HEAP_PROFILER_TYPE_NAME(ArrayBufferContents));
  if (partition_alloc::internal::kAlignment < 16) {
    char* ptr = reinterpret_cast<char*>(data);
    DCHECK_EQ(base::bits::AlignUp(ptr, 16), ptr)
        << "Pointer " << ptr << " not 16B aligned for size " << size;
  }
  InstanceCounters::IncrementCounter(
      InstanceCounters::kArrayBufferContentsCounter);
  return data;
}

void* ArrayBufferContents::AllocateMemoryOrNull(size_t size,
                                                InitializationPolicy policy) {
  return AllocateMemoryWithFlags(size, policy,
                                 partition_alloc::AllocFlags::kReturnNull);
}

void ArrayBufferContents::FreeMemory(void* data) {
  InstanceCounters::DecrementCounter(
      InstanceCounters::kArrayBufferContentsCounter);
  unsigned int flags = 0;
#ifdef V8_ENABLE_SANDBOX
  // See |AllocateMemoryWithFlags|.
  flags |= partition_alloc::FreeFlags::kNoMemoryToolOverride;
#endif
  WTF::Partitions::ArrayBufferPartition()->FreeWithFlags(flags, data);
}

}  // namespace blink
