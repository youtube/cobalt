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
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COBALT_NETWORK_CUSTOM_URL_REQUEST_DATA_JOB_H_
#define COBALT_NETWORK_CUSTOM_URL_REQUEST_DATA_JOB_H_

#include <string>

#include "base/macros.h"
#include "cobalt/network/custom/url_request_simple_job.h"
#include "net/base/completion_once_callback.h"
#include "net/base/net_export.h"
#include "net/url_request/url_request.h"

class GURL;

namespace net {

class HttpResponseHeaders;
class URLRequest;

class NET_EXPORT URLRequestDataJob : public URLRequestSimpleJob {
 public:
  // Extracts info from a data scheme URL. Returns OK if successful. Returns
  // ERR_INVALID_URL otherwise.
  static int BuildResponse(const GURL& url, std::string* mime_type,
                           std::string* charset, std::string* data,
                           HttpResponseHeaders* headers);

  explicit URLRequestDataJob(URLRequest* request);
  ~URLRequestDataJob() override;
  URLRequestDataJob(const URLRequestDataJob&) = delete;
  URLRequestDataJob& operator=(const URLRequestDataJob&) = delete;


  // URLRequestSimpleJob
  int GetData(std::string* mime_type, std::string* charset, std::string* data,
              CompletionOnceCallback callback) const override;
};

}  // namespace net

#endif  // COBALT_NETWORK_CUSTOM_URL_REQUEST_DATA_JOB_H_
