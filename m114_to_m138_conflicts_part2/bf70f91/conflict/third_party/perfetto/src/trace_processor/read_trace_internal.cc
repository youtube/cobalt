/*
 * Copyright (C) 2019 The Android Open Source Project
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

#include "src/trace_processor/read_trace_internal.h"

#include "perfetto/base/logging.h"
#include "perfetto/ext/base/file_utils.h"
#include "perfetto/ext/base/scoped_file.h"
<<<<<<< HEAD
#include "perfetto/ext/base/scoped_mmap.h"
=======
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
#include "perfetto/ext/base/utils.h"
#include "perfetto/protozero/proto_utils.h"
#include "perfetto/trace_processor/trace_processor.h"

#include "perfetto/trace_processor/trace_blob.h"
#include "perfetto/trace_processor/trace_blob_view.h"
#include "src/trace_processor/forwarding_trace_parser.h"
<<<<<<< HEAD
=======
#include "src/trace_processor/importers/gzip/gzip_trace_parser.h"
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
#include "src/trace_processor/importers/proto/proto_trace_tokenizer.h"
#include "src/trace_processor/util/gzip_utils.h"
#include "src/trace_processor/util/status_macros.h"

#include "protos/perfetto/trace/trace.pbzero.h"
#include "protos/perfetto/trace/trace_packet.pbzero.h"

<<<<<<< HEAD
=======
#if TRACE_PROCESSOR_HAS_MMAP()
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>
#endif
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
namespace perfetto {
namespace trace_processor {
namespace {

// 1MB chunk size seems the best tradeoff on a MacBook Pro 2013 - i7 2.8 GHz.
constexpr size_t kChunkSize = 1024 * 1024;

<<<<<<< HEAD
base::Status ReadTraceUsingRead(
=======
util::Status ReadTraceUsingRead(
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
    TraceProcessor* tp,
    int fd,
    uint64_t* file_size,
    const std::function<void(uint64_t parsed_size)>& progress_callback) {
  // Load the trace in chunks using ordinary read().
  for (int i = 0;; i++) {
    if (progress_callback && i % 128 == 0)
      progress_callback(*file_size);

    TraceBlob blob = TraceBlob::Allocate(kChunkSize);
    auto rsize = base::Read(fd, blob.data(), blob.size());
    if (rsize == 0)
      break;

    if (rsize < 0) {
<<<<<<< HEAD
      return base::ErrStatus("Reading trace file failed (errno: %d, %s)", errno,
=======
      return util::ErrStatus("Reading trace file failed (errno: %d, %s)", errno,
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
                             strerror(errno));
    }

    *file_size += static_cast<uint64_t>(rsize);
    TraceBlobView blob_view(std::move(blob), 0, static_cast<size_t>(rsize));
    RETURN_IF_ERROR(tp->Parse(std::move(blob_view)));
  }
<<<<<<< HEAD
  return base::OkStatus();
}
}  // namespace

base::Status ReadTraceUnfinalized(
    TraceProcessor* tp,
    const char* filename,
    const std::function<void(uint64_t parsed_size)>& progress_callback) {
  uint64_t bytes_read = 0;

#if PERFETTO_HAS_MMAP()
  char* no_mmap = getenv("TRACE_PROCESSOR_NO_MMAP");
  bool use_mmap = !no_mmap || *no_mmap != '1';

  if (use_mmap) {
    base::ScopedMmap mapped = base::ReadMmapWholeFile(filename);
    if (mapped.IsValid()) {
      size_t length = mapped.length();
      TraceBlobView whole_mmap(TraceBlob::FromMmap(std::move(mapped)));
      // Parse the file in chunks so we get some status update on stdio.
      static constexpr size_t kMmapChunkSize = 128ul * 1024 * 1024;
      while (bytes_read < length) {
        progress_callback(bytes_read);
        const size_t bytes_read_z = static_cast<size_t>(bytes_read);
        size_t slice_size = std::min(length - bytes_read_z, kMmapChunkSize);
=======
  return util::OkStatus();
}
}  // namespace

util::Status ReadTraceUnfinalized(
    TraceProcessor* tp,
    const char* filename,
    const std::function<void(uint64_t parsed_size)>& progress_callback) {
  base::ScopedFile fd(base::OpenFile(filename, O_RDONLY));
  if (!fd)
    return util::ErrStatus("Could not open trace file (path: %s)", filename);

  uint64_t bytes_read = 0;

#if TRACE_PROCESSOR_HAS_MMAP()
  char* no_mmap = getenv("TRACE_PROCESSOR_NO_MMAP");
  uint64_t whole_size_64 = static_cast<uint64_t>(lseek(*fd, 0, SEEK_END));
  lseek(*fd, 0, SEEK_SET);
  bool use_mmap = !no_mmap || *no_mmap != '1';
  if (sizeof(size_t) < 8 && whole_size_64 > 2147483648ULL)
    use_mmap = false;  // Cannot use mmap on 32-bit systems for files > 2GB.

  if (use_mmap) {
    const size_t whole_size = static_cast<size_t>(whole_size_64);
    void* file_mm = mmap(nullptr, whole_size, PROT_READ, MAP_PRIVATE, *fd, 0);
    if (file_mm != MAP_FAILED) {
      TraceBlobView whole_mmap(TraceBlob::FromMmap(file_mm, whole_size));
      // Parse the file in chunks so we get some status update on stdio.
      static constexpr size_t kMmapChunkSize = 128ul * 1024 * 1024;
      while (bytes_read < whole_size_64) {
        progress_callback(bytes_read);
        const size_t bytes_read_z = static_cast<size_t>(bytes_read);
        size_t slice_size = std::min(whole_size - bytes_read_z, kMmapChunkSize);
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
        TraceBlobView slice = whole_mmap.slice_off(bytes_read_z, slice_size);
        RETURN_IF_ERROR(tp->Parse(std::move(slice)));
        bytes_read += slice_size;
      }  // while (slices)
<<<<<<< HEAD
    }  // if (mapped.IsValid())
  }  // if (use_mmap)
  if (bytes_read == 0)
    PERFETTO_LOG("Cannot use mmap on this system. Falling back on read()");
#endif  // PERFETTO_HAS_MMAP()
  if (bytes_read == 0) {
    base::ScopedFile fd(base::OpenFile(filename, O_RDONLY));
    if (!fd)
      return base::ErrStatus("Could not open trace file (path: %s)", filename);
=======
    }    // if (!MAP_FAILED)
  }      // if (use_mmap)
  if (bytes_read == 0)
    PERFETTO_LOG("Cannot use mmap on this system. Falling back on read()");
#endif  // TRACE_PROCESSOR_HAS_MMAP()
  if (bytes_read == 0) {
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
    RETURN_IF_ERROR(
        ReadTraceUsingRead(tp, *fd, &bytes_read, progress_callback));
  }
  tp->SetCurrentTraceName(filename);

  if (progress_callback)
    progress_callback(bytes_read);
<<<<<<< HEAD
  return base::OkStatus();
=======
  return util::OkStatus();
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
}
}  // namespace trace_processor
}  // namespace perfetto
