// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/upload_element.h"

#include <algorithm>

#include "base/file_util.h"
#include "base/threading/thread_restrictions.h"
#include "net/base/file_stream.h"
#include "net/base/net_errors.h"

namespace net {

UploadElement::UploadElement()
    : type_(TYPE_BYTES),
      bytes_start_(NULL),
      bytes_length_(0),
      file_range_offset_(0),
      file_range_length_(kuint64max),
      override_content_length_(false),
      content_length_computed_(false),
      content_length_(-1),
      offset_(0),
      file_stream_(NULL) {
}

UploadElement::~UploadElement() {
  // In the common case |file__stream_| will be null.
  if (file_stream_) {
    // Temporarily allow until fix: http://crbug.com/72001.
    base::ThreadRestrictions::ScopedAllowIO allow_io;
    file_stream_->CloseSync();
    delete file_stream_;
  }
}

uint64 UploadElement::GetContentLength() {
  if (override_content_length_ || content_length_computed_)
    return content_length_;

  if (type_ == TYPE_BYTES)
    return bytes_length();

  DCHECK_EQ(TYPE_FILE, type_);
  DCHECK(!file_stream_);

  // TODO(darin): This size calculation could be out of sync with the state of
  // the file when we get around to reading it.  We should probably find a way
  // to lock the file or somehow protect against this error condition.

  content_length_computed_ = true;
  content_length_ = 0;

  // We need to open the file here to decide if we should report the file's
  // size or zero.  We cache the open file, so that we can still read it when
  // it comes time to.
  file_stream_ = OpenFileStream();
  if (!file_stream_)
    return 0;

  int64 length = 0;
  if (!file_util::GetFileSize(file_path_, &length))
    return 0;

  if (file_range_offset_ >= static_cast<uint64>(length))
    return 0;  // range is beyond eof

  // compensate for the offset and clip file_range_length_ to eof
  content_length_ =  std::min(length - file_range_offset_, file_range_length_);
  return content_length_;
}

int UploadElement::ReadSync(char* buf, int buf_len) {
  if (type_ == TYPE_BYTES) {
    return ReadFromMemorySync(buf, buf_len);
  } else if (type_ == TYPE_FILE) {
    return ReadFromFileSync(buf, buf_len);
  }

  NOTREACHED();
  return 0;
}

uint64 UploadElement::BytesRemaining() {
  return GetContentLength() - offset_;
}

void UploadElement::ResetOffset() {
  offset_ = 0;

  // Delete the file stream if already opened, so we can reread the file from
  // the beginning.
  if (file_stream_) {
    // Temporarily allow until fix: http://crbug.com/72001.
    base::ThreadRestrictions::ScopedAllowIO allow_io;
    file_stream_->CloseSync();
    delete file_stream_;
    file_stream_ = NULL;
  }
}

FileStream* UploadElement::OpenFileStream() {
  scoped_ptr<FileStream> file(new FileStream(NULL));
  int64 rv = file->OpenSync(
      file_path_,
      base::PLATFORM_FILE_OPEN | base::PLATFORM_FILE_READ);
  if (rv != OK) {
    // If the file can't be opened, we'll just upload an empty file.
    DLOG(WARNING) << "Failed to open \"" << file_path_.value()
                  << "\" for reading: " << rv;
    return NULL;
  }
  if (file_range_offset_) {
    rv = file->SeekSync(FROM_BEGIN, file_range_offset_);
    if (rv < 0) {
      DLOG(WARNING) << "Failed to seek \"" << file_path_.value()
                    << "\" to offset: " << file_range_offset_ << " (" << rv
                    << ")";
      return NULL;
    }
  }

  return file.release();
}

int UploadElement::ReadFromMemorySync(char* buf, int buf_len) {
  DCHECK_LT(0, buf_len);
  DCHECK(type_ == TYPE_BYTES);

  const size_t num_bytes_to_read = std::min(BytesRemaining(),
                                            static_cast<uint64>(buf_len));

  // Check if we have anything to copy first, because we are getting
  // the address of an element in |bytes_| and that will throw an
  // exception if |bytes_| is an empty vector.
  if (num_bytes_to_read > 0)
    memcpy(buf, bytes() + offset_, num_bytes_to_read);

  offset_ += num_bytes_to_read;
  return num_bytes_to_read;
}

int UploadElement::ReadFromFileSync(char* buf, int buf_len) {
  DCHECK_LT(0, buf_len);
  DCHECK_EQ(TYPE_FILE, type_);

  // Open the file of the current element if not yet opened.
  // In common usage, GetContentLength() opened it already.
  if (!file_stream_) {
    // Temporarily allow until fix: http://crbug.com/72001.
    base::ThreadRestrictions::ScopedAllowIO allow_io;
    file_stream_ = OpenFileStream();
  }

  const int num_bytes_to_read =
      static_cast<int>(std::min(BytesRemaining(),
                                static_cast<uint64>(buf_len)));
  if (num_bytes_to_read > 0) {
    int num_bytes_consumed = 0;
    // Temporarily allow until fix: http://crbug.com/72001.
    base::ThreadRestrictions::ScopedAllowIO allow_io;
    // file_stream_ is NULL if the target file is
    // missing or not readable.
    if (file_stream_) {
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

  offset_ += num_bytes_to_read;
  return num_bytes_to_read;
}

}  // namespace net
