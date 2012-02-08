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

UploadDataStream::UploadDataStream(UploadData* upload_data)
    : upload_data_(upload_data),
      element_index_(0),
      element_offset_(0),
      element_file_bytes_remaining_(0),
      total_size_(0),
      current_position_(0),
      initialized_successfully_(false) {
}

UploadDataStream::~UploadDataStream() {
}

int UploadDataStream::Init() {
  DCHECK(!initialized_successfully_);

  total_size_ = upload_data_->GetContentLength();

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

  initialized_successfully_ = true;
  return OK;
}

int UploadDataStream::Read(IOBuffer* buf, int buf_len) {
  std::vector<UploadData::Element>& elements = *upload_data_->elements();

  int bytes_copied = 0;
  while (bytes_copied < buf_len && element_index_ < elements.size()) {
    bool advance_to_next_element = false;

    // This is not const as GetContentLength() is not const.
    UploadData::Element& element = elements[element_index_];

    const size_t free_buffer_space = buf_len - bytes_copied;
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
        memcpy(buf->data() + bytes_copied, &element_data[element_offset_],
               num_bytes_to_copy);
        bytes_copied += num_bytes_to_copy;
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
              element_file_stream_->Read(buf->data() + bytes_copied,
                                         num_bytes_to_read,
                                         CompletionCallback());
        }
        if (num_bytes_consumed <= 0) {
          // If there's less data to read than we initially observed, then
          // pad with zero.  Otherwise the server will hang waiting for the
          // rest of the data.
          memset(buf->data() + bytes_copied, 0, num_bytes_to_read);
          num_bytes_consumed = num_bytes_to_read;
        }
        bytes_copied += num_bytes_consumed;
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

  current_position_ += bytes_copied;
  if (is_chunked() && !IsEOF() && bytes_copied == 0)
    return ERR_IO_PENDING;

  return bytes_copied;
}

void UploadDataStream::AdvanceToNextElement() {
  ++element_index_;
  element_offset_ = 0;
  element_file_bytes_remaining_ = 0;
  if (element_file_stream_.get()) {
    // Temporarily allow until fix: http://crbug.com/72001.
    base::ThreadRestrictions::ScopedAllowIO allow_io;
    element_file_stream_->CloseSync();
    element_file_stream_.reset();
  }
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
