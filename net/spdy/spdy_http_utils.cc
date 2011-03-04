// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/spdy_http_utils.h"

#include <string>

#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/time.h"
#include "net/base/load_flags.h"
#include "net/base/net_util.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_request_info.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_response_info.h"
#include "net/http/http_util.h"

namespace net {

bool SpdyHeadersToHttpResponse(const spdy::SpdyHeaderBlock& headers,
                               HttpResponseInfo* response) {
  std::string version;
  std::string status;

  // The "status" and "version" headers are required.
  spdy::SpdyHeaderBlock::const_iterator it;
  it = headers.find("status");
  if (it == headers.end())
    return false;
  status = it->second;

  // Grab the version.  If not provided by the server,
  it = headers.find("version");
  if (it == headers.end())
    return false;
  version = it->second;

  response->response_time = base::Time::Now();

  std::string raw_headers(version);
  raw_headers.push_back(' ');
  raw_headers.append(status);
  raw_headers.push_back('\0');
  for (it = headers.begin(); it != headers.end(); ++it) {
    // For each value, if the server sends a NUL-separated
    // list of values, we separate that back out into
    // individual headers for each value in the list.
    // e.g.
    //    Set-Cookie "foo\0bar"
    // becomes
    //    Set-Cookie: foo\0
    //    Set-Cookie: bar\0
    std::string value = it->second;
    size_t start = 0;
    size_t end = 0;
    do {
      end = value.find('\0', start);
      std::string tval;
      if (end != value.npos)
        tval = value.substr(start, (end - start));
      else
        tval = value.substr(start);
      raw_headers.append(it->first);
      raw_headers.push_back(':');
      raw_headers.append(tval);
      raw_headers.push_back('\0');
      start = end + 1;
    } while (end != value.npos);
  }

  response->headers = new HttpResponseHeaders(raw_headers);
  response->was_fetched_via_spdy = true;
  return true;
}

void CreateSpdyHeadersFromHttpRequest(const HttpRequestInfo& info,
                                      const HttpRequestHeaders& request_headers,
                                      spdy::SpdyHeaderBlock* headers,
                                      bool direct) {

  HttpRequestHeaders::Iterator it(request_headers);
  while (it.GetNext()) {
    std::string name = StringToLowerASCII(it.name());
    if (name == "connection" || name == "proxy-connection" ||
        name == "transfer-encoding") {
      continue;
    }
    if (headers->find(name) == headers->end()) {
      (*headers)[name] = it.value();
    } else {
      std::string new_value = (*headers)[name];
      new_value.append(1, '\0');  // +=() doesn't append 0's
      new_value += it.value();
      (*headers)[name] = new_value;
    }
  }
  static const char kHttpProtocolVersion[] = "HTTP/1.1";

  (*headers)["version"] = kHttpProtocolVersion;
  (*headers)["method"] = info.method;
  (*headers)["host"] = GetHostAndOptionalPort(info.url);
  (*headers)["scheme"] = info.url.scheme();
  if (direct)
    (*headers)["url"] = HttpUtil::PathForRequest(info.url);
  else
    (*headers)["url"] = HttpUtil::SpecForRequest(info.url);

}

// TODO(gavinp): re-adjust this once SPDY v3 has three priority bits,
// eliminating the need for this folding.
int ConvertRequestPriorityToSpdyPriority(const RequestPriority priority) {
  DCHECK(HIGHEST <= priority && priority < NUM_PRIORITIES);
  switch (priority) {
    case LOWEST:
      return SPDY_PRIORITY_LOWEST - 1;
    case IDLE:
      return SPDY_PRIORITY_LOWEST;
    default:
      return priority;
  }
}

}  // namespace net
