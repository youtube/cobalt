// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_URL_REQUEST_URL_REQUEST_DATA_JOB_H_
#define NET_URL_REQUEST_URL_REQUEST_DATA_JOB_H_
#pragma once

#include <string>

#include "net/url_request/url_request.h"
#include "net/url_request/url_request_simple_job.h"

namespace net {

class URLRequest;

class URLRequestDataJob : public URLRequestSimpleJob {
 public:
  explicit URLRequestDataJob(URLRequest* request);

  static URLRequest::ProtocolFactory Factory;

  // URLRequestSimpleJob
  virtual bool GetData(std::string* mime_type,
                       std::string* charset,
                       std::string* data) const;

 private:
  ~URLRequestDataJob();

  DISALLOW_COPY_AND_ASSIGN(URLRequestDataJob);
};

}  // namespace net

#endif  // NET_URL_REQUEST_URL_REQUEST_DATA_JOB_H_
