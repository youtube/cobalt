// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_URL_REQUEST_URL_REQUEST_NETLOG_PARAMS_H_
#define NET_URL_REQUEST_URL_REQUEST_NETLOG_PARAMS_H_

#include <string>

#include "googleurl/src/gurl.h"
#include "net/base/net_log.h"

// Holds the parameters to emit to the NetLog when starting a URLRequest.
class URLRequestStartEventParameters : public net::NetLog::EventParameters {
 public:
  URLRequestStartEventParameters(const GURL& url,
                                 const std::string& method,
                                 int load_flags);

  const GURL& url() const {
    return url_;
  }

  virtual Value* ToValue() const;

 private:
  const GURL url_;
  const std::string method_;
  const int load_flags_;
};

#endif  // NET_URL_REQUEST_URL_REQUEST_NETLOG_PARAMS_H_
