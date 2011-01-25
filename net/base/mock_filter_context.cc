// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/mock_filter_context.h"

namespace net {

MockFilterContext::MockFilterContext(int buffer_size)
    : buffer_size_(buffer_size),
      is_cached_content_(false),
      is_download_(false),
      is_sdch_response_(false),
      response_code_(-1) {
}

MockFilterContext::~MockFilterContext() {}

bool MockFilterContext::GetMimeType(std::string* mime_type) const {
  *mime_type = mime_type_;
  return true;
}

// What URL was used to access this data?
// Return false if gurl is not present.
bool MockFilterContext::GetURL(GURL* gurl) const {
  *gurl = gurl_;
  return true;
}

// What was this data requested from a server?
base::Time MockFilterContext::GetRequestTime() const {
  return request_time_;
}

bool MockFilterContext::IsCachedContent() const { return is_cached_content_; }

bool MockFilterContext::IsDownload() const { return is_download_; }

bool MockFilterContext::IsSdchResponse() const { return is_sdch_response_; }

int64 MockFilterContext::GetByteReadCount() const { return 0; }

int MockFilterContext::GetResponseCode() const { return response_code_; }

int MockFilterContext::GetInputStreamBufferSize() const { return buffer_size_; }

}  // namespace net
