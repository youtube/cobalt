// Copyright 2024 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_ELF_LOADER_CONTENTION_DIAGNOSTIC_FILE_IMPL_H_
#define STARBOARD_ELF_LOADER_CONTENTION_DIAGNOSTIC_FILE_IMPL_H_

#include <pthread.h>
#include <atomic>
#include <memory>
#include <vector>

#include "starboard/elf_loader/file_impl.h"

namespace elf_loader {

// This class is a diagnostic tool to measure memory bus contention between
// disk I/O and CPU memory access.
class ContentionDiagnosticFileImpl : public FileImpl {
 public:
  ContentionDiagnosticFileImpl();
  ~ContentionDiagnosticFileImpl();

  bool Open(const char* name) override;
  bool ReadFromOffset(int64_t offset, char* buffer, int size) override;

 private:
  struct WorkBlock {
    std::unique_ptr<char[]> data;
    size_t size;
  };

  struct ThreadContext {
    ContentionDiagnosticFileImpl* impl;
    const std::vector<WorkBlock>* blocks;
    std::atomic<size_t>* next_block;
    uint64_t* result_sum;
  };

  static void* WorkerThreadEntry(void* context);
  void RunMemoryWork(const std::vector<WorkBlock>& blocks, const char* label);

  std::unique_ptr<char[]> dummy_dest_;
};

}  // namespace elf_loader

#endif  // STARBOARD_ELF_LOADER_CONTENTION_DIAGNOSTIC_FILE_IMPL_H_
