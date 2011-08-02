// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/server/http_server_request_info.h"

namespace net {

HttpServerRequestInfo::HttpServerRequestInfo() {}

HttpServerRequestInfo::~HttpServerRequestInfo() {}

std::string HttpServerRequestInfo::GetHeaderValue(
    const std::string& header_name) const {
  HttpServerRequestInfo::HeadersMap::const_iterator it =
      headers.find(header_name);
  if (it != headers.end())
    return it->second;
  return "";
}

}  // namespace net
