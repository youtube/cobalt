// Copyright 2024 The Cobalt Authors. All Rights Reserved.
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
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COBALT_NETWORK_CUSTOM_URL_REQUEST_SIMPLE_JOB_H_
#define COBALT_NETWORK_CUSTOM_URL_REQUEST_SIMPLE_JOB_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_piece.h"
#include "cobalt/network/custom/url_range_request_job.h"
#include "net/base/net_export.h"
#include "starboard/types.h"

namespace base {
class RefCountedMemory;
}

namespace net {

using CompletionOnceCallback = base::OnceCallback<void(int)>;

class URLRequest;

class NET_EXPORT URLRequestSimpleJob : public URLRangeRequestJob {
 public:
  explicit URLRequestSimpleJob(URLRequest* request);

  void Start() override;
  void Kill() override;
  int ReadRawData(IOBuffer* buf, int buf_size) override;
  bool GetMimeType(std::string* mime_type) const override;
  bool GetCharset(std::string* charset) override;

 protected:
  ~URLRequestSimpleJob() override;

  // Subclasses must override either GetData or GetRefCountedData to define the
  // way response data is determined.
  // The return value should be:
  //  - OK if data is obtained;
  //  - ERR_IO_PENDING if async processing is needed to finish obtaining data.
  //    This is the only case when |callback| should be called after
  //    completion of the operation. In other situations |callback| should
  //    never be called;
  //  - any other ERR_* code to indicate an error. This code will be used
  //    as the error code in the URLRequestStatus when the URLRequest
  //    is finished.
  virtual int GetData(std::string* mime_type, std::string* charset,
                      std::string* data, CompletionOnceCallback callback) const;

  // Similar to GetData(), except |*data| can share ownership of the bytes
  // instead of copying them into a std::string.
  virtual int GetRefCountedData(std::string* mime_type, std::string* charset,
                                scoped_refptr<base::RefCountedMemory>* data,
                                CompletionOnceCallback callback) const;

  void StartAsync();

 private:
  void OnGetDataCompleted(int result);

  HttpByteRange byte_range_;
  std::string mime_type_;
  std::string charset_;
  scoped_refptr<base::RefCountedMemory> data_;
  int64_t next_data_offset_;
  base::WeakPtrFactory<URLRequestSimpleJob> weak_factory_;
};

}  // namespace net

#endif  // COBALT_NETWORK_CUSTOM_URL_REQUEST_SIMPLE_JOB_H_
