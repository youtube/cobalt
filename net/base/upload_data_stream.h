// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_UPLOAD_DATA_STREAM_H_
#define NET_BASE_UPLOAD_DATA_STREAM_H_

#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/weak_ptr.h"
#include "net/base/completion_callback.h"
#include "net/base/net_export.h"
#include "net/base/upload_data.h"

namespace net {

class DrainableIOBuffer;
class IOBuffer;
class UploadElementReader;

// A class to read all elements from an UploadData object.
class NET_EXPORT UploadDataStream {
 public:
  explicit UploadDataStream(UploadData* upload_data);
  ~UploadDataStream();

  // Initializes the stream. This function must be called exactly once,
  // before calling any other method. It is not valid to call any method
  // (other than the destructor) if Init() returns a failure.
  //
  // Does the initialization synchronously and returns the result if possible,
  // otherwise returns ERR_IO_PENDING and runs the callback with the result.
  //
  // Returns OK on success. Returns ERR_UPLOAD_FILE_CHANGED if the expected
  // file modification time is set (usually not set, but set for sliced
  // files) and the target file is changed.
  int Init(const CompletionCallback& callback);

  // Initializes the stream synchronously.
  // Use this method only if the thread is IO allowed or the data is in-memory.
  int InitSync();

  // When possible, reads up to |buf_len| bytes synchronously from the upload
  // data stream to |buf| and returns the number of bytes read; otherwise,
  // returns ERR_IO_PENDING and calls |callback| with the number of bytes read.
  // Partial reads are allowed. Zero is returned on a call to Read when there
  // are no remaining bytes in the stream, and IsEof() will return true
  // hereafter.
  //
  // If there's less data to read than we initially observed (i.e. the actual
  // upload data is smaller than size()), zeros are padded to ensure that
  // size() bytes can be read, which can happen for TYPE_FILE payloads.
  int Read(IOBuffer* buf, int buf_len, const CompletionCallback& callback);

  // Reads data always synchronously.
  // Use this method only if the thread is IO allowed or the data is in-memory.
  int ReadSync(IOBuffer* buf, int buf_len);

  // Returns the total size of the data stream and the current position.
  // size() is not to be used to determine whether the stream has ended
  // because it is possible for the stream to end before its size is reached,
  // for example, if the file is truncated. When the data is chunked, size()
  // always returns zero.
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

  // TODO(hashimoto): Stop directly accsssing element_readers_ from tests and
  // remove these friend declarations.
  FRIEND_TEST_ALL_PREFIXES(UploadDataStreamTest, InitAsync);
  FRIEND_TEST_ALL_PREFIXES(UploadDataStreamTest, InitAsyncFailureAsync);
  FRIEND_TEST_ALL_PREFIXES(UploadDataStreamTest, InitAsyncFailureSync);
  FRIEND_TEST_ALL_PREFIXES(UploadDataStreamTest, ReadAsync);

  // Runs Init() for all element readers.
  // This method is used to implement Init().
  void InitInternal(int start_index,
                    const CompletionCallback& callback,
                    int previous_result);

  // Finalizes the initialization process.
  // This method is used to implement Init().
  void FinalizeInitialization();

  // Reads data from the element readers.
  // This method is used to implement Read().
  int ReadInternal(scoped_refptr<DrainableIOBuffer> buf,
                   const CompletionCallback& callback);

  // Resumes pending read and calls callback with the result when necessary.
  void ResumePendingRead(scoped_refptr<DrainableIOBuffer> buf,
                         const CompletionCallback& callback,
                         int previous_result);

  // This method is called when a new chunk is available.
  void OnChunkAvailable();

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
  // |total_size_| is set to zero when the data is chunked.
  uint64 total_size_;
  uint64 current_position_;

  // True if the initialization was successful.
  bool initialized_successfully_;

  // Callback to resume reading chunked data.
  base::Closure pending_chunked_read_callback_;

  base::WeakPtrFactory<UploadDataStream> weak_ptr_factory_;

  // TODO(satish): Remove this once we have a better way to unit test POST
  // requests with chunked uploads.
  static bool merge_chunks_;

  DISALLOW_COPY_AND_ASSIGN(UploadDataStream);
};

}  // namespace net

#endif  // NET_BASE_UPLOAD_DATA_STREAM_H_
