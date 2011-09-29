// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_URL_REQUEST_URL_REQUEST_REDIRECT_JOB_H_
#define NET_URL_REQUEST_URL_REQUEST_REDIRECT_JOB_H_
#pragma once

#include "base/task.h"
#include "net/base/net_export.h"
#include "net/url_request/url_request_job.h"

class GURL;

namespace net {

// A URLRequestJob that will redirect the request to the specified
// URL.  This is useful to restart a request at a different URL based
// on the result of another job.
class NET_EXPORT URLRequestRedirectJob : public URLRequestJob {
 public:
  // Valid status codes for the redirect job. Other 30x codes are theoretically
  // valid, but unused so far.
  enum StatusCode {
    REDIRECT_302_FOUND = 302,
    REDIRECT_307_TEMPORARY_REDIRECT = 307,
  };

  // Constructs a job that redirects to the specified URL.
  URLRequestRedirectJob(URLRequest* request, const GURL& redirect_destination);

  // Change the HTTP status code to use for the redirect. Default is
  // REDIRECT_302_FOUND.
  void set_redirect_code(StatusCode code) {
    http_status_code_ = static_cast<int>(code);
  }

  virtual void Start();
  virtual bool IsRedirectResponse(GURL* location, int* http_status_code);

 private:
  virtual ~URLRequestRedirectJob();

  void StartAsync();

  GURL redirect_destination_;
  int http_status_code_;

  ScopedRunnableMethodFactory<URLRequestRedirectJob> method_factory_;
};

}  // namespace net

#endif  // NET_URL_REQUEST_URL_REQUEST_REDIRECT_JOB_H_
