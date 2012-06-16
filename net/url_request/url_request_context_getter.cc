// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/url_request_context_getter.h"

#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "net/url_request/url_request_context.h"

namespace net {

URLRequestContextGetter::URLRequestContextGetter() {}

URLRequestContextGetter::~URLRequestContextGetter() {}

void URLRequestContextGetter::OnDestruct() const {
  scoped_refptr<base::SingleThreadTaskRunner> network_task_runner =
      GetNetworkTaskRunner();
  DCHECK(network_task_runner);
  if (network_task_runner) {
    if (network_task_runner->BelongsToCurrentThread()) {
      delete this;
    } else {
      network_task_runner->DeleteSoon(FROM_HERE, this);
    }
  }
  // If no IO message loop proxy was available, we will just leak memory.
  // This is also true if the IO thread is gone.
}

}  // namespace net
