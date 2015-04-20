// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Copyright (c) 2013 Google Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/shell/http_stream_shell_loader.h"

#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/http/http_request_info.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_response_info.h"
#include "net/http/http_stream_parser.h"
#include "net/http/http_util.h"

// These are a few helper functions used by sub classes implemented for
// different platforms. The original code of these functions below are from
// chromium/net/http/http_stream_parser.cc.

namespace net {

/* static */
std::string HttpStreamShellLoader::GetResponseHeaderLines(
    const net::HttpResponseHeaders& headers) {
  std::string raw_headers = headers.raw_headers();
  const char* null_separated_headers = raw_headers.c_str();
  const char* header_line = null_separated_headers;
  std::string cr_separated_headers;
  while (header_line[0] != 0) {
    cr_separated_headers += header_line;
    cr_separated_headers += "\n";
    header_line += strlen(header_line) + 1;
  }
  return cr_separated_headers;
}

// Return true if |headers| contain multiple |field_name| fields with different
// values.
/* static */
bool HttpStreamShellLoader::HeadersContainMultipleCopiesOfField(
    const net::HttpResponseHeaders& headers,
    const std::string& field_name) {
  void* it = NULL;
  std::string field_value;
  if (!headers.EnumerateHeader(&it, field_name, &field_value))
    return false;
  // There's at least one |field_name| header.  Check if there are any more
  // such headers, and if so, return true if they have different values.
  std::string field_value2;
  while (headers.EnumerateHeader(&it, field_name, &field_value2)) {
    if (field_value != field_value2)
      return true;
  }
  return false;
}

// Parser response headers.
int HttpStreamShellLoader::ParseResponseHeaders(const char* buf,
    int buf_len, const HttpRequestInfo* request, HttpResponseInfo* response) {
  // Do the parse
  std::string raw_headers = HttpUtil::AssembleRawHeaders(buf, buf_len);
  response->headers = new HttpResponseHeaders(raw_headers);
  // Headers are parsed, validate a few special cases
  DCHECK(response->headers);
  // Check for multiple Content-Length headers with no Transfer-Encoding
  // header. If they exist, and have distinct values, it's a potential
  // response smuggling attack.
  if (!response->headers->HasHeader("Transfer-Encoding")) {
    if (HeadersContainMultipleCopiesOfField(*response->headers,
                                            "Content-Length"))
      return ERR_RESPONSE_HEADERS_MULTIPLE_CONTENT_LENGTH;
  }

  // Check for multiple Content-Disposition or Location headers. If they
  // exist, it's also a potential response smuggling attack.
  if (HeadersContainMultipleCopiesOfField(*response->headers,
                                          "Content-Disposition"))
    return ERR_RESPONSE_HEADERS_MULTIPLE_CONTENT_DISPOSITION;
  if (HeadersContainMultipleCopiesOfField(*response->headers, "Location"))
    return ERR_RESPONSE_HEADERS_MULTIPLE_LOCATION;

  response->vary_data.Init(*request, *response->headers);
  DVLOG(1) << __FUNCTION__ << "()"
           << " content_length = \""
           << response->headers->GetContentLength() << "\n\""
           << " headers = \"" << GetResponseHeaderLines(*response->headers)
           << "\"";
  return OK;
}

}  // namespace net
