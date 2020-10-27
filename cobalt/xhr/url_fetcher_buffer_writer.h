// Copyright 2019 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef COBALT_XHR_URL_FETCHER_BUFFER_WRITER_H_
#define COBALT_XHR_URL_FETCHER_BUFFER_WRITER_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "base/task_runner.h"
#include "cobalt/script/array_buffer.h"
#include "net/base/io_buffer.h"
#include "net/url_request/url_fetcher_response_writer.h"

namespace cobalt {
namespace xhr {

class URLFetcherResponseWriter : public net::URLFetcherResponseWriter {
 public:
  class Buffer : public base::RefCountedThreadSafe<Buffer> {
   public:
    typedef script::PreallocatedArrayBufferData PreallocatedArrayBufferData;

    enum Type {
      kString,
      kArrayBuffer,
    };

    explicit Buffer(Type type);

    void DisablePreallocate();
    void Clear();

    int64_t GetAndResetDownloadProgress();
    bool HasProgressSinceLastGetAndReset() const;

    // When the following function is called, Write() can no longer be called to
    // append more data.  It is the responsibility of the user of this class to
    // ensure such behavior.
    const std::string& GetReferenceOfStringAndSeal();
    // Returns a reference to a std::string containing a copy of the data
    // downloaded so far.  The reference is guaranteed to be valid until another
    // public member function is called on this object.
    const std::string& GetTemporaryReferenceOfString();

    void GetAndResetDataAndDownloadProgress(std::string* str);
    void GetAndResetData(PreallocatedArrayBufferData* data);

    void MaybePreallocate(int64_t capacity);
    void Write(const void* buffer, int num_bytes);

   private:
    size_t GetSize_Locked() const;

    // It is possible (but extremely rare) that JS app changes response type
    // after some data has been written on the network thread, in such case we
    // allow to change the buffer type dynamically.
    void UpdateType_Locked(Type type);

    Type type_;
    bool allow_preallocate_ = true;
    bool capacity_known_ = false;
    size_t desired_capacity_ = 0;

    // This class can be accessed by both network and MainWebModule threads.
    mutable base::Lock lock_;

    bool allow_write_ = true;
    size_t download_progress_ = 0;

    // Data is stored in one of the following buffers, depends on the value of
    // |type_|.
    std::string data_as_string_;
    // For use in GetReferenceOfString() so it can return a reference.
    std::string copy_of_data_as_string_;
    PreallocatedArrayBufferData data_as_array_buffer_;
    size_t data_as_array_buffer_size_ = 0;
  };

  explicit URLFetcherResponseWriter(const scoped_refptr<Buffer>& buffer);
  ~URLFetcherResponseWriter() override;

  // URLFetcherResponseWriter overrides:
  int Initialize(net::CompletionOnceCallback callback) override;
  void OnResponseStarted(int64_t content_length) override;
  int Write(net::IOBuffer* buffer, int num_bytes,
            net::CompletionOnceCallback callback) override;
  int Finish(int net_error, net::CompletionOnceCallback callback) override;

 private:
  scoped_refptr<Buffer> buffer_;

  DISALLOW_COPY_AND_ASSIGN(URLFetcherResponseWriter);
};

}  // namespace xhr
}  // namespace cobalt

#endif  // COBALT_XHR_URL_FETCHER_BUFFER_WRITER_H_
