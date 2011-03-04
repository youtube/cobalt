// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/upload_data_stream.h"

#include "base/file_util.h"
#include "base/logging.h"
#include "net/base/file_stream.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"

namespace net {

bool UploadDataStream::merge_chunks_ = true;

UploadDataStream::~UploadDataStream() {
}

UploadDataStream* UploadDataStream::Create(UploadData* data, int* error_code) {
  scoped_ptr<UploadDataStream> stream(new UploadDataStream(data));
  int rv = stream->FillBuf();
  if (error_code)
    *error_code = rv;
  if (rv != OK)
    return NULL;

  return stream.release();
}

void UploadDataStream::MarkConsumedAndFillBuffer(size_t num_bytes) {
  DCHECK_LE(num_bytes, buf_len_);
  DCHECK(!eof_);

  if (num_bytes) {
    buf_len_ -= num_bytes;
    if (buf_len_)
      memmove(buf_->data(), buf_->data() + num_bytes, buf_len_);
  }

  FillBuf();

  current_position_ += num_bytes;
}

UploadDataStream::UploadDataStream(UploadData* data)
    : data_(data),
      buf_(new IOBuffer(kBufSize)),
      buf_len_(0),
      next_element_(0),
      next_element_offset_(0),
      next_element_remaining_(0),
      total_size_(data->is_chunked() ? 0 : data->GetContentLength()),
      current_position_(0),
      eof_(false) {
}

int UploadDataStream::FillBuf() {
  std::vector<UploadData::Element>& elements = *data_->elements();

  while (buf_len_ < kBufSize && next_element_ < elements.size()) {
    bool advance_to_next_element = false;

    UploadData::Element& element = elements[next_element_];

    size_t size_remaining = kBufSize - buf_len_;
    if (element.type() == UploadData::TYPE_BYTES ||
        element.type() == UploadData::TYPE_CHUNK) {
      const std::vector<char>& d = element.bytes();
      size_t count = d.size() - next_element_offset_;

      size_t bytes_copied = std::min(count, size_remaining);

      // Check if we have anything to copy first, because we are getting the
      // address of an element in |d| and that will throw an exception if |d|
      // is an empty vector.
      if (bytes_copied) {
        memcpy(buf_->data() + buf_len_, &d[next_element_offset_], bytes_copied);
        buf_len_ += bytes_copied;
      }

      if (bytes_copied == count) {
        advance_to_next_element = true;
      } else {
        next_element_offset_ += bytes_copied;
      }
    } else {
      DCHECK(element.type() == UploadData::TYPE_FILE);

      if (!next_element_remaining_) {
        // If the underlying file has been changed, treat it as error.
        // Note that the expected modification time from WebKit is based on
        // time_t precision. So we have to convert both to time_t to compare.
        if (!element.expected_file_modification_time().is_null()) {
          base::PlatformFileInfo info;
          if (file_util::GetFileInfo(element.file_path(), &info) &&
              element.expected_file_modification_time().ToTimeT() !=
                  info.last_modified.ToTimeT()) {
            return ERR_UPLOAD_FILE_CHANGED;
          }
        }
        next_element_remaining_ = element.GetContentLength();
        next_element_stream_.reset(element.NewFileStreamForReading());
      }

      int rv = 0;
      int count =
          static_cast<int>(std::min(next_element_remaining_,
                                    static_cast<uint64>(size_remaining)));
      if (count > 0) {
        if (next_element_stream_.get())
          rv = next_element_stream_->Read(buf_->data() + buf_len_, count, NULL);
        if (rv <= 0) {
          // If there's less data to read than we initially observed, then
          // pad with zero.  Otherwise the server will hang waiting for the
          // rest of the data.
          memset(buf_->data() + buf_len_, 0, count);
          rv = count;
        }
        buf_len_ += rv;
      }

      if (static_cast<int>(next_element_remaining_) == rv) {
        advance_to_next_element = true;
      } else {
        next_element_remaining_ -= rv;
      }
    }

    if (advance_to_next_element) {
      ++next_element_;
      next_element_offset_ = 0;
      next_element_remaining_ = 0;
      next_element_stream_.reset();
    }

    if (is_chunked() && !merge_chunks_)
      break;
  }

  if (next_element_ == elements.size() && !buf_len_) {
    if (!data_->is_chunked() ||
        (!elements.empty() && elements.back().is_last_chunk())) {
      eof_ = true;
    }
  }

  return OK;
}

bool UploadDataStream::IsOnLastChunk() const {
  const std::vector<UploadData::Element>& elements = *data_->elements();
  DCHECK(data_->is_chunked());
  return (eof_ ||
          (!elements.empty() &&
           next_element_ == elements.size() &&
           elements.back().is_last_chunk()));
}

}  // namespace net
