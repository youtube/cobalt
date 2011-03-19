// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_URL_REQUEST_URL_REQUEST_ABOUT_JOB_H_
#define NET_URL_REQUEST_URL_REQUEST_ABOUT_JOB_H_
#pragma once

#include <string>

#include "base/task.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_job.h"

namespace net {

class URLRequestAboutJob : public URLRequestJob {
 public:
  explicit URLRequestAboutJob(URLRequest* request);

  static URLRequest::ProtocolFactory Factory;

  // URLRequestJob:
  virtual void Start();
  virtual bool GetMimeType(std::string* mime_type) const;

 private:
  virtual ~URLRequestAboutJob();

  void StartAsync();

  ScopedRunnableMethodFactory<URLRequestAboutJob> method_factory_;
};

}  // namespace net

#endif  // NET_URL_REQUEST_URL_REQUEST_ABOUT_JOB_H_
