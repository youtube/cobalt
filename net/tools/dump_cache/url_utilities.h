// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TOOLS_DUMP_CACHE_URL_UTILITIES_H_
#define NET_TOOLS_DUMP_CACHE_URL_UTILITIES_H_

#include <string>

namespace net {

namespace UrlUtilities {

// Gets the host from an url, strips the port number as well if the url
// has one.
// For example: calling GetUrlHost(www.foo.com:8080/boo) returns www.foo.com
static std::string GetUrlHost(const std::string& url) {
  size_t b = url.find("//");
  if (b == std::string::npos)
    b = 0;
  else
    b += 2;
  size_t next_slash = url.find_first_of('/', b);
  size_t next_colon = url.find_first_of(':', b);
  if (next_slash != std::string::npos
      && next_colon != std::string::npos
      && next_colon < next_slash) {
    return std::string(url, b, next_colon - b);
  }
  if (next_slash == std::string::npos) {
    if (next_colon != std::string::npos) {
      return std::string(url, next_colon - b);
    } else {
      next_slash = url.size();
    }
  }
  return std::string(url, b, next_slash - b);
}

// Gets the path portion of an url.
// e.g   http://www.foo.com/path
//       returns /path
static std::string GetUrlPath(const std::string& url) {
  size_t b = url.find("//");
  if (b == std::string::npos)
    b = 0;
  else
    b += 2;
  b = url.find("/", b);
  if (b == std::string::npos)
    return "/";

  size_t e = url.find("#", b+1);
  if (e != std::string::npos)
    return std::string(url, b, (e - b));
  return std::string(url, b);
}

}  // namespace UrlUtilities

}  // namespace net

#endif  // NET_TOOLS_DUMP_CACHE_URL_UTILITIES_H_

