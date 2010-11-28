// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Invalid URLs go through this URLRequestJob class rather than being passed
// to the default job handler.

#ifndef NET_URL_REQUEST_URL_REQUEST_ERROR_JOB_H_
#define NET_URL_REQUEST_URL_REQUEST_ERROR_JOB_H_
#pragma once

#include "net/url_request/url_request_job.h"

class URLRequestErrorJob : public URLRequestJob {
 public:
  URLRequestErrorJob(net::URLRequest* request, int error);

  virtual void Start();

 private:
  ~URLRequestErrorJob() {}

  void StartAsync();

  int error_;
};

#endif  // NET_URL_REQUEST_URL_REQUEST_ERROR_JOB_H_
