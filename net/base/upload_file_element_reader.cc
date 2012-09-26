// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/upload_file_element_reader.h"

#include "base/bind.h"
#include "base/file_util.h"
#include "base/location.h"
#include "base/threading/thread_restrictions.h"
#include "base/threading/worker_pool.h"
#include "net/base/file_stream.h"
#include "net/base/net_errors.h"

namespace net {

namespace {

// In tests, this value is used to override the return value of
// UploadFileElementReader::GetContentLength() when set to non-zero.
uint64 overriding_content_length = 0;

// This method is used to implement Init().
void InitInternal(const FilePath& path,
                  uint64 range_offset,
                  uint64 range_length,
                  const base::Time& expected_modification_time,
                  scoped_ptr<FileStream>* out_file_stream,
                  uint64* out_content_length,
                  int* out_result) {
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
      file_stream->CloseSync();
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
  out_file_stream->swap(file_stream);

  // If the underlying file has been changed and the expected file modification
  // time is set, treat it as error. Note that the expected modification time
  // from WebKit is based on time_t precision. So we have to convert both to
  // time_t to compare. This check is used for sliced files.
  if (!expected_modification_time.is_null()) {
    base::PlatformFileInfo info;
    if (file_util::GetFileInfo(path, &info) &&
        expected_modification_time.ToTimeT() != info.last_modified.ToTimeT()) {
      *out_result = ERR_UPLOAD_FILE_CHANGED;
      return;
    }
  }

  *out_result = OK;
}

}  // namespace

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
      weak_ptr_factory_(this) {
}

UploadFileElementReader::~UploadFileElementReader() {
  if (file_stream_.get()) {
    base::WorkerPool::PostTask(FROM_HERE,
                               base::Bind(&base::DeletePointer<FileStream>,
                                          file_stream_.release()),
                               true /* task_is_slow */);
  }
}

int UploadFileElementReader::Init(const CompletionCallback& callback) {
  scoped_ptr<FileStream>* file_stream = new scoped_ptr<FileStream>;
  uint64* content_length = new uint64;
  int* result = new int;
  const bool posted = base::WorkerPool::PostTaskAndReply(
      FROM_HERE,
      base::Bind(&InitInternal,
                 path_,
                 range_offset_,
                 range_length_,
                 expected_modification_time_,
                 file_stream,
                 content_length,
                 result),
      base::Bind(&UploadFileElementReader::OnInitCompleted,
                 weak_ptr_factory_.GetWeakPtr(),
                 base::Owned(file_stream),
                 base::Owned(content_length),
                 base::Owned(result),
                 callback),
      true /* task_is_slow */);
  DCHECK(posted);
  return ERR_IO_PENDING;
}

int UploadFileElementReader::InitSync() {
  // Temporarily allow until fix: http://crbug.com/72001.
  base::ThreadRestrictions::ScopedAllowIO allow_io;

  scoped_ptr<FileStream> file_stream;
  uint64 content_length = 0;
  int result = OK;
  InitInternal(path_, range_offset_, range_length_, expected_modification_time_,
               &file_stream, &content_length, &result);
  OnInitCompleted(&file_stream, &content_length, &result, CompletionCallback());
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

int UploadFileElementReader::ReadSync(char* buf, int buf_length) {
  // Temporarily allow until fix: http://crbug.com/72001.
  base::ThreadRestrictions::ScopedAllowIO allow_io;
  DCHECK_LT(0, buf_length);

  const uint64 num_bytes_to_read =
      static_cast<int>(std::min(BytesRemaining(),
                                static_cast<uint64>(buf_length)));
  if (num_bytes_to_read > 0) {
    int num_bytes_consumed = 0;
    // file_stream_ is NULL if the target file is
    // missing or not readable.
    if (file_stream_.get()) {
      num_bytes_consumed =
          file_stream_->ReadSync(buf, num_bytes_to_read);
    }
    if (num_bytes_consumed <= 0) {
      // If there's less data to read than we initially observed, then
      // pad with zero.  Otherwise the server will hang waiting for the
      // rest of the data.
      memset(buf, 0, num_bytes_to_read);
    }
  }
  DCHECK_GE(bytes_remaining_, num_bytes_to_read);
  bytes_remaining_ -= num_bytes_to_read;
  return num_bytes_to_read;
}

void UploadFileElementReader::OnInitCompleted(
    scoped_ptr<FileStream>* file_stream,
    uint64* content_length,
    int* result,
    const CompletionCallback& callback) {
  file_stream_.swap(*file_stream);
  content_length_ = *content_length;
  bytes_remaining_ = GetContentLength();
  if (!callback.is_null())
    callback.Run(*result);
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
