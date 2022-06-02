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

#include <algorithm>
#include <vector>

#include "starboard/elf_loader/lz4_file_impl.h"

#include "starboard/common/log.h"
#include "starboard/memory.h"

namespace starboard {
namespace elf_loader {

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

static size_t GetBlockSize(const LZ4F_frameInfo_t* frame_info) {
  switch (frame_info->blockSizeID) {
    case LZ4F_default:
    case LZ4F_max64KB:
      return 64 * (1 << 10);
    case LZ4F_max256KB:
      return 256 * (1 << 10);
    case LZ4F_max1MB:
      return 1 * (1 << 20);
    case LZ4F_max4MB:
      return 4 * (1 << 20);
    default:
      SB_LOG(INFO) << "Got an unknown block size; continuing with 256KB";
      return 256 * (1 << 10);
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

  size_t header_size = PeekHeaderSize();
  if (LZ4F_isError(header_size)) {
    SB_LOG(ERROR) << LZ4F_getErrorName(header_size);
    return false;
  }

  LZ4F_frameInfo_t frame_info = LZ4F_INIT_FRAMEINFO;
  size_t source_bytes_hint = ConsumeHeader(&frame_info, header_size);
  if (LZ4F_isError(source_bytes_hint)) {
    SB_LOG(ERROR) << LZ4F_getErrorName(source_bytes_hint);
    LZ4F_resetDecompressionContext(lz4f_context_);
    return false;
  }

  // We require the uncompressed data size to be set in the LZ4 frame header so
  // that we can be aggressive with memory allocation during decompression.
  uint64_t content_size = frame_info.contentSize;
  if (content_size <= 0) {
    SB_LOG(ERROR) << "Content size must be present in the LZ4 frame header";
    return false;
  }
  decompressed_data_.resize(content_size);

  // LZ4F_decompress() expects (but does not require) to decode a specific
  // number of source bytes: the size of the current compressed block + the
  // header of the next block. We can meet this expectation often, without
  // allocating much extra space, by using a buffer of size equal to the
  // uncompressed block size.
  int max_compressed_buffer_size = GetBlockSize(&frame_info);

  return Decompress(file_info.size, header_size, max_compressed_buffer_size,
                    source_bytes_hint);
}

size_t LZ4FileImpl::PeekHeaderSize() {
  std::vector<char> source_buffer(LZ4F_MIN_SIZE_TO_KNOW_HEADER_LENGTH);
  FileImpl::ReadFromOffset(0, source_buffer.data(),
                           LZ4F_MIN_SIZE_TO_KNOW_HEADER_LENGTH);
  return LZ4F_headerSize(source_buffer.data(),
                         LZ4F_MIN_SIZE_TO_KNOW_HEADER_LENGTH);
}

size_t LZ4FileImpl::ConsumeHeader(LZ4F_frameInfo_t* frame_info,
                                  size_t header_size) {
  std::vector<char> source_buffer(header_size);
  FileImpl::ReadFromOffset(0, source_buffer.data(), header_size);
  return LZ4F_getFrameInfo(lz4f_context_, frame_info, source_buffer.data(),
                           &header_size);
}

bool LZ4FileImpl::Decompress(size_t file_size,
                             size_t header_size,
                             size_t max_compressed_buffer_size,
                             size_t source_bytes_hint) {
  std::vector<char> compressed_data(max_compressed_buffer_size);

  char* compressed_buffer = compressed_data.data();
  char* decompressed_buffer = decompressed_data_.data();

  size_t compressed_size_remaining = file_size - header_size;
  size_t decompressed_size_current = 0;

  while (source_bytes_hint != 0) {
    size_t compressed_buffer_size =
        std::min(source_bytes_hint, max_compressed_buffer_size);
    if (!FileImpl::ReadFromOffset(file_size - compressed_size_remaining,
                                  compressed_data.data(),
                                  compressed_buffer_size)) {
      decompressed_data_.resize(0);
      return false;
    }

    size_t compressed_buffer_offset = 0;

    compressed_buffer = compressed_data.data();

    while (source_bytes_hint != 0 &&
           compressed_buffer_offset < compressed_buffer_size) {
      size_t compressed = compressed_buffer_size - compressed_buffer_offset;
      size_t decompressed =
          decompressed_data_.size() - decompressed_size_current;

      source_bytes_hint =
          LZ4F_decompress(lz4f_context_, decompressed_buffer, &decompressed,
                          compressed_buffer, &compressed, nullptr);

      if (LZ4F_isError(source_bytes_hint)) {
        SB_LOG(ERROR) << LZ4F_getErrorName(source_bytes_hint);
        LZ4F_resetDecompressionContext(lz4f_context_);
        decompressed_data_.resize(0);
        return false;
      }

      compressed_size_remaining -= compressed;
      decompressed_size_current += decompressed;

      compressed_buffer_offset += compressed;
      compressed_buffer += compressed;

      decompressed_buffer += decompressed;
    }
  }
  return true;
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
