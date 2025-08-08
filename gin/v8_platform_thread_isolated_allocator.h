// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GIN_V8_PLATFORM_THREAD_ISOLATED_ALLOCATOR_H_
#define GIN_V8_PLATFORM_THREAD_ISOLATED_ALLOCATOR_H_

#include "base/allocator/partition_allocator/src/partition_alloc/partition_alloc_buildflags.h"

#if BUILDFLAG(ENABLE_THREAD_ISOLATION)

#if !BUILDFLAG(ENABLE_PKEYS)
#error Not implemented for non-pkey thread isolation
#endif  // BUILDFLAG(ENABLE_PKEYS)

#include "base/allocator/partition_allocator/src/partition_alloc/partition_alloc.h"
#include "gin/gin_export.h"
#include "v8/include/v8-platform.h"

namespace gin {

// This is a wrapper around PartitionAlloc's ThreadIsolated pool that we pass to
// v8.
class GIN_EXPORT ThreadIsolatedAllocator final
    : public v8::ThreadIsolatedAllocator {
 public:
  ThreadIsolatedAllocator();
  ~ThreadIsolatedAllocator() override;

  void Initialize(int pkey);

  void* Allocate(size_t size) override;
  void Free(void* object) override;
  enum Type Type() const override;

  int Pkey() const override;

 private:
  partition_alloc::PartitionAllocator allocator_;
  int pkey_ = -1;
};

}  // namespace gin

#endif  // BUILDFLAG(ENABLE_THREAD_ISOLATION)

#endif  // GIN_V8_PLATFORM_THREAD_ISOLATED_ALLOCATOR_H_
