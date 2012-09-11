// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_UPLOAD_DATA_STREAM_H_
#define NET_BASE_UPLOAD_DATA_STREAM_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_vector.h"
#include "net/base/net_export.h"
#include "net/base/upload_data.h"

namespace net {

class IOBuffer;
class UploadElementReader;

class NET_EXPORT UploadDataStream {
 public:
  explicit UploadDataStream(UploadData* upload_data);
  ~UploadDataStream();

  // Initializes the stream. This function must be called exactly once,
  // before calling any other method. It is not valid to call any method
  // (other than the destructor) if Init() returns a failure.
  //
  // Returns OK on success. Returns ERR_UPLOAD_FILE_CHANGED if the expected
  // file modification time is set (usually not set, but set for sliced
  // files) and the target file is changed.
  int Init();

  // Reads up to |buf_len| bytes from the upload data stream to |buf|. The
  // number of bytes read is returned. Partial reads are allowed.  Zero is
  // returned on a call to Read when there are no remaining bytes in the
  // stream, and IsEof() will return true hereafter.
  //
  // If there's less data to read than we initially observed (i.e. the actual
  // upload data is smaller than size()), zeros are padded to ensure that
  // size() bytes can be read, which can happen for TYPE_FILE payloads.
  //
  // If the upload data stream is chunked (i.e. is_chunked() is true),
  // ERR_IO_PENDING is returned to indicate there is nothing to read at the
  // moment, but more data to come at a later time. If not chunked, reads
  // won't fail.
  int Read(IOBuffer* buf, int buf_len);

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

  // Returns true if all data has been consumed from this upload data
  // stream.
  bool IsEOF() const;

  // Returns true if the upload data in the stream is entirely in memory.
  bool IsInMemory() const;

 private:
  friend class SpdyHttpStreamSpdy2Test;
  friend class SpdyHttpStreamSpdy3Test;
  friend class SpdyNetworkTransactionSpdy2Test;
  friend class SpdyNetworkTransactionSpdy3Test;

  // These methods are provided only to be used by unit tests.
  static void ResetMergeChunks();
  static void set_merge_chunks(bool merge) { merge_chunks_ = merge; }

  scoped_refptr<UploadData> upload_data_;
  ScopedVector<UploadElementReader> element_readers_;

  // Index of the current upload element (i.e. the element currently being
  // read). The index is used as a cursor to iterate over elements in
  // |upload_data_|.
  size_t element_index_;

  // Size and current read position within the upload data stream.
  uint64 total_size_;
  uint64 current_position_;

  // True if the initialization was successful.
  bool initialized_successfully_;

  // TODO(satish): Remove this once we have a better way to unit test POST
  // requests with chunked uploads.
  static bool merge_chunks_;

  DISALLOW_COPY_AND_ASSIGN(UploadDataStream);
};

}  // namespace net

#endif  // NET_BASE_UPLOAD_DATA_STREAM_H_
