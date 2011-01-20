// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Simple implementation of a data: protocol handler.

#include "net/url_request/url_request_data_job.h"

#include "net/base/data_url.h"

namespace net {

URLRequestDataJob::URLRequestDataJob(URLRequest* request)
    : URLRequestSimpleJob(request) {
}

// static
URLRequestJob* URLRequestDataJob::Factory(URLRequest* request,
                                          const std::string& scheme) {
  return new URLRequestDataJob(request);
}

bool URLRequestDataJob::GetData(std::string* mime_type,
                                std::string* charset,
                                std::string* data) const {
  // Check if data URL is valid. If not, don't bother to try to extract data.
  // Otherwise, parse the data from the data URL.
  const GURL& url = request_->url();
  if (!url.is_valid())
    return false;
  return DataURL::Parse(url, mime_type, charset, data);
}

URLRequestDataJob::~URLRequestDataJob() {
}

}  // namespace net
