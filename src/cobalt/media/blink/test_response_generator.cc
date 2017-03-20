// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cobalt/media/blink/test_response_generator.h"

#include <string>

#include "base/format_macros.h"
#include "base/string_number_conversions.h"
#include "base/stringprintf.h"
#include "net/base/net_errors.h"
#include "third_party/WebKit/public/platform/WebString.h"

using blink::WebString;
using blink::WebURLError;
using blink::WebURLResponse;

namespace media {

TestResponseGenerator::TestResponseGenerator(const GURL& gurl,
                                             int64_t content_length)
    : gurl_(gurl), content_length_(content_length) {}

WebURLError TestResponseGenerator::GenerateError() {
  WebURLError error;
  error.reason = net::ERR_ABORTED;
  error.domain = WebString::fromUTF8(net::kErrorDomain);
  return error;
}

WebURLResponse TestResponseGenerator::Generate200() {
  WebURLResponse response(gurl_);
  response.setHTTPStatusCode(200);

  response.setHTTPHeaderField(
      WebString::fromUTF8("Content-Length"),
      WebString::fromUTF8(base::Int64ToString(content_length_)));
  response.setExpectedContentLength(content_length_);
  return response;
}

WebURLResponse TestResponseGenerator::Generate206(int64_t first_byte_offset) {
  return GeneratePartial206(first_byte_offset, content_length_ - 1, kNormal);
}

WebURLResponse TestResponseGenerator::Generate206(int64_t first_byte_offset,
                                                  Flags flags) {
  return GeneratePartial206(first_byte_offset, content_length_ - 1, flags);
}

WebURLResponse TestResponseGenerator::GeneratePartial206(
    int64_t first_byte_offset, int64_t last_byte_offset) {
  return GeneratePartial206(first_byte_offset, last_byte_offset, kNormal);
}

WebURLResponse TestResponseGenerator::GeneratePartial206(
    int64_t first_byte_offset, int64_t last_byte_offset, Flags flags) {
  int64_t range_content_length = content_length_ - first_byte_offset;

  WebURLResponse response(gurl_);
  response.setHTTPStatusCode(206);

  if ((flags & kNoAcceptRanges) == 0) {
    response.setHTTPHeaderField(WebString::fromUTF8("Accept-Ranges"),
                                WebString::fromUTF8("bytes"));
  }

  if ((flags & kNoContentRange) == 0) {
    std::string content_range = base::StringPrintf(
        "bytes %" PRId64 "-%" PRId64 "/", first_byte_offset, last_byte_offset);
    if (flags & kNoContentRangeInstanceSize)
      content_range += "*";
    else
      content_range += base::StringPrintf("%" PRId64, content_length_);
    response.setHTTPHeaderField(WebString::fromUTF8("Content-Range"),
                                WebString::fromUTF8(content_range));
  }

  if ((flags & kNoContentLength) == 0) {
    response.setHTTPHeaderField(
        WebString::fromUTF8("Content-Length"),
        WebString::fromUTF8(base::Int64ToString(range_content_length)));
    response.setExpectedContentLength(range_content_length);
  }
  return response;
}

WebURLResponse TestResponseGenerator::GenerateResponse(int code) {
  WebURLResponse response(gurl_);
  response.setHTTPStatusCode(code);
  return response;
}

WebURLResponse TestResponseGenerator::Generate404() {
  return GenerateResponse(404);
}

WebURLResponse TestResponseGenerator::GenerateFileResponse(
    int64_t first_byte_offset) {
  WebURLResponse response(gurl_);
  response.setHTTPStatusCode(0);

  if (first_byte_offset >= 0) {
    response.setExpectedContentLength(content_length_ - first_byte_offset);
  } else {
    response.setExpectedContentLength(-1);
  }
  return response;
}

}  // namespace media
