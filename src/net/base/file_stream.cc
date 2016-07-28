// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/file_stream.h"

#include "base/location.h"
#include "base/message_loop_proxy.h"
#include "base/task_runner_util.h"
#include "base/threading/thread_restrictions.h"
#include "base/threading/worker_pool.h"
#include "net/base/file_stream_context.h"
#include "net/base/file_stream_net_log_parameters.h"
#include "net/base/net_errors.h"

namespace net {

FileStream::FileStream(NetLog* net_log)
      /* To allow never opened stream to be destroyed on any thread we set flags
         as if stream was opened asynchronously. */
    : open_flags_(base::PLATFORM_FILE_ASYNC),
      bound_net_log_(BoundNetLog::Make(net_log, NetLog::SOURCE_FILESTREAM)),
      context_(new Context(bound_net_log_)) {
  bound_net_log_.BeginEvent(NetLog::TYPE_FILE_STREAM_ALIVE);
}

FileStream::FileStream(base::PlatformFile file, int flags, NetLog* net_log)
    : open_flags_(flags),
      bound_net_log_(BoundNetLog::Make(net_log, NetLog::SOURCE_FILESTREAM)),
      context_(new Context(file, bound_net_log_, open_flags_)) {
  bound_net_log_.BeginEvent(NetLog::TYPE_FILE_STREAM_ALIVE);
}

FileStream::~FileStream() {
  if (!is_async()) {
    base::ThreadRestrictions::AssertIOAllowed();
    context_->CloseSync();
    context_.reset();
  } else {
    context_.release()->Orphan();
  }

  bound_net_log_.EndEvent(NetLog::TYPE_FILE_STREAM_ALIVE);
}

int FileStream::Open(const FilePath& path, int open_flags,
                     const CompletionCallback& callback) {
  if (IsOpen()) {
    DLOG(FATAL) << "File is already open!";
    return ERR_UNEXPECTED;
  }

  open_flags_ = open_flags;
  DCHECK(is_async());
  context_->OpenAsync(path, open_flags, callback);
  return ERR_IO_PENDING;
}

int FileStream::OpenSync(const FilePath& path, int open_flags) {
  base::ThreadRestrictions::AssertIOAllowed();

  if (IsOpen()) {
    DLOG(FATAL) << "File is already open!";
    return ERR_UNEXPECTED;
  }

  open_flags_ = open_flags;
  // TODO(satorux): Put a DCHECK once all async clients are migrated
  // to use Open(). crbug.com/114783
  //
  // DCHECK(!is_async());
  return context_->OpenSync(path, open_flags_);
}

bool FileStream::IsOpen() const {
  return context_->file() != base::kInvalidPlatformFileValue;
}

int FileStream::Seek(Whence whence,
                     int64 offset,
                     const Int64CompletionCallback& callback) {
  if (!IsOpen())
    return ERR_UNEXPECTED;

  // Make sure we're async.
  DCHECK(is_async());
  context_->SeekAsync(whence, offset, callback);
  return ERR_IO_PENDING;
}

int64 FileStream::SeekSync(Whence whence, int64 offset) {
  base::ThreadRestrictions::AssertIOAllowed();

  if (!IsOpen())
    return ERR_UNEXPECTED;

  // If we're in async, make sure we don't have a request in flight.
  DCHECK(!is_async() || !context_->async_in_progress());
  return context_->SeekSync(whence, offset);
}

int64 FileStream::Available() {
  base::ThreadRestrictions::AssertIOAllowed();

  if (!IsOpen())
    return ERR_UNEXPECTED;

  int64 cur_pos = SeekSync(FROM_CURRENT, 0);
  if (cur_pos < 0)
    return cur_pos;

  int64 size = context_->GetFileSize();
  if (size < 0)
    return size;

  DCHECK_GE(size, cur_pos);
  return size - cur_pos;
}

int FileStream::Read(IOBuffer* buf,
                     int buf_len,
                     const CompletionCallback& callback) {
  if (!IsOpen())
    return ERR_UNEXPECTED;

  // read(..., 0) will return 0, which indicates end-of-file.
  DCHECK_GT(buf_len, 0);
  DCHECK(open_flags_ & base::PLATFORM_FILE_READ);
  DCHECK(is_async());

  return context_->ReadAsync(buf, buf_len, callback);
}

int FileStream::ReadSync(char* buf, int buf_len) {
  base::ThreadRestrictions::AssertIOAllowed();

  if (!IsOpen())
    return ERR_UNEXPECTED;

  DCHECK(!is_async());
  // read(..., 0) will return 0, which indicates end-of-file.
  DCHECK_GT(buf_len, 0);
  DCHECK(open_flags_ & base::PLATFORM_FILE_READ);

  return context_->ReadSync(buf, buf_len);
}

int FileStream::ReadUntilComplete(char *buf, int buf_len) {
  base::ThreadRestrictions::AssertIOAllowed();

  int to_read = buf_len;
  int bytes_total = 0;

  do {
    int bytes_read = ReadSync(buf, to_read);
    if (bytes_read <= 0) {
      if (bytes_total == 0)
        return bytes_read;

      return bytes_total;
    }

    bytes_total += bytes_read;
    buf += bytes_read;
    to_read -= bytes_read;
  } while (bytes_total < buf_len);

  return bytes_total;
}

int FileStream::Write(IOBuffer* buf,
                      int buf_len,
                      const CompletionCallback& callback) {
  if (!IsOpen())
    return ERR_UNEXPECTED;

  DCHECK(is_async());
  DCHECK(open_flags_ & base::PLATFORM_FILE_WRITE);
  // write(..., 0) will return 0, which indicates end-of-file.
  DCHECK_GT(buf_len, 0);

  return context_->WriteAsync(buf, buf_len, callback);
}

int FileStream::WriteSync(const char* buf, int buf_len) {
  base::ThreadRestrictions::AssertIOAllowed();

  if (!IsOpen())
    return ERR_UNEXPECTED;

  DCHECK(!is_async());
  DCHECK(open_flags_ & base::PLATFORM_FILE_WRITE);
  // write(..., 0) will return 0, which indicates end-of-file.
  DCHECK_GT(buf_len, 0);

  return context_->WriteSync(buf, buf_len);
}

int64 FileStream::Truncate(int64 bytes) {
  base::ThreadRestrictions::AssertIOAllowed();

  if (!IsOpen())
    return ERR_UNEXPECTED;

  // We'd better be open for writing.
  DCHECK(open_flags_ & base::PLATFORM_FILE_WRITE);

  // Seek to the position to truncate from.
  int64 seek_position = SeekSync(FROM_BEGIN, bytes);
  if (seek_position != bytes)
    return ERR_UNEXPECTED;

  // And truncate the file.
  return context_->Truncate(bytes);
}

int FileStream::Flush(const CompletionCallback& callback) {
  if (!IsOpen())
    return ERR_UNEXPECTED;

  DCHECK(open_flags_ & base::PLATFORM_FILE_WRITE);
  // Make sure we're async.
  DCHECK(is_async());

  context_->FlushAsync(callback);
  return ERR_IO_PENDING;
}

int FileStream::FlushSync() {
  base::ThreadRestrictions::AssertIOAllowed();

  if (!IsOpen())
    return ERR_UNEXPECTED;

  DCHECK(open_flags_ & base::PLATFORM_FILE_WRITE);
  return context_->FlushSync();
}

void FileStream::EnableErrorStatistics() {
  context_->set_record_uma(true);
}

void FileStream::SetBoundNetLogSource(const BoundNetLog& owner_bound_net_log) {
  if ((owner_bound_net_log.source().id == NetLog::Source::kInvalidId) &&
      (bound_net_log_.source().id == NetLog::Source::kInvalidId)) {
    // Both |BoundNetLog|s are invalid.
    return;
  }

  // Should never connect to itself.
  DCHECK_NE(bound_net_log_.source().id, owner_bound_net_log.source().id);

  bound_net_log_.AddEvent(NetLog::TYPE_FILE_STREAM_BOUND_TO_OWNER,
      owner_bound_net_log.source().ToEventParametersCallback());

  owner_bound_net_log.AddEvent(NetLog::TYPE_FILE_STREAM_SOURCE,
      bound_net_log_.source().ToEventParametersCallback());
}

base::PlatformFile FileStream::GetPlatformFileForTesting() {
  return context_->file();
}

}  // namespace net
