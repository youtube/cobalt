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

#ifndef COBALT_NETWORK_CUSTOM_URL_RANGE_REQUEST_JOB_H_
#define COBALT_NETWORK_CUSTOM_URL_RANGE_REQUEST_JOB_H_

#include <vector>

#include "net/base/net_export.h"
#include "net/http/http_byte_range.h"
#include "net/url_request/url_request_job.h"

namespace net {

class HttpRequestHeaders;

// URLRequestJob with support for parsing range requests.
// It is up to subclasses to handle the response
// and deal with an errors parsing the range request header.
// This must be done after Start() has been called.
class NET_EXPORT URLRangeRequestJob : public URLRequestJob {
 public:
  explicit URLRangeRequestJob(URLRequest* request);

  void SetExtraRequestHeaders(const HttpRequestHeaders& headers) override;

  const std::vector<HttpByteRange>& ranges() const { return ranges_; }
  int range_parse_result() const { return range_parse_result_; }

 protected:
  ~URLRangeRequestJob() override;

 private:
  std::vector<HttpByteRange> ranges_;
  int range_parse_result_;
};

}  // namespace net

#endif  // COBALT_NETWORK_CUSTOM_URL_RANGE_REQUEST_JOB_H_
