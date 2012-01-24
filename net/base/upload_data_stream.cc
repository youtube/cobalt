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

const size_t UploadDataStream::kBufferSize = 16384;
bool UploadDataStream::merge_chunks_ = true;

UploadDataStream::~UploadDataStream() {
}

UploadDataStream* UploadDataStream::Create(UploadData* data, int* error_code) {
  scoped_ptr<UploadDataStream> stream(new UploadDataStream(data));
  int rv = stream->FillBuffer();
  if (error_code)
    *error_code = rv;
  if (rv != OK)
    return NULL;

  return stream.release();
}

// static
size_t UploadDataStream::GetBufferSize() {
  return kBufferSize;
}

void UploadDataStream::MarkConsumedAndFillBuffer(size_t num_bytes) {
  DCHECK_LE(num_bytes, buf_len_);
  DCHECK(!eof_);

  if (num_bytes) {
    buf_len_ -= num_bytes;
    // Move the remaining data to the beginning.
    if (buf_len_ > 0)
      memmove(buf_->data(), buf_->data() + num_bytes, buf_len_);
  }

  FillBuffer();

  current_position_ += num_bytes;
}

UploadDataStream::UploadDataStream(UploadData* upload_data)
    : upload_data_(upload_data),
      buf_(new IOBuffer(kBufferSize)),
      buf_len_(0),
      element_index_(0),
      element_offset_(0),
      element_file_bytes_remaining_(0),
      total_size_(upload_data->GetContentLength()),
      current_position_(0),
      eof_(false) {
}

int UploadDataStream::FillBuffer() {
  std::vector<UploadData::Element>& elements = *upload_data_->elements();

  while (buf_len_ < kBufferSize && element_index_ < elements.size()) {
    bool advance_to_next_element = false;

    // This is not const as GetContentLength() is not const.
    UploadData::Element& element = elements[element_index_];

    const size_t free_buffer_space = kBufferSize - buf_len_;
    if (element.type() == UploadData::TYPE_BYTES ||
        element.type() == UploadData::TYPE_CHUNK) {
      const std::vector<char>& element_data = element.bytes();
      const size_t num_bytes_left_in_element =
          element_data.size() - element_offset_;

      const size_t num_bytes_to_copy = std::min(num_bytes_left_in_element,
                                                free_buffer_space);

      // Check if we have anything to copy first, because we are getting
      // the address of an element in |element_data| and that will throw an
      // exception if |element_data| is an empty vector.
      if (num_bytes_to_copy > 0) {
        memcpy(buf_->data() + buf_len_,
               &element_data[element_offset_],
               num_bytes_to_copy);
        buf_len_ += num_bytes_to_copy;
        element_offset_ += num_bytes_to_copy;
      }

      // Advance to the next element if we have consumed all data in the
      // current element.
      if (element_offset_ == element_data.size())
        advance_to_next_element = true;
    } else {
      DCHECK(element.type() == UploadData::TYPE_FILE);

      // Open the file of the current element if not yet opened.
      if (!element_file_stream_.get()) {
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
        element_file_bytes_remaining_ = element.GetContentLength();
        element_file_stream_.reset(element.NewFileStreamForReading());
      }

      const int num_bytes_to_read =
          static_cast<int>(std::min(element_file_bytes_remaining_,
                                    static_cast<uint64>(free_buffer_space)));
      if (num_bytes_to_read > 0) {
        int num_bytes_consumed = 0;
        // Temporarily allow until fix: http://crbug.com/72001.
        base::ThreadRestrictions::ScopedAllowIO allow_io;
        // element_file_stream_.get() is NULL if the target file is
        // missing or not readable.
        if (element_file_stream_.get()) {
          num_bytes_consumed =
              element_file_stream_->Read(buf_->data() + buf_len_,
                                         num_bytes_to_read,
                                         CompletionCallback());
        }
        if (num_bytes_consumed <= 0) {
          // If there's less data to read than we initially observed, then
          // pad with zero.  Otherwise the server will hang waiting for the
          // rest of the data.
          memset(buf_->data() + buf_len_, 0, num_bytes_to_read);
          num_bytes_consumed = num_bytes_to_read;
        }
        buf_len_ += num_bytes_consumed;
        element_file_bytes_remaining_ -= num_bytes_consumed;
      }

      // Advance to the next element if we have consumed all data in the
      // current element.
      if (element_file_bytes_remaining_ == 0)
        advance_to_next_element = true;
    }

    if (advance_to_next_element)
      AdvanceToNextElement();

    if (is_chunked() && !merge_chunks_)
      break;
  }

  eof_ = IsEOF();

  return OK;
}

void UploadDataStream::AdvanceToNextElement() {
  ++element_index_;
  element_offset_ = 0;
  element_file_bytes_remaining_ = 0;
  element_file_stream_.reset();
}

bool UploadDataStream::IsEOF() const {
  const std::vector<UploadData::Element>& elements = *upload_data_->elements();

  // Check if all elements are consumed and the buffer is empty.
  if (element_index_ == elements.size() && !buf_len_) {
    // If the upload data is chunked, check if the last element is the
    // last chunk.
    if (!upload_data_->is_chunked() ||
        (!elements.empty() && elements.back().is_last_chunk())) {
      return true;
    }
  }
  return false;
}

bool UploadDataStream::IsOnLastChunk() const {
  const std::vector<UploadData::Element>& elements = *upload_data_->elements();
  DCHECK(upload_data_->is_chunked());
  return (eof_ ||
          (!elements.empty() &&
           element_index_ == elements.size() &&
           elements.back().is_last_chunk()));
}

bool UploadDataStream::IsInMemory() const {
  return upload_data_->IsInMemory();
}

}  // namespace net
