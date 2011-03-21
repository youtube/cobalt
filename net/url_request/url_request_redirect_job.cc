// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/url_request_redirect_job.h"

#include "base/compiler_specific.h"
#include "base/message_loop.h"

namespace net {

URLRequestRedirectJob::URLRequestRedirectJob(URLRequest* request,
                                             const GURL& redirect_destination)
    : URLRequestJob(request),
      redirect_destination_(redirect_destination),
      ALLOW_THIS_IN_INITIALIZER_LIST(method_factory_(this)) {}

void URLRequestRedirectJob::Start() {
  MessageLoop::current()->PostTask(
      FROM_HERE,
      method_factory_.NewRunnableMethod(&URLRequestRedirectJob::StartAsync));
}

bool URLRequestRedirectJob::IsRedirectResponse(GURL* location,
                                               int* http_status_code) {
  *location = redirect_destination_;
  *http_status_code = 302;
  return true;
}

URLRequestRedirectJob::~URLRequestRedirectJob() {}

void URLRequestRedirectJob::StartAsync() {
  NotifyHeadersComplete();
}

}  // namespace net
