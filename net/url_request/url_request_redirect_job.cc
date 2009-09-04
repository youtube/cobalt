// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/url_request_redirect_job.h"

#include "base/message_loop.h"

URLRequestRedirectJob::URLRequestRedirectJob(URLRequest* request,
                                             GURL redirect_destination)
    : URLRequestJob(request), redirect_destination_(redirect_destination) {
}

void URLRequestRedirectJob::Start() {
  MessageLoop::current()->PostTask(FROM_HERE, NewRunnableMethod(
      this, &URLRequestRedirectJob::StartAsync));
}

void URLRequestRedirectJob::StartAsync() {
  NotifyHeadersComplete();
}

bool URLRequestRedirectJob::IsRedirectResponse(GURL* location,
                                               int* http_status_code) {
  *location = redirect_destination_;
  *http_status_code = 302;
  return true;
}

