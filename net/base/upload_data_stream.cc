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
  std::vector<UploadElement>* elements = upload_data_->elements_mutable();
  {
    base::ThreadRestrictions::ScopedAllowIO allow_io;
    total_size_ = 0;
    if (!is_chunked()) {
      for (size_t i = 0; i < elements->size(); ++i)
        total_size_ += (*elements)[i].GetContentLength();
    }
  }

  // If the underlying file has been changed and the expected file
  // modification time is set, treat it as error. Note that the expected
  // modification time from WebKit is based on time_t precision. So we
  // have to convert both to time_t to compare. This check is used for
  // sliced files.
  for (size_t i = 0; i < elements->size(); ++i) {
    const UploadElement& element = (*elements)[i];
    if (element.type() == UploadElement::TYPE_FILE &&
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
  for (size_t i = 0; i < elements->size(); ++i)
    (*elements)[i].ResetOffset();

  initialized_successfully_ = true;
  return OK;
}

int UploadDataStream::Read(IOBuffer* buf, int buf_len) {
  std::vector<UploadElement>& elements =
      *upload_data_->elements_mutable();

  int bytes_copied = 0;
  while (bytes_copied < buf_len && element_index_ < elements.size()) {
    UploadElement& element = elements[element_index_];

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
  const std::vector<UploadElement>& elements = *upload_data_->elements();

  // Check if all elements are consumed.
  if (element_index_ == elements.size()) {
    // If the upload data is chunked, check if the last chunk is appended.
    if (!upload_data_->is_chunked() || upload_data_->last_chunk_appended())
      return true;
  }
  return false;
}

bool UploadDataStream::IsInMemory() const {
  // Chunks are in memory, but UploadData does not have all the chunks at
  // once. Chunks are provided progressively with AppendChunk() as chunks
  // are ready. Check is_chunked_ here, rather than relying on the loop
  // below, as there is a case that is_chunked_ is set to true, but the
  // first chunk is not yet delivered.
  if (is_chunked())
    return false;

  const std::vector<UploadElement>& elements = *upload_data_->elements();
  for (size_t i = 0; i < elements.size(); ++i) {
    if (elements[i].type() != UploadElement::TYPE_BYTES)
      return false;
  }
  return true;
}

}  // namespace net
