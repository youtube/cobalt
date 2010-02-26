// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/upload_data_stream.h"

#include "base/logging.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"

namespace net {

UploadDataStream::UploadDataStream(UploadData* data)
    : data_(data),
      buf_(new IOBuffer(kBufSize)),
      buf_len_(0),
      next_element_(data->elements().begin()),
      next_element_offset_(0),
      next_element_remaining_(0),
      total_size_(data->GetContentLength()),
      current_position_(0),
      eof_(false) {
  FillBuf();
}

UploadDataStream::~UploadDataStream() {
}

void UploadDataStream::DidConsume(size_t num_bytes) {
  // TODO(vandebo): Change back to a DCHECK when issue 27870 is resolved.
  CHECK(num_bytes <= buf_len_);
  DCHECK(!eof_);

  buf_len_ -= num_bytes;
  if (buf_len_)
    memmove(buf_->data(), buf_->data() + num_bytes, buf_len_);

  FillBuf();

  current_position_ += num_bytes;
}

void UploadDataStream::FillBuf() {
  std::vector<UploadData::Element>::const_iterator end =
      data_->elements().end();

  while (buf_len_ < kBufSize && next_element_ != end) {
    bool advance_to_next_element = false;

    const UploadData::Element& element = *next_element_;

    size_t size_remaining = kBufSize - buf_len_;
    if (element.type() == UploadData::TYPE_BYTES) {
      const std::vector<char>& d = element.bytes();
      size_t count = d.size() - next_element_offset_;

      size_t bytes_copied = std::min(count, size_remaining);

      memcpy(buf_->data() + buf_len_, &d[next_element_offset_], bytes_copied);
      buf_len_ += bytes_copied;

      if (bytes_copied == count) {
        advance_to_next_element = true;
      } else {
        next_element_offset_ += bytes_copied;
      }
    } else {
      DCHECK(element.type() == UploadData::TYPE_FILE);

      if (element.file_range_length() == 0) {
        // If we failed to open the file, then the length is set to zero. The
        // length used when calculating the POST size was also zero. This
        // matches the behaviour of Mozilla.
        next_element_remaining_ = 0;
      } else {
        if (!next_element_stream_.IsOpen()) {
          // We ignore the return value of Open becuase we've already checked
          // !IsOpen, above.
          int flags = base::PLATFORM_FILE_READ |
                      base::PLATFORM_FILE_WRITE;
          next_element_stream_.Open(element.platform_file(), flags);

          uint64 offset = element.file_range_offset();
          if (offset && next_element_stream_.Seek(FROM_BEGIN, offset) < 0) {
            DLOG(WARNING) << "Failed to seek \"" << element.file_path().value()
                          << "\" to offset: " << offset;
          } else {
            next_element_remaining_ = element.file_range_length();
          }
        }
      }

      int rv = 0;
      if (next_element_remaining_ > 0) {
        int count =
            static_cast<int>(std::min(next_element_remaining_,
                                      static_cast<uint64>(size_remaining)));
        rv = next_element_stream_.Read(buf_->data() + buf_len_, count, NULL);
        if (rv < 1) {
          // If the file was truncated between the time that we opened it and
          // now, or if we got an error on reading, then we pad with NULs.
          memset(buf_->data() + buf_len_, 0, count);
          rv = count;
        }

        buf_len_ += rv;
        next_element_remaining_ -= rv;
      } else {
        advance_to_next_element = true;
      }
    }

    if (advance_to_next_element) {
      ++next_element_;
      next_element_offset_ = 0;
      next_element_stream_.Release();
    }
  }

  if (next_element_ == end && !buf_len_) {
    eof_ = true;
    data_->CloseFiles();
  }
}

}  // namespace net
