// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_UPLOAD_DATA_STREAM_H_
#define NET_BASE_UPLOAD_DATA_STREAM_H_
#pragma once

#include "base/memory/scoped_ptr.h"
#include "net/base/net_export.h"
#include "net/base/upload_data.h"

namespace net {

class FileStream;
class IOBuffer;

class NET_EXPORT UploadDataStream {
 public:
  ~UploadDataStream();

  // Returns a new instance of UploadDataStream if it can be created and
  // initialized successfully. If not, NULL will be returned and the error
  // code will be set if the output parameter error_code is not empty.
  static UploadDataStream* Create(UploadData* upload_data, int* error_code);

  // Returns the stream's buffer.
  IOBuffer* buf() const { return buf_; }
  // Returns the length of the data in the stream's buffer.
  size_t buf_len() const { return buf_len_; }

  // TODO(satish): We should ideally have UploadDataStream expose a Read()
  // method which returns data in a caller provided IOBuffer. That would do away
  // with this function and make the interface cleaner as well with less memmove
  // calls.
  //
  // Returns the size of the stream's buffer pointed by buf().
  static size_t GetBufferSize();

  // Call to indicate that a portion of the stream's buffer was consumed.  This
  // call modifies the stream's buffer so that it contains the next segment of
  // the upload data to be consumed.
  void MarkConsumedAndFillBuffer(size_t num_bytes);

  // Sets the callback to be invoked when new chunks are available to upload.
  void set_chunk_callback(ChunkCallback* callback) {
    upload_data_->set_chunk_callback(callback);
  }

  // Returns the total size of the data stream and the current position.
  // size() is not to be used to determine whether the stream has ended
  // because it is possible for the stream to end before its size is reached,
  // for example, if the file is truncated.
  uint64 size() const { return total_size_; }
  uint64 position() const { return current_position_; }

  bool is_chunked() const { return upload_data_->is_chunked(); }

  // Returns whether there is no more data to read, regardless of whether
  // position < size.
  bool eof() const { return eof_; }

  // Returns whether the data available in buf() includes the last chunk in a
  // chunked data stream. This method returns true once the final chunk has been
  // placed in the IOBuffer returned by buf(), in contrast to eof() which
  // returns true only after the data in buf() has been consumed.
  bool IsOnLastChunk() const;

  // Returns true if the upload data in the stream is entirely in memory.
  bool IsInMemory() const;

  // This method is provided only to be used by unit tests.
  static void set_merge_chunks(bool merge) { merge_chunks_ = merge; }

 private:
  // Protects from public access since now we have a static creator function
  // which will do both creation and initialization and might return an error.
  explicit UploadDataStream(UploadData* upload_data);

  // Fills the buffer with any remaining data and sets eof_ if there was nothing
  // left to fill the buffer with.
  // Returns OK if the operation succeeds. Otherwise error code is returned.
  int FillBuffer();

  // Advances to the next element. Updates the internal states.
  void AdvanceToNextElement();

  // Returns true if all data has been consumed from this upload data
  // stream.
  bool IsEOF() const;

  scoped_refptr<UploadData> upload_data_;

  // This buffer is filled with data to be uploaded.  The data to be sent is
  // always at the front of the buffer.  If we cannot send all of the buffer at
  // once, then we memmove the remaining portion and back-fill the buffer for
  // the next "write" call.  buf_len_ indicates how much data is in the buffer.
  scoped_refptr<IOBuffer> buf_;
  size_t buf_len_;

  // Index of the current upload element (i.e. the element currently being
  // read). The index is used as a cursor to iterate over elements in
  // |upload_data_|.
  size_t element_index_;

  // The byte offset into the current element's data buffer if the current
  // element is a TYPE_BYTES or TYPE_DATA element.
  size_t element_offset_;

  // A stream to the currently open file, for the current element if the
  // current element is a TYPE_FILE element.
  scoped_ptr<FileStream> element_file_stream_;

  // The number of bytes remaining to be read from the currently open file
  // if the current element is of TYPE_FILE.
  uint64 element_file_bytes_remaining_;

  // Size and current read position within the upload data stream.
  uint64 total_size_;
  uint64 current_position_;

  // Whether there is no data left to read.
  bool eof_;

  // TODO(satish): Remove this once we have a better way to unit test POST
  // requests with chunked uploads.
  static bool merge_chunks_;
  // The size of the stream's buffer pointed by buf_.
  static const size_t kBufferSize;

  DISALLOW_COPY_AND_ASSIGN(UploadDataStream);
};

}  // namespace net

#endif  // NET_BASE_UPLOAD_DATA_STREAM_H_
