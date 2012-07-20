// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/url_request_redirect_job.h"

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/message_loop.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"

namespace net {

URLRequestRedirectJob::URLRequestRedirectJob(URLRequest* request,
                                             const GURL& redirect_destination)
    : URLRequestJob(request, request->context()->network_delegate()),
      redirect_destination_(redirect_destination),
      http_status_code_(302),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)) {}

void URLRequestRedirectJob::Start() {
  MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&URLRequestRedirectJob::StartAsync,
                 weak_factory_.GetWeakPtr()));
}

bool URLRequestRedirectJob::IsRedirectResponse(GURL* location,
                                               int* http_status_code) {
  *location = redirect_destination_;
  *http_status_code = http_status_code_;
  return true;
}

URLRequestRedirectJob::~URLRequestRedirectJob() {}

void URLRequestRedirectJob::StartAsync() {
  NotifyHeadersComplete();
}

}  // namespace net
