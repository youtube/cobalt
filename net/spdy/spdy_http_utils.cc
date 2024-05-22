// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/spdy_http_utils.h"

#include <string>
#include <vector>

#include "base/strings/escape.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "net/base/load_flags.h"
#include "net/base/url_util.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_request_info.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_response_info.h"
#include "net/http/http_util.h"

namespace net {

namespace {

void AddSpdyHeader(const std::string& name,
                   const std::string& value,
                   spdy::Http2HeaderBlock* headers) {
  if (headers->find(name) == headers->end()) {
    (*headers)[name] = value;
  } else {
    (*headers)[name] = base::StrCat(
        {(*headers)[name].as_string(), base::StringPiece("\0", 1), value});
  }
}

}  // namespace

int SpdyHeadersToHttpResponse(const spdy::Http2HeaderBlock& headers,
                              HttpResponseInfo* response) {
  // The ":status" header is required.
  spdy::Http2HeaderBlock::const_iterator it =
      headers.find(spdy::kHttp2StatusHeader);
  if (it == headers.end())
    return ERR_INCOMPLETE_HTTP2_HEADERS;

  const auto status = it->second;
  std::string raw_headers =
      base::StrCat({"HTTP/1.1 ", status, base::StringPiece("\0", 1)});
  for (const auto& [name, value] : headers) {
    DCHECK_GT(name.size(), 0u);
    if (name[0] == ':') {
      // https://tools.ietf.org/html/rfc7540#section-8.1.2.4
      // Skip pseudo headers.
      continue;
    }
    // For each value, if the server sends a NUL-separated
    // list of values, we separate that back out into
    // individual headers for each value in the list.
    // e.g.
    //    Set-Cookie "foo\0bar"
    // becomes
    //    Set-Cookie: foo\0
    //    Set-Cookie: bar\0
    size_t start = 0;
    size_t end = 0;
    do {
      end = value.find('\0', start);
      base::StringPiece tval;
      if (end != value.npos)
        tval = value.substr(start, (end - start));
      else
        tval = value.substr(start);
      base::StrAppend(&raw_headers,
                      {name, ":", tval, base::StringPiece("\0", 1)});
      start = end + 1;
    } while (end != value.npos);
  }

  response->headers = base::MakeRefCounted<HttpResponseHeaders>(raw_headers);

  // When there are multiple location headers the response is a potential
  // response smuggling attack.
  if (HttpUtil::HeadersContainMultipleCopiesOfField(*response->headers,
                                                    "location")) {
    return ERR_RESPONSE_HEADERS_MULTIPLE_LOCATION;
  }

  response->was_fetched_via_spdy = true;
  return OK;
}

void CreateSpdyHeadersFromHttpRequest(const HttpRequestInfo& info,
                                      const HttpRequestHeaders& request_headers,
                                      spdy::Http2HeaderBlock* headers) {
  (*headers)[spdy::kHttp2MethodHeader] = info.method;
  if (info.method == "CONNECT") {
    (*headers)[spdy::kHttp2AuthorityHeader] = GetHostAndPort(info.url);
  } else {
    (*headers)[spdy::kHttp2AuthorityHeader] = GetHostAndOptionalPort(info.url);
    (*headers)[spdy::kHttp2SchemeHeader] = info.url.scheme();
    (*headers)[spdy::kHttp2PathHeader] = info.url.PathForRequest();
  }

  HttpRequestHeaders::Iterator it(request_headers);
  while (it.GetNext()) {
    std::string name = base::ToLowerASCII(it.name());
    if (name.empty() || name[0] == ':' || name == "connection" ||
        name == "proxy-connection" || name == "transfer-encoding" ||
        name == "host") {
      continue;
    }
    AddSpdyHeader(name, it.value(), headers);
  }
}

void CreateSpdyHeadersFromHttpRequestForWebSocket(
    const GURL& url,
    const HttpRequestHeaders& request_headers,
    spdy::Http2HeaderBlock* headers) {
  (*headers)[spdy::kHttp2MethodHeader] = "CONNECT";
  (*headers)[spdy::kHttp2AuthorityHeader] = GetHostAndOptionalPort(url);
  (*headers)[spdy::kHttp2SchemeHeader] = "https";
  (*headers)[spdy::kHttp2PathHeader] = url.PathForRequest();
  (*headers)[spdy::kHttp2ProtocolHeader] = "websocket";

  HttpRequestHeaders::Iterator it(request_headers);
  while (it.GetNext()) {
    std::string name = base::ToLowerASCII(it.name());
    if (name.empty() || name[0] == ':' || name == "upgrade" ||
        name == "connection" || name == "proxy-connection" ||
        name == "transfer-encoding" || name == "host") {
      continue;
    }
    AddSpdyHeader(name, it.value(), headers);
  }
}

static_assert(HIGHEST - LOWEST < 4 && HIGHEST - MINIMUM_PRIORITY < 6,
              "request priority incompatible with spdy");

spdy::SpdyPriority ConvertRequestPriorityToSpdyPriority(
    const RequestPriority priority) {
  DCHECK_GE(priority, MINIMUM_PRIORITY);
  DCHECK_LE(priority, MAXIMUM_PRIORITY);
  return static_cast<spdy::SpdyPriority>(MAXIMUM_PRIORITY - priority +
                                         spdy::kV3HighestPriority);
}

NET_EXPORT_PRIVATE RequestPriority
ConvertSpdyPriorityToRequestPriority(spdy::SpdyPriority priority) {
  // Handle invalid values gracefully.
  return ((priority - spdy::kV3HighestPriority) >
          (MAXIMUM_PRIORITY - MINIMUM_PRIORITY))
             ? IDLE
             : static_cast<RequestPriority>(
                   MAXIMUM_PRIORITY - (priority - spdy::kV3HighestPriority));
}

NET_EXPORT_PRIVATE void ConvertHeaderBlockToHttpRequestHeaders(
    const spdy::Http2HeaderBlock& spdy_headers,
    HttpRequestHeaders* http_headers) {
  for (const auto& it : spdy_headers) {
    base::StringPiece key = it.first;
    if (key[0] == ':') {
      key.remove_prefix(1);
    }
    std::vector<base::StringPiece> values = base::SplitStringPiece(
        it.second, "\0", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
    for (const auto& value : values) {
      http_headers->SetHeader(key, value);
    }
  }
}

}  // namespace net
