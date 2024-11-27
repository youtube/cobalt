// Copyright 2021 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_ELF_LOADER_LZ4_FILE_IMPL_H_
#define STARBOARD_ELF_LOADER_LZ4_FILE_IMPL_H_

#include <vector>

#include "starboard/elf_loader/file_impl.h"
#include "third_party/lz4_lib/lz4frame.h"

namespace starboard {
namespace elf_loader {

// This class provides opening and reading a file compressed using LZ4 by
// transparently decompressing the entire file into memory on file open.
// The file must be encoded using the LZ4 Frame Format and consist of a single
// frame.
class LZ4FileImpl : public FileImpl {
 public:
  LZ4FileImpl();
  ~LZ4FileImpl();

  // Opens the file specified and decompresses the entire file into memory.
  bool Open(const char* name) override;

  bool ReadFromOffset(int64_t offset, char* buffer, int size) override;

 private:
  // Returns the size of the LZ4 frame header in bytes.
  size_t PeekHeaderSize();

  // Reads the LZ4 frame header information from the file into |frame_info|.
  // Once the header has been read, decompression of the data blocks must begin
  // |header_size| bytes into the file (i.e., just after the frame header
  // section). This function returns a hint for the number of source bytes that
  // LZ4F_decompress() expects for its first invocation.
  size_t ConsumeHeader(LZ4F_frameInfo_t* frame_info, size_t header_size);

  // Decompresses the remainder of the LZ4 file into memory.
  // This function should only be called after the frame header has been read.
  // It repeatedly 1) buffers up to |max_compressed_buffer_size| bytes and 2)
  // asks LZ4F_decompress() to decompress the data; |source_bytes_hint| is only
  // used for the first of these buffer-decompress iterations.
  bool Decompress(size_t file_size,
                  size_t header_size,
                  size_t max_compressed_buffer_size,
                  size_t source_bytes_hint);

  // The entire decompressed file.
  std::vector<char> decompressed_data_;

  // The LZ4 decompression context.
  LZ4F_dctx* lz4f_context_;
};

}  // namespace elf_loader
}  // namespace starboard

#endif  // STARBOARD_ELF_LOADER_LZ4_FILE_IMPL_H_
