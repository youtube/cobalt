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

#include "starboard/elf_loader/chunked_zstd_file_impl.h"

#include <unistd.h>
#include <string.h>
#include <algorithm>
#include <vector>

#include "starboard/common/log.h"
#include "starboard/common/time.h"
#include "starboard/extension/loader_app_metrics.h"
#include "starboard/system.h"

namespace elf_loader {
using ::starboard::CurrentMonotonicTime;

ChunkedZstdFileImpl::ChunkedZstdFileImpl() {
  dctx_ = ZSTD_createDCtx();
}

ChunkedZstdFileImpl::~ChunkedZstdFileImpl() {
  if (dctx_) {
    ZSTD_freeDCtx(dctx_);
  }
}

bool ChunkedZstdFileImpl::Open(const char* name) {
  if (!dctx_) return false;
  if (!FileImpl::Open(name)) return false;

  int64_t start_time_us = CurrentMonotonicTime();
  bool result = Decompress();
  int64_t duration_us = CurrentMonotonicTime() - start_time_us;

  SB_LOG(INFO) << "Zstd chunked decompression took: " << duration_us / 1000 << " ms";

  auto metrics_extension =
      static_cast<const StarboardExtensionLoaderAppMetricsApi*>(
          SbSystemGetExtension(kStarboardExtensionLoaderAppMetricsName));
  if (metrics_extension && metrics_extension->version >= 2) {
    metrics_extension->SetElfDecompressionDurationMicroseconds(duration_us);
  }

  return result;
}

bool ChunkedZstdFileImpl::Decompress() {
  int64_t total_read_time_us = 0;
  int64_t total_zstd_time_us = 0;
  int64_t total_alloc_time_us = 0;

  // 1. Get content size from header.
  const size_t kHeaderSize = ZSTD_FRAMEHEADERSIZE_MAX;
  char header[kHeaderSize];
  int64_t t_read_start = CurrentMonotonicTime();
  if (read(file_, header, kHeaderSize) != static_cast<ssize_t>(kHeaderSize)) {
    return false;
  }
  total_read_time_us += (CurrentMonotonicTime() - t_read_start);

  unsigned long long const content_size = ZSTD_getFrameContentSize(header, kHeaderSize);
  if (content_size == ZSTD_CONTENTSIZE_ERROR || content_size == ZSTD_CONTENTSIZE_UNKNOWN) {
    SB_LOG(ERROR) << "Zstd: Invalid or unknown content size in header";
    return false;
  }

  // Measure allocation (includes zero-initialization by std::vector)
  int64_t t_alloc_start = CurrentMonotonicTime();
  decompressed_data_.resize(content_size);
  total_alloc_time_us = (CurrentMonotonicTime() - t_alloc_start);

  // 2. Loop and decompress chunks.
  lseek(file_, 0, SEEK_SET); // Reset to start
  
  const size_t kReadSize = 256 * 1024;
  std::vector<char> compressed_chunk(kReadSize);
  ZSTD_DCtx_reset(dctx_, ZSTD_reset_session_only);

  size_t total_produced = 0;
  while (total_produced < content_size) {
    int64_t t0 = CurrentMonotonicTime();
    ssize_t bytes_read = read(file_, compressed_chunk.data(), kReadSize);
    total_read_time_us += (CurrentMonotonicTime() - t0);

    if (bytes_read < 0) return false;
    if (bytes_read == 0) break;

    ZSTD_inBuffer input = {compressed_chunk.data(), static_cast<size_t>(bytes_read), 0};
    while (input.pos < input.size) {
      ZSTD_outBuffer output = {decompressed_data_.data(), decompressed_data_.size(), total_produced};
      
      int64_t t1 = CurrentMonotonicTime();
      size_t const ret = ZSTD_decompressStream(dctx_, &output, &input);
      total_zstd_time_us += (CurrentMonotonicTime() - t1);

      if (ZSTD_isError(ret)) {
        SB_LOG(ERROR) << "Zstd error: " << ZSTD_getErrorName(ret);
        return false;
      }
      total_produced = output.pos;
    }
  }

  SB_LOG(INFO) << "Zstd Chunked Profiling Breakdown:";
  SB_LOG(INFO) << "  - Allocation (Zeroing): " << total_alloc_time_us / 1000 << " ms";
  SB_LOG(INFO) << "  - I/O (Disk Reads):     " << total_read_time_us / 1000 << " ms";
  SB_LOG(INFO) << "  - CPU (Decompression): " << total_zstd_time_us / 1000 << " ms";

  return total_produced == content_size;
}

bool ChunkedZstdFileImpl::ReadFromOffset(int64_t offset, char* buffer, int size) {
  if (offset < 0 || static_cast<size_t>(offset + size) > decompressed_data_.size()) {
    return false;
  }
  memcpy(buffer, decompressed_data_.data() + offset, size);
  return true;
}

}  // namespace elf_loader
