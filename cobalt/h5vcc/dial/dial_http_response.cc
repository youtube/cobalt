// Copyright 2016 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <memory>
#include <utility>

#include "cobalt/h5vcc/dial/dial_http_response.h"

#include "base/strings/stringprintf.h"
#include "net/server/http_server_response_info.h"

namespace cobalt {
namespace h5vcc {
namespace dial {
DialHttpResponse::DialHttpResponse(const std::string& path,
                                   const std::string& method)
    : path_(path), method_(method), response_code_(500) {}

void DialHttpResponse::AddHeader(const std::string& header,
                                 const std::string& value) {
  if (!info_) {
    info_.reset(new net::HttpServerResponseInfo());
  }
  info_->AddHeader(header, value);
}

std::unique_ptr<net::HttpServerResponseInfo>
DialHttpResponse::ToHttpServerResponseInfo() {
  if (!info_) {
    info_.reset(new net::HttpServerResponseInfo());
  }
  info_->SetStatusCode(net::HttpStatusCode(response_code_));
  info_->SetBody(body_, mime_type_);
  return std::move(info_);
}

}  // namespace dial
}  // namespace h5vcc
}  // namespace cobalt
