// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/upload_file_element_reader.h"

#include "base/bind.h"
#include "base/file_util.h"
#include "base/location.h"
#include "base/task_runner_util.h"
#include "base/threading/worker_pool.h"
#include "net/base/file_stream.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"

namespace net {

namespace {

// In tests, this value is used to override the return value of
// UploadFileElementReader::GetContentLength() when set to non-zero.
uint64 overriding_content_length = 0;

// This function is used to implement Init().
int InitInternal(const FilePath& path,
                 uint64 range_offset,
                 uint64 range_length,
                 const base::Time& expected_modification_time,
                 UploadFileElementReader::ScopedFileStreamPtr* out_file_stream,
                 uint64* out_content_length) {
  scoped_ptr<FileStream> file_stream(new FileStream(NULL));
  int64 rv = file_stream->OpenSync(
      path, base::PLATFORM_FILE_OPEN | base::PLATFORM_FILE_READ);
  if (rv != OK) {
    // If the file can't be opened, we'll just upload an empty file.
    DLOG(WARNING) << "Failed to open \"" << path.value()
                  << "\" for reading: " << rv;
    file_stream.reset();
  } else if (range_offset) {
    rv = file_stream->SeekSync(FROM_BEGIN, range_offset);
    if (rv < 0) {
      DLOG(WARNING) << "Failed to seek \"" << path.value()
                    << "\" to offset: " << range_offset << " (" << rv << ")";
      file_stream.reset();
    }
  }

  int64 length = 0;
  if (file_stream.get() &&
      file_util::GetFileSize(path, &length) &&
      range_offset < static_cast<uint64>(length)) {
    // Compensate for the offset.
    length = std::min(length - range_offset, range_length);
  }
  *out_content_length = length;
  out_file_stream->reset(file_stream.release());

  // If the underlying file has been changed and the expected file modification
  // time is set, treat it as error. Note that the expected modification time
  // from WebKit is based on time_t precision. So we have to convert both to
  // time_t to compare. This check is used for sliced files.
  if (!expected_modification_time.is_null()) {
    base::PlatformFileInfo info;
    if (file_util::GetFileInfo(path, &info) &&
        expected_modification_time.ToTimeT() != info.last_modified.ToTimeT()) {
      return ERR_UPLOAD_FILE_CHANGED;
    }
  }

  return OK;
}

// This function is used to implement Read().
int ReadInternal(scoped_refptr<IOBuffer> buf,
                 int buf_length,
                 uint64 bytes_remaining,
                 FileStream* file_stream) {
  DCHECK_LT(0, buf_length);

  const uint64 num_bytes_to_read =
      std::min(bytes_remaining, static_cast<uint64>(buf_length));

  if (num_bytes_to_read > 0) {
    int num_bytes_consumed = 0;
    // file_stream is NULL if the target file is missing or not readable.
    if (file_stream) {
      num_bytes_consumed = file_stream->ReadSync(buf->data(),
                                                 num_bytes_to_read);
    }
    if (num_bytes_consumed <= 0) {
      // If there's less data to read than we initially observed, then
      // pad with zero.  Otherwise the server will hang waiting for the
      // rest of the data.
      memset(buf->data(), 0, num_bytes_to_read);
    }
  }
  return num_bytes_to_read;
}

}  // namespace

void UploadFileElementReader::FileStreamDeleter::operator() (
    FileStream* file_stream) const {
  if (file_stream) {
    base::WorkerPool::PostTask(FROM_HERE,
                               base::Bind(&base::DeletePointer<FileStream>,
                                          file_stream),
                               true /* task_is_slow */);
  }
}

UploadFileElementReader::UploadFileElementReader(
    const FilePath& path,
    uint64 range_offset,
    uint64 range_length,
    const base::Time& expected_modification_time)
    : path_(path),
      range_offset_(range_offset),
      range_length_(range_length),
      expected_modification_time_(expected_modification_time),
      content_length_(0),
      bytes_remaining_(0),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)) {
}

UploadFileElementReader::~UploadFileElementReader() {
}

const UploadFileElementReader* UploadFileElementReader::AsFileReader() const {
  return this;
}

int UploadFileElementReader::Init(const CompletionCallback& callback) {
  Reset();

  ScopedFileStreamPtr* file_stream = new ScopedFileStreamPtr;
  uint64* content_length = new uint64;
  const bool posted = base::PostTaskAndReplyWithResult(
      base::WorkerPool::GetTaskRunner(true /* task_is_slow */),
      FROM_HERE,
      base::Bind(&InitInternal,
                 path_,
                 range_offset_,
                 range_length_,
                 expected_modification_time_,
                 file_stream,
                 content_length),
      base::Bind(&UploadFileElementReader::OnInitCompleted,
                 weak_ptr_factory_.GetWeakPtr(),
                 base::Owned(file_stream),
                 base::Owned(content_length),
                 callback));
  DCHECK(posted);
  return ERR_IO_PENDING;
}

int UploadFileElementReader::InitSync() {
  Reset();

  ScopedFileStreamPtr file_stream;
  uint64 content_length = 0;
  const int result = InitInternal(path_, range_offset_, range_length_,
                                  expected_modification_time_,
                                  &file_stream, &content_length);
  OnInitCompleted(&file_stream, &content_length, CompletionCallback(), result);
  return result;
}

uint64 UploadFileElementReader::GetContentLength() const {
  if (overriding_content_length)
    return overriding_content_length;
  return content_length_;
}

uint64 UploadFileElementReader::BytesRemaining() const {
  return bytes_remaining_;
}

int UploadFileElementReader::Read(IOBuffer* buf,
                                  int buf_length,
                                  const CompletionCallback& callback) {
  DCHECK(!callback.is_null());

  if (BytesRemaining() == 0)
    return 0;

  // Save the value of file_stream_.get() before base::Passed() invalidates it.
  FileStream* file_stream_ptr = file_stream_.get();
  // Pass the ownership of file_stream_ to the worker pool to safely perform
  // operation even when |this| is destructed before the read completes.
  const bool posted = base::PostTaskAndReplyWithResult(
      base::WorkerPool::GetTaskRunner(true /* task_is_slow */),
      FROM_HERE,
      base::Bind(&ReadInternal,
                 scoped_refptr<IOBuffer>(buf),
                 buf_length,
                 BytesRemaining(),
                 file_stream_ptr),
      base::Bind(&UploadFileElementReader::OnReadCompleted,
                 weak_ptr_factory_.GetWeakPtr(),
                 base::Passed(&file_stream_),
                 callback));
  DCHECK(posted);
  return ERR_IO_PENDING;
}

int UploadFileElementReader::ReadSync(IOBuffer* buf, int buf_length) {
  const int result = ReadInternal(buf, buf_length, BytesRemaining(),
                                  file_stream_.get());
  OnReadCompleted(file_stream_.Pass(), CompletionCallback(), result);
  return result;
}

void UploadFileElementReader::Reset() {
  weak_ptr_factory_.InvalidateWeakPtrs();
  bytes_remaining_ = 0;
  content_length_ = 0;
  file_stream_.reset();
}

void UploadFileElementReader::OnInitCompleted(
    ScopedFileStreamPtr* file_stream,
    uint64* content_length,
    const CompletionCallback& callback,
    int result) {
  file_stream_.swap(*file_stream);
  content_length_ = *content_length;
  bytes_remaining_ = GetContentLength();
  if (!callback.is_null())
    callback.Run(result);
}

void UploadFileElementReader::OnReadCompleted(
    ScopedFileStreamPtr file_stream,
    const CompletionCallback& callback,
    int result) {
  file_stream_.swap(file_stream);
  DCHECK_GE(static_cast<int>(bytes_remaining_), result);
  bytes_remaining_ -= result;
  if (!callback.is_null())
    callback.Run(result);
}

UploadFileElementReader::ScopedOverridingContentLengthForTests::
ScopedOverridingContentLengthForTests(uint64 value) {
  overriding_content_length = value;
}

UploadFileElementReader::ScopedOverridingContentLengthForTests::
~ScopedOverridingContentLengthForTests() {
  overriding_content_length = 0;
}

}  // namespace net
