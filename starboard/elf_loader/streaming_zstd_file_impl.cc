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

#include "starboard/elf_loader/streaming_zstd_file_impl.h"

#include <unistd.h>
#include <string.h>
#include <algorithm>

#include "starboard/common/log.h"
#include "starboard/common/time.h"

namespace elf_loader {
using ::starboard::CurrentMonotonicTime;

namespace {
const size_t kZstdBufferSize = 128 * 1024;
}

StreamingZstdFileImpl::StreamingZstdFileImpl()
    : dctx_(ZSTD_createDCtx()),
      current_uncompressed_offset_(0),
      decompressed_buf_pos_(0),
      decompressed_buf_size_(0),
      total_read_time_us_(0),
      total_zstd_time_us_(0),
      rewind_count_(0) {
  compressed_buf_.resize(kZstdBufferSize);
  decompressed_buf_.resize(kZstdBufferSize);
}

StreamingZstdFileImpl::~StreamingZstdFileImpl() {
  SB_LOG(INFO) << "Streaming Zstd Profiling Summary:";
  SB_LOG(INFO) << "  - Total I/O time:      " << total_read_time_us_ / 1000 << " ms";
  SB_LOG(INFO) << "  - Total Zstd CPU time: " << total_zstd_time_us_ / 1000 << " ms";
  SB_LOG(INFO) << "  - Total Rewinds:       " << rewind_count_;

  if (dctx_) {
    ZSTD_freeDCtx(dctx_);
  }
}

bool StreamingZstdFileImpl::Open(const char* name) {
  if (!dctx_) {
    return false;
  }
  if (!FileImpl::Open(name)) {
    return false;
  }
  return ResetStream();
}

bool StreamingZstdFileImpl::ResetStream() {
  ZSTD_DCtx_reset(dctx_, ZSTD_reset_session_only);
  current_uncompressed_offset_ = 0;
  decompressed_buf_pos_ = 0;
  decompressed_buf_size_ = 0;
  if (lseek(file_, 0, SEEK_SET) == -1) {
    SB_LOG(ERROR) << "Failed to seek physical file to 0";
    return false;
  }
  return true;
}

bool StreamingZstdFileImpl::ReadFromOffset(int64_t offset, char* buffer, int size) {
  if (offset < current_uncompressed_offset_) {
    rewind_count_++;
    SB_DLOG(INFO) << "Rewinding Zstd stream from " << current_uncompressed_offset_ << " to " << offset;
    if (!ResetStream()) {
      return false;
    }
  }

  // Skip forward if necessary
  if (offset > current_uncompressed_offset_) {
    if (!DecompressToOffset(offset)) {
      return false;
    }
  }

  int bytes_read = 0;
  while (bytes_read < size) {
    if (decompressed_buf_pos_ < decompressed_buf_size_) {
      size_t to_copy = std::min(static_cast<size_t>(size - bytes_read),
                                decompressed_buf_size_ - decompressed_buf_pos_);
      memcpy(buffer + bytes_read, decompressed_buf_.data() + decompressed_buf_pos_, to_copy);
      bytes_read += to_copy;
      decompressed_buf_pos_ += to_copy;
      current_uncompressed_offset_ += to_copy;
    } else {
      // Buffer is empty, decompress more data.
      if (!DecompressToOffset(current_uncompressed_offset_)) {
         return false;
      }
      // If after decompressing we still have no data, we hit EOF.
      if (decompressed_buf_pos_ == decompressed_buf_size_) {
        return bytes_read > 0;
      }
    }
  }

  return true;
}

bool StreamingZstdFileImpl::DecompressToOffset(int64_t target_offset) {
  while (current_uncompressed_offset_ < target_offset || decompressed_buf_pos_ >= decompressed_buf_size_) {
    // If we have data in the buffer, either consume it or use it to satisfy target_offset
    if (decompressed_buf_pos_ < decompressed_buf_size_) {
      size_t can_consume = std::min(static_cast<size_t>(target_offset - current_uncompressed_offset_),
                                    decompressed_buf_size_ - decompressed_buf_pos_);
      decompressed_buf_pos_ += can_consume;
      current_uncompressed_offset_ += can_consume;
      
      // If we've reached the target offset and still have data (or target was current), we're done.
      if (current_uncompressed_offset_ == target_offset && decompressed_buf_pos_ < decompressed_buf_size_) {
        return true;
      }
      
      // If we consumed the whole buffer but haven't reached target, continue to decompress.
      if (decompressed_buf_pos_ < decompressed_buf_size_) {
         // We reached target_offset and still have data in buffer.
         return true;
      }
    }

    // Buffer is empty (or fully consumed), decompress more.
    decompressed_buf_pos_ = 0;
    decompressed_buf_size_ = 0;

    ZSTD_outBuffer output = {decompressed_buf_.data(), decompressed_buf_.size(), 0};

    // Keep reading and decompressing until we produce at least some output or hit EOF.
    bool produced_output = false;
    while (!produced_output) {
      int64_t t0 = CurrentMonotonicTime();
      ssize_t bytes_from_file = read(file_, compressed_buf_.data(), compressed_buf_.size());
      total_read_time_us_ += (CurrentMonotonicTime() - t0);

      if (bytes_from_file < 0) {
        SB_LOG(ERROR) << "Read error from physical file";
        return false;
      }
      if (bytes_from_file == 0) {
        return current_uncompressed_offset_ >= target_offset; // EOF
      }

      ZSTD_inBuffer input = {compressed_buf_.data(), static_cast<size_t>(bytes_from_file), 0};

      while (input.pos < input.size) {
        int64_t t1 = CurrentMonotonicTime();
        size_t const ret = ZSTD_decompressStream(dctx_, &output, &input);
        total_zstd_time_us_ += (CurrentMonotonicTime() - t1);

        if (ZSTD_isError(ret)) {
          SB_LOG(ERROR) << "Zstd streaming error: " << ZSTD_getErrorName(ret);
          return false;
        }
        if (output.pos > 0) {
          produced_output = true;
          break;
        }
      }

      // Push back unconsumed input bytes so next read starts correctly.
      if (input.pos < input.size) {
        lseek(file_, -(static_cast<off_t>(input.size - input.pos)), SEEK_CUR);
      }
    }
    decompressed_buf_size_ = output.pos;
  }
  return true;
}

}  // namespace elf_loader
