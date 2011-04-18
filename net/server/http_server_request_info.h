// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SERVER_HTTP_SERVER_REQUEST_INFO_H_
#define NET_SERVER_HTTP_SERVER_REQUEST_INFO_H_
#pragma once

#include <map>
#include <string>

namespace net {

// Meta information about an HTTP request.
// This is geared toward servers in that it keeps a map of the headers and
// values rather than just a list of header strings (which net::HttpRequestInfo
// does).
class HttpServerRequestInfo {
 public:
  HttpServerRequestInfo();
  ~HttpServerRequestInfo();

  // Request method.
  std::string method;

  // Request line.
  std::string path;

  // Request data.
  std::string data;

  // A map of the names -> values for HTTP headers.
  typedef std::map<std::string, std::string> HeadersMap;
  mutable HeadersMap headers;
};

}  // namespace net

#endif  // NET_SERVER_HTTP_SERVER_REQUEST_INFO_H_
