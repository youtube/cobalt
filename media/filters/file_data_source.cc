// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits>

#include "base/file_util.h"
#include "base/string_util.h"
#include "media/base/filter_host.h"
#include "media/base/filters.h"
#include "media/base/pipeline.h"
#include "media/filters/file_data_source.h"

namespace media {

FileDataSource::FileDataSource()
    : file_(NULL),
      file_size_(0) {
}

FileDataSource::~FileDataSource() {
  Stop();
}

void FileDataSource::Initialize(const std::string& url,
                                FilterCallback* callback) {
  DCHECK(!file_);
  scoped_ptr<FilterCallback> c(callback);
#if defined(OS_WIN)
  FilePath file_path(UTF8ToWide(url));
#else
  FilePath file_path(url);
#endif
  if (file_util::GetFileSize(file_path, &file_size_)) {
    file_ = file_util::OpenFile(file_path, "rb");
  }
  if (!file_) {
    file_size_ = 0;
    host()->SetError(PIPELINE_ERROR_URL_NOT_FOUND);
    callback->Run();
    return;
  }
  media_format_.SetAsString(MediaFormat::kMimeType,
                            mime_type::kApplicationOctetStream);
  media_format_.SetAsString(MediaFormat::kURL, url);
  host()->SetTotalBytes(file_size_);
  host()->SetBufferedBytes(file_size_);
  callback->Run();
}

void FileDataSource::Stop() {
  AutoLock l(lock_);
  if (file_) {
    file_util::CloseFile(file_);
    file_ = NULL;
    file_size_ = 0;
  }
}

const MediaFormat& FileDataSource::media_format() {
  return media_format_;
}

void FileDataSource::Read(int64 position, size_t size, uint8* data,
                          ReadCallback* read_callback) {
  DCHECK(file_);
  AutoLock l(lock_);
  if (file_) {
#if defined(OS_WIN)
    if (_fseeki64(file_, position, SEEK_SET)) {
      read_callback->RunWithParams(
          Tuple1<size_t>(static_cast<size_t>(DataSource::kReadError)));
      return;
    }
#else
    CHECK(position <= std::numeric_limits<int32>::max());
    // TODO(hclam): Change fseek() to support 64-bit position.
    if (fseek(file_, static_cast<int32>(position), SEEK_SET)) {
      read_callback->RunWithParams(
          Tuple1<size_t>(static_cast<size_t>(DataSource::kReadError)));
      return;
    }
#endif
    size_t size_read = fread(data, 1, size, file_);
    if (size_read == size || !ferror(file_)) {
      read_callback->RunWithParams(
          Tuple1<size_t>(static_cast<size_t>(size_read)));
      return;
    }
  }

  read_callback->RunWithParams(Tuple1<size_t>(static_cast<size_t>(kReadError)));
}

bool FileDataSource::GetSize(int64* size_out) {
  DCHECK(size_out);
  DCHECK(file_);
  AutoLock l(lock_);
  *size_out = file_size_;
  return (NULL != file_);
}

bool FileDataSource::IsSeekable() {
  // A file data source is always seekable.
  return true;
}

}  // namespace media
