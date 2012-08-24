// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/upload_data.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/threading/worker_pool.h"

namespace net {

namespace {

// Helper function for GetContentLength().
void OnGetContentLengthComplete(
    const UploadData::ContentLengthCallback& callback,
    uint64* content_length) {
  callback.Run(*content_length);
}

}  // namespace

UploadData::UploadData()
    : identifier_(0),
      chunk_callback_(NULL),
      is_chunked_(false),
      last_chunk_appended_(false) {
}

void UploadData::AppendBytes(const char* bytes, int bytes_len) {
  DCHECK(!is_chunked_);
  if (bytes_len > 0) {
    elements_.push_back(UploadElement());
    elements_.back().SetToBytes(bytes, bytes_len);
  }
}

void UploadData::AppendFileRange(const FilePath& file_path,
                                 uint64 offset, uint64 length,
                                 const base::Time& expected_modification_time) {
  DCHECK(!is_chunked_);
  elements_.push_back(UploadElement());
  elements_.back().SetToFilePathRange(file_path, offset, length,
                                      expected_modification_time);
}

void UploadData::AppendChunk(const char* bytes,
                             int bytes_len,
                             bool is_last_chunk) {
  DCHECK(is_chunked_);
  DCHECK(!last_chunk_appended_);
  elements_.push_back(UploadElement());
  elements_.back().SetToBytes(bytes, bytes_len);
  last_chunk_appended_ = is_last_chunk;
  if (chunk_callback_)
    chunk_callback_->OnChunkAvailable();
}

void UploadData::set_chunk_callback(ChunkCallback* callback) {
  chunk_callback_ = callback;
}

void UploadData::GetContentLength(const ContentLengthCallback& callback) {
  uint64* result = new uint64(0);
  const bool task_is_slow = true;
  const bool posted = base::WorkerPool::PostTaskAndReply(
      FROM_HERE,
      base::Bind(&UploadData::DoGetContentLength, this, result),
      base::Bind(&OnGetContentLengthComplete, callback, base::Owned(result)),
      task_is_slow);
  DCHECK(posted);
}

uint64 UploadData::GetContentLengthSync() {
  uint64 content_length = 0;
  DoGetContentLength(&content_length);
  return content_length;
}

bool UploadData::IsInMemory() const {
  // Chunks are in memory, but UploadData does not have all the chunks at
  // once. Chunks are provided progressively with AppendChunk() as chunks
  // are ready. Check is_chunked_ here, rather than relying on the loop
  // below, as there is a case that is_chunked_ is set to true, but the
  // first chunk is not yet delivered.
  if (is_chunked_)
    return false;

  for (size_t i = 0; i < elements_.size(); ++i) {
    if (elements_[i].type() != UploadElement::TYPE_BYTES)
      return false;
  }
  return true;
}

void UploadData::ResetOffset() {
  for (size_t i = 0; i < elements_.size(); ++i)
    elements_[i].ResetOffset();
}

void UploadData::SetElements(const std::vector<UploadElement>& elements) {
  elements_ = elements;
}

void UploadData::DoGetContentLength(uint64* content_length) {
  *content_length = 0;

  if (is_chunked_)
    return;

  for (size_t i = 0; i < elements_.size(); ++i)
    *content_length += elements_[i].GetContentLength();
}

UploadData::~UploadData() {
}

}  // namespace net
