// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/upload_data_stream.h"

#include "base/logging.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/upload_element_reader.h"

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
      initialized_successfully_(false),
      weak_ptr_factory_(this) {
  const std::vector<UploadElement>& elements = *upload_data_->elements();
  for (size_t i = 0; i < elements.size(); ++i)
    element_readers_.push_back(UploadElementReader::Create(elements[i]));
}

UploadDataStream::~UploadDataStream() {
}

int UploadDataStream::Init(const CompletionCallback& callback) {
  DCHECK(!initialized_successfully_);

  // Use fast path when initialization can be done synchronously.
  if (IsInMemory())
    return InitSync();

  InitInternal(0, callback, OK);
  return ERR_IO_PENDING;
}

int UploadDataStream::InitSync() {
  DCHECK(!initialized_successfully_);

  // Initialize all readers synchronously.
  for (size_t i = 0; i < element_readers_.size(); ++i) {
    UploadElementReader* reader = element_readers_[i];
    const int result = reader->InitSync();
    if (result != OK) {
      element_readers_.clear();
      return result;
    }
  }

  FinalizeInitialization();
  return OK;
}

int UploadDataStream::Read(IOBuffer* buf, int buf_len) {
  DCHECK(initialized_successfully_);

  // Initialize readers for newly appended chunks.
  if (is_chunked()) {
    const std::vector<UploadElement>& elements = *upload_data_->elements();
    DCHECK_LE(element_readers_.size(), elements.size());

    for (size_t i = element_readers_.size(); i < elements.size(); ++i) {
      const UploadElement& element = elements[i];
      DCHECK_EQ(UploadElement::TYPE_BYTES, element.type());
      UploadElementReader* reader = UploadElementReader::Create(element);

      const int rv = reader->InitSync();
      DCHECK_EQ(rv, OK);
      element_readers_.push_back(reader);
    }
  }

  int bytes_copied = 0;
  while (bytes_copied < buf_len && element_index_ < element_readers_.size()) {
    UploadElementReader* reader = element_readers_[element_index_];
    bytes_copied += reader->ReadSync(buf->data() + bytes_copied,
                                     buf_len - bytes_copied);
    if (reader->BytesRemaining() == 0)
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
  DCHECK(initialized_successfully_);
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

  for (size_t i = 0; i < element_readers_.size(); ++i) {
    if (!element_readers_[i]->IsInMemory())
      return false;
  }
  return true;
}

void UploadDataStream::InitInternal(int start_index,
                                    const CompletionCallback& callback,
                                    int previous_result) {
  DCHECK(!initialized_successfully_);
  DCHECK_NE(ERR_IO_PENDING, previous_result);

  // Check the last result.
  if (previous_result != OK) {
    element_readers_.clear();
    callback.Run(previous_result);
    return;
  }

  // Call Init() for all elements.
  for (size_t i = start_index; i < element_readers_.size(); ++i) {
    UploadElementReader* reader = element_readers_[i];
    // When new_result is ERR_IO_PENDING, InitInternal() will be called
    // with start_index == i + 1 when reader->Init() finishes.
    const int new_result = reader->Init(
        base::Bind(&UploadDataStream::InitInternal,
                   weak_ptr_factory_.GetWeakPtr(),
                   i + 1,
                   callback));
    if (new_result != OK) {
      if (new_result != ERR_IO_PENDING) {
        element_readers_.clear();
        callback.Run(new_result);
      }
      return;
    }
  }

  // Finalize initialization.
  FinalizeInitialization();
  callback.Run(OK);
}

void UploadDataStream::FinalizeInitialization() {
  DCHECK(!initialized_successfully_);
  if (!is_chunked()) {
    uint64 total_size = 0;
    for (size_t i = 0; i < element_readers_.size(); ++i) {
      UploadElementReader* reader = element_readers_[i];
      total_size += reader->GetContentLength();
    }
    total_size_ = total_size;
  }
  initialized_successfully_ = true;
}

}  // namespace net
