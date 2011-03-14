// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits>

#include "base/file_util.h"
#include "base/logging.h"
#include "base/utf_string_conversions.h"
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
  DCHECK(!file_);
}

PipelineError FileDataSource::Initialize(const std::string& url) {
  DCHECK(!file_);
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
    return PIPELINE_ERROR_URL_NOT_FOUND;
  }
  media_format_.SetAsString(MediaFormat::kURL, url);

  if (host()) {
    host()->SetTotalBytes(file_size_);
    host()->SetBufferedBytes(file_size_);
  }

  return PIPELINE_OK;
}

void FileDataSource::set_host(FilterHost* filter_host) {
  DataSource::set_host(filter_host);
  if (file_) {
    host()->SetTotalBytes(file_size_);
    host()->SetBufferedBytes(file_size_);
  }
}

void FileDataSource::Stop(FilterCallback* callback) {
  base::AutoLock l(lock_);
  if (file_) {
    file_util::CloseFile(file_);
    file_ = NULL;
    file_size_ = 0;
  }
  if (callback) {
    callback->Run();
    delete callback;
  }
}

const MediaFormat& FileDataSource::media_format() {
  return media_format_;
}

void FileDataSource::Read(int64 position, size_t size, uint8* data,
                          ReadCallback* read_callback) {
  DCHECK(file_);
  base::AutoLock l(lock_);
  scoped_ptr<ReadCallback> callback(read_callback);
  if (file_) {
#if defined(OS_WIN)
    if (_fseeki64(file_, position, SEEK_SET)) {
      callback->RunWithParams(
          Tuple1<size_t>(static_cast<size_t>(DataSource::kReadError)));
      return;
    }
#else
    CHECK(position <= std::numeric_limits<int32>::max());
    // TODO(hclam): Change fseek() to support 64-bit position.
    if (fseek(file_, static_cast<int32>(position), SEEK_SET)) {
      callback->RunWithParams(
          Tuple1<size_t>(static_cast<size_t>(DataSource::kReadError)));
      return;
    }
#endif
    size_t size_read = fread(data, 1, size, file_);
    if (size_read == size || !ferror(file_)) {
      callback->RunWithParams(
          Tuple1<size_t>(static_cast<size_t>(size_read)));
      return;
    }
  }

  callback->RunWithParams(Tuple1<size_t>(static_cast<size_t>(kReadError)));
}

bool FileDataSource::GetSize(int64* size_out) {
  DCHECK(size_out);
  DCHECK(file_);
  base::AutoLock l(lock_);
  *size_out = file_size_;
  return (NULL != file_);
}

bool FileDataSource::IsStreaming() {
  return false;
}

}  // namespace media
