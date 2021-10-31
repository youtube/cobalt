// Copyright 2018 Google Inc. All Rights Reserved.
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

#include <errno.h>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "base/task_runner.h"
#include "base/task_runner_util.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "starboard/types.h"

#if defined(STARBOARD)
#include "starboard/system.h"
#define net_err SbSystemGetLastError()
#else
#define net_err errno
#endif

namespace net {

FileStream::Context::Context(const scoped_refptr<base::TaskRunner>& task_runner)
    : async_in_progress_(false),
      orphaned_(false),
      task_runner_(task_runner) {
}

FileStream::Context::Context(base::File file,
                             const scoped_refptr<base::TaskRunner>& task_runner)
    : file_(std::move(file)),
      async_in_progress_(false),
      orphaned_(false),
      task_runner_(task_runner) {}

FileStream::Context::~Context() = default;

int FileStream::Context::Read(IOBuffer* in_buf,
                              int buf_len,
                              CompletionOnceCallback callback) {
  DCHECK(!async_in_progress_);

  scoped_refptr<IOBuffer> buf = in_buf;
  const bool posted = base::PostTaskAndReplyWithResult(
      task_runner_.get(), FROM_HERE,
      base::BindOnce(&Context::ReadFileImpl, base::Unretained(this), buf,
                     buf_len),
      base::BindOnce(&Context::OnAsyncCompleted, base::Unretained(this),
                     IntToInt64(std::move(callback))));
  DCHECK(posted);

  async_in_progress_ = true;
  return ERR_IO_PENDING;
}

int FileStream::Context::Write(IOBuffer* in_buf,
                               int buf_len,
                               CompletionOnceCallback callback) {
  DCHECK(!async_in_progress_);

  scoped_refptr<IOBuffer> buf = in_buf;
  const bool posted = base::PostTaskAndReplyWithResult(
      task_runner_.get(), FROM_HERE,
      base::BindOnce(&Context::WriteFileImpl, base::Unretained(this), buf,
                     buf_len),
      base::BindOnce(&Context::OnAsyncCompleted, base::Unretained(this),
                     IntToInt64(std::move(callback))));
  DCHECK(posted);

  async_in_progress_ = true;
  return ERR_IO_PENDING;
}

FileStream::Context::IOResult FileStream::Context::SeekFileImpl(
    int64_t offset) {
  int64_t res = file_.Seek(base::File::FROM_BEGIN, offset);
  if (res == -1)
    return IOResult::FromOSError(net_err);

  return IOResult(res, 0);
}

void FileStream::Context::OnFileOpened() {
}

FileStream::Context::IOResult FileStream::Context::ReadFileImpl(
    scoped_refptr<IOBuffer> buf,
    int buf_len) {
  int res = file_.ReadAtCurrentPosNoBestEffort(buf->data(), buf_len);
  if (res == -1)
    return IOResult::FromOSError(net_err);

  return IOResult(res, 0);
}

FileStream::Context::IOResult FileStream::Context::WriteFileImpl(
    scoped_refptr<IOBuffer> buf,
    int buf_len) {
  int res = file_.WriteAtCurrentPosNoBestEffort(buf->data(), buf_len);
  if (res == -1)
    return IOResult::FromOSError(net_err);

  return IOResult(res, 0);
}

}  // namespace net
