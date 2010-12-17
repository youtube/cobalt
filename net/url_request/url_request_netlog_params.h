// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_URL_REQUEST_URL_REQUEST_NETLOG_PARAMS_H_
#define NET_URL_REQUEST_URL_REQUEST_NETLOG_PARAMS_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "googleurl/src/gurl.h"
#include "net/base/net_log.h"
#include "net/base/request_priority.h"

namespace net {

// Holds the parameters to emit to the NetLog when starting a URLRequest.
class URLRequestStartEventParameters : public NetLog::EventParameters {
 public:
  URLRequestStartEventParameters(const GURL& url,
                                 const std::string& method,
                                 int load_flags,
                                 RequestPriority priority);

  const GURL& url() const {
    return url_;
  }

  int load_flags() const {
    return load_flags_;
  }

  virtual Value* ToValue() const;

 private:
  const GURL url_;
  const std::string method_;
  const int load_flags_;
  const RequestPriority priority_;

  DISALLOW_COPY_AND_ASSIGN(URLRequestStartEventParameters);
};

}  // namespace net

#endif  // NET_URL_REQUEST_URL_REQUEST_NETLOG_PARAMS_H_
