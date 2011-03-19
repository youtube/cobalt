// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Simple implementation of about: protocol handler that treats everything as
// about:blank.  No other about: features should be available to web content,
// so they're not implemented here.

#include "net/url_request/url_request_about_job.h"

#include "base/compiler_specific.h"
#include "base/message_loop.h"

namespace net {

URLRequestAboutJob::URLRequestAboutJob(URLRequest* request)
    : URLRequestJob(request),
      ALLOW_THIS_IN_INITIALIZER_LIST(method_factory_(this)) {
}

// static
URLRequestJob* URLRequestAboutJob::Factory(URLRequest* request,
                                           const std::string& scheme) {
  return new URLRequestAboutJob(request);
}

void URLRequestAboutJob::Start() {
  // Start reading asynchronously so that all error reporting and data
  // callbacks happen as they would for network requests.
  MessageLoop::current()->PostTask(
      FROM_HERE,
      method_factory_.NewRunnableMethod(&URLRequestAboutJob::StartAsync));
}

bool URLRequestAboutJob::GetMimeType(std::string* mime_type) const {
  *mime_type = "text/html";
  return true;
}

URLRequestAboutJob::~URLRequestAboutJob() {
}

void URLRequestAboutJob::StartAsync() {
  NotifyHeadersComplete();
}

}  // namespace net
