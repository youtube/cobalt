/*
 * Copyright 2016 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "cobalt/h5vcc/dial/dial_http_response.h"

#include "base/stringprintf.h"

namespace cobalt {
namespace h5vcc {
namespace dial {
DialHttpResponse::DialHttpResponse(const std::string& path,
                                   const std::string& method)
    : path_(path), method_(method), response_code_(0) {}

void DialHttpResponse::AddHeader(const std::string& header,
                                 const std::string& value) {
  headers_.push_back(
      base::StringPrintf("%s: %s", header.c_str(), value.c_str()));
}

scoped_ptr<net::HttpServerResponseInfo>
DialHttpResponse::ToHttpServerResponseInfo() const {
  scoped_ptr<net::HttpServerResponseInfo> info(
      new net::HttpServerResponseInfo());
  info->response_code = response_code_;
  info->mime_type = mime_type_;
  info->body = body_;
  info->headers = headers_;
  return info.Pass();
}

}  // namespace dial
}  // namespace h5vcc
}  // namespace cobalt
