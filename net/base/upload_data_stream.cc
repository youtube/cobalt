// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/upload_data_stream.h"

#include "base/file_util.h"
#include "base/logging.h"
#include "base/threading/thread_restrictions.h"
#include "net/base/file_stream.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"

namespace net {

bool UploadDataStream::merge_chunks_ = true;

// static
void UploadDataStream::ResetMergeChunks() {
  // WARNING: merge_chunks_ must match the above initializer.
  merge_chunks_ = true;
}

UploadDataStream::UploadDataStream(UploadData* upload_data)
    : upload_data_(upload_data),
      element_index_(0),
      total_size_(0),
      current_position_(0),
      initialized_successfully_(false) {
}

UploadDataStream::~UploadDataStream() {
}

int UploadDataStream::Init() {
  DCHECK(!initialized_successfully_);

  {
    base::ThreadRestrictions::ScopedAllowIO allow_io;
    total_size_ = upload_data_->GetContentLengthSync();
  }

  // If the underlying file has been changed and the expected file
  // modification time is set, treat it as error. Note that the expected
  // modification time from WebKit is based on time_t precision. So we
  // have to convert both to time_t to compare. This check is used for
  // sliced files.
  const std::vector<UploadData::Element>& elements = *upload_data_->elements();
  for (size_t i = 0; i < elements.size(); ++i) {
    const UploadData::Element& element = elements[i];
    if (element.type() == UploadData::TYPE_FILE &&
        !element.expected_file_modification_time().is_null()) {
      // Temporarily allow until fix: http://crbug.com/72001.
      base::ThreadRestrictions::ScopedAllowIO allow_io;
      base::PlatformFileInfo info;
      if (file_util::GetFileInfo(element.file_path(), &info) &&
          element.expected_file_modification_time().ToTimeT() !=
          info.last_modified.ToTimeT()) {
        return ERR_UPLOAD_FILE_CHANGED;
      }
    }
  }

  // Reset the offset, as upload_data_ may already be read (i.e. UploadData
  // can be reused for a new UploadDataStream).
  upload_data_->ResetOffset();

  initialized_successfully_ = true;
  return OK;
}

int UploadDataStream::Read(IOBuffer* buf, int buf_len) {
  std::vector<UploadData::Element>& elements = *upload_data_->elements();

  int bytes_copied = 0;
  while (bytes_copied < buf_len && element_index_ < elements.size()) {
    UploadData::Element& element = elements[element_index_];

    bytes_copied += element.ReadSync(buf->data() + bytes_copied,
                                     buf_len - bytes_copied);

    if (element.BytesRemaining() == 0)
        ++element_index_;

    if (is_chunked() && !merge_chunks_)
      break;
  }

  current_position_ += bytes_copied;
  if (is_chunked() && !IsEOF() && bytes_copied == 0)
    return ERR_IO_PENDING;

  return bytes_copied;
}

bool UploadDataStream::IsEOF() const {
  const std::vector<UploadData::Element>& elements = *upload_data_->elements();

  // Check if all elements are consumed.
  if (element_index_ == elements.size()) {
    // If the upload data is chunked, check if the last element is the
    // last chunk.
    if (!upload_data_->is_chunked() ||
        (!elements.empty() && elements.back().is_last_chunk())) {
      return true;
    }
  }
  return false;
}

bool UploadDataStream::IsInMemory() const {
  DCHECK(initialized_successfully_);

  return upload_data_->IsInMemory();
}

}  // namespace net
