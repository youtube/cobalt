// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/file_stream_context.h"

#include "base/location.h"
#include "base/message_loop_proxy.h"
#include "base/task_runner_util.h"
#include "base/threading/thread_restrictions.h"
#include "base/threading/worker_pool.h"
#include "net/base/file_stream_net_log_parameters.h"
#include "net/base/net_errors.h"

namespace {

void CallInt64ToInt(const net::CompletionCallback& callback, int64 result) {
  callback.Run(static_cast<int>(result));
}

}

namespace net {

void FileStream::Context::Orphan() {
  DCHECK(!orphaned_);

  orphaned_ = true;
  if (file_ != base::kInvalidPlatformFileValue)
    bound_net_log_.EndEvent(NetLog::TYPE_FILE_STREAM_OPEN);

  if (!async_in_progress_) {
    CloseAndDelete();
  } else if (file_ != base::kInvalidPlatformFileValue) {
    CancelIo(file_);
  }
}

void FileStream::Context::OpenAsync(const FilePath& path,
                                    int open_flags,
                                    const CompletionCallback& callback) {
  DCHECK(!async_in_progress_);

  BeginOpenEvent(path);

  const bool posted = base::PostTaskAndReplyWithResult(
      base::WorkerPool::GetTaskRunner(true /* task_is_slow */),
      FROM_HERE,
      base::Bind(&Context::OpenFileImpl,
                 base::Unretained(this), path, open_flags),
      base::Bind(&Context::OnOpenCompleted,
                 base::Unretained(this), callback));
  DCHECK(posted);

  async_in_progress_ = true;
}

int FileStream::Context::OpenSync(const FilePath& path, int open_flags) {
  DCHECK(!async_in_progress_);

  BeginOpenEvent(path);
  OpenResult result = OpenFileImpl(path, open_flags);
  file_ = result.file;
  if (file_ == base::kInvalidPlatformFileValue) {
    result.error_code = ProcessOpenError(result.error_code);
  } else {
    // TODO(satorux): Remove this once all async clients are migrated to use
    // Open(). crbug.com/114783
    if (open_flags & base::PLATFORM_FILE_ASYNC)
      OnAsyncFileOpened();
  }
  return result.error_code;
}

void FileStream::Context::CloseSync() {
  DCHECK(!async_in_progress_);
  if (file_ != base::kInvalidPlatformFileValue) {
    base::ClosePlatformFile(file_);
    file_ = base::kInvalidPlatformFileValue;
    bound_net_log_.EndEvent(NetLog::TYPE_FILE_STREAM_OPEN);
  }
}

void FileStream::Context::SeekAsync(Whence whence,
                                    int64 offset,
                                    const Int64CompletionCallback& callback) {
  DCHECK(!async_in_progress_);

  const bool posted = base::PostTaskAndReplyWithResult(
      base::WorkerPool::GetTaskRunner(true /* task is slow */),
      FROM_HERE,
      base::Bind(&Context::SeekFileImpl,
                 base::Unretained(this), whence, offset),
      base::Bind(&Context::ProcessAsyncResult,
                 base::Unretained(this), callback, FILE_ERROR_SOURCE_SEEK));
  DCHECK(posted);

  async_in_progress_ = true;
}

int64 FileStream::Context::SeekSync(Whence whence, int64 offset) {
  int64 result = SeekFileImpl(whence, offset);
  CheckForIOError(&result, FILE_ERROR_SOURCE_SEEK);
  return result;
}

void FileStream::Context::FlushAsync(const CompletionCallback& callback) {
  DCHECK(!async_in_progress_);

  const bool posted = base::PostTaskAndReplyWithResult(
      base::WorkerPool::GetTaskRunner(true /* task is slow */),
      FROM_HERE,
      base::Bind(&Context::FlushFileImpl,
                 base::Unretained(this)),
      base::Bind(&Context::ProcessAsyncResult,
                 base::Unretained(this), IntToInt64(callback),
                 FILE_ERROR_SOURCE_FLUSH));
  DCHECK(posted);

  async_in_progress_ = true;
}

int FileStream::Context::FlushSync() {
  int64 result = FlushFileImpl();
  CheckForIOError(&result, FILE_ERROR_SOURCE_FLUSH);
  return result;
}

int FileStream::Context::RecordAndMapError(int error,
                                           FileErrorSource source) const {
  // The following check is against incorrect use or bug. File descriptor
  // shouldn't ever be closed outside of FileStream while it still tries to do
  // something with it.
  DCHECK(error != ERROR_BAD_FILE);
  Error net_error = MapSystemError(error);

  if (!orphaned_) {
    bound_net_log_.AddEvent(NetLog::TYPE_FILE_STREAM_ERROR,
                            base::Bind(&NetLogFileStreamErrorCallback,
                                       source, error, net_error));
  }
  RecordFileError(error, source, record_uma_);
  return net_error;
}

void FileStream::Context::BeginOpenEvent(const FilePath& path) {
  std::string file_name = path.AsUTF8Unsafe();
  bound_net_log_.BeginEvent(NetLog::TYPE_FILE_STREAM_OPEN,
                            NetLog::StringCallback("file_name", &file_name));
}

FileStream::Context::OpenResult FileStream::Context::OpenFileImpl(
    const FilePath& path, int open_flags) {
  // FileStream::Context actually closes the file asynchronously, independently
  // from FileStream's destructor. It can cause problems for users wanting to
  // delete the file right after FileStream deletion. Thus we are always
  // adding SHARE_DELETE flag to accommodate such use case.
  open_flags |= base::PLATFORM_FILE_SHARE_DELETE;
  OpenResult result;
  result.error_code = OK;
  result.file = base::CreatePlatformFile(path, open_flags, NULL, NULL);
  if (result.file == base::kInvalidPlatformFileValue)
    result.error_code = GetLastErrno();

  return result;
}

int FileStream::Context::ProcessOpenError(int error_code) {
  bound_net_log_.EndEvent(NetLog::TYPE_FILE_STREAM_OPEN);
  return RecordAndMapError(error_code, FILE_ERROR_SOURCE_OPEN);
}

void FileStream::Context::OnOpenCompleted(const CompletionCallback& callback,
                                          OpenResult result) {
  file_ = result.file;
  if (file_ == base::kInvalidPlatformFileValue)
    result.error_code = ProcessOpenError(result.error_code);
  else if (!orphaned_)
    OnAsyncFileOpened();
  OnAsyncCompleted(IntToInt64(callback), result.error_code);
}

void FileStream::Context::CloseAndDelete() {
  DCHECK(!async_in_progress_);

  if (file_ == base::kInvalidPlatformFileValue) {
    delete this;
  } else {
    const bool posted = base::WorkerPool::PostTaskAndReply(
        FROM_HERE,
        base::Bind(base::IgnoreResult(&base::ClosePlatformFile), file_),
        base::Bind(&Context::OnCloseCompleted, base::Unretained(this)),
        true /* task_is_slow */);
    DCHECK(posted);
    file_ = base::kInvalidPlatformFileValue;
  }
}

void FileStream::Context::OnCloseCompleted() {
  delete this;
}

Int64CompletionCallback FileStream::Context::IntToInt64(
    const CompletionCallback& callback) {
  return base::Bind(&CallInt64ToInt, callback);
}

void FileStream::Context::CheckForIOError(int64* result,
                                          FileErrorSource source) {
  if (*result < 0)
    *result = RecordAndMapError(static_cast<int>(*result), source);
}

void FileStream::Context::ProcessAsyncResult(
    const Int64CompletionCallback& callback,
    FileErrorSource source,
    int64 result) {
  CheckForIOError(&result, source);
  OnAsyncCompleted(callback, result);
}

void FileStream::Context::OnAsyncCompleted(
    const Int64CompletionCallback& callback,
    int64 result) {
  // Reset this before Run() as Run() may issue a new async operation. Also it
  // should be reset before CloseAsync() because it shouldn't run if any async
  // operation is in progress.
  async_in_progress_ = false;
  if (orphaned_)
    CloseAndDelete();
  else
    callback.Run(result);
}

}  // namespace net

