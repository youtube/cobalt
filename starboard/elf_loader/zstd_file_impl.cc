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

#include "starboard/elf_loader/zstd_file_impl.h"

#include <sys/stat.h>
#include <string.h>
#include <algorithm>
#include <vector>

#include "starboard/common/log.h"
#include "starboard/common/time.h"
#include "starboard/extension/loader_app_metrics.h"
#include "starboard/system.h"

namespace elf_loader {
using ::starboard::CurrentMonotonicTime;

ZstdFileImpl::ZstdFileImpl() {
  dctx_ = ZSTD_createDCtx();
  if (!dctx_) {
    SB_LOG(ERROR) << "Failed to create Zstd decompression context";
  }
}

ZstdFileImpl::~ZstdFileImpl() {
  if (dctx_) {
    ZSTD_freeDCtx(dctx_);
  }
}

bool ZstdFileImpl::Open(const char* name) {
  SB_DCHECK(name);

  if (!dctx_) {
    return false;
  }

  struct stat file_info;

  if (!FileImpl::Open(name) || fstat(file_, &file_info)) {
    return false;
  }

  int64_t decompression_start_time_us = CurrentMonotonicTime();

  bool result = Decompress(file_info.st_size);

  int64_t decompression_end_time_us = CurrentMonotonicTime();
  int64_t decompression_duration_us =
      decompression_end_time_us - decompression_start_time_us;
  SB_LOG(INFO) << "Zstd decompression took: " << decompression_duration_us / 1000
               << " ms";

  auto metrics_extension =
      static_cast<const StarboardExtensionLoaderAppMetricsApi*>(
          SbSystemGetExtension(kStarboardExtensionLoaderAppMetricsName));
  if (metrics_extension &&
      strcmp(metrics_extension->name,
             kStarboardExtensionLoaderAppMetricsName) == 0 &&
      metrics_extension->version >= 2) {
    metrics_extension->SetElfDecompressionDurationMicroseconds(
        decompression_duration_us);
  }

  return result;
}

bool ZstdFileImpl::Decompress(size_t file_size) {
  int64_t t_io_start = CurrentMonotonicTime();
  std::vector<char> compressed_data(file_size);
  if (!FileImpl::ReadFromOffset(0, compressed_data.data(), file_size)) {
    return false;
  }
  int64_t t_io_duration = CurrentMonotonicTime() - t_io_start;

  unsigned long long const content_size = ZSTD_getFrameContentSize(compressed_data.data(), file_size);
  if (content_size == ZSTD_CONTENTSIZE_ERROR || content_size == ZSTD_CONTENTSIZE_UNKNOWN) {
    return false;
  }

  int64_t t_alloc_start = CurrentMonotonicTime();
  decompressed_data_.resize(content_size);
  int64_t t_alloc_duration = CurrentMonotonicTime() - t_alloc_start;

  int64_t t_cpu_start = CurrentMonotonicTime();
  size_t const decompression_result = ZSTD_decompressDCtx(
      dctx_, decompressed_data_.data(), content_size, compressed_data.data(), file_size);
  int64_t t_cpu_duration = CurrentMonotonicTime() - t_cpu_start;

  if (ZSTD_isError(decompression_result)) {
    SB_LOG(ERROR) << "Zstd decompression error: " << ZSTD_getErrorName(decompression_result);
    decompressed_data_.clear();
    return false;
  }

  SB_LOG(INFO) << "Zstd One-Shot Profiling Breakdown:";
  SB_LOG(INFO) << "  - Allocation (Zeroing): " << t_alloc_duration / 1000 << " ms";
  SB_LOG(INFO) << "  - I/O (Disk Reads):     " << t_io_duration / 1000 << " ms";
  SB_LOG(INFO) << "  - CPU (Decompression): " << t_cpu_duration / 1000 << " ms";

  return true;
}

bool ZstdFileImpl::ReadFromOffset(int64_t offset, char* buffer, int size) {
  if (!dctx_) {
    return false;
  }

  if ((offset < 0) || (static_cast<size_t>(offset + size) > decompressed_data_.size())) {
    return false;
  }
  memcpy(buffer, decompressed_data_.data() + offset, size);
  return true;
}

}  // namespace elf_loader
