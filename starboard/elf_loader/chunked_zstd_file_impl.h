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

#ifndef STARBOARD_ELF_LOADER_CHUNKED_ZSTD_FILE_IMPL_H_
#define STARBOARD_ELF_LOADER_CHUNKED_ZSTD_FILE_IMPL_H_

#include <vector>

#include "starboard/elf_loader/file_impl.h"
#define ZSTD_STATIC_LINKING_ONLY
#include "third_party/zstd/src/lib/zstd.h"

namespace elf_loader {

// This class decompresses the entire file into memory on Open, but it reads
// the compressed source file in chunks to save transient RAM.
class ChunkedZstdFileImpl : public FileImpl {
 public:
  ChunkedZstdFileImpl();
  ~ChunkedZstdFileImpl();

  bool Open(const char* name) override;
  bool ReadFromOffset(int64_t offset, char* buffer, int size) override;

 private:
  bool Decompress();

  std::vector<char> decompressed_data_;
  ZSTD_DCtx* dctx_;
};

}  // namespace elf_loader

#endif  // STARBOARD_ELF_LOADER_CHUNKED_ZSTD_FILE_IMPL_H_
