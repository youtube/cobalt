// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/upload_data.h"

#include "base/file_util.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "net/base/file_stream.h"
#include "net/base/net_errors.h"

namespace net {

UploadData::Element::Element()
    : type_(TYPE_BYTES),
      file_range_offset_(0),
      file_range_length_(kuint64max),
      is_last_chunk_(false),
      override_content_length_(false),
      content_length_computed_(false),
      content_length_(-1),
      file_stream_(NULL) {
}

UploadData::Element::~Element() {
  // In the common case |file__stream_| will be null.
  delete file_stream_;
}

void UploadData::Element::SetToChunk(const char* bytes, int bytes_len) {
  std::string chunk_length = StringPrintf("%X\r\n", bytes_len);
  bytes_.clear();
  bytes_.insert(bytes_.end(), chunk_length.data(),
                chunk_length.data() + chunk_length.length());
  bytes_.insert(bytes_.end(), bytes, bytes + bytes_len);
  const char* crlf = "\r\n";
  bytes_.insert(bytes_.end(), crlf, crlf + 2);
  type_ = TYPE_CHUNK;
  is_last_chunk_ = (bytes_len == 0);
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
  file_stream_ = NewFileStreamForReading();
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

FileStream* UploadData::Element::NewFileStreamForReading() {
  // In common usage GetContentLength() will call this first and store the
  // result into |file_| and a subsequent call (from UploadDataStream) will
  // get the cached open FileStream.
  if (file_stream_) {
    FileStream* file = file_stream_;
    file_stream_ = NULL;
    return file;
  }

  scoped_ptr<FileStream> file(new FileStream());
  int64 rv = file->Open(file_path_,
                      base::PLATFORM_FILE_OPEN | base::PLATFORM_FILE_READ);
  if (rv != OK) {
    // If the file can't be opened, we'll just upload an empty file.
    DLOG(WARNING) << "Failed to open \"" << file_path_.value()
                  << "\" for reading: " << rv;
    return NULL;
  }
  if (file_range_offset_) {
    rv = file->Seek(FROM_BEGIN, file_range_offset_);
    if (rv < 0) {
      DLOG(WARNING) << "Failed to seek \"" << file_path_.value()
                    << "\" to offset: " << file_range_offset_ << " (" << rv
                    << ")";
      return NULL;
    }
  }

  return file.release();
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

void UploadData::AppendFile(const FilePath& file_path) {
  DCHECK(!is_chunked_);
  elements_.push_back(Element());
  elements_.back().SetToFilePath(file_path);
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

void UploadData::AppendChunk(const char* bytes, int bytes_len) {
  DCHECK(is_chunked_);
  elements_.push_back(Element());
  elements_.back().SetToChunk(bytes, bytes_len);
  if (chunk_callback_)
    chunk_callback_->OnChunkAvailable();
}

void UploadData::set_chunk_callback(ChunkCallback* callback) {
  chunk_callback_ = callback;
}

uint64 UploadData::GetContentLength() {
  DCHECK(!is_chunked_);
  uint64 len = 0;
  std::vector<Element>::iterator it = elements_.begin();
  for (; it != elements_.end(); ++it)
    len += (*it).GetContentLength();
  return len;
}

void UploadData::SetElements(const std::vector<Element>& elements) {
  elements_ = elements;
}

UploadData::~UploadData() {
}

}  // namespace net
