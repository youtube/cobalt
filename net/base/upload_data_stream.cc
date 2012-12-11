// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/upload_data_stream.h"

#include "base/logging.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/upload_bytes_element_reader.h"
#include "net/base/upload_element_reader.h"

namespace net {

namespace {

// A subclass of UplodBytesElementReader which owns the data given as a vector.
class UploadOwnedBytesElementReader : public UploadBytesElementReader {
 public:
  UploadOwnedBytesElementReader(std::vector<char>* data)
      : UploadBytesElementReader(&(*data)[0], data->size()) {
    data_.swap(*data);
  }

  virtual ~UploadOwnedBytesElementReader() {}

 private:
  std::vector<char> data_;
};

}  // namespace

bool UploadDataStream::merge_chunks_ = true;

// static
void UploadDataStream::ResetMergeChunks() {
  // WARNING: merge_chunks_ must match the above initializer.
  merge_chunks_ = true;
}

UploadDataStream::UploadDataStream(
    ScopedVector<UploadElementReader>* element_readers,
    int64 identifier)
    : element_index_(0),
      total_size_(0),
      current_position_(0),
      identifier_(identifier),
      is_chunked_(false),
      last_chunk_appended_(false),
      initialized_successfully_(false),
      weak_ptr_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
  element_readers_.swap(*element_readers);
}

UploadDataStream::UploadDataStream(Chunked /*chunked*/, int64 identifier)
    : element_index_(0),
      total_size_(0),
      current_position_(0),
      identifier_(identifier),
      is_chunked_(true),
      last_chunk_appended_(false),
      initialized_successfully_(false),
      weak_ptr_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
}

UploadDataStream::~UploadDataStream() {
}

int UploadDataStream::Init(const CompletionCallback& callback) {
  Reset();
  // Use fast path when initialization can be done synchronously.
  if (IsInMemory())
    return InitSync();

  return InitInternal(0, callback);
}

int UploadDataStream::InitSync() {
  Reset();
  // Initialize all readers synchronously.
  for (size_t i = 0; i < element_readers_.size(); ++i) {
    UploadElementReader* reader = element_readers_[i];
    const int result = reader->InitSync();
    if (result != OK)
      return result;
  }

  FinalizeInitialization();
  return OK;
}

int UploadDataStream::Read(IOBuffer* buf,
                           int buf_len,
                           const CompletionCallback& callback) {
  DCHECK(initialized_successfully_);
  DCHECK(!callback.is_null());
  DCHECK_GT(buf_len, 0);
  return ReadInternal(new DrainableIOBuffer(buf, buf_len), callback);
}

int UploadDataStream::ReadSync(IOBuffer* buf, int buf_len) {
  DCHECK(initialized_successfully_);
  DCHECK_GT(buf_len, 0);
  return ReadInternal(new DrainableIOBuffer(buf, buf_len),
                      CompletionCallback());
}

bool UploadDataStream::IsEOF() const {
  DCHECK(initialized_successfully_);
  // Check if all elements are consumed.
  if (element_index_ == element_readers_.size()) {
    // If the upload data is chunked, check if the last chunk is appended.
    if (!is_chunked_ || last_chunk_appended_)
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
  if (is_chunked_)
    return false;

  for (size_t i = 0; i < element_readers_.size(); ++i) {
    if (!element_readers_[i]->IsInMemory())
      return false;
  }
  return true;
}

void UploadDataStream::AppendChunk(const char* bytes,
                                   int bytes_len,
                                   bool is_last_chunk) {
  DCHECK(is_chunked_);
  DCHECK(!last_chunk_appended_);
  last_chunk_appended_ = is_last_chunk;

  // Initialize a reader for the newly appended chunk. We leave |total_size_| at
  // zero, since for chunked uploads, we may not know the total size.
  std::vector<char> data(bytes, bytes + bytes_len);
  UploadElementReader* reader = new UploadOwnedBytesElementReader(&data);
  const int rv = reader->InitSync();
  DCHECK_EQ(OK, rv);
  element_readers_.push_back(reader);

  // Resume pending read.
  if (!pending_chunked_read_callback_.is_null()) {
    base::Closure callback = pending_chunked_read_callback_;
    pending_chunked_read_callback_.Reset();
    callback.Run();
  }
}

void UploadDataStream::Reset() {
  weak_ptr_factory_.InvalidateWeakPtrs();
  pending_chunked_read_callback_.Reset();
  initialized_successfully_ = false;
  current_position_ = 0;
  total_size_ = 0;
  element_index_ = 0;
}

int UploadDataStream::InitInternal(int start_index,
                                   const CompletionCallback& callback) {
  DCHECK(!initialized_successfully_);

  // Call Init() for all elements.
  for (size_t i = start_index; i < element_readers_.size(); ++i) {
    UploadElementReader* reader = element_readers_[i];
    // When new_result is ERR_IO_PENDING, InitInternal() will be called
    // with start_index == i + 1 when reader->Init() finishes.
    const int result = reader->Init(
        base::Bind(&UploadDataStream::ResumePendingInit,
                   weak_ptr_factory_.GetWeakPtr(),
                   i + 1,
                   callback));
    if (result != OK)
      return result;
  }

  // Finalize initialization.
  FinalizeInitialization();
  return OK;
}

void UploadDataStream::ResumePendingInit(int start_index,
                                         const CompletionCallback& callback,
                                         int previous_result) {
  DCHECK(!initialized_successfully_);
  DCHECK(!callback.is_null());
  DCHECK_NE(ERR_IO_PENDING, previous_result);

  // Check the last result.
  if (previous_result != OK) {
    callback.Run(previous_result);
    return;
  }

  const int result = InitInternal(start_index, callback);
  if (result != ERR_IO_PENDING)
    callback.Run(result);
}

void UploadDataStream::FinalizeInitialization() {
  DCHECK(!initialized_successfully_);
  if (!is_chunked_) {
    uint64 total_size = 0;
    for (size_t i = 0; i < element_readers_.size(); ++i) {
      UploadElementReader* reader = element_readers_[i];
      total_size += reader->GetContentLength();
    }
    total_size_ = total_size;
  }
  initialized_successfully_ = true;
}

int UploadDataStream::ReadInternal(scoped_refptr<DrainableIOBuffer> buf,
                                   const CompletionCallback& callback) {
  DCHECK(initialized_successfully_);

  while (element_index_ < element_readers_.size()) {
    UploadElementReader* reader = element_readers_[element_index_];

    if (reader->BytesRemaining() == 0) {
      ++element_index_;
      continue;
    }

    if (buf->BytesRemaining() == 0)
      break;

    // Some tests need chunks to be kept unmerged.
    if (!merge_chunks_ && is_chunked_ && buf->BytesConsumed())
      break;

    int result = OK;
    if (!callback.is_null()) {
      result = reader->Read(
          buf,
          buf->BytesRemaining(),
          base::Bind(base::IgnoreResult(&UploadDataStream::ResumePendingRead),
                     weak_ptr_factory_.GetWeakPtr(),
                     buf,
                     callback));
    } else {
      result = reader->ReadSync(buf, buf->BytesRemaining());
      DCHECK_NE(ERR_IO_PENDING, result);
    }
    if (result == ERR_IO_PENDING)
      return ERR_IO_PENDING;
    DCHECK_GE(result, 0);
    buf->DidConsume(result);
  }

  const int bytes_copied = buf->BytesConsumed();
  current_position_ += bytes_copied;

  if (is_chunked_ && !IsEOF() && bytes_copied == 0) {
    DCHECK(!callback.is_null());
    DCHECK(pending_chunked_read_callback_.is_null());
    pending_chunked_read_callback_ =
        base::Bind(&UploadDataStream::ResumePendingRead,
                   weak_ptr_factory_.GetWeakPtr(),
                   buf,
                   callback,
                   OK);
    return ERR_IO_PENDING;
  }
  return bytes_copied;
}

void UploadDataStream::ResumePendingRead(scoped_refptr<DrainableIOBuffer> buf,
                                         const CompletionCallback& callback,
                                         int previous_result) {
  DCHECK_GE(previous_result, 0);

  // Add the last result.
  buf->DidConsume(previous_result);

  DCHECK(!callback.is_null());
  const int result = ReadInternal(buf, callback);
  if (result != ERR_IO_PENDING)
    callback.Run(result);
}

}  // namespace net
