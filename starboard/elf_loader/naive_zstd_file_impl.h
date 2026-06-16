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

#ifndef STARBOARD_ELF_LOADER_NAIVE_ZSTD_FILE_IMPL_H_
#define STARBOARD_ELF_LOADER_NAIVE_ZSTD_FILE_IMPL_H_

#include <vector>

#include "starboard/elf_loader/file_impl.h"

namespace elf_loader {

// Provides inefficient opening and reading of a Zstandard compressed file by
// transparently decompressing it into memory on file open.
//
// This "naive" implementation uses a single thread for decompression and
// should not be used in production. The .zst file must consist of a single
// frame.
//
// Lifetime and Ownership:
// This class is owned by the ElfLoaderImpl instance and its lifetime is tied
// to it.
class NaiveZstdFileImpl : public FileImpl {
 public:
  NaiveZstdFileImpl() = default;
  ~NaiveZstdFileImpl() override = default;

  // Opens the specified file and decompresses it into memory.
  bool Open(const char* name) override;

  // Reads the requested bytes from memory.
  bool ReadFromOffset(int64_t offset, char* buffer, int size) override;

 private:
  std::vector<char> decompressed_data_;
};

}  // namespace elf_loader

#endif  // STARBOARD_ELF_LOADER_NAIVE_ZSTD_FILE_IMPL_H_
