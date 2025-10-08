/*
 * Copyright (C) 2017 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef SRC_TRACING_TEST_TEST_SHARED_MEMORY_H_
#define SRC_TRACING_TEST_TEST_SHARED_MEMORY_H_

#include <stddef.h>

#include <memory>

<<<<<<< HEAD
#include "perfetto/ext/tracing/core/shared_memory.h"
#include "src/tracing/core/in_process_shared_memory.h"
=======
#include "perfetto/ext/base/paged_memory.h"
#include "perfetto/ext/tracing/core/shared_memory.h"
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))

namespace perfetto {

// A dummy implementation of shared memory for single process unittests
// (just a wrapper around malloc() that fits the SharedMemory API).
<<<<<<< HEAD
using TestSharedMemory = InProcessSharedMemory;

// An implementation of the SharedMemory that doesn't own any memory, but just
// points to memory owned by another SharedMemory.
//
// This is useful to test two components that own separate SharedMemory that
// really point to the same memory underneath without setting up real posix
// shared memory.
class TestRefSharedMemory : public SharedMemory {
 public:
  // N.B. `*mem` must outlive `*this`.
  explicit TestRefSharedMemory(SharedMemory* mem)
      : start_(mem->start()), size_(mem->size()) {}
  ~TestRefSharedMemory() override;

  static std::unique_ptr<TestRefSharedMemory> Create(SharedMemory* mem) {
    return std::make_unique<TestRefSharedMemory>(mem);
  }

  // SharedMemory implementation.
  using SharedMemory::start;  // Equal priority to const and non-const versions
  const void* start() const override;
  size_t size() const override;

 private:
  void* start_;
=======
class TestSharedMemory : public SharedMemory {
 public:
  class Factory : public SharedMemory::Factory {
   public:
    ~Factory() override;
    std::unique_ptr<SharedMemory> CreateSharedMemory(size_t size) override;
  };

  explicit TestSharedMemory(size_t size);
  ~TestSharedMemory() override;

  void* start() const override { return mem_.Get(); }
  size_t size() const override { return size_; }

  base::PagedMemory mem_;
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
  size_t size_;
};

}  // namespace perfetto

#endif  // SRC_TRACING_TEST_TEST_SHARED_MEMORY_H_
