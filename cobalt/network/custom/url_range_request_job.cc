// Copyright 2024 The Cobalt Authors. All Rights Reserved.
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
//
// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cobalt/network/custom/url_range_request_job.h"

#include <string>

#include "net/base/net_errors.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_util.h"

namespace net {

URLRangeRequestJob::URLRangeRequestJob(URLRequest* request)
    : URLRequestJob(request), range_parse_result_(OK) {}

URLRangeRequestJob::~URLRangeRequestJob() = default;

void URLRangeRequestJob::SetExtraRequestHeaders(
    const HttpRequestHeaders& headers) {
  std::string range_header;
  if (headers.GetHeader(HttpRequestHeaders::kRange, &range_header)) {
    if (!HttpUtil::ParseRangeHeader(range_header, &ranges_)) {
      range_parse_result_ = ERR_REQUEST_RANGE_NOT_SATISFIABLE;
    }
  }
}

}  // namespace net
