// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// For 64-bit file access (off_t = off64_t, lseek64, etc).
#define _FILE_OFFSET_BITS 64

#include "net/base/file_stream_context.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/file_path.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/posix/eintr_wrapper.h"
#include "base/task_runner_util.h"
#include "base/threading/worker_pool.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"

#if defined(OS_ANDROID)
// Android's bionic libc only supports the LFS transitional API.
#define off_t off64_t
#define lseek lseek64
#define stat stat64
#define fstat fstat64
#endif

namespace net {

// We cast back and forth, so make sure it's the size we're expecting.
COMPILE_ASSERT(sizeof(int64) == sizeof(off_t), off_t_64_bit);

// Make sure our Whence mappings match the system headers.
COMPILE_ASSERT(FROM_BEGIN   == SEEK_SET &&
               FROM_CURRENT == SEEK_CUR &&
               FROM_END     == SEEK_END, whence_matches_system);

FileStream::Context::Context(const BoundNetLog& bound_net_log)
    : file_(base::kInvalidPlatformFileValue),
      record_uma_(false),
      async_in_progress_(false),
      orphaned_(false),
      bound_net_log_(bound_net_log) {
}

FileStream::Context::Context(base::PlatformFile file,
                             const BoundNetLog& bound_net_log,
                             int /* open_flags */)
    : file_(file),
      record_uma_(false),
      async_in_progress_(false),
      orphaned_(false),
      bound_net_log_(bound_net_log) {
}

FileStream::Context::~Context() {
}

int64 FileStream::Context::GetFileSize() const {
  struct stat info;
  if (fstat(file_, &info) != 0)
    return RecordAndMapError(errno, FILE_ERROR_SOURCE_GET_SIZE);

  return static_cast<int64>(info.st_size);
}

int FileStream::Context::ReadAsync(IOBuffer* in_buf,
                                   int buf_len,
                                   const CompletionCallback& callback) {
  DCHECK(!async_in_progress_);

  scoped_refptr<IOBuffer> buf = in_buf;
  const bool posted = base::PostTaskAndReplyWithResult(
      base::WorkerPool::GetTaskRunner(true /* task is slow */),
      FROM_HERE,
      base::Bind(&Context::ReadFileImpl,
                 base::Unretained(this), buf, buf_len),
      base::Bind(&Context::ProcessAsyncResult,
                 base::Unretained(this), IntToInt64(callback),
                 FILE_ERROR_SOURCE_READ));
  DCHECK(posted);

  async_in_progress_ = true;
  return ERR_IO_PENDING;
}

int FileStream::Context::ReadSync(char* in_buf, int buf_len) {
  scoped_refptr<IOBuffer> buf = new WrappedIOBuffer(in_buf);
  int64 result = ReadFileImpl(buf, buf_len);
  CheckForIOError(&result, FILE_ERROR_SOURCE_READ);
  return result;
}

int FileStream::Context::WriteAsync(IOBuffer* in_buf,
                                    int buf_len,
                                    const CompletionCallback& callback) {
  DCHECK(!async_in_progress_);

  scoped_refptr<IOBuffer> buf = in_buf;
  const bool posted = base::PostTaskAndReplyWithResult(
      base::WorkerPool::GetTaskRunner(true /* task is slow */),
      FROM_HERE,
      base::Bind(&Context::WriteFileImpl,
                 base::Unretained(this), buf, buf_len),
      base::Bind(&Context::ProcessAsyncResult,
                 base::Unretained(this), IntToInt64(callback),
                 FILE_ERROR_SOURCE_WRITE));
  DCHECK(posted);

  async_in_progress_ = true;
  return ERR_IO_PENDING;
}

int FileStream::Context::WriteSync(const char* in_buf, int buf_len) {
  scoped_refptr<IOBuffer> buf = new WrappedIOBuffer(in_buf);
  int64 result = WriteFileImpl(buf, buf_len);
  CheckForIOError(&result, FILE_ERROR_SOURCE_WRITE);
  return result;
}

int FileStream::Context::Truncate(int64 bytes) {
  int result = ftruncate(file_, bytes);
  if (result == 0)
    return bytes;

  return RecordAndMapError(errno, FILE_ERROR_SOURCE_SET_EOF);
}

int64 FileStream::Context::SeekFileImpl(Whence whence, int64 offset) {
  off_t res = lseek(file_, static_cast<off_t>(offset),
                    static_cast<int>(whence));
  if (res == static_cast<off_t>(-1))
    return errno;

  return res;
}

int64 FileStream::Context::FlushFileImpl() {
  ssize_t res = HANDLE_EINTR(fsync(file_));
  if (res == -1)
    return errno;

  return res;
}

int64 FileStream::Context::ReadFileImpl(scoped_refptr<IOBuffer> buf,
                                        int buf_len) {
  // Loop in the case of getting interrupted by a signal.
  ssize_t res = HANDLE_EINTR(read(file_, buf->data(),
                                  static_cast<size_t>(buf_len)));
  if (res == -1)
    return errno;

  return res;
}

int64 FileStream::Context::WriteFileImpl(scoped_refptr<IOBuffer> buf,
                                         int buf_len) {
  ssize_t res = HANDLE_EINTR(write(file_, buf->data(), buf_len));
  if (res == -1)
    return errno;

  return res;
}

}  // namespace net
