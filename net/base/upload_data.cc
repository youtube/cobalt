// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/upload_data.h"

#include <algorithm>

#include "base/bind.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "base/threading/thread_restrictions.h"
#include "base/threading/worker_pool.h"
#include "base/tracked_objects.h"
#include "net/base/file_stream.h"
#include "net/base/net_errors.h"

namespace net {

namespace {

// Helper function for GetContentLength().
void OnGetContentLengthComplete(
    const UploadData::ContentLengthCallback& callback,
    uint64* content_length) {
  callback.Run(*content_length);
}

}  // namespace

UploadData::Element::Element()
    : type_(TYPE_BYTES),
      file_range_offset_(0),
      file_range_length_(kuint64max),
      is_last_chunk_(false),
      override_content_length_(false),
      content_length_computed_(false),
      content_length_(-1),
      offset_(0),
      file_stream_(NULL) {
}

UploadData::Element::~Element() {
  // In the common case |file__stream_| will be null.
  if (file_stream_) {
    // Temporarily allow until fix: http://crbug.com/72001.
    base::ThreadRestrictions::ScopedAllowIO allow_io;
    file_stream_->CloseSync();
    delete file_stream_;
  }
}

void UploadData::Element::SetToChunk(const char* bytes,
                                     int bytes_len,
                                     bool is_last_chunk) {
  bytes_.clear();
  bytes_.insert(bytes_.end(), bytes, bytes + bytes_len);
  type_ = TYPE_CHUNK;
  is_last_chunk_ = is_last_chunk;
}

uint64 UploadData::Element::GetContentLength() {
  if (override_content_length_ || content_length_computed_)
    return content_length_;

  if (type_ == TYPE_BYTES || type_ == TYPE_CHUNK)
    return static_cast<uint64>(bytes_.size());
  else if (type_ == TYPE_BLOB)
    // The blob reference will be resolved later.
    return 0;

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

int UploadData::Element::ReadSync(char* buf, int buf_len) {
  if (type_ == UploadData::TYPE_BYTES || type_ == UploadData::TYPE_CHUNK) {
    return ReadFromMemorySync(buf, buf_len);
  } else if (type_ == UploadData::TYPE_FILE) {
    return ReadFromFileSync(buf, buf_len);
  }

  NOTREACHED();
  return 0;
}

uint64 UploadData::Element::BytesRemaining() {
  return GetContentLength() - offset_;
}

void UploadData::Element::ResetOffset() {
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

FileStream* UploadData::Element::OpenFileStream() {
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

int UploadData::Element::ReadFromMemorySync(char* buf, int buf_len) {
  DCHECK_LT(0, buf_len);
  DCHECK(type_ == UploadData::TYPE_BYTES || type_ == UploadData::TYPE_CHUNK);

  const size_t num_bytes_to_read = std::min(BytesRemaining(),
                                            static_cast<uint64>(buf_len));

  // Check if we have anything to copy first, because we are getting
  // the address of an element in |bytes_| and that will throw an
  // exception if |bytes_| is an empty vector.
  if (num_bytes_to_read > 0) {
    memcpy(buf, &bytes_[offset_], num_bytes_to_read);
  }

  offset_ += num_bytes_to_read;
  return num_bytes_to_read;
}

int UploadData::Element::ReadFromFileSync(char* buf, int buf_len) {
  DCHECK_LT(0, buf_len);
  DCHECK_EQ(UploadData::TYPE_FILE, type_);

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

UploadData::UploadData()
    : identifier_(0),
      chunk_callback_(NULL),
      is_chunked_(false) {
}

void UploadData::AppendBytes(const char* bytes, int bytes_len) {
  DCHECK(!is_chunked_);
  if (bytes_len > 0) {
    elements_.push_back(Element());
    elements_.back().SetToBytes(bytes, bytes_len);
  }
}

void UploadData::AppendFileRange(const FilePath& file_path,
                                 uint64 offset, uint64 length,
                                 const base::Time& expected_modification_time) {
  DCHECK(!is_chunked_);
  elements_.push_back(Element());
  elements_.back().SetToFilePathRange(file_path, offset, length,
                                      expected_modification_time);
}

void UploadData::AppendBlob(const GURL& blob_url) {
  DCHECK(!is_chunked_);
  elements_.push_back(Element());
  elements_.back().SetToBlobUrl(blob_url);
}

void UploadData::AppendChunk(const char* bytes,
                             int bytes_len,
                             bool is_last_chunk) {
  DCHECK(is_chunked_);
  elements_.push_back(Element());
  elements_.back().SetToChunk(bytes, bytes_len, is_last_chunk);
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
    if (elements_[i].type() != TYPE_BYTES)
      return false;
  }
  return true;
}

void UploadData::ResetOffset() {
  for (size_t i = 0; i < elements_.size(); ++i)
    elements_[i].ResetOffset();
}

void UploadData::SetElements(const std::vector<Element>& elements) {
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
