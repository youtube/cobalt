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

// Simple implementation of a data: protocol handler.

#include "cobalt/network/custom/url_request_data_job.h"

#include "net/base/data_url.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"
#include "url/gurl.h"

namespace net {

int URLRequestDataJob::BuildResponse(const GURL& url, std::string* mime_type,
                                     std::string* charset, std::string* data,
                                     HttpResponseHeaders* headers) {
  if (!DataURL::Parse(url, mime_type, charset, data)) return ERR_INVALID_URL;

  // |mime_type| set by DataURL::Parse() is guaranteed to be in
  //     token "/" token
  // form. |charset| can be an empty string.

  DCHECK(!mime_type->empty());

  if (headers) {
    headers->ReplaceStatusLine("HTTP/1.1 200 OK");
    // "charset" in the Content-Type header is specified explicitly to follow
    // the "token" ABNF in the HTTP spec. When DataURL::Parse() call is
    // successful, it's guaranteed that the string in |charset| follows the
    // "token" ABNF.
    std::string content_type_header = *mime_type;
    if (!charset->empty()) content_type_header.append(";charset=" + *charset);
    headers->AddHeader("Content-Type", content_type_header);
  }

  return OK;
}

URLRequestDataJob::URLRequestDataJob(URLRequest* request)
    : URLRequestSimpleJob(request) {}

int URLRequestDataJob::GetData(std::string* mime_type, std::string* charset,
                               std::string* data,
                               CompletionOnceCallback callback) const {
  // Check if data URL is valid. If not, don't bother to try to extract data.
  // Otherwise, parse the data from the data URL.
  const GURL& url = request_->url();
  if (!url.is_valid()) return ERR_INVALID_URL;

  // TODO(tyoshino): Get the headers and export via
  // URLRequestJob::GetResponseInfo().
  return BuildResponse(url, mime_type, charset, data, NULL);
}

URLRequestDataJob::~URLRequestDataJob() = default;

}  // namespace net
