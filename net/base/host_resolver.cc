// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/host_resolver.h"

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "net/base/net_errors.h"

namespace net {

SingleRequestHostResolver::SingleRequestHostResolver(HostResolver* resolver)
    : resolver_(resolver),
      cur_request_(NULL),
      cur_request_callback_(NULL),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          callback_(this, &SingleRequestHostResolver::OnResolveCompletion)) {
  DCHECK(resolver_ != NULL);
}

SingleRequestHostResolver::~SingleRequestHostResolver() {
  Cancel();
}

int SingleRequestHostResolver::Resolve(const HostResolver::RequestInfo& info,
                                       AddressList* addresses,
                                       CompletionCallback* callback,
                                       LoadLog* load_log) {
  DCHECK(!cur_request_ && !cur_request_callback_) << "resolver already in use";

  HostResolver::RequestHandle request = NULL;

  // We need to be notified of completion before |callback| is called, so that
  // we can clear out |cur_request_*|.
  CompletionCallback* transient_callback = callback ? &callback_ : NULL;

  int rv = resolver_->Resolve(
      info, addresses, transient_callback, &request, load_log);

  if (rv == ERR_IO_PENDING) {
    // Cleared in OnResolveCompletion().
    cur_request_ = request;
    cur_request_callback_ = callback;
  }

  return rv;
}

void SingleRequestHostResolver::Cancel() {
  if (cur_request_) {
    resolver_->CancelRequest(cur_request_);
    cur_request_ = NULL;
  }
}

void SingleRequestHostResolver::OnResolveCompletion(int result) {
  DCHECK(cur_request_ && cur_request_callback_);

  CompletionCallback* callback = cur_request_callback_;

  // Clear the outstanding request information.
  cur_request_ = NULL;
  cur_request_callback_ = NULL;

  // Call the user's original callback.
  callback->Run(result);
}

}  // namespace net
