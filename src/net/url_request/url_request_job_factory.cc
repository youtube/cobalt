// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/url_request_job_factory.h"

namespace net {

URLRequestJobFactory::ProtocolHandler::~ProtocolHandler() {}

URLRequestJobFactory::Interceptor::~Interceptor() {}

bool URLRequestJobFactory::Interceptor::WillHandleProtocol(
    const std::string& protocol) const {
  return false;
}

URLRequestJobFactory::URLRequestJobFactory() {}

URLRequestJobFactory::~URLRequestJobFactory() {}

}  // namespace net
