// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_URL_REQUEST_URL_REQUEST_SIMPLE_JOB_H_
#define NET_URL_REQUEST_URL_REQUEST_SIMPLE_JOB_H_
#pragma once

#include <string>

#include "base/task.h"
#include "net/url_request/url_request_job.h"

namespace net {

class URLRequest;

class URLRequestSimpleJob : public URLRequestJob {
 public:
  explicit URLRequestSimpleJob(URLRequest* request);

  virtual void Start();
  virtual bool ReadRawData(IOBuffer* buf, int buf_size, int *bytes_read);
  virtual bool GetMimeType(std::string* mime_type) const;
  virtual bool GetCharset(std::string* charset);

 protected:
  ~URLRequestSimpleJob();

  // subclasses must override the way response data is determined.
  virtual bool GetData(std::string* mime_type,
                       std::string* charset,
                       std::string* data) const = 0;

 protected:
  void StartAsync();

 private:
  std::string mime_type_;
  std::string charset_;
  std::string data_;
  int data_offset_;
  ScopedRunnableMethodFactory<URLRequestSimpleJob> method_factory_;
};

}  // namespace net

#endif  // NET_URL_REQUEST_URL_REQUEST_SIMPLE_JOB_H_
