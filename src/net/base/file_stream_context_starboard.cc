// Copyright 2015 Google Inc. All Rights Reserved.
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

#include "net/base/file_stream_context.h"

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

namespace net {

FileStream::Context::Context(const BoundNetLog& bound_net_log)
    : file_(base::kInvalidPlatformFileValue),
      record_uma_(false),
      async_in_progress_(false),
      orphaned_(false),
      bound_net_log_(bound_net_log) {}

FileStream::Context::Context(base::PlatformFile file,
                             const BoundNetLog& bound_net_log,
                             int /* open_flags */)
    : file_(file),
      record_uma_(false),
      async_in_progress_(false),
      orphaned_(false),
      bound_net_log_(bound_net_log) {}

FileStream::Context::~Context() {}

int64 FileStream::Context::GetFileSize() const {
  base::PlatformFileInfo info;
  if (!base::GetPlatformFileInfo(file_, &info)) {
    return RecordAndMapError(GetLastErrno(), FILE_ERROR_SOURCE_GET_SIZE);
  }

  return info.size;
}

int FileStream::Context::ReadAsync(IOBuffer* in_buf,
                                   int buf_len,
                                   const CompletionCallback& callback) {
  DCHECK(!async_in_progress_);

  scoped_refptr<IOBuffer> buf = in_buf;
  const bool posted = base::PostTaskAndReplyWithResult(
      base::WorkerPool::GetTaskRunner(true /* task is slow */), FROM_HERE,
      base::Bind(&Context::ReadFileImpl, base::Unretained(this), buf, buf_len),
      base::Bind(&Context::ProcessAsyncResult, base::Unretained(this),
                 IntToInt64(callback), FILE_ERROR_SOURCE_READ));
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
      base::WorkerPool::GetTaskRunner(true /* task is slow */), FROM_HERE,
      base::Bind(&Context::WriteFileImpl, base::Unretained(this), buf, buf_len),
      base::Bind(&Context::ProcessAsyncResult, base::Unretained(this),
                 IntToInt64(callback), FILE_ERROR_SOURCE_WRITE));
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
  if (base::TruncatePlatformFile(file_, bytes))
    return bytes;

  return RecordAndMapError(GetLastErrno(), FILE_ERROR_SOURCE_SET_EOF);
}

int64 FileStream::Context::SeekFileImpl(Whence whence, int64 offset) {
  int64 res = base::SeekPlatformFile(
      file_, static_cast<base::PlatformFileWhence>(whence), offset);
  if (res == -1)
    return GetLastErrno();

  return res;
}

int64 FileStream::Context::FlushFileImpl() {
  if (base::FlushPlatformFile(file_)) {
    return 0;
  }

  return GetLastErrno();
}

int64 FileStream::Context::ReadFileImpl(scoped_refptr<IOBuffer> buf,
                                        int buf_len) {
  // Loop in the case of getting interrupted by a signal.
  int64 res = base::ReadPlatformFileAtCurrentPos(file_, buf->data(), buf_len);
  if (res == -1)
    return GetLastErrno();

  return res;
}

int64 FileStream::Context::WriteFileImpl(scoped_refptr<IOBuffer> buf,
                                         int buf_len) {
  int64 res = base::WritePlatformFileAtCurrentPos(file_, buf->data(), buf_len);
  if (res == -1)
    return GetLastErrno();

  return res;
}

}  // namespace net
