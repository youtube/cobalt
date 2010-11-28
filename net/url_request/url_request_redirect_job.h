// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_URL_REQUEST_URL_REQUEST_REDIRECT_JOB_H_
#define NET_URL_REQUEST_URL_REQUEST_REDIRECT_JOB_H_
#pragma once

#include "net/url_request/url_request_job.h"

class GURL;

// A URLRequestJob that will redirect the request to the specified
// URL.  This is useful to restart a request at a different URL based
// on the result of another job.
class URLRequestRedirectJob : public URLRequestJob {
 public:
  // Constructs a job that redirects to the specified URL.
  URLRequestRedirectJob(net::URLRequest* request, GURL redirect_destination);

  virtual void Start();
  bool IsRedirectResponse(GURL* location, int* http_status_code);

 private:
  ~URLRequestRedirectJob() {}

  void StartAsync();

  GURL redirect_destination_;
};

#endif  // NET_URL_REQUEST_URL_REQUEST_REDIRECT_JOB_H_
