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

#include <vector>

#include "starboard/elf_loader/lz4_file_impl.h"

#include "starboard/common/log.h"
#include "starboard/memory.h"

namespace starboard {
namespace elf_loader {
namespace {

// The size of the buffer that the file to be decompressed is read into, 16KiB.
// This value was explicitly chosen after testing various buffer sizes on a
// Raspberry Pi 2.
constexpr int kCompressedBufferSize = 16 * 1024;

// The rate of growth of the buffer used for decompression, 32KiB.
constexpr int kDecompressedGrowthSize = 32 * 1024;

}  // namespace

LZ4FileImpl::LZ4FileImpl() {
  const LZ4F_errorCode_t lz4f_error_code =
      LZ4F_createDecompressionContext(&lz4f_context_, LZ4F_VERSION);

  if (lz4f_error_code != 0) {
    SB_LOG(ERROR) << LZ4F_getErrorName(lz4f_error_code);
    lz4f_context_ = nullptr;
  }
}

LZ4FileImpl::~LZ4FileImpl() {
  if (!lz4f_context_) {
    return;
  }

  const LZ4F_errorCode_t lz4f_error_code =
      LZ4F_freeDecompressionContext(lz4f_context_);

  if (lz4f_error_code != 0) {
    SB_LOG(ERROR) << LZ4F_getErrorName(lz4f_error_code);
  }
}

bool LZ4FileImpl::Open(const char* name) {
  SB_DCHECK(name);

  if (!lz4f_context_) {
    return false;
  }

  SbFileInfo file_info;

  if (!FileImpl::Open(name) || !SbFileGetInfo(file_, &file_info)) {
    return false;
  }

  std::vector<char> compressed_data(kCompressedBufferSize);

  char* compressed_buffer = compressed_data.data();
  char* decompressed_buffer = nullptr;

  size_t compressed_size_remaining = file_info.size;
  size_t decompressed_size_current = 0;

  // Assume the content size will double when decompressed.
  decompressed_data_.resize(file_info.size * 2);

  while (true) {
    if (!FileImpl::ReadFromOffset(file_info.size - compressed_size_remaining,
                                  compressed_data.data(),
                                  kCompressedBufferSize)) {
      decompressed_data_.resize(0);
      return false;
    }

    size_t compressed_buffer_offset = 0;

    compressed_buffer = compressed_data.data();

    while (compressed_buffer_offset < kCompressedBufferSize) {
      // Ensure we always have a minimum of |kDecompressedGrowthSize| bytes
      // available.
      if (decompressed_data_.size() - decompressed_size_current <
          kDecompressedGrowthSize) {
        decompressed_data_.resize(decompressed_size_current +
                                  kDecompressedGrowthSize);
      }

      // We need to set |decompressed_buffer| each iteration since resizing
      // |decompressed_data_| invalidates the pointer.
      decompressed_buffer =
          decompressed_data_.data() + decompressed_size_current;

      size_t compressed =
          compressed_size_remaining < kCompressedBufferSize
              ? compressed_size_remaining
              : kCompressedBufferSize - compressed_buffer_offset;
      size_t decompressed =
          decompressed_data_.size() - decompressed_size_current;

      const size_t result =
          LZ4F_decompress(lz4f_context_, decompressed_buffer, &decompressed,
                          compressed_buffer, &compressed, nullptr);

      if (LZ4F_isError(result)) {
        SB_LOG(ERROR) << LZ4F_getErrorName(result);
        LZ4F_resetDecompressionContext(lz4f_context_);
        decompressed_data_.resize(0);
        return false;
      }

      compressed_size_remaining -= compressed;
      decompressed_size_current += decompressed;

      if (compressed_size_remaining <= 0) {
        decompressed_data_.resize(decompressed_size_current);
        return true;
      }

      compressed_buffer_offset += compressed;
      compressed_buffer += compressed;
    }
  }
  return false;
}

bool LZ4FileImpl::ReadFromOffset(int64_t offset, char* buffer, int size) {
  SB_DCHECK(lz4f_context_);

  if ((offset < 0) || (offset + size >= decompressed_data_.size())) {
    return false;
  }
  memcpy(buffer, decompressed_data_.data() + offset, size);
  return true;
}

}  // namespace elf_loader
}  // namespace starboard
